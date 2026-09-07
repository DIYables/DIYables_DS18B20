/*
 * DIYables_DS18B20 - BlinkWhileReading Example
 *
 * This example shows that the library does not block. A DS18B20 needs up
 * to 750 ms to convert a temperature, and reading the result takes another
 * 9 ms on the wire. During all of that the LED keeps blinking every 100 ms,
 * because the sketch never waits for the sensor.
 *
 * Every call of sensor.loop() is timed, and the longest one seen so far is
 * printed with each reading. It should stay close to the figure that
 * getMaxLoopTime() reports, around 90 us.
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
#define BLINK_TIME 100

// Some boards, many generic ESP32 boards for example, do not define
// LED_BUILTIN. Change the pin below to the one your LED is wired to.
#ifdef LED_BUILTIN
  #define LED_PIN LED_BUILTIN
#else
  #define LED_PIN 13
#endif

DIYables_DS18B20 sensor(SENSOR_PIN);

unsigned long lastBlinkTime = 0;
bool ledState = LOW;
unsigned long maxLoopTime = 0;
unsigned long lastGoodTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);

  if (sensor.begin())
    Serial.println("DS18B20 sensor found");
  else
    Serial.println("DS18B20 sensor NOT found, check the wiring and the 4.7k resistor");

  // Measured on this board in begin(). No call of sensor.loop() takes
  // longer than this, so it can be used as a real time budget
  Serial.print("Worst case of one sensor.loop() call: ");
  Serial.print(sensor.getMaxLoopTime());
  Serial.println(" us");

  sensor.setInterval(2000);

  lastGoodTime = millis();
}

void loop() {
  // Task 1: read the temperature without blocking. The call is timed to
  // show how long it really holds the sketch up. micros() costs a little
  // itself, so the figure is slightly pessimistic
  unsigned long start = micros();
  sensor.loop();
  unsigned long spent = micros() - start;

  if (spent > maxLoopTime)
    maxLoopTime = spent;

  if (sensor.isTemperatureReady()) {
    lastGoodTime = millis();

    Serial.print("Temperature: ");
    Serial.print(sensor.getTemperatureC());
    Serial.print("°C / ");
    Serial.print(sensor.getTemperatureF());
    Serial.print("°F, longest sensor.loop() so far: ");
    Serial.print(maxLoopTime);
    Serial.println(" us");
  }

  // A reading that fails prints nothing by itself, so report a run of them
  if (millis() - lastGoodTime >= 6000) {
    lastGoodTime = millis();

    if (sensor.isConnected())
      Serial.println("No valid reading for 6 s. The sensor answers but the data is corrupted.");
    else
      Serial.println("No valid reading for 6 s. The sensor does not answer, check the wiring and the 4.7k resistor.");
  }

  // Task 2: blink the LED, it keeps running while the sensor converts
  if (millis() - lastBlinkTime >= BLINK_TIME) {
    lastBlinkTime = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
