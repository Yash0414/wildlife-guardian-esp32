# Troubleshooting

## Camera does not initialize
- Check the camera module is seated correctly.
- Confirm the firmware targets XIAO ESP32-S3 Sense.
- Check Serial Monitor for camera initialization errors.

## Wi-Fi or ntfy connection fails
- Confirm the board is connected to a working 2.4 GHz Wi-Fi network.
- Check DNS and outbound HTTPS access.
- Verify the ntfy topic is spelled correctly.
- Do not post private topic names or Wi-Fi credentials in public issue reports.

## Too many motion alerts
- Increase the motion threshold or cooldown in the working firmware.
- Test under stable lighting and avoid moving the camera during calibration.

## Photos or SD-card history are missing
SD-card history is not included by this scaffold. Add and test that feature before documenting it as implemented.
