# wildlife-guardian-esp32
ESP32-S3 Sense camera-based motion monitoring and ntfy photo alerts
# Wildlife Guardian — ESP32-S3 Sense

An ESP32-S3 Sense-based wildlife and motion monitoring project.

## Features
- Camera-based motion detection
- JPEG photo capture
- ntfy notification integration
- Configurable motion sensitivity and alert cooldown
- Planned: SD-card image history and AI-based animal detection

## Hardware
- Seeed Studio XIAO ESP32-S3 Sense
- USB data cable
- Wi-Fi connection
- Optional microSD storage, depending on the selected hardware configuration

## Getting Started
1. Install Arduino IDE.
2. Install the ESP32 board support package.
3. Select the XIAO ESP32S3 board.
4. Configure your local Wi-Fi credentials in `secrets.h`.
5. Configure your private ntfy topic.
6. Upload the firmware and open Serial Monitor at 115200 baud.

## Project Status
Motion detection and ntfy connectivity are being developed and tested. Features marked as planned are not yet implemented.

## Safety and Privacy
Do not commit Wi-Fi passwords, API keys, or private notification topics. Follow applicable privacy rules when capturing images of people.

## Contributions
Suggestions, bug reports, and improvements are welcome.

## Author
Yash0414

