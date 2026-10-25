# Wildlife Guardian — ESP32-S3 Sense

Camera-based motion monitoring and photo alerts using the Seeed Studio XIAO ESP32-S3 Sense.

## Current features
- Camera-based motion detection
- JPEG photo capture
- ntfy notification integration
- Configurable motion sensitivity and alert cooldown

## Hardware
- Seeed Studio XIAO ESP32-S3 Sense
- USB data cable
- Wi-Fi network
- Phone with the ntfy app or access to the relevant ntfy topic

## Repository layout
- `firmware/` — Arduino firmware
- `docs/` — setup, wiring and troubleshooting guides


## Getting started
1. Open `firmware/wildlife_guardian.ino` in Arduino IDE.
2. Copy your known-working firmware into that file.
3. Review `docs/setup.md` and `docs/wiring.md`.
4. Set Wi-Fi credentials and your private ntfy topic locally. Do not commit real passwords, API keys or private topic names.
5. Upload the firmware to the XIAO ESP32-S3 Sense and open Serial Monitor to verify startup.

## Status
**Implemented:** camera-based motion detection, JPEG capture and ntfy alerts (as configured in the working firmware).

**Planned / not yet included in this repository:** SD-card image history, web-based image gallery and AI-based animal classification. Update this section only after those features are implemented and tested.

## Documentation
- [Setup guide](docs/setup.md)
- [Wiring guide](docs/wiring.md)
- [Troubleshooting](docs/troubleshooting.md)

## Contributions
Issues and improvements are welcome. 
