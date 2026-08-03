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

This firmware uses MQTT so the ESP32 can publish incubator readings and receive remote commands. MQTT needs a broker, which is the middle server that receives messages from one client and forwards them to other clients subscribed to the same topic.

For this project, use **Eclipse Mosquitto only** as the external broker.

> Current firmware expectation: plain MQTT on port `1883`, no TLS, and no MQTT username/password. This matches the code in `ArduinoScript.ino`, where `mqttClient.setServer(mqtt_server, 1883)` is used and the ESP32 connects with `mqttClient.connect(mqtt_client_id)`. Do not expose this broker directly to the public internet unless you first add authentication/TLS support to the firmware.

### What you need

| Item | Where to get it | Purpose |
|------|-----------------|---------|
| Eclipse Mosquitto broker | https://mosquitto.org/download/ | The MQTT server that the ESP32 connects to. |
| MQTT Explorer | https://mqtt-explorer.com/ | Optional desktop tool for checking topics, telemetry, and test commands. |
| ESP32 firmware files | This repository, especially `ArduinoScript/ArduinoScript.ino` and `ArduinoScript/secrets.h` | The MQTT client that publishes and receives incubator data. |
| Broker computer IP address | From `ipconfig` on Windows, or your router client list | The address placed in `MQTT_SERVER`. |

### Recommended setup choice

Use **plain MQTT on port `1883` inside your local WiFi/LAN**.

This is recommended for this firmware because the code already uses a normal `WiFiClient`, not `WiFiClientSecure`. MQTT over TLS normally uses port `8883`, certificates, and secure client code changes. If you are just setting up the incubator on your own WiFi network, use port `1883`.

### Network layout

Your setup should look like this:

```text
ESP32 Incubator  --->  WiFi Router  --->  Computer running Mosquitto
       |                                      |
       | publishes/subscribes MQTT            | broker IP goes in secrets.h
       |                                      |
       +-------------- MQTT port 1883 --------+
```

The ESP32 and the Mosquitto computer must be on the same network unless you have intentionally configured routing/VPN access.

### Step 1: Install Mosquitto on Windows

1. Open the official Mosquitto download page:
   https://mosquitto.org/download/
2. Under **Windows**, download the latest `mosquitto-...-install-windows-x64.exe` installer.
3. Run the installer.
4. Accept the default install location unless you have a reason to change it.
5. The usual install folder is:

```text
C:\Program Files\mosquitto
```

Expected result:

```text
C:\Program Files\mosquitto\mosquitto.exe
C:\Program Files\mosquitto\mosquitto.conf
C:\Program Files\mosquitto\mosquitto_sub.exe
C:\Program Files\mosquitto\mosquitto_pub.exe
```

<img src="https://i.imgur.com/iv75Lzr.png" alt="Mosquitto installed folder showing" width="650">


### Step 2: Configure Mosquitto for ESP32 LAN access

Mosquitto 2.x commonly needs an explicit listener before other devices on the network can connect. Open this file as Administrator:

```text
C:\Program Files\mosquitto\mosquitto.conf
```

Add these lines near the end of the file:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

What these lines mean:

| Line | Meaning |
|------|---------|
| `listener 1883 0.0.0.0` | Mosquitto listens for MQTT connections on port `1883` from network devices, not only from the broker computer itself. |
| `allow_anonymous true` | Allows clients without username/password. This is required by the current firmware unless you edit `mqttClient.connect()` to use credentials. |

Save the file.

Expected config snippet:

```conf
# ESP32 incubator local MQTT listener
listener 1883 0.0.0.0
allow_anonymous true
```

<img src="https://i.imgur.com/1FgSqjX.png" alt="mosquitto.conf with the listener and allow_anonymous lines visible" width="650">


### Step 3: Start or restart Mosquitto

If Mosquitto is installed as a Windows service, restart it:

```powershell
Restart-Service mosquitto
```

If you want to run it manually for testing, open PowerShell as Administrator:

```powershell
cd "C:\Program Files\mosquitto"
.\mosquitto.exe -c .\mosquitto.conf -v
```

Expected manual broker output:

```text
mosquitto version ... starting
Config loaded from .\mosquitto.conf.
Opening ipv4 listen socket on port 1883.
mosquitto version ... running
```

Leave this window open while testing. The `-v` flag enables verbose logs, so you should later see ESP32 connection, subscribe, and publish messages.

<img src="https://i.imgur.com/ZVvIyy3.png" alt="CMD showing Mosquitto running and listening on port 1883" width="650">


### Step 4: Allow port 1883 through Windows Firewall

If the ESP32 cannot connect, Windows Firewall may be blocking inbound MQTT traffic.

1. Open **Windows Defender Firewall with Advanced Security**.
2. Go to **Inbound Rules**.
3. Create a **New Rule**.
4. Choose **Port**.
5. Choose **TCP** and enter:

```text
1883
```

6. Choose **Allow the connection**.
7. Apply it to your private network profile.
8. Name it something clear, for example:

```text
Mosquitto MQTT 1883
```

Expected result: another device on the same WiFi/LAN can connect to the broker computer on TCP port `1883`.

### Step 5: Find the broker computer IP address

The ESP32 needs the IP address of the computer running Mosquitto.

On the Mosquitto computer, open PowerShell or Command Prompt:

```powershell
ipconfig
```

Look for the active WiFi or Ethernet adapter and copy the **IPv4 Address**.

Example:

```text
Wireless LAN adapter Wi-Fi:
   IPv4 Address. . . . . . . . . . . : 192.168.1.100
```

Use the IPv4 address, not `localhost` and not `127.0.0.1`.

Important:

| Address | Use it for ESP32? | Why |
|---------|-------------------|-----|
| `192.168.x.x` / `10.x.x.x` / `172.16-31.x.x` | Yes | Normal local network address reachable by the ESP32. |
| `localhost` | No | Means "this same device"; on the ESP32 it would point to the ESP32 itself. |
| `127.0.0.1` | No | Same problem as `localhost`. |

### Step 6: Configure the ESP32 MQTT server

Create your real secrets file if you have not already done so:

```text
ArduinoScript/secrets.example.h  ->  ArduinoScript/secrets.h
```

Edit `ArduinoScript/secrets.h`:

```cpp
#pragma once

const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* MQTT_SERVER = "192.168.1.100";
```

Replace `192.168.1.100` with the IPv4 address of the computer running Mosquitto.

Make sure:

| Setting | Correct value |
|---------|---------------|
| `WIFI_SSID` | The same WiFi/LAN used by the Mosquitto computer. |
| `WIFI_PASSWORD` | The password for that WiFi. |
| `MQTT_SERVER` | The Mosquitto computer IPv4 address. |
| MQTT port | `1883`, already set in `ArduinoScript.ino`. |
| MQTT username/password | None, because the current firmware does not send credentials. |


### Step 7: Upload and check Serial Monitor

Upload the firmware to the ESP32, then open Serial Monitor at:

```text
115200 baud
```

Expected successful output:

```text
Connecting to WiFi...
WiFi connected
IP address: 192.168.1.xxx
Attempting MQTT connection...connected
```

If MQTT connects, the ESP32 subscribes to:

```text
incubator/commands
```

It also publishes current settings after connecting.

### Step 8: Test with Mosquitto command-line tools

Open a second PowerShell window on the Mosquitto computer:

```powershell
cd "C:\Program Files\mosquitto"
```

Subscribe to all incubator topics:

```powershell
.\mosquitto_sub.exe -h localhost -p 1883 -t "incubator/#" -v
```

Expected telemetry output after the ESP32 publishes:

```text
incubator/settings {"type":"settings","temp_on":37.2,"temp_off":37.7,"humid_on":55.0,"humid_off":60.0,"day":0}
incubator/telemetry {"temp":37.4,"humidity":58.2,"door":"closed","heater":"OFF","humidifier":"OFF","roller":"OFF","day":0,"mode":"ONLINE"}
```

Open a third PowerShell window and publish a test command:

```powershell
cd "C:\Program Files\mosquitto"
.\mosquitto_pub.exe -h localhost -p 1883 -t "incubator/commands" -m "system:getsettings:1"
```

Expected result:

```text
incubator/settings {"type":"settings","temp_on":37.2,"temp_off":37.7,"humid_on":55.0,"humid_off":60.0,"day":0}
```

### Step 9: Optional quick check with MQTT Explorer

MQTT Explorer gives a visual topic tree. Download it from:

```text
https://mqtt-explorer.com/
```

Create a connection with:

| Field | Value |
|-------|-------|
| Host | Mosquitto computer IP address, for example `192.168.1.100` |
| Port | `1883` |
| Username | Leave blank |
| Password | Leave blank |
| Encryption / TLS | Off |

After connecting, you should see the `incubator` topic tree when the ESP32 publishes data.


### Topics used by the firmware

| Topic | Direction from ESP32 | Description |
|-------|----------------------|-------------|
| `incubator/telemetry` | Publish | Live JSON payload with temperature, humidity, door state, relay states, day counter, and mode. |
| `incubator/commands` | Subscribe | Commands sent to the ESP32. Publish commands here from MQTT Explorer or `mosquitto_pub`. |
| `incubator/settings` | Publish | Current configured thresholds and day counter. Published after MQTT connection and when settings are requested. |
| `incubator/errors` | Publish | Error messages such as sensor failure. |

### Command topic and payload format

All commands are published to:

```text
incubator/commands
```

The payload format is:

```text
device:command:value
```

Examples:

| Payload | What it does |
|---------|--------------|
| `temp:on:37.2` | Set heater ON threshold to `37.2 C`. |
| `temp:off:37.7` | Set heater OFF threshold to `37.7 C`. |
| `humid:on:55.0` | Set humidifier ON threshold to `55%`. |
| `humid:off:60.0` | Set humidifier OFF threshold to `60%`. |
| `heater:state:1` | Manually turn heater ON. |
| `heater:state:0` | Manually turn heater OFF. |
| `humidifier:state:1` | Manually turn humidifier ON. |
| `humidifier:state:0` | Manually turn humidifier OFF. |
| `roller:state:1` | Turn roller motor ON. |
| `roller:state:0` | Turn roller motor OFF. |
| `candling:state:1` | Turn candling LED ON. |
| `candling:state:0` | Turn candling LED OFF. |
| `system:day:5` | Set day counter to `5`. |
| `system:getsettings:1` | Ask the ESP32 to publish current settings to `incubator/settings`. |
| `system:mode:offline` | Switch to local/offline control. |
| `system:mode:online` | Switch back to WiFi/MQTT mode. |
| `system:reset:1` | Restart the ESP32. |

For the complete command behavior, check the `handleMQTTCommand()` function in `ArduinoScript/ArduinoScript.ino`.

### MQTT setup troubleshooting

| Problem | What to check |
|---------|---------------|
| Serial Monitor repeats `Attempting MQTT connection...failed` | Mosquitto is not running, `MQTT_SERVER` is wrong, port `1883` is blocked, or ESP32 and broker are on different networks. |
| Mosquitto only works from the broker computer | Check that `mosquitto.conf` has `listener 1883 0.0.0.0`, then restart Mosquitto. |
| ESP32 uses WiFi but MQTT does not connect | Confirm the broker computer IP with `ipconfig`; do not use `localhost` in `secrets.h`. |
| `mosquitto_sub` shows nothing | Wait for telemetry, confirm the ESP32 says MQTT connected, and subscribe to `incubator/#`. |
| MQTT Explorer cannot connect | Use host `192.168.x.x`, port `1883`, TLS off, username/password blank. |
| Connection is refused | Mosquitto is stopped, listening on another port, or the firewall is blocking inbound TCP `1883`. |
| Connection times out | Network path issue: wrong IP, different WiFi network, guest WiFi isolation, router isolation, or firewall. |
| You want username/password | Current firmware does not send credentials. Add credentials to `mqttClient.connect()` first, then configure a Mosquitto password file. |
| You want TLS/port `8883` | Current firmware is not configured for MQTT TLS. This requires secure client changes and certificate handling before changing the broker to TLS. |

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
