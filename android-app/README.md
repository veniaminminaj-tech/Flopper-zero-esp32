# Flopper Zero Control for Android

Android USB-OTG serial monitor for ESP32-WROOM boards.

## Features
- USB host / OTG connection
- Automatic detection of supported USB-UART adapters
- 115200 baud, 8 data bits, no parity, 1 stop bit
- Live serial log, clear log, connection status
- No command transmission: this app is a read-only monitor

## Build APK
The GitHub Actions workflow **Build Flopper Zero Android APK** builds a debug APK on pushes affecting this folder or its workflow.

1. Open the repository's **Actions** tab.
2. Select **Build Flopper Zero Android APK**.
3. Open the latest successful run.
4. Download the **FlopperZero-Android-APK** artifact.
5. Extract it and install `app-debug.apk` on Android.

## Connect
1. Use a USB OTG adapter and a data-capable USB cable.
2. Connect the ESP32 board to the phone/tablet.
3. Open the app and tap **ПІДКЛЮЧИТИ**.
4. Approve the Android USB permission prompt.

The app reads UART output only. It does not send attack, disruption, credential-capture, or other control commands.
