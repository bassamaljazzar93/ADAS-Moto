/*
 * MyUtils.h - Hardware Configuration and Utilities
 * ADAS-Moto V1.0 System
 */

#ifndef MY_UTILS_H
#define MY_UTILS_H

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb
#define SerialAT Serial1

#include <Arduino.h>
#include <DFRobot_BMX160.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>

#include <PubSubClient.h>


// ====================================================================
// BMX160 ACCELEROMETER/GYROSCOPE SENSOR
// ====================================================================
DFRobot_BMX160 bmx160;

// ====================================================================
// GSM MODEM CONFIGURATION
// ====================================================================

// Enable AT command debugging (comment out to disable)
#define DUMP_AT_COMMANDS

// ====================================================================
// NETWORK SETTINGS - Update these based on your network provider
// ====================================================================
#define GSM_PIN ""  // PIN code if any, leave empty if none

// GPRS credentials - Update based on your service provider
const char apn[]      = "YOUR_APN";   // TODO: Set your carrier APN
const char gprsUser[] = "";           // Usually empty
const char gprsPass[] = "";           // Usually empty

// ====================================================================
// MODEM SETUP WITH DEBUGGING
// ====================================================================
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

// ====================================================================
// POWER MANAGEMENT
// ====================================================================
#define uS_TO_S_FACTOR      1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP       30          // Time ESP32 will go to sleep (in seconds)

// ====================================================================
// SERIAL COMMUNICATION
// ====================================================================
#define UART_BAUD           115200

// ====================================================================
// ESP32 PIN DEFINITIONS FOR SIM7600
// ====================================================================
// Modem UART pins
#define MODEM_TX            27
#define MODEM_RX            26

// Modem control pins
#define MODEM_PWRKEY        4   // Power key - used to turn modem on/off
#define MODEM_DTR           32  // Data Terminal Ready
#define MODEM_RI            33  // Ring Indicator
#define MODEM_FLIGHT        25  // Flight mode control
#define MODEM_STATUS        34  // Status indicator

// ====================================================================
// SD CARD PIN DEFINITIONS
// ====================================================================
#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13

// ====================================================================
// OTHER PINS
// ====================================================================
#define LED_PIN             12  // Status LED

#endif // MY_UTILS_H
