# Wiring Guide

## Current camera-only setup
The XIAO ESP32-S3 Sense has an onboard camera connector. Connect the camera module using the board's matching connector; do not wire camera signals manually unless following the official board documentation.

For the current camera + Wi-Fi + ntfy setup, the PIR sensor is not required if motion is detected by comparing camera frames.

## If adding an external PIR sensor later
Before wiring, verify the exact PIR module model and its output voltage. Connect:
- PIR VCC → a supply voltage supported by that exact module
- PIR GND → XIAO GND
- PIR OUT → a GPIO explicitly selected in the firmware

Do not assume a GPIO number without checking the chosen board pinout and existing firmware pin assignments.

Add a verified circuit diagram to `../images/` and embed it here when available.
