// -------------------------------------------------------
// Kombi-Code 10.10.2025 (für Arduino Mega)
// Failsafe + Sensoren + SD Logging + Aktions-Logging
// IBus-Empfänger über Serial1 (HardwareSerial, Pins 19=RX1, 18=TX1)
// -------------------------------------------------------

#include <IBusBM.h>
#include <SPI.h>
#include <SD.h>

IBusBM IBus;
File logFile;
char filename[15];

// --- Zeitsteuerung ---
unsigned long previousMillis = 0UL;
unsigned long lastLogTime = 0;
unsigned long startTime;
const unsigned long startupDelay = 2000; // 2 sec
bool sdInitialized = false;

// --- Pins ---
#define PIN_CLK_PULL_SENSOR 5
#define PIN_DATA_PULL_SENSOR 9
#define PIN_HOCH_AN 2
#define PIN_HOCH_RICHTUNG 16   // (A2)
#define PIN_SCHWENKEN_AN 4
#define PIN_SCHWENKEN_RICHTUNG 6
#define PIN_SAEGE 7
#define PIN_MAGNET 3
#define PIN_PUMPE 8
#define PIN_SPANNER_LOCKER 17  // (A3)
#define PIN_SPANNER_FEST 18    // (A4)
#define PIN_SD_CS 10
#define DEADZONE 50

// --- Analoge Eingänge ---
int sensorPinVolt = A1;
int sensorPinAmp  = A5;

// --- Konstanten ACS712 ---
const float VREF = 5.0;
const float sensitivity = 0.100;
const float offsetV = VREF / 2.0;
const float Imax = 20.0;

// --- Hilfsfunktion zum Logging ---
void logAction(String action) {
  if (!sdInitialized) return;
  unsigned long seconds = millis() / 1000;
  int hours = (seconds / 3600) % 24;
  int minutes = (seconds / 60) % 60;
  int secs = seconds % 60;

  logFile.print(hours); logFile.print(":");
  logFile.print(minutes); logFile.print(":");
  logFile.print(secs); logFile.print(" - ");
  logFile.println(action);
  logFile.flush();

  // Auch zur seriellen Ausgabe:
  Serial.print("[LOG] ");
  Serial.println(action);
}

// --- Alles sicher abschalten ---
void failsafeStop() {
  digitalWrite(PIN_HOCH_AN, HIGH);
  digitalWrite(PIN_HOCH_RICHTUNG, HIGH);
  digitalWrite(PIN_SCHWENKEN_AN, HIGH);
  digitalWrite(PIN_SCHWENKEN_RICHTUNG, HIGH);
  digitalWrite(PIN_SAEGE, HIGH);
  digitalWrite(PIN_MAGNET, HIGH);
  digitalWrite(PIN_PUMPE, LOW);
  digitalWrite(PIN_SPANNER_LOCKER, HIGH);
  digitalWrite(PIN_SPANNER_FEST, HIGH);
}

// --- Hilfsfunktion: Empfange Kanal ---
int readChannel(byte channelInput, int minLimit, int maxLimit, int defaultValue) {
  uint16_t ch = IBus.readChannel(channelInput);
  if (ch < 100) return defaultValue;
  return map(ch, 1000, 2000, minLimit, maxLimit);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);       // USB Debug
  Serial1.begin(115200);      // iBus-Empfänger
  IBus.begin(Serial1);        // HardwareSerial → korrekt für Mega

  startTime = millis();

  pinMode(PIN_CLK_PULL_SENSOR, INPUT);
  pinMode(PIN_DATA_PULL_SENSOR, INPUT);
  pinMode(PIN_HOCH_AN, OUTPUT);
  pinMode(PIN_HOCH_RICHTUNG, OUTPUT);
  pinMode(PIN_SCHWENKEN_AN, OUTPUT);
  pinMode(PIN_SCHWENKEN_RICHTUNG, OUTPUT);
  pinMode(PIN_SAEGE, OUTPUT);
  pinMode(PIN_MAGNET, OUTPUT);
  pinMode(PIN_PUMPE, OUTPUT);
  pinMode(PIN_SPANNER_LOCKER, OUTPUT);
  pinMode(PIN_SPANNER_FEST, OUTPUT);

  failsafeStop();
  Serial.println("Systemstart OK. IBus aktiv auf Serial1 (Pin 19 RX1).");
}

// --- LOOP ---
void loop() {
 
 /* // --- SD-Karte erst nach 2s ---
  if (!sdInitialized) {
    if (millis() - startTime >= startupDelay) {
      if (!SD.begin(PIN_SD_CS)) {
        Serial.println("SD init failed!");
        startTime = millis();
        return;
      }
      int fileIndex = 0;
      do {
        sprintf(filename, "LOG%d.TXT", fileIndex++);
      } while (SD.exists(filename));
      logFile = SD.open(filename, FILE_WRITE);
      if (!logFile) {
        Serial.println("Could not create file");
        return;
      }
      Serial.print("Logging to: ");
      Serial.println(filename);
      sdInitialized = true;
    }
    return;
  } */

  // --- IBus ---
  IBus.loop();

  int ch1_value = readChannel(0, -100, 100, 0);
  int ch2_value = readChannel(1, -100, 100, 0);
  int ch3_value = readChannel(2, -100, 100, 0);
  int ch4_value = readChannel(3, -100, 100, 0);
  int ch5_value = readChannel(4, -100, 100, 0);
  int ch9_value = IBus.readChannel(8);

  // --- Spannung & Strom ---
  int value1 = analogRead(sensorPinVolt);
  float voltage1 = value1 * (VREF / 1023.0);
  float percent1 = (value1 / 1023.0) * 100.0;

  int value5 = analogRead(sensorPinAmp);
  float voltage5 = value5 * (VREF / 1023.0);
  float current = (voltage5 - offsetV) / sensitivity;
  if (current < 0) current = 0;
  if (current > Imax) current = Imax;
  float percent5 = (current / Imax) * 100.0;

  Serial.print(" VOLT=");
  Serial.print(voltage1, 2);
  Serial.print("V ");
  Serial.print(percent1, 1);
  Serial.print("%   ");

  Serial.print(" AMPERE=");
  Serial.print(current, 2);
  Serial.print("A ");
  Serial.print(percent5, 1);
  Serial.println("%");

  // --- SD Logging (alle 1s Messwerte) ---
  if (millis() - lastLogTime >= 1000) {
    lastLogTime += 1000;
    unsigned long seconds = millis() / 1000;
    int hours = (seconds / 3600) % 24;
    int minutes = (seconds / 60) % 60;
    int secs = seconds % 60;

    logFile.print(hours); logFile.print(":");
    logFile.print(minutes); logFile.print(":");
    logFile.print(secs);
    logFile.print("  ");
    logFile.print(voltage1, 2); logFile.print("V ");
    logFile.print(current, 2); logFile.println("A");
    logFile.flush();
  }

  // --- Failsafe ---
  if (ch9_value > 2000) {
    failsafeStop();
    Serial.println("FAILSAFE aktiviert!");
    logAction("FAILSAFE aktiviert");
    return;
  }

  // --- Steuerlogik ---
  if (ch1_value > DEADZONE) {
    digitalWrite(PIN_HOCH_RICHTUNG, HIGH);
    digitalWrite(PIN_SCHWENKEN_AN, LOW);
    logAction("Schwenken AN");
  } else if (ch1_value < -DEADZONE) {
    digitalWrite(PIN_SCHWENKEN_AN, HIGH);
    digitalWrite(PIN_HOCH_RICHTUNG, LOW);
    logAction("Schwenken RUECK");
  } else {
    digitalWrite(PIN_SCHWENKEN_AN, HIGH);
    digitalWrite(PIN_HOCH_RICHTUNG, HIGH);
  }

  if (ch2_value > DEADZONE) {
    digitalWrite(PIN_SCHWENKEN_RICHTUNG, HIGH);
    digitalWrite(PIN_HOCH_AN, LOW);
    logAction("Hochfahren AN");
  } else if (ch2_value < -DEADZONE) {
    digitalWrite(PIN_HOCH_AN, HIGH);
    digitalWrite(PIN_SCHWENKEN_RICHTUNG, LOW);
    logAction("Hochfahren RUECK");
  } else {
    digitalWrite(PIN_HOCH_AN, HIGH);
    digitalWrite(PIN_SCHWENKEN_RICHTUNG, HIGH);
  }

  if (ch4_value > DEADZONE) {
    digitalWrite(PIN_MAGNET, HIGH);
    digitalWrite(PIN_SAEGE, LOW);
    logAction("Magnet AN / Saege AN");

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > 9700) {
      digitalWrite(PIN_PUMPE, HIGH);
      logAction("Pumpe AN");
    } else {
      digitalWrite(PIN_PUMPE, LOW);
    }
    if (currentMillis - previousMillis > 10000) previousMillis = currentMillis;
  } else if (ch4_value < -DEADZONE) {
    digitalWrite(PIN_SAEGE, HIGH);
    digitalWrite(PIN_PUMPE, LOW);
    digitalWrite(PIN_MAGNET, LOW);
    logAction("Saege AUS / Magnet AUS / Pumpe AUS");
  } else {
    digitalWrite(PIN_SAEGE, HIGH);
    digitalWrite(PIN_MAGNET, HIGH);
    digitalWrite(PIN_PUMPE, LOW);
  }

  if (ch5_value > DEADZONE) {
    digitalWrite(PIN_SPANNER_FEST, HIGH);
    digitalWrite(PIN_SPANNER_LOCKER, LOW);
    logAction("Spanner FEST");
  } else if (ch5_value < -DEADZONE) {
    digitalWrite(PIN_SPANNER_LOCKER, HIGH);
    digitalWrite(PIN_SPANNER_FEST, LOW);
    logAction("Spanner LOCKER");
  } else {
    digitalWrite(PIN_SPANNER_LOCKER, HIGH);
    digitalWrite(PIN_SPANNER_FEST, HIGH);
  }

  delay(200);
}
