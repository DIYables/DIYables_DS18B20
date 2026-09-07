/*
 * DIYables_DS18B20 - ScanAddresses Example
 *
 * Every DS18B20 has a unique 8 byte ROM address printed into the chip.
 * The address is needed when several sensors share the same pin. This
 * example scans the pin and prints the address of each sensor it finds.
 * Write the addresses down, they are used in the MultipleSensors example.
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

DIYables_DS18B20 bus(SENSOR_PIN);

void setup() {
  Serial.begin(9600);
  bus.begin();

  Serial.println("Scanning the 1-Wire bus...");

  uint8_t address[DS18B20_ADDRESS_SIZE];
  char text[DS18B20_ADDRESS_STRING_SIZE];
  int count = 0;

  bus.resetSearch();
  while (bus.searchNext(address)) {
    count++;

    DIYables_DS18B20::addressToString(address, text);
    Serial.print(count);
    Serial.print(": ");
    Serial.print(text);

    if (address[0] == DS18B20_FAMILY_CODE)
      Serial.println("  (DS18B20)");
    else
      Serial.println("  (another 1-Wire device, not a DS18B20)");
  }

  Serial.print("Found ");
  Serial.print(count);
  Serial.println(" device(s)");

  if (count == 0)
    Serial.println("Check the wiring and the 4.7k resistor between DATA and VCC");
}

void loop() {
}
