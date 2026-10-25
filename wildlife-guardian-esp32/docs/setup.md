# Setup Guide

## Requirements
- Seeed Studio XIAO ESP32-S3 Sense
- USB data cable capable of data transfer
- Arduino IDE with ESP32 board support installed
- A 2.4 GHz Wi-Fi network
- ntfy app or browser access to your configured ntfy topic

## Configure
1. Open `firmware/wildlife_guardian.ino`.
2. Add your known-working firmware.
3. Enter Wi-Fi credentials and ntfy topic locally in the firmware/configuration.
4. Keep private credentials out of Git commits.
5. Select the correct XIAO ESP32-S3 board and COM port in Arduino IDE.
6. Upload the sketch.
7. Open Serial Monitor at the baud rate specified by the sketch.

## Verify
- Confirm the board connects to Wi-Fi.
- Confirm the camera initializes.
- Trigger motion in front of the camera.
- Confirm a photo is captured and the ntfy alert is received.

Exact board-menu names and required libraries depend on the firmware version you use.
