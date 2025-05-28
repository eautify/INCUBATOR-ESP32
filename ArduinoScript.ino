#include <DHT.h>
#include <avr/wdt.h> // Watchdog timer

#define DHTPIN 2       
#define DHTTYPE DHT22  
#define CANDLING_PIN 4    
#define DOOR_SWITCH_PIN 5 
int DOOR_ALARM_PIN = 6;

DHT dht(DHTPIN, DHTTYPE);

// Debounce variables
const int debounceDelay = 50;
int lastDoorState = HIGH;
int doorState = HIGH;
unsigned long lastDebounceTime = 0;

// DHT reading interval
unsigned long lastDHTRead = 0;
const int dhtInterval = 2000; 

void setup() {
    Serial.begin(9600);
    wdt_enable(WDTO_8S); // Enable watchdog timer

    pinMode(CANDLING_PIN, OUTPUT);
    pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
    pinMode(DOOR_ALARM_PIN, OUTPUT);

    digitalWrite(CANDLING_PIN, HIGH);  // Start off (HIGH = off for relay)
    digitalWrite(DOOR_ALARM_PIN, LOW); // Alarm off

    dht.begin();
}

void loop() {
    wdt_reset(); // Reset watchdog timer

    // Reed Switch Debounce
    int reading = digitalRead(DOOR_SWITCH_PIN);
    if (reading != lastDoorState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != doorState) {
            doorState = reading;
            controlDoorAlarm(doorState);
        }
    }
    lastDoorState = reading;

    // DHT Sensor Reading
    if (millis() - lastDHTRead >= dhtInterval) {
        lastDHTRead = millis();

        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            Serial.print(temperature);
            Serial.print(",");
            Serial.print(humidity);
            Serial.print(",");
            Serial.println(doorState == LOW ? "OPEN" : "CLOSED");
        } else {
            Serial.print("?");
            Serial.print(",");
            Serial.print("?");
            Serial.print(",");
            Serial.println(doorState == LOW ? "OPEN" : "CLOSED");
        }
    }

    // Process Serial Command for Candling
    // processSerialInput(); commented for future use
}

void controlDoorAlarm(int state) {
    if (state == LOW) {
        digitalWrite(DOOR_ALARM_PIN, HIGH);  // Door open → alarm ON
    } else {
        digitalWrite(DOOR_ALARM_PIN, LOW);   // Door closed → alarm OFF
    }
}


// TODO: ADJUST CODE FOR FUTURE PYTHON CODE COMPATIBILITY
// void processSerialInput() {
//     if (Serial.available() > 0) {
//         char receivedData[3]; // e.g., "C1\n"
//         int bytesRead = Serial.readBytesUntil('\n', receivedData, 3);
//         receivedData[bytesRead] = '\0';

//         if (bytesRead == 2) {
//             char relay = receivedData[0];
//             int state = receivedData[1] - '0';

//             if (relay == 'C') {
//                 digitalWrite(CANDLING_PIN, state ? LOW : HIGH); // LOW = ON
//             }
//         }
//     }
// }
