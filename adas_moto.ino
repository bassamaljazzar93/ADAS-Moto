/*
 * ADAS-Moto V1.0 - Emergency Detection System with GPS & MQTT
 * Bike Safety & SOS System
 */

#include "LCD.h"
#include "MyUtils.h"
#include "SensorBLE.h"
#define SerialAT Serial1

// ====================================================================
// THRESHOLD SETTINGS
// ====================================================================
struct ThresholdConfig {
  int BLE_ACCEL_THRESHOLD = 13;
  float BMX_ACCEL_THRESHOLD = 30.0;
  int LIGHT_THRESHOLD = 0;
  
  unsigned long ACCIDENT_CONFIRM_TIME = 10000;
  unsigned long SOS_BUTTON_HOLD_TIME = 3000;
  unsigned long BUZZER_TOGGLE_INTERVAL = 100;
  
  const char* EMERGENCY_NUMBER_1 = "+000000000000";  // TODO: Set your emergency number
  const char* EMERGENCY_NUMBER_2 = "+000000000000";  // TODO: Set your emergency number
  
  const char* USER_NAME = "User";        // TODO: Set user name
  const char* USER_AGE = "00";           // TODO: Set user age
  const char* USER_GENDER = "Unknown";   // TODO: Set user gender
} config;

// ====================================================================
// PIN DEFINITIONS
// ====================================================================
#define SOS_BUTTON_PIN      2
#define RESET_BUTTON_PIN    4
#define BUZZER_PIN          5

// ==================== MQTT Configuration ====================
// TODO: Replace with your own MQTT broker credentials
#define IO_USERNAME  "YOUR_USERNAME"
#define IO_KEY       "YOUR_API_KEY"
#define MQTT_SERVER  "io.adafruit.com"
#define MQTT_PORT    1883

#define FEED_GPS         IO_USERNAME "/feeds/gpsloc/csv"
#define FEED_ACCSENSOR   IO_USERNAME "/feeds/accsensor"
#define FEED_LOG         IO_USERNAME "/feeds/log"
#define FEED_SOS         IO_USERNAME "/feeds/sos"
#define FEED_RESET       IO_USERNAME "/feeds/rest"
#define FEED_THRESHOLD   IO_USERNAME "/feeds/threshold"
#define FEED_PHONE       IO_USERNAME "/feeds/no"
#define FEED_PHONE2      IO_USERNAME "/feeds/no2"
#define FEED_ATCOMMAND   IO_USERNAME "/feeds/atcommand"
#define FEED_SOSSTATE    IO_USERNAME "/feeds/sosstate"


TinyGsmClient client(modem);
PubSubClient mqtt(client);

// ====================================================================
// Rate limiting and MQTT publish helper
// ====================================================================
bool resetMsgSent = false;
unsigned long lastMQTTPublish = 0;
const unsigned long PUBLISH_INTERVAL = 1000;

bool publishMQTT(const char* topic, const char* payload) {
  unsigned long now = millis();
  if (now - lastMQTTPublish < PUBLISH_INTERVAL) {
    delay(PUBLISH_INTERVAL - (now - lastMQTTPublish));
  }
  bool result = mqtt.publish(topic, payload);
  lastMQTTPublish = millis();
  return result;
}

bool publishMQTT(const char* topic, const String &payload) {
  return publishMQTT(topic, payload.c_str());
}

bool mqtt_connected = false;

// ====================================================================
// GLOBAL VARIABLES
// ====================================================================
int16_t ax, ay, az, gx, gy, gz;

int buttonState = LOW;
int buttonCount = 0;
unsigned long buttonDownTime = 0;
int reset_bt_state = LOW;
bool signal_state = false;
bool ring_state = false;
bool accidentDetected = false;
bool accident_confirmed = true;
bool helmetDetected = false;
bool sos_active = false;

unsigned long accidentStartTime = 0;
unsigned long buzzerMillis = 0;
unsigned long lastGPSRead = 0;
unsigned long lastAccelPublish = 0;

const char* Bat_value;
bool reply = false;
bool resetFlag = true;

// GPS Variables
float lat = 0, lon = 0, speed_kph = 0, altitude = 0;
int vsat = 0, usat = 0;
float accuracy = 0;
bool gps_ready = false;

// Phone numbers (can be updated via MQTT)
String phone1 = "+000000000000";  // TODO: Set your emergency number
String phone2 = "+000000000000";  // TODO: Set your emergency number



// ====================================================================
// GPS FUNCTIONS
// ====================================================================
bool readGPS() {
  if (modem.getGPS(&lat, &lon, &speed_kph, &altitude, &vsat, &usat, &accuracy)) {
    if (lat != 0 && lon != 0) {
      if (!gps_ready) {
        gps_ready = true;
        Serial.println("GPS signal acquired!");
        if (mqtt_connected) {
          publishMQTT(FEED_LOG, "GPS Ready");
        }
      }
      return true;
    }
  }
  return false;
}

String getGPSLocation() {
  for (int i = 0; i < 5; i++) {
    if (readGPS()) {
      break;
    }
    Serial.println("Getting GPS... " + String(i+1) + "/5");
    delay(2000);
  }
  
  if (lat != 0 && lon != 0) {
    String mapLink = "http://www.google.com/maps/place/" + String(lat, 6) + "," + String(lon, 6);
    
    if (mqtt_connected) {
      char gpsBuffer[100];
      snprintf(gpsBuffer, sizeof(gpsBuffer), "0,%.6f,%.6f,%.1f", lat, lon, altitude);
      publishMQTT(FEED_GPS, gpsBuffer);
      
      String logMsg = "GPS: " + String(lat, 6) + "," + String(lon, 6) + " Alt:" + String(altitude, 1) + "m";
      publishMQTT(FEED_LOG, logMsg.c_str());
    }
    
    return mapLink;
  }
  
  return "Location unavailable";
}

// ====================================================================
// MODEM INITIALIZATION
// ====================================================================
void modem_on() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(300);
  digitalWrite(MODEM_PWRKEY, LOW);

  pinMode(MODEM_FLIGHT, OUTPUT);
  digitalWrite(MODEM_FLIGHT, HIGH);

  int i = 40;
  Serial.println(F("\r\n# Startup #"));
  Serial.println(F("# Sending \"AT\" to Modem. Waiting for Response"));
  
  while (i) {
    SerialAT.println(F("AT"));
    Serial.print(F("."));
    delay(500);

    if (SerialAT.available()) {
      String r = SerialAT.readString();
      Serial.println("\r\n# Response:\r\n" + r);
      if (r.indexOf("OK") >= 0) {
        reply = true;
        break;
      }
    }

    if (Serial.available() && !reply) {
      Serial.read();
      Serial.println(F("\r\n# Modem is not yet online."));
    }

    if (i == 35) {
      Serial.println(F("\r\n# Modem did not yet answer. Probably Power loss?"));
    }
    
    delay(500);
    i--;
  }
  Serial.println(F("#\r\n"));
}

// ==================== Setup GPS ====================
void setupGPS() {
  Serial.println("\nSetting up GPS...");
  
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println("Failed to enable GPIO4 for GPS");
  }
  
  modem.enableGPS();
  delay(3000);
  
  Serial.println("Waiting for GPS connection...");
}

// ==================== GPRS Connection ====================
bool connectGPRS() {
  Serial.println("\nConnecting to internet...");
  
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Failed to connect to internet");
    return false;
  }
  
  Serial.println("Connected to internet!");
  
  IPAddress local = modem.localIP();
  Serial.print("IP Address: ");
  Serial.println(local);
  
  return true;
}

// ==================== MQTT Callback ====================
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) {
    msg += (char)payload[i];
  }
  
  Serial.println("MQTT: " + String(topic) + " = " + msg);
  
  // Handle threshold updates
  if (String(topic).indexOf("threshold") >= 0) {
    float newThreshold = msg.toFloat();
    if (newThreshold > 0 && newThreshold < 100) {
      config.BMX_ACCEL_THRESHOLD = newThreshold;
      Serial.println("Threshold updated to: " + String(newThreshold));
      publishMQTT(FEED_LOG, ("Threshold updated: " + String(newThreshold)).c_str());
    }
  }
  
  // Handle phone number updates
  if (String(topic).indexOf("/no2") >= 0) {
    phone2 = msg;
    Serial.println("Phone 2 updated: " + phone2);
    publishMQTT(FEED_LOG, ("Phone2: " + phone2).c_str());
  } else if (String(topic).indexOf("/no") >= 0) {
    phone1 = msg;
    Serial.println("Phone 1 updated: " + phone1);
    publishMQTT(FEED_LOG, ("Phone1: " + phone1).c_str());
  }
  
  // Handle reset command
  if (String(topic).indexOf("rest") >= 0) {
    if (msg == "1" || msg == "true" || msg == "reset") {
      buttonCount = 0;
      digitalWrite(BUZZER_PIN, LOW);
      accidentDetected = false;
      accident_confirmed = true;
      sos_active = false;
      Serial.println("System reset via MQTT");
      publishMQTT(FEED_LOG, "System Reset");
      publishMQTT(FEED_SOS, "0");
    }
  }
  
  // Handle SOS command
  if (String(topic).indexOf("sos") >= 0 && String(topic).indexOf("accsensor") < 0) {
    if (msg == "1" || msg == "true" || msg == "activate") {
      Serial.println("SOS activated via MQTT");
      triggerSOS();
    }
  }
  
  // Handle AT commands
  if (String(topic).indexOf("atcommand") >= 0) {
    Serial.println("Executing AT command: " + msg);
    publishMQTT(FEED_LOG, ("AT CMD: " + msg).c_str());
    
    if (msg.equalsIgnoreCase("location") || msg.equalsIgnoreCase("gps")) {
      String location = getGPSLocation();
      publishMQTT(FEED_LOG, location.c_str());
      Serial.println("Location: " + location);
    } 
    else if (msg.equalsIgnoreCase("call")) {
      makePhoneCall(phone1);
    }
    else if (msg.equalsIgnoreCase("restart") || msg.equalsIgnoreCase("reset")) {
      publishMQTT(FEED_LOG, "Restarting system...");
      delay(1000);
      ESP.restart();
    }
    else if (msg.startsWith("AT")) {
      SerialAT.println(msg);
      delay(1000);
      if (SerialAT.available()) {
        String response = SerialAT.readString();
        Serial.println("AT Response: " + response);
        publishMQTT(FEED_LOG, response.c_str());
      }
    }
  }
}

// ==================== MQTT Connect ====================
bool mqttConnect() {
  Serial.print("Connecting to MQTT broker... ");
  
  if (mqtt.connect("ADAS_Moto_V1", IO_USERNAME, IO_KEY)) {
    Serial.println("Connected!");
    mqtt_connected = true;
    publishMQTT(FEED_LOG, "ADAS-Moto V1.0 Started");
    
    mqtt.subscribe(FEED_THRESHOLD);
    mqtt.subscribe(FEED_PHONE);
    mqtt.subscribe(FEED_PHONE2);
    mqtt.subscribe(FEED_RESET);
    mqtt.subscribe(FEED_SOS);
    mqtt.subscribe(FEED_ATCOMMAND);
    
    publishMQTT(FEED_LOG, "Subscribed to all feeds");
    
    return true;
  }
  
  Serial.print("Failed! (");
  Serial.print(mqtt.state());
  Serial.println(")");
  mqtt_connected = false;
  return false;
}

// ====================================================================
// PHONE CALL
// ====================================================================
void makePhoneCall(String number) {
  String callCommand = "ATD" + number + ";";
  SerialAT.println(callCommand);
  delay(500);
  
  SerialAT.println("AT+CECH=0x0000"); delay(500);
  SerialAT.println("AT+CNSN=0x7fff"); delay(500);
  SerialAT.println("AT+CMICGAIN=8"); delay(500);
  SerialAT.println("AT+COUTGAIN=8"); delay(500);
  SerialAT.println("AT+CRXVOL=0xffff"); delay(500);
  SerialAT.println("AT+CTXMICGAIN=1,0xffff"); delay(500);
  
  Serial.println("Calling: " + number);
}

// ====================================================================
// SEND SMS TO BOTH NUMBERS
// ====================================================================
void sendSMSToNumber(String number, String message) {
  SerialAT.println("AT+CMGF=1");
  delay(1000);
  
  String smsCommand = "AT+CMGS=\"" + number + "\"";
  SerialAT.println(smsCommand);
  delay(1000);
  
  SerialAT.println(message);
  delay(1000);
  SerialAT.println((char)26);
  
  Serial.println("SMS sent to: " + number);
  delay(2000);
}

// ====================================================================
// TRIGGER SOS
// ====================================================================
void triggerSOS() {
  sos_active = true;
  reset_bt_state = digitalRead(RESET_BUTTON_PIN);
  if (reset_bt_state == HIGH) {
    buttonCount = 0;
    digitalWrite(BUZZER_PIN, LOW);
    accidentDetected = false;
    accident_confirmed = true;
    sos_active = false;
    Serial.println("Accident detection cancelled by user");
    
    if (mqtt_connected) {
      publishMQTT(FEED_LOG, "Reset by button");
      publishMQTT(FEED_SOS, "0");
      publishMQTT(FEED_SOSSTATE, "0");
    }
  }
  
  String location = getGPSLocation();
  digitalWrite(BUZZER_PIN, LOW);
  
  makePhoneCall(phone1);
  delay(500);
  
  String smsContent = "=== ADAS-Moto EMERGENCY ===\n";
  smsContent += "SOS ACTIVATED\n";
  smsContent += "Name: " + String(config.USER_NAME) + "\n";
  smsContent += "Age: " + String(config.USER_AGE) + "\n";
  smsContent += "Gender: " + String(config.USER_GENDER) + "\n";
  smsContent += "Location: " + location;
  
  sendSMSToNumber(phone1, smsContent);
  sendSMSToNumber(phone2, smsContent);
  
  if (mqtt_connected) {
    publishMQTT(FEED_SOS, "1");
    publishMQTT(FEED_LOG, "SOS Activated - SMS sent to both numbers");
    publishMQTT(FEED_SOSSTATE, "1");
  }
  
  Serial.println("SOS Complete!");
  publishMQTT(FEED_SOSSTATE, "0");
}

// ====================================================================
// SEND ACCIDENT ALERT
// ====================================================================
void sendAccidentAlert(float acceleration) {
  reset_bt_state = digitalRead(RESET_BUTTON_PIN);
  if (reset_bt_state == HIGH) {
    buttonCount = 0;
    digitalWrite(BUZZER_PIN, LOW);
    accidentDetected = false;
    accident_confirmed = true;
    sos_active = false;
    Serial.println("Accident detection cancelled by user");
    
    if (mqtt_connected) {
      publishMQTT(FEED_LOG, "Reset by button");
      publishMQTT(FEED_SOS, "0");
      publishMQTT(FEED_SOSSTATE, "0");
    }
  }
  
  String location = getGPSLocation();
  digitalWrite(BUZZER_PIN, LOW);
  
  makePhoneCall(phone1);
  delay(500);
  
  String smsContent = "=== ADAS-Moto EMERGENCY ===\n";
  smsContent += "ACCIDENT DETECTED\n";
  smsContent += "Acceleration: " + String(acceleration, 2) + " m/s2\n";
  smsContent += "Name: " + String(config.USER_NAME) + "\n";
  smsContent += "Age: " + String(config.USER_AGE) + "\n";
  smsContent += "Gender: " + String(config.USER_GENDER) + "\n";
  smsContent += "Location: " + location;
  
  sendSMSToNumber(phone1, smsContent);
  sendSMSToNumber(phone2, smsContent);
  
  if (mqtt_connected) {
    publishMQTT(FEED_ACCSENSOR, String(acceleration).c_str());
    publishMQTT(FEED_SOS, "1");
    publishMQTT(FEED_LOG, ("Accident: " + String(acceleration) + " m/s2").c_str());
  }
  
  Serial.println("Accident alert sent!");
  publishMQTT(FEED_SOSSTATE, "0");
}

// ====================================================================
// SETUP
// ====================================================================
void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100);

  // Initialize BMX160 sensor
  if (bmx160.begin() != true) {
    Serial.println("BMX160 initialization failed");
  }

  // Initialize OLED display
  u8g2.begin();
  u8g2.setContrast(255);

  // Initialize modem
  modem_on();
  
  // Setup GPS
  setupGPS();

  // Connect to GPRS
  if (connectGPRS()) {
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqttConnect();
  } else {
    Serial.println("No internet - system will work without MQTT");
  }
  
  if (reply) {
    Serial.println(F("***********************************************************"));
    Serial.println(F(" Modem connected successfully"));
    Serial.println(F("***********************************************************\n"));
    signal_state = true;
  } else {
    Serial.println(F("***********************************************************"));
    Serial.println(F(" Failed to connect to the modem!"));
    Serial.println(F("***********************************************************\n"));
    signal_state = false;
  }

  // Initialize BLE sensors
  setupSensorBLE();

  // Initialize buttons and buzzer
  pinMode(SOS_BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("ADAS-Moto V1.0 Ready!");
}

// ====================================================================
// MAIN LOOP
// ====================================================================
void loop() {
  unsigned long currentMillis = millis();
  
  // Update OLED display
  updateDisplay();
  // Check SOS button
  sos_button_call();
  // Update BLE sensor data
  loopSensorBLE();

  // MQTT handling
  if (mqtt_connected) {
    if (!mqtt.connected()) {
      Serial.println("MQTT disconnected, reconnecting...");
      mqtt_connected = false;
      mqttConnect();
    } else {
      mqtt.loop();
    }
  }
  
  // GPS reading (every 10 seconds)
  if (millis() - lastGPSRead > 10000) {
    lastGPSRead = millis();
    readGPS();
  }

  // Check helmet status via light sensor
  int lightValue = getLightValue();
  helmetDetected = (lightValue > config.LIGHT_THRESHOLD);

  // Get BLE accelerometer data (MPU6050)
  getMPU6050Values(ax, ay, az, gx, gy, gz);
  
  // Check for accident via BLE accelerometer
  if (ax > config.BLE_ACCEL_THRESHOLD && !helmetDetected) {
    if (accident_confirmed) {
      accidentStartTime = currentMillis;
      accident_confirmed = false;
      Serial.printf("BLE Accident detected! Accel X: %d\n", ax);
    }
  }

  // Get local BMX160 accelerometer data
  sBmx160SensorData_t Ogyro;
  bmx160.getAllData(NULL, &Ogyro, NULL);
  
  float x = abs(Ogyro.x);
  float y = abs(Ogyro.y);
  float z = abs(Ogyro.z);
  float acceleration = sqrt(x * x + y * y + z * z);
  
  // Send acceleration to MQTT every 3 seconds
  if (mqtt_connected && (currentMillis - lastAccelPublish >= 3000)) {
    lastAccelPublish = currentMillis;
    publishMQTT(FEED_ACCSENSOR, String(acceleration).c_str());
  }

  // Check for accident via BMX160 accelerometer
  if (acceleration > config.BMX_ACCEL_THRESHOLD) {
    if (accident_confirmed) {
      accidentStartTime = currentMillis;
      accident_confirmed = false;
      Serial.printf("BMX160 Accident detected! Accel: %.2f\n", acceleration);
      publishMQTT(FEED_ACCSENSOR, String(acceleration).c_str());
      publishMQTT(FEED_SOSSTATE, "1");
    }
    delay(300);
  }

  // Handle reset button
  reset_bt_state = digitalRead(RESET_BUTTON_PIN);
  if (reset_bt_state == HIGH) {
    resetFlag = true;
    buttonCount = 0;
    digitalWrite(BUZZER_PIN, LOW);
    accidentDetected = false;
    accident_confirmed = true;
    sos_active = false;
    Serial.println("Accident detection cancelled by user");
    
    if (mqtt_connected && !resetMsgSent) {
      publishMQTT(FEED_LOG, "Reset by button");
      publishMQTT(FEED_SOS, "0");
      publishMQTT(FEED_SOSSTATE, "0");
      resetMsgSent = true;
    }
  } else {
    resetFlag = false;
    resetMsgSent = false;
  }

  // Buzzer control during accident detection
  if (!accident_confirmed) {
    if (millis() - buzzerMillis >= config.BUZZER_TOGGLE_INTERVAL) {
      buzzerMillis = millis();
      
      if (digitalRead(RESET_BUTTON_PIN) == HIGH) {
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
      }
    }

    // Send alert after confirmation time
    if (currentMillis - accidentStartTime >= config.ACCIDENT_CONFIRM_TIME) {
      digitalWrite(BUZZER_PIN, HIGH);
      sendAccidentAlert(acceleration);
      accidentDetected = false;
      accident_confirmed = true;
    }
  }

  // Check for incoming calls
  if (SerialAT.available()) {
    String r = SerialAT.readString();
    Serial.println("Response: " + r);
    
    if (r.indexOf("RING") >= 0) {
      ring_state = true;
    } else {
      ring_state = false;
    }
  }

  // Pass through serial commands
  if (Serial.available()) {
    SerialAT.write(Serial.read());
  }

  delay(1);
}

// ====================================================================
// SOS BUTTON HANDLER
// ====================================================================
void sos_button_call(){
  buttonState = digitalRead(SOS_BUTTON_PIN);    
  if (buttonState == HIGH) {
    sos_active = false;
    buttonCount++;
    if (buttonCount == 1) {
      buttonDownTime = millis();
    }
    unsigned long currentTime = millis();
    if ((currentTime - buttonDownTime) >= 3000) {
      Serial.println("Button held down for 3 seconds.");
      sos_active = true;
      buttonDownTime = currentTime;
      accidentStartTime = millis();
      accident_confirmed = false; 
      buttonCount = 0; 
    }
    if ((currentTime - buttonDownTime) > 3000) {
      buttonCount = 0;
    }
  }
}

// ====================================================================
// DISPLAY UPDATE
// ====================================================================
void updateDisplay() {
  u8g2.firstPage();
  do {
    if (accident_confirmed && !sos_active) {
      // Default screen - show ADAS-Moto branding
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.setCursor(16, 65);
      u8g2.print("ADAS-Moto");
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(48, 82);
      u8g2.print("V1.0");
    }
    
    if (!accident_confirmed || sos_active) {
      u8g2.drawXBMP(30, 30, 70, 70, SOS_icon);
    }
    
    u8g2.drawXBMP(0, 0, 20, 20, signal_icon);
    
    if (!helmetDetected) {
      u8g2.drawXBMP(55, 0, 20, 20, helmet_icon);
    }
    
    u8g2.drawXBMP(108, 0, 20, 20, battery_icon);
    
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(20, 125);
    u8g2.print("ADAS-Moto V1.0");
    
    if (ring_state) {
      u8g2.setCursor(0, 120);
      u8g2.print("Incoming Call");
    }
    
    if (gps_ready) {
      u8g2.setCursor(0, 10);
      u8g2.print("GPS");
    }
  } while (u8g2.nextPage());
}
