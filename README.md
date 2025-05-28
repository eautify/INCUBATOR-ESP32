Arduino Incubator Controller

This project is an Arduino-based incubator controller with:

- DHT22 sensor for temperature and humidity measurements.
- Door switch with debounce logic and door-open alarm.
- Candling relay control (currently disabled, planned for future serial control).
- Watchdog timer enabled for system reliability.

---------------------------------------------------------------------

Pin Assignments

Function          : Arduino Pin
------------------------------
DHT22 Data        : 2
Candling Relay    : 4
Door Switch       : 5
Door Alarm        : 6

---------------------------------------------------------------------

Features

- Reads temperature and humidity every 2 seconds.
- Detects door open/close state with debounce to prevent false alarms.
- Activates alarm when door is opened.
- Outputs sensor data and door state over serial in CSV format:

  temperature,humidity,door_state

  Example:

  36.5,85.0,OPEN

- Watchdog timer enabled with 8-second timeout to reset if code hangs.

---------------------------------------------------------------------

Serial Communication

- Serial commands for controlling candling relay are currently disabled.
- Sensor data is continuously sent every 2 seconds over serial.
- Future updates will enable serial control of the candling relay.

---------------------------------------------------------------------

Usage

1. Connect DHT22 sensor, door switch, candling relay, and alarm to the pins above.
2. Upload the Arduino sketch.
3. Open Serial Monitor at 9600 baud to see sensor readings and door status.
4. Door alarm triggers automatically when the door opens.

---------------------------------------------------------------------

Future Improvements

- Add serial command processing for candling relay control.
- Integrate with Python or other applications for remote monitoring and control.

---------------------------------------------------------------------

Dependencies

- DHT sensor library: https://github.com/adafruit/DHT-sensor-library
- Arduino AVR Watchdog Timer (built into Arduino AVR core)

---------------------------------------------------------------------

License

MIT License — free and open source.
