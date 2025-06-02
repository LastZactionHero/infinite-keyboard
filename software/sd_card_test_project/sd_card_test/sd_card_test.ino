#include <Arduino.h>
#include "FS.h"   // File system base for ESP32
#include "SD.h"   // SD card library for SPI mode
#include "SPI.h"  // SPI library

// SD Card Pin Definitions (SPI Mode)
#define SD_CD1_PIN    13  // Card Detect 1 (Dedicated socket switch)
#define SD_MISO_PIN   14  // SD Card DAT0 is MISO in SPI
#define SD_SCLK_PIN   21  // SD Card CLK is SCLK in SPI
#define SD_MOSI_PIN   47  // SD Card CMD is MOSI in SPI
#define SD_CS_PIN     48  // SD Card CD/DAT3 is CS in SPI

// LED Pin Definitions
#define LED_POWER_PIN 11  // Power LED
#define LED_RUNNING_PIN 10  // Running LED / SD Init Success
#define LED_MODE_A_PIN 9   // Mode A LED / Error Indicator
#define LED_MODE_B_PIN 3   // Mode B LED / Test Success Indicator

// Test file name
const char* TEST_FILE_SPI = "/testSPI.txt"; // Changed name slightly to avoid conflict if old file exists
const char* TEST_DATA_SPI = "SD Card SPI Test Data - Success!";

// SPIClass instance. VSPI is typically SPI2 on ESP32. HSPI is SPI3.
// You can choose either if they are free. Let's use VSPI.
// SPIClass spi(VSPI);

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial monitor to connect
  Serial.println("\nSD Card SPI Test Program Starting (ESP32-S3)...");

  // Initialize LEDs
  pinMode(LED_POWER_PIN, OUTPUT);
  pinMode(LED_RUNNING_PIN, OUTPUT);
  pinMode(LED_MODE_A_PIN, OUTPUT);
  pinMode(LED_MODE_B_PIN, OUTPUT);
  
  digitalWrite(LED_POWER_PIN, LOW);     // Assuming LOW turns ON
  digitalWrite(LED_RUNNING_PIN, HIGH);  // Assuming HIGH turns OFF
  digitalWrite(LED_MODE_A_PIN, HIGH);   // Assuming HIGH turns OFF
  digitalWrite(LED_MODE_B_PIN, HIGH);   // Assuming HIGH turns OFF

  // Check for SD card presence using dedicated CD1 pin
  pinMode(SD_CD1_PIN, INPUT_PULLUP);
  delay(50);
  if (digitalRead(SD_CD1_PIN) == HIGH) { // Adjust based on your CD1 switch logic
    Serial.println("Card not detected on CD1 (IO13). Please insert card.");
    digitalWrite(LED_MODE_A_PIN, LOW); // Indicate error
    return;
  } else {
    Serial.println("Card detected on CD1 (IO13).");
  }

  Serial.println("Initializing SD card in SPI mode...");
  
  // Initialize SPI bus with custom pins
  // Arguments: sck, miso, mosi, ss (ss can be -1 if CS is handled by SD.begin)
  // spi.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1); 
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1); 
  // if (!SD.begin(SD_CS_PIN, SPI, 4000000))
  // Initialize SD card
  // SD.begin(chipSelectPin, spi_instance, frequency_hz, mountpoint, max_files)
  // A common starting frequency for SPI is 4MHz.
  // if (!SD.begin(SD_CS_PIN, spi, 4000000)) { // Pass CS pin, SPI instance, and frequency
  if (!SD.begin(SD_CS_PIN, SPI, 4000)) {
    Serial.println("SPI Mode: Card Mount Failed or Card not present.");
    Serial.println("Possible reasons:");
    Serial.println("1. Incorrect wiring for SPI (MOSI, MISO, SCLK, CS).");
    Serial.println("2. CS pin not driven correctly or MISO pull-up missing.");
    Serial.println("3. SD Card not seated properly or faulty.");
    Serial.println("4. Power supply issues.");
    digitalWrite(LED_MODE_A_PIN, LOW);  // Indicate failure
    return;
  }
  
  Serial.println("SPI Mode: SD card initialized successfully.");
  digitalWrite(LED_RUNNING_PIN, LOW);  // Indicate SD init success
  
  // Print card info
  uint8_t cardType = SD.cardType();
  Serial.print("Card Type: ");
  if(cardType == CARD_NONE) {
    Serial.println("None");
  } else if(cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if(cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("Card Size: %lluMB\n", cardSize);
  
  runSDCardSPITest();
}

void runSDCardSPITest() {
  Serial.println("\nStarting SD card SPI read/write test...");
  
  // Write test
  Serial.println("Writing SPI test data to SD card...");
  File testFile = SD.open(TEST_FILE_SPI, FILE_WRITE);
  if (!testFile) {
    Serial.println("Error opening SPI test file for writing!");
    digitalWrite(LED_MODE_A_PIN, LOW);
    return;
  }
  
  size_t bytesWritten = testFile.println(TEST_DATA_SPI);
  if (bytesWritten > 0) {
    Serial.println("SPI write successful.");
  } else {
    Serial.println("SPI write failed!");
    digitalWrite(LED_MODE_A_PIN, LOW);
    testFile.close();
    return;
  }
  testFile.close();

  // Read test
  Serial.println("Reading SPI test data from SD card...");
  testFile = SD.open(TEST_FILE_SPI, FILE_READ);
  if (!testFile) {
    Serial.println("Error opening SPI test file for reading!");
    digitalWrite(LED_MODE_A_PIN, LOW);
    return;
  }

  String readData = "";
  if (testFile.available()) {
    readData = testFile.readStringUntil('\n');
  }
  testFile.close();
  readData.trim(); 
  String expectedData = String(TEST_DATA_SPI);

  if (readData == expectedData) {
    Serial.println("SPI read successful and data matches!");
    Serial.println("Test data read: \"" + readData + "\"");
    digitalWrite(LED_MODE_B_PIN, LOW);
  } else {
    Serial.println("SPI data verification failed!");
    Serial.println("Expected: \"" + expectedData + "\"");
    Serial.println("Got: \"" + readData + "\"");
    digitalWrite(LED_MODE_A_PIN, LOW);
  }

  Serial.println("Removing SPI test file...");
  if (SD.remove(TEST_FILE_SPI)) {
    Serial.println("SPI test file removed successfully.");
  } else {
    Serial.println("Error removing SPI test file.");
  }
}

void loop() {
  delay(1000);
}