# 🐣 Arduino Incubator Controller

This is an Arduino-based incubator controller that:

- Monitors **temperature** and **humidity** using a **DHT22 sensor**
- Displays readings on a **16x2 I2C LCD**
- Controls a **heater**, **humidifier** amd **LED Candling** using **relays**
- Detects **door status** via a **reed switch**
- Sends all data to the **Serial Monitor** for integration with Python or logging tools

---

## 📦 Features

- 🌡️ Real-time temperature and humidity monitoring  
- 🔁 Automatic control of heating and humidifying elements  
- 🚪 Door open/close detection  
- 🖥️ Display of current status on 16x2 I2C LCD  
- 📡 Serial output of all readings and actuator states  

---

## 🔧 Hardware Requirements

| Component          | Description                                |
|--------------------|--------------------------------------------|
| Arduino Uno/Nano   | Or any compatible board                    |
| DHT22 Sensor       | Temperature and humidity sensor            |
| 16x2 LCD (I2C)     | With I2C backpack (default address 0x27)   |
| Relay Module (2x)  | To control heater and humidifier           |
| Reed Switch        | For detecting door open/closed state       |
| Heater Element     | Incubator heating                          |
| Humidifier         | Ultrasonic or resistive humidifier         |
| Jumper Wires       | Male-to-male or male-to-female             |
| Power Supply       | As per heating/humidifier requirements     |

---

## 🔌 Pin Connections

| Component          | Arduino Pin | Notes                         |
|--------------------|-------------|-------------------------------|
| DHT22              | D2          | Data pin only                 |
| Heater Relay       | D3          | IN1                           |
| Humidifier Relay   | D4          | IN3                           |
| Reed Switch        | D5          | Pull-up resistor enabled      |
| LED Candling Relay | D6          | IN2						   |
| I2C LCD (SDA/SCL)  | A4/A5       | For Uno (check your board)    |

---

## 🧠 Code Behavior

- **Temperature < 37.5°C** → Heater ON  
- **Humidity < 55% RH** → Humidifier ON  
- **Reed switch HIGH** → Door CLOSED  
- **Reed switch LOW** → Door OPEN  

Every 2 seconds, data is printed to the Serial Monitor like:

Temp:37.4,Humidity:54.2,Door:CLOSED,Heater:ON,Humidifier:ON

---

## 💻 Serial Data Format

The serial data is structured as:

Temp:<float>,Humidity:<float>,Door:<OPEN/CLOSED>,Heater:<ON/OFF>,Humidifier:<ON/OFF>

This format is ideal for Python parsing, logging, or plotting via serial.

---

## 📚 Libraries Required

Install the following via **Arduino Library Manager**:

- DHT sensor library: https://github.com/adafruit/DHT-sensor-library
- LiquidCrystal_I2C: https://github.com/johnrickman/LiquidCrystal_I2C