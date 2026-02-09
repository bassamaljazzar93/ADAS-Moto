/*
 * SensorBLE.h - BLE Sensor Communication
 * Connects to external sensor via Bluetooth Low Energy
 * Sensors: AP3216 (Light) and MPU6050 (Accelerometer/Gyroscope)
 */

#ifndef SENSOR_BLE_H
#define SENSOR_BLE_H

#include <NimBLEDevice.h>

// ====================================================================
// BLE SENSOR CONFIGURATION
// ====================================================================
// Service UUID for both sensors
static BLEUUID serviceUUIDs[] = {
    BLEUUID("6a800001-b5a3-f393-e0a9-e50e24dcca9e")
};

// Characteristic UUIDs
static BLEUUID charUUIDs[] = { 
    BLEUUID("6a803216-b5a3-f393-e0a9-e50e24dcca9e"),  // AP3216 Light Sensor
    BLEUUID("6a806050-b5a3-f393-e0a9-e50e24dcca9e")   // MPU6050 Accelerometer & Gyroscope
};

// Sensor device address - TODO: Update with your BLE sensor MAC address
static BLEAddress sensorProAddress("00:00:00:00:00:00");

// ====================================================================
// GLOBAL SENSOR VALUES
// ====================================================================
int16_t lightValue = 0;
int16_t accelX = 0, accelY = 0, accelZ = 0;
int16_t gyroX = 0, gyroY = 0, gyroZ = 0;

// ====================================================================
// SENSOR SCALES
// ====================================================================
const float ACCEL_SCALE = 2.0 / 32768.0;      // +/-2g range
const float GYRO_SCALE = 2000.0 / 32768.0;    // +/-2000 deg/s range

// ====================================================================
// FUNCTION DECLARATIONS
// ====================================================================
bool connectAndSubscribeToSensorPro();

// ====================================================================
// SETUP BLE SENSOR
// ====================================================================
void setupSensorBLE() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client for Sensor Pro");

    // Initialize NimBLE
    NimBLEDevice::init("");
    NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
    NimBLEDevice::setScanDuplicateCacheSize(200);

    // Connect to sensor
    if (!connectAndSubscribeToSensorPro()) {
        Serial.println("Failed to connect to Sensor Pro!");
        Serial.println("Make sure device is powered on and in range.");
    } else {
        Serial.println("Sensor Pro connected successfully!");
    }
}

// ====================================================================
// LOOP - Currently not used but kept for future use
// ====================================================================
void loopSensorBLE() {
    // Future implementation: Check connection status, reconnect if needed
}

// ====================================================================
// CONNECT AND SUBSCRIBE TO SENSOR PRO
// ====================================================================
bool connectAndSubscribeToSensorPro() {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    Serial.println("Scanning for Sensor Pro...");

    // Scan for 10 seconds
    NimBLEScanResults scanResults = pScan->start(10);
    bool deviceFound = false;
    NimBLEAdvertisedDevice* advertisedDevice = nullptr;

    // Find our specific sensor device
    for (int i = 0; i < scanResults.getCount(); i++) {
        NimBLEAdvertisedDevice scannedDevice = scanResults.getDevice(i);
        if (scannedDevice.getAddress().equals(sensorProAddress)) {
            deviceFound = true;
            advertisedDevice = new NimBLEAdvertisedDevice(scannedDevice);
            Serial.println("Found Sensor Pro device!");
            break;
        }
    }

    if (!deviceFound || advertisedDevice == nullptr) {
        Serial.println("Sensor Pro not found during scan");
        return false;
    }

    // Create BLE client and connect
    Serial.println("Connecting to Sensor Pro...");
    NimBLEClient* pClient = NimBLEDevice::createClient();

    if (!pClient->connect(advertisedDevice)) {
        Serial.println("Connection failed");
        delete advertisedDevice;
        return false;
    }

    Serial.println("Connected successfully!");

    // Subscribe to all services and characteristics
    for (int i = 0; i < sizeof(serviceUUIDs)/sizeof(serviceUUIDs[0]); i++) {
        NimBLERemoteService* pRemoteService = pClient->getService(serviceUUIDs[i]);
        
        if (pRemoteService == nullptr) {
            Serial.print("Failed to find service: ");
            Serial.println(serviceUUIDs[i].toString().c_str());
            continue;
        }

        // Subscribe to each characteristic
        for (int j = 0; j < sizeof(charUUIDs)/sizeof(charUUIDs[0]); j++) {
            NimBLERemoteCharacteristic* pRemoteCharacteristic = 
                pRemoteService->getCharacteristic(charUUIDs[j]);
            
            if (pRemoteCharacteristic == nullptr) {
                Serial.print("Failed to find characteristic: ");
                Serial.println(charUUIDs[j].toString().c_str());
                continue;
            }

            // Subscribe to notifications
            if (pRemoteCharacteristic->canNotify() || pRemoteCharacteristic->canIndicate()) {
                pRemoteCharacteristic->subscribe(
                    true,
                    [j](NimBLERemoteCharacteristic* pRemoteCharacteristic, 
                        uint8_t* pData, size_t length, bool isNotify) {
                        
                        if (j == 0) {
                            // AP3216 Light Sensor
                            if (length >= 2) {
                                lightValue = (int16_t)((pData[0] << 8) | pData[1]);
                            }
                        } 
                        else if (j == 1) {
                            // MPU6050 Accelerometer & Gyroscope
                            if (length >= 12) {
                                // Parse raw data
                                accelX = (int16_t)((pData[0] << 8) | pData[1]);
                                accelY = (int16_t)((pData[2] << 8) | pData[3]);
                                accelZ = (int16_t)((pData[4] << 8) | pData[5]);
                                gyroX = (int16_t)((pData[6] << 8) | pData[7]);
                                gyroY = (int16_t)((pData[8] << 8) | pData[9]);
                                gyroZ = (int16_t)((pData[10] << 8) | pData[11]);
                            }
                        }
                    },
                    true
                );
                Serial.print("Subscribed to characteristic ");
                Serial.println(j);
            } else {
                Serial.println("Characteristic does not support notifications");
            }
        }
    }

    delete advertisedDevice;
    return true;
}

// ====================================================================
// GET LIGHT VALUE
// ====================================================================
int16_t getLightValue() {
    return lightValue;
}

// ====================================================================
// GET MPU6050 VALUES (PROCESSED)
// ====================================================================
void getMPU6050Values(int16_t &ax, int16_t &ay, int16_t &az, 
                      int16_t &gx, int16_t &gy, int16_t &gz) {
    // Convert raw values to absolute values
    float xa = abs(accelX);
    float ya = abs(accelY);
    float za = abs(accelZ);
    
    // Convert to g-force and scale
    float accelerationx = (xa * ACCEL_SCALE) * 10;
    float accelerationy = (ya * ACCEL_SCALE) * 10;
    float accelerationz = (za * ACCEL_SCALE) * 10;
    
    // Calculate total acceleration magnitude
    float accelerationAll = sqrt(
        accelerationx * accelerationx + 
        accelerationy * accelerationy + 
        accelerationz * accelerationz
    );
    
    // Process gyroscope data
    float xg = abs(gyroX);
    float yg = abs(gyroY);
    float zg = abs(gyroZ);
    
    float gyroscopex = xg * GYRO_SCALE;
    float gyroscopey = yg * GYRO_SCALE;
    float gyroscopez = zg * GYRO_SCALE;
    
    // Calculate total gyroscope magnitude
    float gyroscopeAll = sqrt(
        gyroscopex * gyroscopex + 
        gyroscopey * gyroscopey + 
        gyroscopez * gyroscopez
    );
    
    // Return processed values
    ax = (int16_t)accelerationAll;
    ay = (int16_t)accelerationy;
    az = (int16_t)accelerationz;
    gx = (int16_t)gyroscopeAll;
    gy = (int16_t)gyroscopey;
    gz = (int16_t)gyroscopez;
}

#endif // SENSOR_BLE_H
