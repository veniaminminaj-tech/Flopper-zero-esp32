# Flopper-zero-esp32

Safe ESP32-WROOM-32 OLED + joystick project.

## Features
- OLED menu (SSD1306 128x64, I2C address 0x3C)
- Joystick navigation
- Passive, read-only nearby Wi-Fi network scan (SSID and signal strength)
- Snake mini-game
- About/safety screen

## Removed from this safe edition
This edition does not transmit crafted Wi-Fi frames, disconnect devices, create lookalike access points, collect passwords, capture credentials, or guess Wi-Fi passwords.

## Hardware
- ESP32-WROOM-32 / ESP32 Dev Module
- SSD1306 128x64 OLED on the default ESP32 I2C pins
- Joystick X: GPIO34, Y: GPIO35, switch: GPIO32

## Build
Every push to `main` starts **Build ESP32-WROOM firmware**. You can also start it manually:
1. Open the **Actions** tab.
2. Select **Build ESP32-WROOM firmware**.
3. Tap **Run workflow**.
4. Download the `flopper-zero-esp32-wroom-bin` artifact after the run succeeds.

The workflow compiles for `esp32:esp32:esp32` and uploads the resulting firmware files from `bin/`.
