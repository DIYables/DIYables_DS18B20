/*
 * DIYables_DS18B20 - Resolution Example
 *
 * The DS18B20 can be set to 9, 10, 11 or 12 bits. A lower resolution is
 * less precise but converts much faster, which matters when readings are
 * needed often. This example sets the resolution and shows the step size
 * and the conversion time that go with it.
 *
 * 9 bits  = 0.5°C    step, about 94 ms
 * 10 bits = 0.25°C   step, about 188 ms
 * 11 bits = 0.125°C  step, about 375 ms
 * 12 bits = 0.0625°C step, about 750 ms
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

void setup() {
  Serial.begin(9600);

  if (!sensor.begin()) {
    Serial.println("DS18B20 sensor NOT found, check the wiring and the 4.7k resistor");
    return;
  }

  Serial.print("Resolution at power on: ");
  Serial.print(sensor.getResolution());
  Serial.println(" bits");

  Serial.print("Sensor is powered by: ");
  Serial.println(sensor.isParasitePower() ? "the data line (parasite)" : "its own VCC pin");

  // Fast readings with a coarse step. Pass true as second argument to
  // store the setting in the sensor so it survives a power cycle.
  sensor.setResolution(DS18B20_RESOLUTION_10BIT);

  Serial.print("New resolution: ");
  Serial.print(sensor.getResolution());
  Serial.println(" bits");

  Serial.print("Conversion now takes up to ");
  Serial.print(sensor.getConversionTime());
  Serial.println(" ms");

  // Read as fast as the sensor allows
  sensor.setInterval(200);
}

void loop() {
  sensor.loop();

  if (sensor.isTemperatureReady()) {
    Serial.print("Temperature: ");
    Serial.print(sensor.getTemperatureC());
    Serial.print("°C / ");
    Serial.print(sensor.getTemperatureF());
    Serial.print("°F, raw value: ");
    Serial.println(sensor.getRawTemperature());
  }
}
