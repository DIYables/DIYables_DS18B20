/*
 * Copyright (c) 2026, DIYables.io. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the DIYables.io nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DIYABLES.IO "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DIYABLES.IO BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DIYables_DS18B20_h
#define DIYables_DS18B20_h

#include <Arduino.h>

// Resolution in bits
#define DS18B20_RESOLUTION_9BIT   9
#define DS18B20_RESOLUTION_10BIT  10
#define DS18B20_RESOLUTION_11BIT  11
#define DS18B20_RESOLUTION_12BIT  12

// Value returned when no valid reading is available
#define DS18B20_INVALID_TEMPERATURE  (-127.0)

// ROM address of a sensor is 8 bytes
#define DS18B20_ADDRESS_SIZE  8

// Minimum buffer size for addressToString(), including the terminating zero
#define DS18B20_ADDRESS_STRING_SIZE  24

// First byte of the ROM address of a DS18B20
#define DS18B20_FAMILY_CODE  0x28

// Default time between two readings in automatic mode, in milliseconds
#define DS18B20_DEFAULT_INTERVAL  1000

class DIYables_DS18B20 {
public:
    DIYables_DS18B20(int pin);
    DIYables_DS18B20(int pin, const uint8_t *address);

    // Runs once in setup(). This one does wait, see the reference
    bool begin();

    // Call this on every pass of the sketch loop(), in both modes. It
    // advances the 1-Wire transfer one time slot at a time and returns
    bool loop();

    // Automatic mode, on by default
    void setAutoMode(bool enabled);
    bool isAutoMode();
    bool isTemperatureReady();
    void setInterval(unsigned long intervalMs);
    unsigned long getInterval();

    // Manual mode, switch it on with setAutoMode(false)
    bool requestTemperature();
    bool isConversionDone();
    bool readTemperature();
    bool isBusy();

    // Worst case duration of one call of loop(), in microseconds
    unsigned long getMaxLoopTime();

    // Last reading
    float   getTemperatureC();
    float   getTemperatureF();
    int16_t getRawTemperature();
    bool    isValid();
    bool    isConnected();

    // Configuration, meant for setup(), these wait for the bus
    bool    setResolution(uint8_t bits, bool saveToEeprom = false);
    uint8_t getResolution();
    unsigned long getConversionTime();
    bool    isParasitePower();

    // ROM address, needed only when several sensors share one pin
    void setAddress(const uint8_t *address);
    void useSkipRom();
    bool getAddress(uint8_t *address);
    bool readAddress(uint8_t *address);
    void resetSearch();
    bool searchNext(uint8_t *address);

    // Helpers
    static void    addressToString(const uint8_t *address, char *buffer);
    static uint8_t crc8(const uint8_t *data, uint8_t length);

private:
    // What the sensor is doing
    enum {
        STATE_IDLE       = 0,
        STATE_CONVERTING = 1
    };

    // What the transfer running on the bus is for
    enum {
        JOB_NONE    = 0,
        JOB_CONVERT = 1,
        JOB_READ    = 2
    };

    // Where the transfer has got to. Every step is one atomic time slot
    // followed by a wait that the sketch is free to spend elsewhere
    enum {
        XFER_IDLE = 0,
        XFER_BUS_IDLE,
        XFER_RESET_LOW,
        XFER_RESET_RECOVER,
        XFER_WRITE_BIT,
        XFER_READ_BIT,
        XFER_COMPLETE
    };

    int      _pin;
    uint8_t  _address[DS18B20_ADDRESS_SIZE];
    bool     _useAddress;

    uint8_t       _state;
    unsigned long _conversionStart;
    unsigned long _conversionTime;
    unsigned long _lastPollTime;
    unsigned long _lastReadTime;
    unsigned long _interval;

    bool     _autoMode;
    bool     _didWork;
    bool     _newData;
    bool     _connected;
    bool     _valid;
    bool     _parasite;
    bool     _strongPullup;

    uint8_t  _resolution;
    int16_t  _raw;
    uint8_t  _scratchpad[9];

    // Transfer state machine
    uint8_t       _job;
    uint8_t       _step;
    unsigned long _stepStart;
    unsigned long _stepWait;
    unsigned long _jobStart;
    uint8_t       _txBuffer[DS18B20_ADDRESS_SIZE + 2];
    uint8_t       _txLength;
    uint8_t       _txIndex;
    uint8_t       _rxLength;
    uint8_t       _rxIndex;
    uint8_t       _bitIndex;
    uint8_t       _shiftRegister;

    // Bit timing adapted to the speed of the digital I/O of the board
    uint8_t  _gpioUs;
    uint8_t  _readLowUs;
    uint8_t  _readSampleUs;
    uint8_t  _writeOneLowUs;
    uint8_t  _writeZeroLowUs;
    uint8_t  _presenceDelayUs;
    unsigned long _maxLoopUs;

    // ROM search state
    uint8_t  _searchAddress[DS18B20_ADDRESS_SIZE];
    uint8_t  _searchLastDiscrepancy;
    bool     _searchDone;

    void    calibrateTiming();
    void    driveLow();
    void    releaseBus();
    void    applyStrongPullup();
    void    releaseStrongPullup();

    // Atomic time slots. These are the only parts that cannot be split
    uint8_t slotPresence();
    void    slotWriteBit(uint8_t bit);
    uint8_t slotReadBit();

    // Blocking primitives, used by begin() and the setup() helpers only
    bool    reset();
    void    writeByte(uint8_t value);
    uint8_t readByte();
    bool    selectDevice();
    bool    readScratchpad();
    bool    writeScratchpad(uint8_t th, uint8_t tl, uint8_t config);
    void    detectPowerSupply();

    // Transfer state machine
    void    buildCommand(uint8_t command);
    void    beginTransfer(uint8_t job);
    void    startWait(unsigned long microseconds);
    bool    service();
    bool    finishTransfer();
    void    abortTransfer();
    void    startConvert();
    void    startRead();

    bool    validateScratchpad();
    void    storeTemperature();
    void    updateConversionTime();
};

#endif // DIYables_DS18B20_h
