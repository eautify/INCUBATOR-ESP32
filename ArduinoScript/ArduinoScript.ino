#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <avr/wdt.h>  // Watchdog

// LCD settings
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT22 settings
#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Relay control pins
#define HEATER_RELAY_PIN 3
#define HUMIDIFIER_RELAY_PIN 4

// Reed switch pin
#define REED_SWITCH_PIN 5

// Thresholds
const float HEATER_ON_THRESHOLD = 37.0;   // Turn heater ON below this temp
const float HEATER_OFF_THRESHOLD = 37.5;  // Turn heater OFF above this temp
float targetHumidity = 55.0;

// Global state variables
bool heaterOn = false;
bool humidifierOn = false;

void setup() {
  Serial.begin(9600);
  Wire.setClock(10000); // Optional - remove if not necessary
  lcd.init();
  lcd.backlight();
  
  dht.begin();
  
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);

  // Initialize relays to OFF state
  digitalWrite(HEATER_RELAY_PIN, HIGH); // Heater OFF
  digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH); // Humidifier OFF
  heaterOn = false;
  humidifierOn = false;

  wdt_enable(WDTO_4S); // Enable 4-second watchdog
}

// Retry functions
float readTemperatureWithRetry() {
  for (int i = 0; i < 3; i++) {
    float t = dht.readTemperature();
    if (!isnan(t)) return t;
    delay(100);
  }
  return NAN;
}

float readHumidityWithRetry() {
  for (int i = 0; i < 3; i++) {
    float h = dht.readHumidity();
    if (!isnan(h)) return h;
    delay(100);
  }
  return NAN;
}

void loop() {
  wdt_reset(); // Reset watchdog at the start of each loop

  float temp = readTemperatureWithRetry();
  float humidity = readHumidityWithRetry();
  bool doorClosed = digitalRead(REED_SWITCH_PIN) == HIGH;

  if (isnan(temp) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error     ");
    if (Serial) Serial.println("Error: Failed to read from DHT sensor.");
    delay(2000);
    return;
  }

  // Control heater with hysteresis (thermostat behavior)
  if (temp <= HEATER_ON_THRESHOLD && !heaterOn) {
    digitalWrite(HEATER_RELAY_PIN, HIGH); // Heater ON
    heaterOn = true;
  } 
  else if (temp >= HEATER_OFF_THRESHOLD && heaterOn) {
    digitalWrite(HEATER_RELAY_PIN, LOW); // Heater OFF
    heaterOn = false;
  }

  // Control humidifier (simple threshold)
  if (humidity < targetHumidity && !humidifierOn) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, LOW); // Humidifier ON
    humidifierOn = true;
  } 
  else if (humidity >= targetHumidity && humidifierOn) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH); // Humidifier OFF
    humidifierOn = false;
  }

  // LCD Display
  lcd.setCursor(0, 0);
  lcd.print("--=[ DAY 00 ]=--");

  lcd.setCursor(2, 1);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  // Serial output (guarded)
  if (Serial) {
    Serial.print("Temp:");
    Serial.print(temp);
    Serial.print(",Humidity:");
    Serial.print(humidity);
    Serial.print(",Door:");
    Serial.print(doorClosed ? "CLOSED" : "OPEN");
    Serial.print(",Heater:");
    Serial.print(heaterOn ? "ON" : "OFF");
    Serial.print(",Humidifier:");
    Serial.println(humidifierOn ? "ON" : "OFF");
  }

  delay(2000);
}