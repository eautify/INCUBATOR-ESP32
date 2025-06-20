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
#define REED_SWITCH_PIN 5 // Reed switch pin
#define CANDLING_PIN 6    // Candling Pin

// Temperature thresholds
const float HEATER_ON_THRESHOLD = 37.0;
const float HEATER_OFF_THRESHOLD = 37.5;

// Humidity thresholds
const float HUMIDIFIER_ON_THRESHOLD = 50.0;
const float HUMIDIFIER_OFF_THRESHOLD = 55.0;

// Global state variables
bool heaterOn = false;
bool humidifierOn = false;
bool candlingOn = false;
int dayCounter = 0;

void setup() {
  Serial.begin(9600);
  Wire.setClock(10000); // Optional: remove if not necessary
  lcd.init();
  lcd.backlight();

  dht.begin();

  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CANDLING_PIN, OUTPUT);

  // Initialize relays to OFF state (LOW signal for NC relays)
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  digitalWrite(CANDLING_PIN, HIGH); // HIGH keeps candling OFF
  heaterOn = false;
  humidifierOn = false;

  // Read dayCounter from EEPROM and validate
  EEPROM.get(0, dayCounter);
  if (dayCounter < 0 || dayCounter > 99) {
    dayCounter = 0;
    EEPROM.put(0, dayCounter);
  }

  wdt_enable(WDTO_4S); // Enable 4-second watchdog
}

void loop() {
  wdt_reset(); // Reset watchdog at the start of each loop

  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool doorClosed = digitalRead(REED_SWITCH_PIN) == LOW;

  // Variables for display/serial values (ERR or float string)
  char tempDisplay[8];
  char humDisplay[8];

  if (isnan(temp) || isnan(humidity)) {
    // DHT sensor error: turn off relays, set display strings to "ERR"
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

    // LCD display for sensor error
    lcd.setCursor(0, 0);
    lcd.print("DHT Sensor Error");
    lcd.setCursor(0, 1);
    lcd.print("  Fix DHT now!  ");
  } else {
    // Normal operation: convert floats to strings for LCD/Serial
    dtostrf(temp, 4, 1, tempDisplay);
    dtostrf(humidity, 4, 1, humDisplay);

    // Control heater with hysteresis (NC relay logic)
    if (temp <= HEATER_ON_THRESHOLD && !heaterOn) {
      digitalWrite(HEATER_RELAY_PIN, HIGH);
      heaterOn = true;
    } else if (temp >= HEATER_OFF_THRESHOLD && heaterOn) {
      digitalWrite(HEATER_RELAY_PIN, LOW);
      heaterOn = false;
    }

    // Control humidifier with hysteresis (NC relay logic)
    if (humidity <= HUMIDIFIER_ON_THRESHOLD && !humidifierOn) {
      digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH);
      humidifierOn = true;
    } else if (humidity >= HUMIDIFIER_OFF_THRESHOLD && humidifierOn) {
      digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
      humidifierOn = false;
    }

    // LCD display for normal operation
    lcd.setCursor(0, 0);
    lcd.print("--=[ DAY ");
    if (dayCounter < 10) lcd.print("0");
    lcd.print(dayCounter);
    lcd.print(" ]==-");

    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C ");
    lcd.print("H:");
    lcd.print(humidity, 1);
    lcd.print("% ");
  }

  // Handle incoming serial commands
  handleSerialInput();

  // Serial output: consistent format for all states
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

  Serial.flush(); // Ensure serial data is sent before delay
  delay(2000);
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() >= 3 && input.charAt(1) == ':') {
      char device = input.charAt(0);
      String command = input.substring(2);

      if (device == 'C') { // Candling control
        digitalWrite(CANDLING_PIN, command == "1" ? LOW : HIGH);
        candlingOn = (command == "1");
      } else if (device == 'D') { // Day counter set
        int newDay = command.toInt();
        if (newDay >= 0 && newDay <= 99) {
          dayCounter = newDay;
          EEPROM.put(0, dayCounter);
        } else {
          dayCounter = 99; // Invalid input defaults to 99
          EEPROM.put(0, dayCounter);
        }
      } else if (device == 'R') {
        if (command == "reset") {
          Serial.flush();             // Ensure all serial data is sent
          wdt_enable(WDTO_15MS);      // Enable watchdog with 15ms timeout
          while (true) {}             // Wait for reset
        }
      }
    }
  }
}