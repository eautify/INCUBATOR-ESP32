#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SHT31.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// ============================================
// CONFIGURATION SECTION
// ============================================

// --- Network Configuration ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_client_id = "incubator_01";
const char* mqtt_topic_telemetry = "incubator/telemetry";
const char* mqtt_topic_commands = "incubator/commands";

// --- Pin Definitions ---
#define HEATER_RELAY_PIN 25    // Heater control
#define HUMIDIFIER_RELAY_PIN 26 // Humidifier control
#define REED_SWITCH_PIN 27     // Door sensor
#define CANDLING_PIN 32        // Candling LED control
#define ROLLER_PIN 33          // Roller motor control
#define ERROR_LED_PIN 2        // Built-in LED for errors

// --- Device Constants ---
#define EEPROM_SIZE 512
#define SENSOR_UPDATE_INTERVAL 2000  // 2 seconds
#define CONTROL_LOOP_INTERVAL 100    // 100ms for control
#define NETWORK_RETRY_INTERVAL 30000 // 30 seconds

// ============================================
// GLOBAL OBJECTS
// ============================================

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============================================
// DATA STRUCTURES
// ============================================

enum SystemMode { MODE_OFFLINE, MODE_ONLINE, MODE_FAILSAFE };

struct Settings {
  float temp_on_threshold;     // Turn heater ON when below this
  float temp_off_threshold;    // Turn heater OFF when above this
  float humid_on_threshold;    // Turn humidifier ON when below this
  float humid_off_threshold;   // Turn humidifier OFF when above this
  int dayCounter;
  char device_id[16];
  bool network_enabled;
};

// [REMOVED] Schedule struct deleted

// ============================================
// GLOBAL VARIABLES
// ============================================

Settings settings;
// [REMOVED] Schedule object deleted
SystemMode currentMode = MODE_OFFLINE;

// Device State
bool heaterOn = false;
bool humidifierOn = false;
bool candlingOn = false;
bool rollerOn = false;
bool doorClosed = true;

// Sensor Values
float currentTemp = 0;
float currentHumidity = 0;

// Timers
unsigned long lastSensorUpdate = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastNetworkAttempt = 0;

// Error tracking
bool sensorError = false;
int networkRetryCount = 0;
const int MAX_NETWORK_RETRIES = 5;

// ============================================
// EEPROM FUNCTIONS
// ============================================

void saveSettings() {
  EEPROM.put(0, settings);
  // [REMOVED] Schedule save deleted
  EEPROM.commit();
  Serial.println("Settings saved to EEPROM");
}

void loadSettings() {
  EEPROM.get(0, settings);
  // [REMOVED] Schedule load deleted

  // Validate settings
  if (isnan(settings.temp_on_threshold) || settings.temp_on_threshold <= 0) {
    // Default settings - HYSTERESIS CONTROL ONLY
    settings.temp_on_threshold = 37.2; 
    settings.temp_off_threshold = 37.7; 
    settings.humid_on_threshold = 55.0; 
    settings.humid_off_threshold = 60.0; 
    settings.dayCounter = 0;
    strcpy(settings.device_id, "incubator_01");
    settings.network_enabled = true;
    
    // [REMOVED] Default schedule settings deleted
    
    saveSettings();
    Serial.println("Loaded default settings");
  }
}

// ============================================
// NETWORK FUNCTIONS
// ============================================

void setupWiFi() {
  if (!settings.network_enabled) {
    currentMode = MODE_OFFLINE;
    return;
  }
  
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    currentMode = MODE_ONLINE;
    networkRetryCount = 0;
  } else {
    Serial.println("\nWiFi connection failed");
    currentMode = MODE_OFFLINE;
    networkRetryCount++;
  }
}

void reconnectMQTT() {
  if (currentMode != MODE_ONLINE) return;
  
  // Check WiFi first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnect...");
    setupWiFi();
    return;
  }
  
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(mqtt_client_id)) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic_commands);
      // Publish current settings on connect
      publishSettings();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void publishSettings() {
  if (currentMode == MODE_ONLINE && mqttClient.connected()) {
    char settingsMsg[200];
    snprintf(settingsMsg, sizeof(settingsMsg),
      "{\"type\":\"settings\",\"temp_on\":%.1f,\"temp_off\":%.1f,\"humid_on\":%.1f,\"humid_off\":%.1f,\"day\":%d}",
      settings.temp_on_threshold,
      settings.temp_off_threshold,
      settings.humid_on_threshold,
      settings.humid_off_threshold,
      settings.dayCounter
    );
    mqttClient.publish("incubator/settings", settingsMsg);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println(message);
  handleMQTTCommand(message);
}

void handleMQTTCommand(String command) {
  // MQTT Command Format: "device:command:value"
  
  int firstColon = command.indexOf(':');
  int secondColon = command.lastIndexOf(':');
  
  if (firstColon == -1 || secondColon == -1) return;
  
  String device = command.substring(0, firstColon);
  String cmd = command.substring(firstColon + 1, secondColon);
  String value = command.substring(secondColon + 1);
  
  Serial.print("Parsed MQTT: ");
  Serial.print(device);
  Serial.print(" ");
  Serial.print(cmd);
  Serial.print(" ");
  Serial.println(value);
  
  // Temperature Thresholds
  if (device == "temp") {
    if (cmd == "on") {
      float temp = value.toFloat();
      if (temp > 20 && temp < 40) {
        settings.temp_on_threshold = temp;
        saveSettings();
        Serial.print("Set temp_on_threshold to: ");
        Serial.println(temp);
      }
    } else if (cmd == "off") {
      float temp = value.toFloat();
      if (temp > 20 && temp < 40) {
        settings.temp_off_threshold = temp;
        saveSettings();
        Serial.print("Set temp_off_threshold to: ");
        Serial.println(temp);
      }
    }
  }
  // Humidity Thresholds
  else if (device == "humid") {
    if (cmd == "on") {
      float humid = value.toFloat();
      if (humid > 30 && humid < 80) {
        settings.humid_on_threshold = humid;
        saveSettings();
        Serial.print("Set humid_on_threshold to: ");
        Serial.println(humid);
      }
    } else if (cmd == "off") {
      float humid = value.toFloat();
      if (humid > 30 && humid < 80) {
        settings.humid_off_threshold = humid;
        saveSettings();
        Serial.print("Set humid_off_threshold to: ");
        Serial.println(humid);
      }
    }
  }
  // Device Control
  else if (device == "heater") {
    if (cmd == "state") {
      bool state = (value == "1" || value == "on");
      digitalWrite(HEATER_RELAY_PIN, state ? HIGH : LOW);
      heaterOn = state;
      Serial.print("Heater manually set to: ");
      Serial.println(state ? "ON" : "OFF");
    }
  }
  else if (device == "humidifier") {
    if (cmd == "state") {
      bool state = (value == "1" || value == "on");
      digitalWrite(HUMIDIFIER_RELAY_PIN, state ? HIGH : LOW);
      humidifierOn = state;
      Serial.print("Humidifier manually set to: ");
      Serial.println(state ? "ON" : "OFF");
    }
  }
  else if (device == "roller") {
    if (cmd == "state") {
      bool state = (value == "1" || value == "on");
      digitalWrite(ROLLER_PIN, state ? LOW : HIGH);
      rollerOn = state;
      Serial.print("Roller manually set to: ");
      Serial.println(state ? "ON" : "OFF");
    }
  }
  else if (device == "candling") {
    if (cmd == "state") {
      bool state = (value == "1" || value == "on");
      digitalWrite(CANDLING_PIN, state ? LOW : HIGH);
      candlingOn = state;
      Serial.print("Candling manually set to: ");
      Serial.println(state ? "ON" : "OFF");
    }
  }
  // System Commands
  else if (device == "system") {
      if (cmd == "day") {
        int day = value.toInt();
        if (day >= 0 && day <= 99) {
          settings.dayCounter = day;
          saveSettings();
          Serial.print("Day counter set to: ");
          Serial.println(day);
        }
      } 
      else if (cmd == "reset") {
        Serial.println("MQTT Reset Command Received. Restarting...");
        delay(500); 
        ESP.restart();
      }
      else if (cmd == "mode") {
        if (value == "offline") {
          currentMode = MODE_OFFLINE;
        } else if (value == "online") {
          currentMode = MODE_ONLINE;
          setupWiFi();
        }
      } else if (cmd == "getsettings") {
        publishSettings();
      }
    }
}

// ============================================
// SENSOR FUNCTIONS
// ============================================

void readSensors() {
  // SHT31 Read
  float temp = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    sensorError = true;
    if (currentMode == MODE_ONLINE) {
      char errorMsg[50];
      snprintf(errorMsg, sizeof(errorMsg), "SHT31 sensor failure");
      mqttClient.publish("incubator/errors", errorMsg);
    }
    
    // Enter failsafe if sensor error persists
    static int errorCount = 0;
    errorCount++;
    if (errorCount > 5) {
      enterFailsafeMode();
    }
    
    return;
  }
  
  sensorError = false;
  currentTemp = temp;
  currentHumidity = humidity;
  
  // Read door sensor
  doorClosed = digitalRead(REED_SWITCH_PIN) == HIGH;
  
  // Safety checks
  if (temp > 40.0 || temp < 20.0 || humidity > 90.0 || humidity < 20.0) {
    enterFailsafeMode();
  }
}

// ============================================
// CONTROL FUNCTIONS - HYSTERESIS ONLY
// ============================================

void controlHeater() {
  // HYSTERESIS CONTROL: Turn ON when below on_threshold, OFF when above off_threshold
  
  if (currentTemp <= settings.temp_on_threshold && !heaterOn) {
    digitalWrite(HEATER_RELAY_PIN, HIGH);
    heaterOn = true;
    Serial.print("Heater ON - Temp: ");
    Serial.print(currentTemp);
    Serial.print(" <= ");
    Serial.println(settings.temp_on_threshold);
  }
  else if (currentTemp >= settings.temp_off_threshold && heaterOn) {
    digitalWrite(HEATER_RELAY_PIN, LOW);
    heaterOn = false;
    Serial.print("Heater OFF - Temp: ");
    Serial.print(currentTemp);
    Serial.print(" >= ");
    Serial.println(settings.temp_off_threshold);
  }
}

void controlHumidifier() {
  // HYSTERESIS CONTROL: Turn ON when below on_threshold, OFF when above off_threshold
  
  if (currentHumidity <= settings.humid_on_threshold && !humidifierOn) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH);
    humidifierOn = true;
    Serial.print("Humidifier ON - Humidity: ");
    Serial.print(currentHumidity);
    Serial.print(" <= ");
    Serial.println(settings.humid_on_threshold);
  }
  else if (currentHumidity >= settings.humid_off_threshold && humidifierOn) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
    humidifierOn = false;
    Serial.print("Humidifier OFF - Humidity: ");
    Serial.print(currentHumidity);
    Serial.print(" >= ");
    Serial.println(settings.humid_off_threshold);
  }
}

// [REMOVED] checkSchedule() function deleted

// ============================================
// FAILSAFE MODE
// ============================================

void enterFailsafeMode() {
  currentMode = MODE_FAILSAFE;
  
  // Disable all actuators
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  digitalWrite(CANDLING_PIN, HIGH);
  digitalWrite(ROLLER_PIN, HIGH);
  
  heaterOn = false;
  humidifierOn = false;
  candlingOn = false;
  rollerOn = false;
  
  // Blink error LED
  for (int i = 0; i < 10; i++) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    delay(100);
    digitalWrite(ERROR_LED_PIN, LOW);
    delay(100);
  }
  
  // Display error on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FAILSAFE MODE");
  lcd.setCursor(0, 1);
  lcd.print("Check System!");
}

// ============================================
// DISPLAY FUNCTIONS
// ============================================

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("--=[ DAY ");
  if (settings.dayCounter < 10) lcd.print("0");
  lcd.print(settings.dayCounter);
  lcd.print(" ]==-");
  
  lcd.setCursor(0, 1);
  if (sensorError) {
    lcd.print("Sensor Error!   ");
  } else {
    lcd.print("T:");
    lcd.print(currentTemp, 1);
    lcd.print("C ");
    lcd.print("H:");
    lcd.print(currentHumidity, 1);
    lcd.print("% ");
    
    // Show mode indicator
    if (currentMode == MODE_ONLINE) {
      lcd.print("W");
    } else if (currentMode == MODE_FAILSAFE) {
      lcd.print("F");
    } else {
      lcd.print("L"); // Local/Offline
    }
  }
}

// ============================================
// COMMUNICATION FUNCTIONS
// ============================================

void sendTelemetry() {
  // Send via MQTT if online
  if (currentMode == MODE_ONLINE && mqttClient.connected()) {
    char jsonPayload[256];
    const char* doorState = doorClosed ? "closed" : "open";
    const char* heaterState = heaterOn ? "ON" : "OFF";
    const char* humidifierState = humidifierOn ? "ON" : "OFF";
    const char* rollerState = rollerOn ? "ON" : "OFF";
    const char* modeState = (currentMode == MODE_ONLINE) ? "ONLINE" : "OFFLINE";
    
    snprintf(jsonPayload, sizeof(jsonPayload),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"door\":\"%s\",\"heater\":\"%s\",\"humidifier\":\"%s\",\"roller\":\"%s\",\"day\":%d,\"mode\":\"%s\"}",
      currentTemp,
      currentHumidity,
      doorState,
      heaterState,
      humidifierState,
      rollerState,
      settings.dayCounter,
      modeState
    );
    mqttClient.publish(mqtt_topic_telemetry, jsonPayload);
  }
  
  // Also send via Serial
  Serial.print("Temp:");
  Serial.print(currentTemp, 1);
  Serial.print(",Humidity:");
  Serial.print(currentHumidity, 1);
  Serial.print(",Door:");
  Serial.print(doorClosed ? "CLOSED" : "OPEN");
  Serial.print(",Heater:");
  Serial.print(heaterOn ? "ON" : "OFF");
  Serial.print(",Humidifier:");
  Serial.print(humidifierOn ? "ON" : "OFF");
  Serial.print(",Day:");
  Serial.print(settings.dayCounter);
  Serial.println();
}

// ============================================
// SERIAL COMMAND HANDLER (Legacy Support)
// ============================================

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    // Legacy commands
    if (input.length() >= 4 && input.charAt(2) == ':') {
      String prefix = input.substring(0, 2);
      float value = input.substring(3).toFloat();
      
      if (prefix == "TH") {
        settings.temp_on_threshold = value;
        saveSettings();
        Serial.print("Set temp_on_threshold to: ");
        Serial.println(value);
      }
      else if (prefix == "TF") {
        settings.temp_off_threshold = value;
        saveSettings();
        Serial.print("Set temp_off_threshold to: ");
        Serial.println(value);
      }
      else if (prefix == "HH") {
        settings.humid_on_threshold = value;
        saveSettings();
        Serial.print("Set humid_on_threshold to: ");
        Serial.println(value);
      }
      else if (prefix == "HF") {
        settings.humid_off_threshold = value;
        saveSettings();
        Serial.print("Set humid_off_threshold to: ");
        Serial.println(value);
      }
      
    } else if (input.length() >= 3 && input.charAt(1) == ':') {
      char device = input.charAt(0);
      String command = input.substring(2);
      
      if (device == 'C') {
        digitalWrite(CANDLING_PIN, command == "1" ? LOW : HIGH);
        candlingOn = (command == "1");
      } else if (device == 'L') {
        digitalWrite(ROLLER_PIN, command == "1" ? LOW : HIGH);
        rollerOn = (command == "1");
      } else if (device == 'D') {
        int newDay = command.toInt();
        if (newDay >= 0 && newDay <= 99) {
          settings.dayCounter = newDay;
          saveSettings();
        }
      } else if (device == 'R' && command == "reset") {
        delay(100);
        ESP.restart();
      } else if (device == 'M') {
        if (command == "online") {
          currentMode = MODE_ONLINE;
          setupWiFi();
        } else if (command == "offline") {
          currentMode = MODE_OFFLINE;
        }
      }
    }
  }
}

// ============================================
// MAIN SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  
  // Initialize hardware
  Wire.begin(); 
  lcd.init();
  lcd.backlight();
  
  // Initialize SHT31
  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 Not Found! Check Wiring.");
    lcd.setCursor(0, 0);
    lcd.print("SHT31 Error!");
  } else {
    Serial.println("SHT31 Found!");
  }
  
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("EEPROM initialization failed!");
    delay(1000);
    ESP.restart();
  }
  
  // Load settings
  loadSettings();
  
  // Initialize pins
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CANDLING_PIN, OUTPUT);
  pinMode(ROLLER_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  // Set default states
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  digitalWrite(CANDLING_PIN, HIGH); // HIGH = OFF (active LOW)
  digitalWrite(ROLLER_PIN, HIGH);   // HIGH = OFF (active LOW)
  digitalWrite(ERROR_LED_PIN, LOW);
  
  // Connect to network
  setupWiFi();
  
  // Setup MQTT
  if (currentMode == MODE_ONLINE) {
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(mqttCallback);
    reconnectMQTT();
  }
  
  // Initial display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Incubator v2.0");
  lcd.setCursor(0, 1);
  lcd.print("SHT31 Mode");
  delay(2000);
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
  unsigned long currentMillis = millis();
  
  // 1. Handle Serial Input (always)
  handleSerialInput();
  
  // 2. Network Management
  if (currentMode == MODE_ONLINE) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  } else if (currentMode == MODE_OFFLINE) {
    if (currentMillis - lastNetworkAttempt > NETWORK_RETRY_INTERVAL) {
      lastNetworkAttempt = currentMillis;
      if (networkRetryCount < MAX_NETWORK_RETRIES) {
        setupWiFi();
      }
    }
  }
  
  // 3. Sensor Reading (every 2 seconds)
  if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    lastSensorUpdate = currentMillis;
    readSensors();
    updateDisplay();
    sendTelemetry();
  }
  
  // 4. Control Loop (every 100ms)
  if (currentMillis - lastControlUpdate >= CONTROL_LOOP_INTERVAL) {
    lastControlUpdate = currentMillis;
    if (currentMode != MODE_FAILSAFE) {
      controlHeater();
      controlHumidifier();
    }
  }
}