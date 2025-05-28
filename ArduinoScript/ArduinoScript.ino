#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

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
float targetTemp = 37.5;
float targetHumidity = 55.0;

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  
  dht.begin();
  
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
}

void loop() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool doorClosed = digitalRead(REED_SWITCH_PIN) == HIGH;

  if (isnan(temp) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error     ");
    Serial.println("Error: Failed to read from DHT sensor.");
    delay(2000);
    return;
  }

  // Control heater
  bool heaterOn = false;
  if (temp < targetTemp) {
    digitalWrite(HEATER_RELAY_PIN, HIGH);
    heaterOn = true;
  } else {
    digitalWrite(HEATER_RELAY_PIN, LOW);
  }

  // Control humidifier
  bool humidifierOn = false;
  if (humidity < targetHumidity) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH);
    humidifierOn = true;
  } else {
    digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  }

  // LCD Display
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Door:");
  lcd.print(doorClosed ? "CLOSED " : "OPEN   ");

  // Serial output with actuator states
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

  delay(2000);
}
