#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <avr/wdt.h>
#include <EEPROM.h>

// LCD settings
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT22 settings
#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Relay control pins
#define HEATER_RELAY_PIN 3
#define HUMIDIFIER_RELAY_PIN 4
#define REED_SWITCH_PIN 5
#define CANDLING_PIN 6

// --- NEW: Settings Struct ---
// This struct holds all the values we want to save in EEPROM.
// It makes reading and writing to memory much cleaner.
struct Settings {
  float temp_on_threshold;
  float temp_off_threshold;
  float humid_on_threshold;
  float humid_off_threshold;
  int dayCounter;
};
Settings settings; // Create a global instance of our settings

// Global state variables
bool heaterOn = false;
bool humidifierOn = false;
bool candlingOn = false;

// --- Helper function to save settings to EEPROM ---
void saveSettings() {
  EEPROM.put(0, settings);
}

void setup() {
  Serial.begin(9600);
  Wire.setClock(10000);
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP); // Use internal pull-up for stability
  pinMode(CANDLING_PIN, OUTPUT);

  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  digitalWrite(CANDLING_PIN, HIGH);
  heaterOn = false;
  humidifierOn = false;

  // --- NEW: Load settings from EEPROM on startup ---
  EEPROM.get(0, settings);

  // Validate the loaded settings. If they seem invalid (like after a fresh
  // flash), initialize them with safe default values and save them.
  if (isnan(settings.temp_on_threshold) || settings.temp_on_threshold <= 0 || settings.dayCounter < 0 || settings.dayCounter > 99) {
    settings.temp_on_threshold = 37.2;
    settings.temp_off_threshold = 37.7;
    settings.humid_on_threshold = 55.0;
    settings.humid_off_threshold = 60.0;
    settings.dayCounter = 0;
    saveSettings(); // Save these defaults to EEPROM
  }

  wdt_enable(WDTO_4S);
}

void loop() {
  wdt_reset();

  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool doorClosed = digitalRead(REED_SWITCH_PIN) == LOW;

  char tempDisplay[8];
  char humDisplay[8];

  if (isnan(temp) || isnan(humidity)) {
    if (heaterOn) {
      digitalWrite(HEATER_RELAY_PIN, LOW);
      heaterOn = false;
    }
    if (humidifierOn) {
      digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
      humidifierOn = false;
    }
    strcpy(tempDisplay, "ERR");
    strcpy(humDisplay, "ERR");

    lcd.setCursor(0, 0);
    lcd.print("DHT Sensor Error");
    lcd.setCursor(0, 1);
    lcd.print("  Fix DHT now!  ");
  } else {
    dtostrf(temp, 4, 1, tempDisplay);
    dtostrf(humidity, 4, 1, humDisplay);

    // --- UPDATED: Use dynamic settings from the settings struct ---
    if (temp <= settings.temp_on_threshold && !heaterOn) {
      digitalWrite(HEATER_RELAY_PIN, HIGH);
      heaterOn = true;
    } else if (temp >= settings.temp_off_threshold && heaterOn) {
      digitalWrite(HEATER_RELAY_PIN, LOW);
      heaterOn = false;
    }

    if (humidity <= settings.humid_on_threshold && !humidifierOn) {
      digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH);
      humidifierOn = true;
    } else if (humidity >= settings.humid_off_threshold && humidifierOn) {
      digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
      humidifierOn = false;
    }

    lcd.setCursor(0, 0);
    lcd.print("--=[ DAY ");
    if (settings.dayCounter < 10) lcd.print("0");
    lcd.print(settings.dayCounter);
    lcd.print(" ]==-");

    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C ");
    lcd.print("H:");
    lcd.print(humidity, 1);
    lcd.print("% ");
  }

  handleSerialInput();

  Serial.print("Temp:");
  Serial.print(tempDisplay);
  Serial.print(",Humidity:");
  Serial.print(humDisplay);
  Serial.print(",Door:");
  Serial.print(doorClosed ? "CLOSED" : "OPEN");
  Serial.print(",Heater:");
  Serial.print(heaterOn ? "ON" : "OFF");
  Serial.print(",Humidifier:");
  Serial.println(humidifierOn ? "ON" : "OFF");

  Serial.flush();
  delay(2000);
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() >= 4 && input.charAt(2) == ':') {
      String prefix = input.substring(0, 2);  // "TH", "TF", etc.
      String valueStr = input.substring(3);   // "37.2", etc.
      float value = valueStr.toFloat();

      if (prefix == "TH") {
        settings.temp_on_threshold = value;
      } else if (prefix == "TF") {
        settings.temp_off_threshold = value;
      } else if (prefix == "HH") {
        settings.humid_on_threshold = value;
      } else if (prefix == "HF") {
        settings.humid_off_threshold = value;
      }
      saveSettings();
    } 
    else if (input.length() >= 3 && input.charAt(1) == ':') {
      char device = input.charAt(0);
      String command = input.substring(2);

      if (device == 'C') {
        digitalWrite(CANDLING_PIN, command == "1" ? LOW : HIGH);
        candlingOn = (command == "1");
      } else if (device == 'D') {
        int newDay = command.toInt();
        if (newDay >= 0 && newDay <= 99) {
          settings.dayCounter = newDay;
          saveSettings();
        }
      } else if (device == 'R' && command == "reset") {
        Serial.flush();
        wdt_enable(WDTO_15MS);
        while (true) {}
      }
    }
  }
}
