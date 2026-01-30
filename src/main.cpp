/* ==========================================================================
   PROJECT: SMART GARDEN - FINAL STABLE VERSION
   ========================================================================== */
#include "secrets.h" 

#define BLYNK_TEMPLATE_ID   SECRET_BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_NAME SECRET_BLYNK_TEMPLATE_NAME
#define BLYNK_AUTH_TOKEN    SECRET_BLYNK_AUTH_TOKEN
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define PIN_TANK        35 
#define PIN_LDR         32 
#define PIN_SERVO       15 

// --- CALIBRATION: CRITICAL SETTINGS ---
// 1. Dip sensor fully in water. Check "RAW TANK" in Serial Monitor.
// 2. Set WATER_SENSOR_MAX to that number (likely 1500-2200, NOT 4095).
const int WATER_SENSOR_MAX = 1800;  // <--- LOWER THIS if you never get 100%
const int WATER_SENSOR_MIN = 0;

const int TANK_EMPTY_THRESHOLD = 10;   // Below 10% = SAFETY STOP
const int LIGHT_TRIGGER_ON = 80;       // Light > 80% = OPEN
const int LIGHT_TRIGGER_OFF = 60;      // Light < 60% = CLOSE

// Servo Settings
const int PULSE_MIN = 500;
const int PULSE_MAX = 2500;
const int ANGLE_CLOSED = 0;
const int ANGLE_OPEN = 54; 

// Keypad
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, {'4','5','6','B'},
  {'7','8','9','C'}, {'*','0','#','D'}
};
byte rowPins[ROWS] = {26, 25, 33, 4};  
byte colPins[COLS] = {13, 12, 14, 27}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo valveServo;
BlynkTimer timer;

// State
bool manualMode = false;      
bool valveState = false;      

// --- SMOOTHING FUNCTION ---
// Takes 10 readings and averages them to remove noise
int readSmooth(int pin) {
    long sum = 0;
    for(int i=0; i<10; i++) {
        sum += analogRead(pin);
        delay(5); // Small delay between reads
    }
    return sum / 10;
}

void setValve(bool open) {
    // Safety check inside Actuator Logic
    int raw = readSmooth(PIN_TANK);
    int pct = map(raw, WATER_SENSOR_MIN, WATER_SENSOR_MAX, 0, 100);

    // SAFETY INTERLOCK
    if (open && pct < TANK_EMPTY_THRESHOLD) {
         Serial.println("[CRITICAL] Safety Lockout: Water too low.");
         Blynk.logEvent("tank_low", "CRITICAL: Tank Empty!");
         valveServo.write(ANGLE_CLOSED);
         valveState = false;
         Blynk.virtualWrite(V1, 0); 
         Blynk.virtualWrite(V2, 0); 
         return; 
    }

    if (open && !valveState) {
        valveServo.write(ANGLE_OPEN); 
        valveState = true;
        Blynk.virtualWrite(V1, 1);
        Blynk.virtualWrite(V2, 1); 
        Serial.println(">>> ACTUATOR: Valve OPEN");
    } else if (!open && valveState) {
        valveServo.write(ANGLE_CLOSED);
        valveState = false;
        Blynk.virtualWrite(V1, 0);
        Blynk.virtualWrite(V2, 0); 
        Serial.println(">>> ACTUATOR: Valve CLOSED");
    }
}

void setMode(bool manual) {
    manualMode = manual;
    Blynk.virtualWrite(V0, manual ? 1 : 0); 
    Serial.printf(">>> SYSTEM MODE: %s\n", manual ? "MANUAL" : "AUTOMATIC");
    if (!manual) setValve(false); 
}

void sendSensors() {
    Blynk.virtualWrite(V3, 0); 

    // 1. Get SMOOTHED Raw Values
    int rawTank = readSmooth(PIN_TANK);
    int rawLight = readSmooth(PIN_LDR);

    // 2. Map to Percentages
    int tankPct = map(rawTank, WATER_SENSOR_MIN, WATER_SENSOR_MAX, 0, 100);
    int lightPct = map(rawLight, 0, 4095, 100, 0); // Inverted LDR Logic

    // Clamp limits (0-100)
    if (tankPct > 100) tankPct = 100;
    if (tankPct < 0) tankPct = 0;

    // 3. Send to Blynk
    Blynk.virtualWrite(V4, tankPct);
    Blynk.virtualWrite(V5, lightPct);
    
    // 4. DEBUG PRINT (Look at this line in Serial Monitor!)
    Serial.printf("RAW TANK: %d | RAW LIGHT: %d || Water: %d%% | Light: %d%%\n", 
                  rawTank, rawLight, tankPct, lightPct);

    // 5. AUTOMATION KERNEL
    if (!manualMode) {
        // Priority 1: SAFETY (Is tank empty?)
        if (tankPct < TANK_EMPTY_THRESHOLD) {
            if (valveState) {
                Serial.println("AUTO: Safety Stop (Low Water)");
                setValve(false);
            }
        } 
        // Priority 2: TRIGGER (Is there enough light?)
        else {
            if (lightPct > LIGHT_TRIGGER_ON) {
                if (!valveState) {
                    Serial.println("AUTO: Flashlight Detected -> OPENING");
                    setValve(true);
                }
            }
            else if (lightPct < LIGHT_TRIGGER_OFF) {
                if (valveState) {
                    Serial.println("AUTO: Light Low -> CLOSING");
                    setValve(false);
                }
            }
        }
    }
}

BLYNK_WRITE(V0) { bool reqMode = param.asInt() == 1; if (reqMode != manualMode) setMode(reqMode); }
BLYNK_WRITE(V1) { if (manualMode) { setValve(param.asInt() == 1); } else { Blynk.virtualWrite(V1, 0); } }

void setup() {
    Serial.begin(115200);
    Serial.println("\n  SMART GARDEN: SYSTEM START  ");
    valveServo.setPeriodHertz(50);
    valveServo.attach(PIN_SERVO, PULSE_MIN, PULSE_MAX);
    valveServo.write(ANGLE_CLOSED); 
    
    Serial.print("Connecting to Blynk...");
    Blynk.begin(SECRET_BLYNK_AUTH_TOKEN, SECRET_WIFI_SSID, SECRET_WIFI_PASS);
    Serial.println(" CONNECTED.");
    timer.setInterval(2000L, sendSensors);
}

void loop() {
    Blynk.run();
    timer.run();
    char key = keypad.getKey();
    if (key) {
        if (key == 'A') setMode(true);
        if (key == 'B') setMode(false);
        if (manualMode) {
            if (key == '1') setValve(true);
            if (key == '0') setValve(false);
        }
    }
}

