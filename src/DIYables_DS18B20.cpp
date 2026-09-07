/*
 * Copyright (c) 2026, DIYables.io. All rights reserved.
 */

#include "DIYables_DS18B20.h"

// 1-Wire ROM commands
#define DS18B20_CMD_SEARCH_ROM   0xF0
#define DS18B20_CMD_READ_ROM     0x33
#define DS18B20_CMD_MATCH_ROM    0x55
#define DS18B20_CMD_SKIP_ROM     0xCC

// DS18B20 function commands
#define DS18B20_CMD_CONVERT_T        0x44
#define DS18B20_CMD_WRITE_SCRATCHPAD 0x4E
#define DS18B20_CMD_READ_SCRATCHPAD  0xBE
#define DS18B20_CMD_COPY_SCRATCHPAD  0x48
#define DS18B20_CMD_READ_POWER       0xB4

// Number of GPIO calls used to measure the speed of the board
#define DS18B20_CALIBRATION_LOOPS  16

// The conversion is polled on the bus at most once every this many ms
#define DS18B20_POLL_PERIOD  10

// Waits between two time slots. They are minimums, a longer wait is
// always allowed, which is what lets the sketch run in between
#define DS18B20_RESET_LOW_US     500
#define DS18B20_RESET_RECOVER_US 410
#define DS18B20_WRITE_ONE_TAIL_US 60
#define DS18B20_WRITE_ZERO_TAIL_US 10
#define DS18B20_READ_TAIL_US     55
#define DS18B20_BUS_IDLE_TIMEOUT_US 250


DIYables_DS18B20::DIYables_DS18B20(int pin) {
    _pin        = pin;
    _useAddress = false;
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        _address[i] = 0;

    _state           = STATE_IDLE;
    _conversionStart = 0;
    _conversionTime  = 800;
    _lastPollTime    = 0;
    _lastReadTime    = 0;
    _interval        = DS18B20_DEFAULT_INTERVAL;

    _autoMode     = true;
    _didWork      = false;
    _newData      = false;
    _connected    = false;
    _valid        = false;
    _parasite     = false;
    _strongPullup = false;

    _resolution = DS18B20_RESOLUTION_12BIT;
    _raw        = 0;
    for (uint8_t i = 0; i < 9; i++)
        _scratchpad[i] = 0;

    _job           = JOB_NONE;
    _step          = XFER_IDLE;
    _stepStart     = 0;
    _stepWait      = 0;
    _jobStart      = 0;
    _txLength      = 0;
    _txIndex       = 0;
    _rxLength      = 0;
    _rxIndex       = 0;
    _bitIndex      = 0;
    _shiftRegister = 0;
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE + 2; i++)
        _txBuffer[i] = 0;

    // Safe defaults, replaced by calibrateTiming() in begin()
    _gpioUs         = 1;
    _readLowUs      = 3;
    _readSampleUs   = 7;
    _writeOneLowUs  = 5;
    _writeZeroLowUs = 59;
    _presenceDelayUs = 69;
    _maxLoopUs      = 100;

    resetSearch();
}

DIYables_DS18B20::DIYables_DS18B20(int pin, const uint8_t *address) {
    // Reuse the single sensor constructor, then attach the ROM address
    *this = DIYables_DS18B20(pin);
    setAddress(address);
}

bool DIYables_DS18B20::begin() {
    digitalWrite(_pin, LOW);
    pinMode(_pin, INPUT);

    calibrateTiming();

    _job   = JOB_NONE;
    _step  = XFER_IDLE;
    _state = STATE_IDLE;

    // The first reading in automatic mode starts on the first call of loop()
    _lastReadTime = millis() - _interval;
    _lastPollTime = millis();
    _newData      = false;

    if (!reset())
        return false;

    detectPowerSupply();

    // Cache the resolution the sensor is currently configured with
    readScratchpad();

    return _connected;
}

/* ------------------------------------------------------------------
 * Bit timing
 *
 * digitalWrite(), pinMode() and digitalRead() are fast on a 240 MHz
 * ESP32 but cost several microseconds on a 16 MHz AVR. The delays of
 * the read and write time slots are therefore reduced by the measured
 * cost of a single digital I/O call, so the same code meets the 1-Wire
 * timing on both slow and fast boards.
 * ------------------------------------------------------------------ */
void DIYables_DS18B20::calibrateTiming() {
    volatile uint8_t sink = 0;
    unsigned long start, elapsed, perCall;

    pinMode(_pin, INPUT);

    noInterrupts();
    start = micros();
    for (uint8_t i = 0; i < DS18B20_CALIBRATION_LOOPS; i++) {
        pinMode(_pin, INPUT);
        sink = (uint8_t)(sink + (uint8_t)digitalRead(_pin));
    }
    elapsed = micros() - start;
    interrupts();
    (void)sink;

    // Two digital I/O calls per loop, rounded up so the delays stay short
    perCall = (elapsed + (2UL * DS18B20_CALIBRATION_LOOPS) - 1) / (2UL * DS18B20_CALIBRATION_LOOPS);
    if (perCall > 8)
        perCall = 8;
    _gpioUs = (uint8_t)perCall;

    /* The targets below sit in the middle of each window the datasheet
     * allows, not on its edge. Two things eat into a delay and both are
     * paid for here: a digital I/O call costs _gpioUs before the pin
     * actually moves, and delayMicroseconds() itself runs about 1 us
     * short on AVR. Aiming at a spec minimum leaves nothing for either,
     * which puts the pulse under the limit.
     */

    // Read slot: low about 5 us, sampled about 12 us after the falling
    // edge. The sample has to happen within 15 us of that edge
    _readLowUs = (_gpioUs >= 4) ? 1 : (uint8_t)(5 - _gpioUs);

    int16_t sample = (int16_t)12 - (int16_t)(2 * _gpioUs) - (int16_t)_readLowUs;
    _readSampleUs = (sample < 1) ? 1 : (uint8_t)sample;

    // Write 1 slot: low about 8 us, the window is 1 to 15 us
    _writeOneLowUs = (_gpioUs >= 7) ? 1 : (uint8_t)(8 - _gpioUs);

    // Write 0 slot: low about 65 us, the window is 60 to 120 us. The
    // sensor samples the line up to 60 us after the falling edge, so a
    // pulse any shorter can be read back as a one
    _writeZeroLowUs = (_gpioUs >= 60) ? 5 : (uint8_t)(65 - _gpioUs);

    // Presence window: the sensor answers a reset 15 to 60 us after the
    // bus is released and holds the line down for 60 to 240 us, so the
    // guaranteed low window is 60 to 75 us and the bus must be sampled in
    // the middle of it, at 70 us. The digital read costs _gpioUs before
    // the pin is actually sampled, so the delay has to be that much less
    _presenceDelayUs = (_gpioUs >= 60) ? 10 : (uint8_t)(70 - _gpioUs);

    /* Worst case of one call of loop().
     *
     * Only one unit of work is ever done per call, so the worst case is
     * the longest of them. Two candidates can win, depending on the board:
     *
     *   the presence window   the delay plus two digital I/O calls
     *   the CRC of a reading  no I/O at all, pure CPU, so it is timed here
     *
     * The write zero slot is always shorter than the presence window and
     * the read slot is shorter again, so neither can be the worst case.
     */
    unsigned long slotWorst = (unsigned long)_presenceDelayUs + 2UL * (unsigned long)_gpioUs;

    uint8_t pattern[9] = {0x28, 0xFF, 0x64, 0x1E, 0x0F, 0x2C, 0x1A, 0x9B, 0x5A};
    start = micros();
    for (uint8_t i = 0; i < 4; i++)
        sink = (uint8_t)(sink + crc8(pattern, 8));
    unsigned long crcWorst = (micros() - start + 3) / 4;

    if (crcWorst > slotWorst)
        slotWorst = crcWorst;

    // Margin for micros(), the switch dispatch and the bookkeeping
    _maxLoopUs = slotWorst + 15;
}

void DIYables_DS18B20::driveLow() {
    digitalWrite(_pin, LOW);
    pinMode(_pin, OUTPUT);
}

void DIYables_DS18B20::releaseBus() {
    pinMode(_pin, INPUT);
}

void DIYables_DS18B20::applyStrongPullup() {
    digitalWrite(_pin, HIGH);
    pinMode(_pin, OUTPUT);
    _strongPullup = true;
}

void DIYables_DS18B20::releaseStrongPullup() {
    if (_strongPullup) {
        pinMode(_pin, INPUT);
        digitalWrite(_pin, LOW);
        _strongPullup = false;
    }
}

/* ------------------------------------------------------------------
 * Atomic time slots
 *
 * These three are the only parts of the protocol that cannot be split.
 * A read has to be sampled within 15 us of the falling edge and a zero
 * has to be released before 120 us, so there is nowhere to hand control
 * back to the sketch inside them. Everything else in this file is a
 * minimum wait with no upper limit and is handed back.
 * ------------------------------------------------------------------ */
uint8_t DIYables_DS18B20::slotPresence() {
    uint8_t presence;

    noInterrupts();
    releaseBus();
    delayMicroseconds(_presenceDelayUs);
    presence = (digitalRead(_pin) == LOW) ? 1 : 0;
    interrupts();

    return presence;
}

void DIYables_DS18B20::slotWriteBit(uint8_t bit) {
    noInterrupts();
    driveLow();
    delayMicroseconds(bit ? _writeOneLowUs : _writeZeroLowUs);
    releaseBus();
    interrupts();
}

uint8_t DIYables_DS18B20::slotReadBit() {
    uint8_t bit;

    noInterrupts();
    driveLow();
    delayMicroseconds(_readLowUs);
    releaseBus();
    delayMicroseconds(_readSampleUs);
    bit = (digitalRead(_pin) == HIGH) ? 1 : 0;
    interrupts();

    return bit;
}

/* ------------------------------------------------------------------
 * Blocking primitives
 *
 * Used by begin() and by the helpers that belong in setup(), where
 * waiting a few milliseconds once does no harm. The reading path in
 * loop() never calls these.
 * ------------------------------------------------------------------ */
bool DIYables_DS18B20::reset() {
    unsigned long start;

    releaseStrongPullup();
    releaseBus();

    start = micros();
    while (digitalRead(_pin) == LOW) {
        if (micros() - start > DS18B20_BUS_IDLE_TIMEOUT_US) {
            _connected = false;
            return false;
        }
    }

    driveLow();
    delayMicroseconds(DS18B20_RESET_LOW_US);

    _connected = (slotPresence() == 1);
    delayMicroseconds(DS18B20_RESET_RECOVER_US);

    return _connected;
}

void DIYables_DS18B20::writeByte(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = (uint8_t)(value & 0x01);
        slotWriteBit(bit);
        delayMicroseconds(bit ? DS18B20_WRITE_ONE_TAIL_US : DS18B20_WRITE_ZERO_TAIL_US);
        value >>= 1;
    }
}

uint8_t DIYables_DS18B20::readByte() {
    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; i++) {
        if (slotReadBit())
            value |= (uint8_t)(1 << i);
        delayMicroseconds(DS18B20_READ_TAIL_US);
    }
    return value;
}

uint8_t DIYables_DS18B20::crc8(const uint8_t *data, uint8_t length) {
    uint8_t crc = 0;

    while (length--) {
        uint8_t inbyte = *data++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (uint8_t)((crc ^ inbyte) & 0x01);
            crc >>= 1;
            if (mix)
                crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

bool DIYables_DS18B20::selectDevice() {
    if (!reset())
        return false;

    if (_useAddress) {
        writeByte(DS18B20_CMD_MATCH_ROM);
        for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
            writeByte(_address[i]);
    } else {
        writeByte(DS18B20_CMD_SKIP_ROM);
    }
    return true;
}

bool DIYables_DS18B20::readScratchpad() {
    if (!selectDevice())
        return false;

    writeByte(DS18B20_CMD_READ_SCRATCHPAD);
    for (uint8_t i = 0; i < 9; i++)
        _scratchpad[i] = readByte();

    return validateScratchpad();
}

bool DIYables_DS18B20::writeScratchpad(uint8_t th, uint8_t tl, uint8_t config) {
    if (!selectDevice())
        return false;

    writeByte(DS18B20_CMD_WRITE_SCRATCHPAD);
    writeByte(th);
    writeByte(tl);
    writeByte(config);
    return true;
}

void DIYables_DS18B20::detectPowerSupply() {
    if (!selectDevice()) {
        _parasite = false;
        return;
    }

    writeByte(DS18B20_CMD_READ_POWER);
    // A parasite powered sensor pulls the bus low to answer
    _parasite = (slotReadBit() == 0);
    delayMicroseconds(DS18B20_READ_TAIL_US);
    reset();
}

bool DIYables_DS18B20::validateScratchpad() {
    uint8_t allSame = 1;

    // A missing sensor reads as all ones, a shorted bus as all zeros
    for (uint8_t i = 1; i < 9; i++) {
        if (_scratchpad[i] != _scratchpad[0])
            allSame = 0;
    }
    if (allSame)
        return false;

    if (crc8(_scratchpad, 8) != _scratchpad[8])
        return false;

    _resolution = (uint8_t)(((_scratchpad[4] >> 5) & 0x03) + 9);
    updateConversionTime();
    return true;
}

void DIYables_DS18B20::storeTemperature() {
    int16_t raw = (int16_t)(((uint16_t)_scratchpad[1] << 8) | (uint16_t)_scratchpad[0]);

    // The unused low bits are undefined below 12 bit resolution
    switch (_resolution) {
        case DS18B20_RESOLUTION_9BIT:  raw &= ~((int16_t)7); break;
        case DS18B20_RESOLUTION_10BIT: raw &= ~((int16_t)3); break;
        case DS18B20_RESOLUTION_11BIT: raw &= ~((int16_t)1); break;
        default: break;
    }

    _raw = raw;
}

void DIYables_DS18B20::updateConversionTime() {
    // Datasheet maximum plus a small margin
    switch (_resolution) {
        case DS18B20_RESOLUTION_9BIT:  _conversionTime = 100; break;
        case DS18B20_RESOLUTION_10BIT: _conversionTime = 200; break;
        case DS18B20_RESOLUTION_11BIT: _conversionTime = 400; break;
        default:                       _conversionTime = 800; break;
    }
}

/* ------------------------------------------------------------------
 * Transfer state machine
 *
 * A transfer is always the same shape: a reset, then _txLength bytes
 * out, then _rxLength bytes in. One call of service() performs at most
 * one atomic time slot and then returns, so the longest the sketch is
 * ever held up is a single slot.
 * ------------------------------------------------------------------ */
void DIYables_DS18B20::buildCommand(uint8_t command) {
    _txLength = 0;

    if (_useAddress) {
        _txBuffer[_txLength++] = DS18B20_CMD_MATCH_ROM;
        for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
            _txBuffer[_txLength++] = _address[i];
    } else {
        _txBuffer[_txLength++] = DS18B20_CMD_SKIP_ROM;
    }

    _txBuffer[_txLength++] = command;
}

void DIYables_DS18B20::startWait(unsigned long microseconds) {
    _stepStart = micros();
    _stepWait  = microseconds;
}

void DIYables_DS18B20::beginTransfer(uint8_t job) {
    _job           = job;
    _step          = XFER_BUS_IDLE;
    _jobStart      = micros();
    _txIndex       = 0;
    _rxIndex       = 0;
    _bitIndex      = 0;
    _shiftRegister = _txBuffer[0];
    startWait(0);
}

void DIYables_DS18B20::startConvert() {
    releaseStrongPullup();
    buildCommand(DS18B20_CMD_CONVERT_T);
    _rxLength = 0;
    beginTransfer(JOB_CONVERT);
}

void DIYables_DS18B20::startRead() {
    releaseStrongPullup();
    buildCommand(DS18B20_CMD_READ_SCRATCHPAD);
    _rxLength = 9;
    beginTransfer(JOB_READ);
}

void DIYables_DS18B20::abortTransfer() {
    releaseBus();
    _job   = JOB_NONE;
    _step  = XFER_IDLE;
    _state = STATE_IDLE;
    _valid = false;
}

bool DIYables_DS18B20::finishTransfer() {
    uint8_t job = _job;

    _job  = JOB_NONE;
    _step = XFER_IDLE;

    if (job == JOB_CONVERT) {
        _conversionStart = millis();
        _lastPollTime    = millis();
        _state           = STATE_CONVERTING;
        return false;
    }

    _state = STATE_IDLE;

    if (!validateScratchpad()) {
        _valid = false;
        return false;
    }

    storeTemperature();
    _valid   = true;
    _newData = true;
    return true;
}

bool DIYables_DS18B20::service() {
    _didWork = false;

    if (_job == JOB_NONE)
        return false;

    // Every wait between two time slots is a minimum with no maximum, so
    // a late call is always safe. A slow sketch makes the reading take
    // longer, it never breaks the protocol.
    if (micros() - _stepStart < _stepWait)
        return false;

    // One unit of work per call and no more, this is what makes the
    // worst case of loop() a single time slot
    _didWork = true;

    switch (_step) {
        case XFER_BUS_IDLE:
            // Nothing may drive the bus low before the reset pulse
            if (digitalRead(_pin) == HIGH) {
                driveLow();
                _step = XFER_RESET_LOW;
                startWait(DS18B20_RESET_LOW_US);
            } else if (micros() - _jobStart > DS18B20_BUS_IDLE_TIMEOUT_US) {
                _connected = false;
                abortTransfer();
            }
            break;

        case XFER_RESET_LOW:
            // The 480 us low is over, look for the presence pulse
            _connected = (slotPresence() == 1);
            if (!_connected) {
                abortTransfer();
                break;
            }
            _step = XFER_RESET_RECOVER;
            startWait(DS18B20_RESET_RECOVER_US);
            break;

        case XFER_RESET_RECOVER:
            _step          = XFER_WRITE_BIT;
            _txIndex       = 0;
            _bitIndex      = 0;
            _shiftRegister = _txBuffer[0];
            startWait(0);
            break;

        case XFER_WRITE_BIT: {
            uint8_t bit  = (uint8_t)(_shiftRegister & 0x01);
            bool lastBit = (_bitIndex == 7) && (_txIndex + 1 >= _txLength);

            // A parasite powered sensor has to be fed within 10 us of the
            // end of Convert T, so the pull-up goes on inside the slot
            bool needPullup = lastBit && (_job == JOB_CONVERT) && _parasite;

            _shiftRegister >>= 1;

            noInterrupts();
            driveLow();
            delayMicroseconds(bit ? _writeOneLowUs : _writeZeroLowUs);
            if (needPullup) {
                digitalWrite(_pin, HIGH);
                pinMode(_pin, OUTPUT);
            } else {
                releaseBus();
            }
            interrupts();

            if (needPullup)
                _strongPullup = true;

            startWait(bit ? DS18B20_WRITE_ONE_TAIL_US : DS18B20_WRITE_ZERO_TAIL_US);

            _bitIndex++;
            if (_bitIndex >= 8) {
                _bitIndex = 0;
                _txIndex++;

                if (_txIndex >= _txLength) {
                    if (_rxLength > 0) {
                        _step          = XFER_READ_BIT;
                        _rxIndex       = 0;
                        _shiftRegister = 0;
                    } else {
                        _step = XFER_COMPLETE;
                    }
                } else {
                    _shiftRegister = _txBuffer[_txIndex];
                }
            }
            break;
        }

        case XFER_READ_BIT: {
            uint8_t bit = slotReadBit();
            startWait(DS18B20_READ_TAIL_US);

            if (bit)
                _shiftRegister |= (uint8_t)(1 << _bitIndex);

            _bitIndex++;
            if (_bitIndex >= 8) {
                _scratchpad[_rxIndex] = _shiftRegister;
                _shiftRegister        = 0;
                _bitIndex             = 0;
                _rxIndex++;

                if (_rxIndex >= _rxLength)
                    _step = XFER_COMPLETE;
            }
            break;
        }

        case XFER_COMPLETE:
            return finishTransfer();

        default:
            abortTransfer();
            break;
    }

    return false;
}

/* ------------------------------------------------------------------
 * Public non blocking interface
 * ------------------------------------------------------------------ */
bool DIYables_DS18B20::loop() {
    bool landed = service();

    // While a transfer owns the bus nothing else may be started, and a
    // call that has already done its one unit of work stops here so the
    // worst case stays at a single time slot
    if (_job != JOB_NONE || _didWork)
        return landed;

    switch (_state) {
        case STATE_IDLE:
            if (_autoMode && (millis() - _lastReadTime >= _interval)) {
                // The period is counted from the start of one reading to
                // the start of the next, so the interval is the real period
                _lastReadTime = millis();
                startConvert();
            }
            break;

        case STATE_CONVERTING:
            if (_autoMode && isConversionDone())
                startRead();
            break;

        default:
            _state = STATE_IDLE;
            break;
    }

    return landed;
}

void DIYables_DS18B20::setAutoMode(bool enabled) {
    _autoMode = enabled;
}

bool DIYables_DS18B20::isAutoMode() {
    return _autoMode;
}

bool DIYables_DS18B20::isBusy() {
    return _job != JOB_NONE;
}

unsigned long DIYables_DS18B20::getMaxLoopTime() {
    return _maxLoopUs;
}

bool DIYables_DS18B20::requestTemperature() {
    if (_job != JOB_NONE)
        return false;

    startConvert();
    return true;
}

bool DIYables_DS18B20::isConversionDone() {
    // A transfer owns the bus, either the Convert T command is still
    // going out or the scratchpad is being read back. Polling the bus
    // here would cut into that transfer
    if (_job != JOB_NONE)
        return false;

    if (_state != STATE_CONVERTING)
        return true;

    if (millis() - _conversionStart >= _conversionTime)
        return true;

    // A sensor with its own power supply answers a read slot with 0 while
    // it is busy and with 1 when the conversion is finished. This must not
    // be done in parasite mode, the line has to stay high there.
    if (!_parasite) {
        if (millis() - _lastPollTime >= DS18B20_POLL_PERIOD) {
            uint8_t done;

            _lastPollTime = millis();
            done = slotReadBit();

            // The slot has to be finished before the bus is touched again.
            // The poll that answers 1 is followed straight away by the
            // reset of the scratchpad read, so the tail cannot be skipped
            delayMicroseconds(DS18B20_READ_TAIL_US);

            if (done)
                return true;
        }
    }

    return false;
}

bool DIYables_DS18B20::readTemperature() {
    if (_job != JOB_NONE)
        return false;

    if (_state == STATE_CONVERTING && !isConversionDone())
        return false;

    startRead();
    return true;
}

bool DIYables_DS18B20::isTemperatureReady() {
    if (_newData) {
        _newData = false;
        return true;
    }
    return false;
}

void DIYables_DS18B20::setInterval(unsigned long intervalMs) {
    _interval = intervalMs;
}

unsigned long DIYables_DS18B20::getInterval() {
    return _interval;
}

/* ------------------------------------------------------------------
 * Results
 * ------------------------------------------------------------------ */
float DIYables_DS18B20::getTemperatureC() {
    if (!_valid)
        return (float)DS18B20_INVALID_TEMPERATURE;
    return (float)_raw / 16.0;
}

float DIYables_DS18B20::getTemperatureF() {
    if (!_valid)
        return (float)DS18B20_INVALID_TEMPERATURE;
    return getTemperatureC() * 1.8 + 32.0;
}

int16_t DIYables_DS18B20::getRawTemperature() {
    return _raw;
}

bool DIYables_DS18B20::isValid() {
    return _valid;
}

bool DIYables_DS18B20::isConnected() {
    return _connected;
}

/* ------------------------------------------------------------------
 * Configuration, for setup()
 * ------------------------------------------------------------------ */
bool DIYables_DS18B20::setResolution(uint8_t bits, bool saveToEeprom) {
    uint8_t config;
    uint8_t th, tl;

    // The bus is shared, a blocking helper may not cut into a transfer
    if (_job != JOB_NONE || _state == STATE_CONVERTING)
        return false;

    if (bits < DS18B20_RESOLUTION_9BIT)
        bits = DS18B20_RESOLUTION_9BIT;
    if (bits > DS18B20_RESOLUTION_12BIT)
        bits = DS18B20_RESOLUTION_12BIT;

    // The alarm registers share the same command, so keep their values
    if (!readScratchpad())
        return false;

    th = _scratchpad[2];
    tl = _scratchpad[3];
    config = (uint8_t)(0x1F | ((bits - 9) << 5));

    if (!writeScratchpad(th, tl, config))
        return false;

    if (saveToEeprom) {
        if (!selectDevice())
            return false;
        writeByte(DS18B20_CMD_COPY_SCRATCHPAD);
        if (_parasite)
            applyStrongPullup();
        // The internal EEPROM needs up to 10 ms. This is the only place in
        // the library that waits on a millisecond scale, and it is meant
        // to be used from setup()
        delay(20);
        releaseStrongPullup();
    }

    _resolution = bits;
    updateConversionTime();
    return true;
}

uint8_t DIYables_DS18B20::getResolution() {
    return _resolution;
}

unsigned long DIYables_DS18B20::getConversionTime() {
    return _conversionTime;
}

bool DIYables_DS18B20::isParasitePower() {
    return _parasite;
}

/* ------------------------------------------------------------------
 * ROM address
 * ------------------------------------------------------------------ */
void DIYables_DS18B20::setAddress(const uint8_t *address) {
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        _address[i] = address[i];
    _useAddress = true;
}

void DIYables_DS18B20::useSkipRom() {
    _useAddress = false;
}

bool DIYables_DS18B20::getAddress(uint8_t *address) {
    if (!_useAddress)
        return false;
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        address[i] = _address[i];
    return true;
}

bool DIYables_DS18B20::readAddress(uint8_t *address) {
    uint8_t rom[DS18B20_ADDRESS_SIZE];

    if (_job != JOB_NONE || _state == STATE_CONVERTING)
        return false;

    // Read ROM works only when a single sensor is connected to the pin
    if (!reset())
        return false;

    writeByte(DS18B20_CMD_READ_ROM);
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        rom[i] = readByte();

    if (crc8(rom, 7) != rom[7])
        return false;

    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        address[i] = rom[i];
    return true;
}

void DIYables_DS18B20::resetSearch() {
    _searchLastDiscrepancy = 0;
    _searchDone            = false;
    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        _searchAddress[i] = 0;
}

bool DIYables_DS18B20::searchNext(uint8_t *address) {
    uint8_t lastZero = 0;

    if (_job != JOB_NONE || _state == STATE_CONVERTING)
        return false;

    if (_searchDone)
        return false;

    if (!reset()) {
        _searchDone            = true;
        _searchLastDiscrepancy = 0;
        return false;
    }

    writeByte(DS18B20_CMD_SEARCH_ROM);

    for (uint8_t position = 1; position <= 64; position++) {
        uint8_t byteNumber = (uint8_t)((position - 1) >> 3);
        uint8_t bitMask    = (uint8_t)(1 << ((position - 1) & 0x07));
        uint8_t direction;

        uint8_t bitValue = slotReadBit();
        delayMicroseconds(DS18B20_READ_TAIL_US);
        uint8_t complement = slotReadBit();
        delayMicroseconds(DS18B20_READ_TAIL_US);

        if (bitValue == 1 && complement == 1) {
            // No sensor answered any more
            _searchDone            = true;
            _searchLastDiscrepancy = 0;
            return false;
        }

        if (bitValue != complement) {
            // All remaining sensors have the same bit at this position
            direction = bitValue;
        } else {
            // The sensors disagree, pick a branch of the search tree
            if (position < _searchLastDiscrepancy)
                direction = (_searchAddress[byteNumber] & bitMask) ? 1 : 0;
            else if (position == _searchLastDiscrepancy)
                direction = 1;
            else
                direction = 0;

            if (direction == 0)
                lastZero = position;
        }

        if (direction)
            _searchAddress[byteNumber] |= bitMask;
        else
            _searchAddress[byteNumber] &= (uint8_t)~bitMask;

        slotWriteBit(direction);
        delayMicroseconds(direction ? DS18B20_WRITE_ONE_TAIL_US : DS18B20_WRITE_ZERO_TAIL_US);
    }

    _searchLastDiscrepancy = lastZero;
    if (_searchLastDiscrepancy == 0)
        _searchDone = true;

    if (crc8(_searchAddress, 7) != _searchAddress[7]) {
        // A corrupted address means the search cannot be continued safely
        _searchDone = true;
        return false;
    }

    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++)
        address[i] = _searchAddress[i];
    return true;
}

void DIYables_DS18B20::addressToString(const uint8_t *address, char *buffer) {
    const char *hex = "0123456789ABCDEF";
    uint8_t position = 0;

    for (uint8_t i = 0; i < DS18B20_ADDRESS_SIZE; i++) {
        buffer[position++] = hex[address[i] >> 4];
        buffer[position++] = hex[address[i] & 0x0F];
        if (i < DS18B20_ADDRESS_SIZE - 1)
            buffer[position++] = ':';
    }
    buffer[position] = '\0';
}
