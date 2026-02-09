# ADAS-Moto V1.0 - Emergency Detection System

**Bike Safety & SOS System**

## Project Overview

ADAS-Moto V1.0 is an intelligent emergency detection and response system designed for motorcyclists. The system automatically detects accidents using dual accelerometer sensors and provides immediate emergency response including GPS location sharing, SMS alerts, and automated phone calls to emergency contacts.

### Key Features

- **Dual Accident Detection**: BMX160 + MPU6050 (via BLE) accelerometers
- **Helmet Detection**: AP3216 light sensor for helmet wearing detection
- **GPS Tracking**: Real-time location with SIM7600 module
- **Automatic Emergency Response**:
  - Automated phone call to emergency contact
  - SMS alerts to two emergency numbers
  - GPS location sharing via Google Maps link
- **MQTT Cloud Integration**: Real-time monitoring via Adafruit IO
- **Manual SOS Button**: 3-second hold activation
- **Visual Display**: 128x128 OLED showing system status
- **Remote Control**: Update settings via MQTT (thresholds, phone numbers, commands)

## Hardware Components

### Main Components
- **ESP32** - Main microcontroller
- **SIM7600** - 4G LTE modem with GPS
- **BMX160** - Local accelerometer/gyroscope sensor
- **U8G2 OLED Display** - 128x128 pixels (SH1107)
- **BLE Sensor Module** - External MPU6050 + AP3216 (Helmet sensor)

### Input/Output
- SOS Button (Pin 2)
- Reset Button (Pin 4)
- Buzzer (Pin 5)
- Status LED (Pin 12)

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                    ADAS-Moto V1.0                       │
│                                                     │
│  ┌─────────┐    ┌──────────┐    ┌─────────────┐    │
│  │ ESP32   │◄───┤ SIM7600  │◄───┤ SIM Card    │    │
│  │         │    │ (4G+GPS) │    │             │    │
│  └────┬────┘    └──────────┘    └─────────────┘    │
│       │                                            │
│       ├─► BMX160 (Accelerometer)                   │
│       ├─► BLE ──► Helmet Sensor (MPU6050+AP3216)   │
│       ├─► OLED Display                             │
│       ├─► Buzzer + LED                             │
│       └─► Buttons (SOS + Reset)                    │
└─────────────────────────────────────────────────────┘
           │
           ▼
    ┌──────────────┐
    │ MQTT Cloud   │
    │ Adafruit IO  │
    └──────────────┘
```

## Quick Start

### 1. Hardware Setup
1. Connect all components according to the pin definitions in `MyUtils.h`
2. Insert activated SIM card with data plan
3. Power the system via USB or battery

### 2. Software Installation
1. Install Arduino IDE 1.8.x or 2.x
2. Install required libraries:
   - TinyGSM
   - PubSubClient
   - DFRobot_BMX160
   - U8g2
   - NimBLE-Arduino
3. Update configuration in code (see Configuration section)
4. Upload code to ESP32

### 3. Configuration
Edit these values in `adas_moto.ino`:

```cpp
// Emergency Contacts
const char* EMERGENCY_NUMBER_1 = "+000000000000";  // Your number
const char* EMERGENCY_NUMBER_2 = "+000000000000";  // Your number

// User Information
const char* USER_NAME = "User";
const char* USER_AGE = "00";
const char* USER_GENDER = "Unknown";

// MQTT Credentials
#define IO_USERNAME  "YOUR_USERNAME"
#define IO_KEY       "YOUR_API_KEY"

// Thresholds
int BLE_ACCEL_THRESHOLD = 13;      // BLE sensor threshold
float BMX_ACCEL_THRESHOLD = 30.0;  // BMX160 threshold (m/s²)
```

Also update in `MyUtils.h`:
```cpp
const char apn[] = "YOUR_APN";  // Your carrier APN
```

And in `SensorBLE.h`:
```cpp
static BLEAddress sensorProAddress("XX:XX:XX:XX:XX:XX");  // Your BLE sensor MAC
```

## Operation

### Normal Mode
- System displays ADAS-Moto branding on OLED
- GPS icon appears when GPS is ready
- Helmet icon appears if helmet is not detected
- Continuously monitors acceleration

### Accident Detection
1. If acceleration exceeds threshold (30 m/s²)
2. Buzzer starts beeping
3. Display shows SOS icon
4. 10-second countdown begins
5. User can cancel by pressing Reset button
6. After 10 seconds:
   - Automated phone call to Contact 1
   - SMS sent to both contacts with:
     - User information
     - GPS coordinates
     - Google Maps link
     - Acceleration value

### Manual SOS
1. Press and hold SOS button for 3 seconds
2. Same emergency response as accident detection

### Reset
- Press Reset button to cancel false alarms
- Stops buzzer and resets system state

## MQTT Remote Control

The system can be controlled remotely via MQTT feeds:

| Feed | Purpose | Example |
|------|---------|---------|
| `threshold` | Update acceleration threshold | `35.0` |
| `no` | Update primary phone number | `+1234567890` |
| `no2` | Update secondary phone number | `+1234567890` |
| `rest` | Reset system | `1` or `reset` |
| `sos` | Trigger SOS remotely | `1` |
| `atcommand` | Send custom commands | `location`, `AT+CSQ` |

## Project Structure

```
adas-moto/
├── adas_moto.ino    # Main application
├── LCD.h                    # Display icons and config
├── SensorBLE.h             # BLE sensor communication
├── MyUtils.h               # Hardware configuration
└── README.md               # This file
```

## Safety Features

1. **Dual Sensor Redundancy**: Two independent accelerometers
2. **10-Second Confirmation**: Prevents false alarms
3. **Manual Override**: Reset button to cancel
4. **Helmet Detection**: Warns if helmet not worn
5. **Battery Indicator**: Visual battery status
6. **Signal Indicator**: Network connection status

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| TinyGSM | Latest | SIM7600 modem communication |
| PubSubClient | Latest | MQTT client |
| DFRobot_BMX160 | Latest | BMX160 accelerometer driver |
| U8g2 | Latest | OLED display driver |
| NimBLE-Arduino | Latest | BLE communication |

## License

MIT License - See LICENSE file for details.

## Author

Bassam Almadani

---

**⚠️ SAFETY NOTICE**: This is a safety device. Regular testing and maintenance are required. Always ride safely and wear proper protective equipment.
