# ESP32‑Incubator‑Controller

Firmware for an ESP32‑based egg incubator with temperature and humidity control (hysteresis), remote monitoring & control over MQTT, local LCD display, and failsafe protection.

## Overview

The controller maintains a stable environment inside an incubator using:
- **SHT31** sensor for temperature and humidity readings.
- **Hysteresis control** for heater and humidifier relays.
- **Optional MQTT** connection for telemetry and remote commands.
- **Local 16×2 I2C LCD** for day counter and live sensor data.
- **Door sensor (reed switch)** and **candling LED**.
- **Failsafe mode** – disables all outputs if sensor fails or environmental limits are exceeded.

All configurable settings (thresholds, day counter, network flag) are stored in **EEPROM** and survive power cycles.

---

## Hardware Requirements

- ESP32 development board (e.g., DOIT ESP32 DevKit V1)
- SHT31 temperature/humidity sensor (I2C)
- I2C 16×2 LCD (address `0x27`)
- Relay modules (heater, humidifier, roller motor)
- Candling LED (active‑low control)
- Reed switch (door sensor, normally open)
- Built‑in or external error LED (active‑high)
- 5V / appropriate power supply for relays and ESP32

---

## Pin Mapping (Wiring Diagram)

| Component          | ESP32 Pin | Direction | Notes                         |
|--------------------|-----------|-----------|-------------------------------|
| Heater relay       | GPIO 25   | Output    | HIGH = heater ON              |
| Humidifier relay   | GPIO 26   | Output    | HIGH = humidifier ON          |
| Reed switch (door) | GPIO 27   | Input     | Internal pull‑up, HIGH = closed |
| Candling LED       | GPIO 32   | Output    | LOW = LED ON                  |
| Roller motor relay | GPIO 33   | Output    | LOW = motor ON (active‑low)   |
| Error LED          | GPIO 2    | Output    | Built‑in LED, HIGH = error    |
| SHT31 (I2C)        | 21 (SDA) <br> 22 (SCL) | -    | Address `0x44`                |
| LCD (I2C)          | 21 (SDA) <br> 22 (SCL) | -    | Address `0x27`                |

> All relays should be connected through appropriate driver circuits (e.g., transistor or relay module). Active levels are as listed above.

---

## Required Libraries

Install the following libraries using the **Arduino Library Manager** or manually from GitHub:

| Library                     | Version (tested) | Purpose                    |
|-----------------------------|------------------|----------------------------|
| `LiquidCrystal I2C` (by Frank de Brabander) | 1.1.2+ | I2C LCD control            |
| `Adafruit SHT31`            | 2.2.0+           | Read SHT31 sensor          |
| `PubSubClient`              | 2.8.0+           | MQTT communication         |
| `WiFi`                      | built‑in         | ESP32 WiFi                 |
| `EEPROM`                    | built‑in         | Persistent settings        |

**Installation (Arduino IDE)**  
`Tools → Manage Libraries…` → search for each name → install.

---

## Installation & Setup

### 1. Obtain the code
Clone or download the repository:
```bash
https://github.com/eautify/INCUBATOR-ESP32.git
```
Open `ArduinoScript.ino` in Arduino IDE.

### 2. Configure `secrets.h`
- Rename `secrets.example.h` → `secrets.h`.
- Edit `secrets.h` with your credentials:
```cpp
#pragma once

const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
const char* MQTT_SERVER = "192.168.1.100";   // IP or hostname
```
> MQTT port is hardcoded to `1883` (unencrypted). Adjust `mqttClient.setServer()` in code if needed.

### 3. Select board and upload
- Board: `ESP32 Dev Module` (or your specific ESP32 variant)
- Port: select the correct COM / tty port
- Press **Upload**

After upload, open **Serial Monitor** (115200 baud) to see status messages.

---

## MQTT Setup (External Broker)

The controller publishes telemetry and can receive commands. You need an MQTT broker reachable by the ESP32.

### Quick broker setup on a local machine (Linux / Raspberry Pi):
```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```
For Windows/macOS, install **Mosquitto** or use a public broker (not recommended for production).

### Using a cloud broker (e.g., HiveMQ Cloud, EMQX Cloud):
- Create an instance, note the hostname and port (usually 1883 for plain TCP, or 8883 for TLS – TLS not supported by this firmware).
- In `secrets.h`, set `MQTT_SERVER` to the broker’s IP or hostname.

> **No username/password** – the firmware does not implement MQTT authentication. Secure your broker network or add authentication in `mqttClient.connect()` if required.

### Topics used by the firmware

| Topic                     | Direction | Description                             |
|---------------------------|-----------|-----------------------------------------|
| `incubator/telemetry`     | Publish   | JSON with temp, humidity, states, day   |
| `incubator/commands`      | Subscribe | Commands for thresholds and manual control |
| `incubator/settings`      | Publish   | Current thresholds & day counter        |
| `incubator/errors`        | Publish   | Error messages (e.g., sensor failure)   |

Command format expected on `incubator/commands`:  
`device:command:value`  
Examples:
- `temp:on:37.2`   → set heater ON threshold to 37.2°C
- `temp:off:37.7`  → set heater OFF threshold to 37.7°C
- `humid:on:55.0`  → set humidifier ON threshold to 55%
- `humid:off:60.0` → set humidifier OFF threshold to 60%
- `heater:state:1` → manually turn heater ON (bypass hysteresis)
- `system:day:5`   → set day counter to 5
- `system:reset`   → restart ESP32
- `system:mode:offline` → switch to local control (no MQTT)

For a complete list, refer to the `handleMQTTCommand()` function in the source code.

---

## Operation

### Normal mode
- The LCD shows `DAY nn` on the first line.
- Second line shows temperature (`T:xx.xC`), humidity (`H:xx.x%`), and mode indicator:
  - `W` = WiFi + MQTT online
  - `L` = offline (local only)
  - `F` = failsafe mode

### Hysteresis control (default)
- **Heater**: turns ON when temperature ≤ `temp_on_threshold`, turns OFF when ≥ `temp_off_threshold`.
- **Humidifier**: turns ON when humidity ≤ `humid_on_threshold`, turns OFF when ≥ `humid_off_threshold`.

### Manual override via MQTT
Commands `heater:state:1/0`, `humidifier:state:1/0`, `roller:state:1/0`, `candling:state:1/0` directly set the output. Manual control stays active until next control loop (every 100 ms) where hysteresis may override it.

### Legacy serial commands (for debugging)
Format: `CC:value` where `CC` is a two‑letter code:
- `TH:37.2` – set heater ON threshold
- `TF:37.7` – set heater OFF threshold
- `HH:55.0` – set humidifier ON threshold
- `HF:60.0` – set humidifier OFF threshold
- `C:1` / `C:0` – turn candling ON/OFF
- `L:1` / `L:0` – turn roller ON/OFF
- `D:5` – set day counter
- `R:reset` – restart ESP32
- `M:online` / `M:offline` – switch networking mode

### Failsafe mode
The controller enters failsafe if:
- SHT31 sensor fails to read for 5 consecutive cycles.
- Temperature goes above 40°C or below 20°C.
- Humidity goes above 90% or below 20%.

In failsafe:
- All relays are turned OFF.
- Error LED blinks 10 times.
- LCD shows `FAILSAFE MODE`.
- The device remains in failsafe until power‑cycled or manually reset.

---

## Configuration Storage

Settings are saved in EEPROM (address 0) and restored on startup. Defaults (if EEPROM is invalid):

| Parameter            | Default value |
|----------------------|---------------|
| Heater ON threshold  | 37.2 °C       |
| Heater OFF threshold | 37.7 °C       |
| Humidifier ON threshold | 55.0 %     |
| Humidifier OFF threshold| 60.0 %     |
| Day counter          | 0             |
| Device ID            | `incubator_01`|
| Network enabled      | `true`        |

To reset to defaults, clear EEPROM (e.g., upload an empty sketch with `EEPROM.begin()` and `EEPROM.clear()`) or modify `loadSettings()`.

---

## Documentation

The source code (`ArduinoScript.ino`) is **heavily commented** and organised into sections:
- Configuration & pin definitions
- Data structures
- EEPROM, network, MQTT callbacks
- Sensor reading, hysteresis control, failsafe
- Display and serial commands

For any further understanding, refer to the inline comments. The code follows a modular structure – changes to control logic can be made inside `controlHeater()` / `controlHumidifier()`.

---

## Troubleshooting

| Symptom                                | Likely cause & solution                     |
|----------------------------------------|---------------------------------------------|
| LCD shows `SHT31 Error!`               | Sensor not wired or wrong I2C address. Check power and pins. |
| WiFi not connecting                    | Wrong SSID/password in `secrets.h`; network out of range. |
| MQTT not connecting                    | Broker not running; firewall blocking port 1883; wrong IP in `secrets.h`. |
| Heater/humidifier not switching        | Relay wiring wrong; GPIO not toggling (check with multimeter). |
| Failsafe triggers immediately          | Sensor gives invalid readings – check SHT31. |
| Roller or candling LED inactive        | Active‑low logic: `LOW` = ON. Verify external wiring matches. |

For further help, enable verbose serial output (already enabled) and monitor `Serial.println()` messages.

---

## License & Contribution

This firmware is released under the **MIT License**. Feel free to modify for your incubator needs. The original author intends this repository for a smooth handover – please keep documentation up‑to‑date.
