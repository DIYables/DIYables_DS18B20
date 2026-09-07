# DIYables DS18B20 Library for the DS18B20 Temperature Sensor

A non-blocking Arduino library for the DS18B20 1-Wire temperature sensor. It uses only standard Arduino APIs, so it runs on every board that supports the Arduino API.

![DS18B20 Temperature Sensor](https://diyables.io/images/products/ds18b20-temperature-sensor.jpg)

## Product
* [DS18B20 Temperature Sensor](https://diyables.io/products/ds18b20-temperature-sensor)

## Tested Hardware

| Board                   | Tested |
|-------------------------|:------:|
| Arduino Uno R3          |   ✅   |
| Arduino Uno R4 WiFi     |   ✅   |
| Arduino Uno R4 Minima   |   ✅   |
| Arduino Mega            |   ✅   |
| Arduino Due             |   ✅   |
| Arduino Giga            |   ✅   |
| [DIYables STEM V4 IoT *(works like Arduino Uno R4 WiFi)*](https://diyables.io/products/diyables-stem-v4-iot-fully-compatible-with-arduino-uno-r4-wifi) |   ✅   |
| [DIYables STEM V4B IoT *(works like Arduino Uno R4 WiFi)*](https://diyables.io/products/diyables-stem-v4b-iot-development-board-compatible-with-arduino-uno-r4-wifi-ra4m1-32-bit-arm-cortex-m4-with-esp32-s3-wifi-bluetooth-usb-c-for-learning-prototyping-education) |   ✅   |
| [DIYables STEM V3 *(works like Arduino Uno R3)*](https://diyables.io/products/diyables-stem-v3-fully-compatible-with-arduino-uno-r3) |   ✅   |
| [DIYables STEM V4B Edu *(works like Arduino Uno R4 Minima)*](https://diyables.io/stem-v4-edu) |   ✅   |
| [DIYables MEGA2560 R3 *(works like Arduino Mega 2560 Rev3)*](https://diyables.io/atmega2560-board) |   ✅   |
| [DIYables Nano R3 *(works like Arduino Nano R3)*](https://diyables.io/nano-board) |   ✅   |
| [DIYables ESP32 Development Board](https://diyables.io/esp32-board) |   ✅   |
| [DIYables ESP32 S3, Uno-form factor](https://diyables.io/products/esp32-s3-development-board-with-esp32-s3-wroom-1-n16r8-wifi-bluetooth-uno-compatible-form-factor-works-with-arduino-ide) |   ✅   |
| Other boards            |   Not yet, expected to work    |

## Features

- Non-blocking down to the single bit. The 750 ms conversion and the 1-Wire transfer are both driven by a state machine, so a call of loop() performs at most one time slot and returns.
- getMaxLoopTime() reports the worst case of a single loop() call, measured on the board it runs on, so it can be used as a real time budget.
- Calling loop() late never corrupts a reading. Every wait between two time slots is a minimum with no upper limit, so a slow sketch only stretches the reading out.
- Two ways to use it: an automatic mode where the library schedules the readings, or a manual mode where the sketch decides when to start a conversion and when to collect the result.
- Several sensors on one pin, with ROM address search built in.
- Resolution from 9 to 12 bits, optionally stored in the sensor EEPROM.
- CRC check on every reading, so corrupted data is reported instead of returned.
- Detects whether the sensor runs on its own supply or on parasite power, and drives the strong pull-up when it has to.
- Bit timing adapts to the speed of the board, which is what makes plain digitalWrite() and pinMode() good enough on both a 16 MHz AVR and a 240 MHz ESP32.
- Standard Arduino API only, no external library, no processor specific code.

## Processing Time

All values are at 12 bit resolution with one sensor.

| | Arduino Uno (16 MHz) | ESP32 (240 MHz) |
|---|---|---|
| **Longest single loop() call** | **about 90 us** | **about 86 us** |
| A loop() call with nothing to do | about 4 us | about 1 us |
| CPU time per complete reading | about 2.7 ms | about 2.5 ms |
| Elapsed time per complete reading | about 9 ms | about 9 ms |
| The 750 ms conversion | free, the sketch runs through it | free |

The first row is the figure to design against. `getMaxLoopTime()` returns it at runtime, measured on the board in use.

CPU time is almost the same on both boards because it is set by the 1-Wire pulse widths, which the protocol fixes at 6 us for a one, 60 us for a zero and 12 us for a read. A faster board does not shorten them.

Elapsed time is longer than CPU time because the waits between time slots go back to the sketch.

These calls use the bus in one go and belong in setup():

| Call | Arduino Uno |
|---|---|
| begin() | about 11 ms, about 20 ms when a ROM address is used |
| readAddress() | about 6 ms |
| searchNext() | about 14 ms per sensor found |
| setResolution(bits) | about 11 ms |
| setResolution(bits, true) | about 32 ms, it writes the sensor EEPROM |

## Hardware Required

- An Arduino, ESP32, ESP8266 or any other board with the Arduino API
- One or more DS18B20 temperature sensors
- One 4.7k resistor between the data line and VCC
- Jumper wires

## Pin Mapping (DS18B20)

| DS18B20 Pin | Connect To |
|---|---|
| GND | GND |
| DATA | Any digital pin, pin 4 in the examples |
| VCC | 5V or 3.3V |

A 4.7k pull-up resistor between DATA and VCC is required. One resistor is enough even when several sensors share the pin.

## Quick Start

```cpp
#include <DIYables_DS18B20.h>

DIYables_DS18B20 sensor(4);  // DS18B20 data pin

void setup() {
  Serial.begin(9600);
  sensor.begin();
  sensor.setInterval(1000);  // a new reading every second
}

void loop() {
  sensor.loop();  // at most one time slot, about 90 us

  if (sensor.isTemperatureReady()) {
    Serial.print("Temperature: ");
    Serial.print(sensor.getTemperatureC());
    Serial.println(" C");
  }

  // The rest of the sketch keeps running here
}
```

## Examples

- **ReadTemperature**: the simplest non-blocking reading of one sensor.
- **BlinkWhileReading**: blinks an LED every 100 ms while the sensor converts, which shows that nothing is blocked.
- **ManualRequest**: starts the conversion by hand and collects the result later, for readings triggered by an event.
- **ScanAddresses**: prints the unique ROM address of every sensor on the pin.
- **MultipleSensors**: reads several sensors that share one pin, all converting at the same time.
- **Resolution**: changes the resolution between 9 and 12 bits and shows the effect on the conversion time.

## API Reference

See [DIYables_DS18B20 Library Reference](https://newbiely.com/library-references/diyables-ds18b20-library-reference) for the complete API documentation including all constructors, methods, and constants.

## Tutorials

- [Arduino - DS18B20 Temperature Sensor](https://arduinogetstarted.com/tutorials/arduino-ds18b20-temperature-sensor)
- [Arduino Uno R4 - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-uno-r4/arduino-uno-r4-ds18b20-temperature-sensor)
- [Arduino Uno Q - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-uno-q/arduino-uno-q-ds18b20-temperature-sensor)
- [Arduino Mega - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-mega/arduino-mega-ds18b20-temperature-sensor)
- [Arduino Giga R1 WiFi - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-giga/arduino-giga-r1-wifi-ds18b20-temperature-sensor)
- [Arduino Nano - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-nano/arduino-nano-ds18b20-temperature-sensor)
- [Arduino Nano ESP32 - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-nano-esp32/arduino-nano-esp32-ds18b20-temperature-sensor)
- [Arduino Nano 33 IoT - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-nano-iot/arduino-nano-33-iot-ds18b20-temperature-sensor)
- [Arduino MKR WiFi 1010 - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/arduino-mkr/arduino-mkr-wifi-1010-ds18b20-temperature-sensor)
- [ESP32 - DS18B20 Temperature Sensor](https://esp32io.com/tutorials/esp32-ds18b20-temperature-sensor)
- [ESP32 S3 - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/esp32-s3/esp32-s3-ds18b20-temperature-sensor)
- [ESP32 C3 Super Mini - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/esp32-c3/esp32-c3-ds18b20-temperature-sensor)
- [ESP8266 - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/esp8266/esp8266-ds18b20-temperature-sensor)
- [Raspberry Pi Pico - DS18B20 Temperature Sensor](https://newbiely.com/tutorials/raspberry-pico/raspberry-pi-pico-ds18b20-temperature-sensor)

## License

BSD. See [license.txt](license.txt).
