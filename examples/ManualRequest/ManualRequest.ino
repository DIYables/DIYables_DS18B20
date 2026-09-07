/*
 * DIYables_DS18B20 - ManualRequest Example
 *
 * This example controls the conversion by hand instead of using loop().
 * The sketch starts a conversion with requestTemperature(), keeps doing
 * its own work, checks isConversionDone() and finally collects the value
 * with readTemperature(). Use this mode when the reading has to be
 * triggered by an event such as a button press or a network command.
 *
 * Wiring: DS18B20 DATA to pin 4, VCC to 5V (or 3.3V), GND to GND.
 * A 4.7k resistor is needed between DATA and VCC.
 *
 * Tutorial: https://diyables.io/dp/B0BPFYQT8C
 *
 * TESTED HARDWARE:
 * - Arduino Uno R3
 * - Arduino Uno R4 WiFi
 * - Arduino Uno R4 Minima
 * - Arduino Mega
 * - Arduino Due
 * - Arduino Giga
 * - DIYables STEM V3: https://diyables.io/stem-v3
 * - DIYables STEM V4 IoT: https://diyables.io/stem-v4-iot
 * - DIYables STEM V4B IoT: https://diyables.io/stem-v4b-iot
 * - DIYables STEM V4B Edu: https://diyables.io/stem-v4-edu
 * - DIYables MEGA2560 R3: https://diyables.io/atmega2560-board
 * - DIYables Nano R3: https://diyables.io/nano-board
 * - DIYables ESP32 Board: https://diyables.io/esp32-board
 * - DIYables ESP32 S3, Uno-form factor: https://diyables.io/esp32-s3-uno
 * - It is expected to work with other boards
 */

#include <DIYables_DS18B20.h>

#define SENSOR_PIN 4
#define PERIOD     3000

DIYables_DS18B20 sensor(SENSOR_PIN);

#define PHASE_WAIT      0
#define PHASE_CONVERT   1
#define PHASE_READ      2

int phase = PHASE_WAIT;
unsigned long lastRequestTime = 0;

void setup() {
  Serial.begin(9600);

  if (sensor.begin())
    Serial.println("DS18B20 sensor found");
  else
    Serial.println("DS18B20 sensor NOT found, check the wiring and the 4.7k resistor");

  // Nothing is started on its own, this sketch decides when to read
  sensor.setAutoMode(false);

  Serial.print("Conversion time: ");
  Serial.print(sensor.getConversionTime());
  Serial.println(" ms");
}

void loop() {
  // Always call this. It moves the 1-Wire transfer forward by one time
  // slot and returns, it never waits for the sensor
  sensor.loop();

  switch (phase) {
    case PHASE_WAIT:
      if (millis() - lastRequestTime >= PERIOD) {
        lastRequestTime = millis();
        sensor.requestTemperature();
        phase = PHASE_CONVERT;
        Serial.println("Conversion started, the sketch is free to do other work");
      }
      break;

    case PHASE_CONVERT:
      // True once the command has gone out and the sensor has finished
      if (sensor.isConversionDone()) {
        sensor.readTemperature();
        phase = PHASE_READ;
      }
      break;

    case PHASE_READ:
      // The scratchpad is read bit by bit across several calls of loop(),
      // isBusy() goes false when the last bit has arrived
      if (!sensor.isBusy()) {
        if (sensor.isValid()) {
          Serial.print("Finished after ");
          Serial.print(millis() - lastRequestTime);
          Serial.print(" ms, temperature: ");
          Serial.print(sensor.getTemperatureC());
          Serial.print("°C / ");
          Serial.print(sensor.getTemperatureF());
          Serial.println("°F");
        } else {
          Serial.println("Reading failed, no sensor or corrupted data");
        }
        phase = PHASE_WAIT;
      }
      break;
  }

  // Any other code can run here, it is never delayed by the sensor
}
