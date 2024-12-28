# BLE Machine Tracker

This project uses an ESP32 to scan for BLE devices and log machine operation times to Google Sheets using Google Apps Script.

## Features
- Detects BLE devices with specific UUIDs.
- Logs worker operation times to Google Sheets.
- Displays the start time, end time, and duration of operation.

## Requirements
- ESP32 board
- Relay module
- WiFi connection
- Google Apps Script for logging data

## Usage
1. Replace `WIFI_SSID`, `WIFI_PASSWORD`, and `GOOGLE_SCRIPT_URL` in `main.cpp` with your own credentials.
2. Upload the code to your ESP32 using Arduino IDE.
3. Monitor the serial output to verify the connection and operation.

## License
This project is licensed under the MIT License.
