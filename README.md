#  Smart Garden - IoT Master Control Unit

![PlatformIO](https://img.shields.io/badge/PlatformIO-Core-orange) ![ESP32](https://img.shields.io/badge/Device-ESP32-blue) ![Blynk](https://img.shields.io/badge/IoT-Blynk-green) ![Status](https://img.shields.io/badge/Status-Active-success)

**Authors:** Nicolo' Carmelo Occhino & Francesca Calcagno  
**Course:** Master's Degree in Data Science course of IOT-BASED APPLICATIONS FOR INTELLIGENT SYSTEMS by Professor Davide Patti 

## 📖 Project Overview
The **Smart Garden** is an IoT-enabled embedded system designed to automate irrigation tasks while providing real-time telemetry and remote control. 
Built on the **ESP32** architecture, integrating local human-machine interfaces (Keypad) with cloud-based control (Blynk).

Key features include a **safety-critical architecture** (preventing pump/valve operation when water is low), hybrid control modes (Manual/Auto), and secure credential management.

##  Hardware Architecture

The system is powered by an **ESP32 DOIT DEVKIT V1** and manages the following peripherals:

| Component | Type | GPIO Pin | Notes |
| :--- | :--- | :--- | :--- |
| **Valve Actuator** | DFRobot Servo (SER0053) | **15** | Powered via 5V rail. Calibrated range: 0-54 steps (approx 90°). |
| **Water Level** | Resistive Sensor | **35** | Analog Input (0-4095). Used for safety interlock. |
| **Light Sensor** | LDR Module | **32** | Analog Input. Inverted logic mapped to %. |
| **Local Input** | Membrane Keypad 4x4 | **Multiple** | Matrix transposed via software correction. |

*Note: Humidity sensor support has been architected but disabled in the current firmware revision as per project requirements.*

###  Pinout Configuration (Keypad)
The keypad is mapped as follows in the firmware:
* **Rows:** GPIO 26, 25, 33, 4
* **Columns:** GPIO 13, 12, 14, 27

##  Software Features

### 1. Hybrid Control Modes
* **AUTO Mode (Default):** The system enters a safety standby state. It continuously monitors the water tank level. If the tank is empty, it enforces a system lockout.
* **MANUAL Mode:** Allows the user to force the valve OPEN/CLOSED via the physical Keypad or the Blynk App. *Note: The empty-tank safety check remains active even in Manual mode to protect the hardware.*

### 2. IoT Integration (Blynk)
Real-time bi-directional synchronization via Virtual Pins:
* `V0`: Mode Switch (Auto/Manual)
* `V1`: Valve Command (Open/Close)
* `V2`: Real-time Valve Status LED
* `V3-V5`: Telemetry Data (Humidity*, Water, Light)

### 3. Safety Mechanisms
* **Dry-Run Protection:** The valve is mechanically prevented from opening if the water sensor reads below the `TANK_EMPTY` threshold (<10%).
* **Notification System:** Push notifications are sent to the user's smartphone upon critical events (e.g., "Tank Empty").

### Prerequisites
* VS Code with **PlatformIO** extension.
* ESP32 Drivers (CP210x).

### Security Configuration (`secrets.h`)
This project uses a `.gitignore` policy to secure credentials. You must manually create the secrets file:

1.  Navigate to the `include/` or `src/` folder.
2.  Create a file named `secrets.h`.
3.  Paste the following code and insert your credentials:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// --- BLYNK CONFIGURATION ---
#define SECRET_BLYNK_TEMPLATE_ID    "YOUR_TEMPLATE_ID"
#define SECRET_BLYNK_TEMPLATE_NAME  "YOUR_TEMPLATE_NAME"
#define SECRET_BLYNK_AUTH_TOKEN     "YOUR_AUTH_TOKEN"

// --- WIFI CREDENTIALS ---
#define SECRET_WIFI_SSID            "YOUR_WIFI_SSID"
#define SECRET_WIFI_PASS            "YOUR_WIFI_PASSWORD"

#endif
