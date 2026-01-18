/* ==========================================================================
   PROJECT: SMART GARDEN - MASTER CONTROL UNIT
   PLATFORM: ESP32 + Blynk IoT
   HARDWARE: DFRobot Servo, Water Level Sensor, LDR, Keypad
   AUTHORS: Nicolo' Carmelo Occhino & Francesca Calcagno 
   
   It control firmware. Manages local/remote control of irrigation valve.
   ========================================================================== */

#include <Arduino.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "secrets.h" // Import credentials (ignored by Git for security)

// Credentials imported from secrets.h
#define BLYNK_TEMPLATE_ID   SECRET_BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_NAME SECRET_BLYNK_TEMPLATE_NAME
#define BLYNK_AUTH_TOKEN    SECRET_BLYNK_AUTH_TOKEN
#define BLYNK_PRINT Serial


#define PIN_TANK        35 // Analog Input: Water Level Sensor
#define PIN_LDR         32 // Analog Input: Light Dependent Resistor (LDR)
#define PIN_SERVO       15 // PWM Output: Servo Motor Control Line

// Configuration for the Servo Motor (DFRobot SER0053)
const int PULSE_MIN = 500;
const int PULSE_MAX = 2500;
const int ANGLE_CLOSED = 0;
const int ANGLE_OPEN = 54; // Calibration: 54 steps map to approx. 90° physical rotation

// Keypad configuration (4x4 Matrix) 
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, 
  {'4','5','6','B'},
  {'7','8','9','C'}, 
  {'*','0','#','D'}
};
// Pin mapping corrected for transposed matrix
byte rowPins[ROWS] = {26, 25, 33, 4};  
byte colPins[COLS] = {13, 12, 14, 27}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo valveServo;
BlynkTimer timer;

// System states Variables
bool manualMode = false;      // V0: 0=Auto, 1=Manual
bool valveState = false;      // V2: 0=Closed, 1=Open
bool notificationSent = false; 

// Threshold
const int TANK_EMPTY = 10;    // Safety Lockout if Water Level < 10%

// ACTUATOR CONTROL LOGIC

/**
 * Controls the physical valve and syncs with App.
 * Includes safety interlock for water level.
 */
void setValve(bool open) {
    int tankPct = map(analogRead(PIN_TANK), 0, 4095, 0, 100);

    // SAFETY INTERLOCK: Tank Empty
    if (open && tankPct < TANK_EMPTY && !manualMode) {
         Serial.println("[WARNING] Safety Lockout: Tank Empty.");
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
    if (!manual) setValve(false); // Default to safe state in Auto
}

// SENSOR ACQUISITION LOGIC

void sendSensors() {

    Blynk.virtualWrite(V3, 0); 

    // Water level processing
    int rawTank = analogRead(PIN_TANK);
    int tankPct = map(rawTank, 0, 4095, 0, 100);

    // Light level processing (inverted)
    int rawLight = analogRead(PIN_LDR);
    int lightPct = map(rawLight, 0, 4095, 100, 0); 

    // Telemetry Transmission
    Blynk.virtualWrite(V4, tankPct);
    Blynk.virtualWrite(V5, lightPct);
    
    Serial.printf("[TELEMETRY] Water Lvl: %d%% | Light: %d%%\n", tankPct, lightPct);

    // Kernel automation
    if (!manualMode) {
        // Priority 1: Resource Availability
        if (tankPct < TANK_EMPTY) {
            if (valveState) { 
                Serial.println("AUTO LOGIC: Tank Empty -> STOP."); 
                setValve(false); 
            }
        } 
        // Auto Mode effectively acts as a "Safety Standby" mode. It will not open the valve automatically.
        else {
            if (valveState) { 
                Serial.println("AUTO LOGIC: Default State -> CLOSED."); 
                setValve(false); 
            }
        }
    }
    
    // Event Management
    if (tankPct < TANK_EMPTY && !notificationSent) {
        Blynk.logEvent("tank_low", "Alert: Water Tank Empty.");
        notificationSent = true;
    } else if (tankPct > TANK_EMPTY + 10) {
        notificationSent = false; 
    }
}

// HANDLE BLYNK EVENTS

BLYNK_WRITE(V0) { 
    bool reqMode = param.asInt() == 1;
    if (reqMode != manualMode) setMode(reqMode);
}

BLYNK_WRITE(V1) { 
    if (manualMode) {
        setValve(param.asInt() == 1);
    } else { 
        Blynk.virtualWrite(V1, 0); // Reject if in Auto
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- SMART GARDEN: SYSTEM START ---");

    // Initialize Actuators
    valveServo.setPeriodHertz(50);
    valveServo.attach(PIN_SERVO, PULSE_MIN, PULSE_MAX);
    valveServo.write(ANGLE_CLOSED); 

    // Initialize Connectivity
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
        Serial.print("Input: "); Serial.println(key);
        if (key == 'A') setMode(true);
        if (key == 'B') setMode(false);
        if (manualMode) {
            if (key == '1') setValve(true);
            if (key == '0') setValve(false);
        }
    }
}