/*
 * DIYables_DS18B20 - MultipleSensors Example
 *
 * Several DS18B20 sensors can share one pin. This example finds all of
 * them, starts the conversion of all sensors at the same time with a
 * single command, and reads each sensor by its ROM address once the
 * conversion is finished. Nothing blocks while the sensors convert.
 *
 * Important: do not start a conversion on one sensor while another one
 * is still converting. A reset on the bus stops a conversion in progress,
 * which is why all sensors are triggered together here.
 *
 * Wiring: connect the DATA pin of every sensor to pin 4, VCC to 5V (or
 * 3.3V), GND to GND. One 4.7k resistor between DATA and VCC is enough
 * for the whole bus.
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

#define SENSOR_PIN  4
#define MAX_SENSORS 4
#define PERIOD      2000

// This object talks to every sensor at once, it starts all conversions
DIYables_DS18B20 bus(SENSOR_PIN);

// These objects each talk to one sensor, they read the results
DIYables_DS18B20 sensors[MAX_SENSORS] = {
  DIYables_DS18B20(SENSOR_PIN),
  DIYables_DS18B20(SENSOR_PIN),
  DIYables_DS18B20(SENSOR_PIN),
  DIYables_DS18B20(SENSOR_PIN)
};

#define PHASE_WAIT     0
#define PHASE_CONVERT  1
#define PHASE_READ     2

int sensorCount = 0;
int phase = PHASE_WAIT;
int readIndex = 0;
unsigned long lastRequestTime = 0;

void setup() {
  Serial.begin(9600);
  bus.begin();
  bus.setAutoMode(false);

  uint8_t address[DS18B20_ADDRESS_SIZE];
  char text[DS18B20_ADDRESS_STRING_SIZE];

  bus.resetSearch();
  while (sensorCount < MAX_SENSORS && bus.searchNext(address)) {
    if (address[0] != DS18B20_FAMILY_CODE)
      continue;

    sensors[sensorCount].setAddress(address);
    sensors[sensorCount].begin();
    sensors[sensorCount].setAutoMode(false);

    DIYables_DS18B20::addressToString(address, text);
    Serial.print("Sensor ");
    Serial.print(sensorCount);
    Serial.print(" at ");
    Serial.println(text);

    sensorCount++;
  }

  Serial.print("Found ");
  Serial.print(sensorCount);
  Serial.println(" DS18B20 sensor(s)");
}

void loop() {
  // Every object needs its transfer moved forward, only one of them
  // owns the bus at a time so this is cheap
  bus.loop();
  for (int i = 0; i < sensorCount; i++)
    sensors[i].loop();

  if (sensorCount == 0)
    return;

  switch (phase) {
    case PHASE_WAIT:
      if (millis() - lastRequestTime >= PERIOD) {
        lastRequestTime = millis();
        // One command starts the conversion of every sensor on the pin
        bus.requestTemperature();
        phase = PHASE_CONVERT;
      }
      break;

    case PHASE_CONVERT:
      if (bus.isConversionDone()) {
        readIndex = 0;
        sensors[0].readTemperature();
        phase = PHASE_READ;
      }
      break;

    case PHASE_READ:
      // The sensors are read one after another, each one bit by bit
      if (!sensors[readIndex].isBusy()) {
        Serial.print("Sensor ");
        Serial.print(readIndex);
        Serial.print(": ");

        if (sensors[readIndex].isValid()) {
          Serial.print(sensors[readIndex].getTemperatureC());
          Serial.print("°C / ");
          Serial.print(sensors[readIndex].getTemperatureF());
          Serial.println("°F");
        } else {
          Serial.println("read failed");
        }

        readIndex++;
        if (readIndex < sensorCount) {
          sensors[readIndex].readTemperature();
        } else {
          Serial.println();
          phase = PHASE_WAIT;
        }
      }
      break;
  }
}
