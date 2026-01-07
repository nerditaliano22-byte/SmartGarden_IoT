#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

/*
 * SMART GARDEN - REAL HARDWARE VERSION
 * * HARDWARE WIRING GUIDE:
 * 1. SERVO (Valve Proxy): 
 * - Signal -> GPIO 18
 * - VCC -> VIN (5V)
 * - GND -> GND
 * * 2. 2-PIN MOISTURE SENSOR (Voltage Divider):
 * - Pin 1 -> 3.3V
 * - Pin 2 -> GPIO 34 AND to a 10k Resistor
 * - Other leg of Resistor -> GND
 * * 3. DHT SENSOR (Temp):
 * - Data Pin -> GPIO 15
 * * 4. LCD DISPLAY (I2C):
 * - SDA -> GPIO 21
 * - SCL -> GPIO 22
 * - VCC -> VIN (5V)
 */

// --- PIN DEFINITIONS ---
#define PIN_MOISTURE_PROXY 34  
#define PIN_SERVO          18  
#define PIN_DHT            15  
#define PIN_ALARM          5   // Optional Red LED

// --- CONSTANTS ---
const int DRY_THRESHOLD = 30;     // Value in %
const int WET_THRESHOLD = 70;     // Value in %
const int WATERING_TIME = 5000;   // Max watering time (ms)

// --- OBJECTS ---
DHT dht(PIN_DHT, DHT22);          // Change to DHT11 if using the blue sensor
Servo waterValve;                 
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27 is standard

// --- STATES ---
enum State { IDLE, SENSING, WATERING, ERROR };
State currentState = IDLE;

// --- VARIABLES ---
unsigned long lastCheckTime = 0;
unsigned long stateStartTime = 0;
int currentMoisturePct = 0;
float currentTemp = 0.0;

// --- HELPER FUNCTIONS ---
void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- SYSTEM BOOT ---");

  // 1. Initialize Sensors
  dht.begin();
  
  // 2. Initialize Servo
  waterValve.setPeriodHertz(50);    // Standard 50hz servo
  waterValve.attach(PIN_SERVO, 500, 2400); 
  waterValve.write(0);              // Start CLOSED (0 degrees)

  pinMode(PIN_ALARM, OUTPUT);

  // 3. Initialize LCD
  lcd.init();
  lcd.backlight();
  
  updateLCD("Smart Garden", "System Init...");
  delay(2000);
}

void loop() {
  unsigned long now = millis();

  switch (currentState) {
    
    // --- STATE 1: IDLE ---
    case IDLE:
      if (now - lastCheckTime > 3000) { // Wait 3 seconds
        currentState = SENSING;
      }
      break;

    // --- STATE 2: SENSING ---
    case SENSING:
      {
        // A. Read Moisture (Analog)
        int rawVal = analogRead(PIN_MOISTURE_PROXY);
        // Map 0-4095 to 0-100% 
        // Note: With a voltage divider, 0 is usually dry, 4095 is wet/shorted.
        currentMoisturePct = map(rawVal, 0, 4095, 0, 100); 
        
        // B. Read Temp
        currentTemp = dht.readTemperature();

        // C. Debug & Display
        Serial.printf("Moisture: %d%% | Temp: %.1fC\n", currentMoisturePct, currentTemp);
        String statusLine = "H:" + String(currentMoisturePct) + "% T:" + String((int)currentTemp) + "C";
        updateLCD("Scanning...", statusLine);

        // D. Logic Decision
        if (isnan(currentTemp)) {
           Serial.println("Error: DHT Failed");
           currentState = ERROR;
        }
        else if (currentMoisturePct < DRY_THRESHOLD) {
           Serial.println("-> DECISION: Dry. Opening Valve.");
           currentState = WATERING;
           stateStartTime = now;
        } 
        else {
           currentState = IDLE;
           lastCheckTime = now;
           updateLCD("System Idle", statusLine); 
        }
      }
      break;

    // --- STATE 3: WATERING ---
    case WATERING:
      {
        // Open Valve (90 degrees)
        waterValve.write(90); 
        updateLCD("WATERING...", "Valve: OPEN");

        // Check time limit
        if (now - stateStartTime > WATERING_TIME) {
           Serial.println("-> Safety Timeout. Closing.");
           waterValve.write(0); // Close Valve
           currentState = IDLE;
           lastCheckTime = now;
        }
      }
      break;

    // --- STATE 4: ERROR ---
    case ERROR:
      digitalWrite(PIN_ALARM, HIGH);
      updateLCD("HARDWARE ERROR", "Check Sensors");
      waterValve.write(0); // Ensure closed
      delay(1000);
      digitalWrite(PIN_ALARM, LOW);
      delay(1000);
      break;
  }
}