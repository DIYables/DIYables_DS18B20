/*
 * DIYables_DS18B20 - ReadTemperature Example
 *
 * This example reads the temperature from a single DS18B20 sensor without
 * blocking. The library starts the conversion, returns immediately, and
 * collects the result on a later call of loop(). There is no delay() in
 * the sketch, so the rest of the code keeps running at full speed.
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

DIYables_DS18B20 sensor(SENSOR_PIN);

unsigned long lastGoodTime = 0;

void setup() {
  Serial.begin(9600);

  if (sensor.begin())
    Serial.println("DS18B20 sensor found");
  else
    Serial.println("DS18B20 sensor NOT found, check the wiring and the 4.7k resistor");

  // Take a new reading every second
  sensor.setInterval(1000);

  lastGoodTime = millis();
}

void loop() {
  // Drives the sensor, never waits for the conversion
  sensor.loop();

  if (sensor.isTemperatureReady()) {
    lastGoodTime = millis();

    Serial.print("Temperature: ");
    Serial.print(sensor.getTemperatureC());
    Serial.print("°C / ");
    Serial.print(sensor.getTemperatureF());
    Serial.println("°F");
  }

  // A reading that fails prints nothing by itself, so report a run of them
  if (millis() - lastGoodTime >= 5000) {
    lastGoodTime = millis();

    if (sensor.isConnected())
      Serial.println("No valid reading for 5 s. The sensor answers but the data is corrupted.");
    else
      Serial.println("No valid reading for 5 s. The sensor does not answer, check the wiring and the 4.7k resistor.");
  }
}
