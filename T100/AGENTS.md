# T100 Project Guide

## Project overview

T100 is an ESP8266/ESP32 tracked-robot controller. The firmware drives two 9 V DC geared motors, exposes a browser controller over Wi-Fi, and serves the controller UI from the LittleFS filesystem.

## Repository layout

- `T100.ino` — firmware entry point and web server/control logic.
- `config.h` — board pins, timing, PWM, Wi-Fi, and control settings.
- `platformio.ini` — PlatformIO environments for the supported boards.
- `data/` — files uploaded to LittleFS, including the browser controller.
- `docs/` — original hardware images and supporting references.
- `README.md` — English hardware and power documentation.

## Development workflow

- Use PlatformIO from the repository root.
- Build the ESP8266 target with `pio run -e d1_mini`.
- Build the ESP32 target with `pio run -e esp32dev`.
- Upload firmware with `pio run -e d1_mini -t upload` or the matching environment.
- Upload the web assets separately with `pio run -e d1_mini -t uploadfs`.
- Run `git diff --check` before committing.

Do not commit build output, credentials, or other generated files. Wi-Fi credentials belong in the ignored secrets configuration, not in tracked source files.

## Safety requirements

- Treat motor-control changes as safety-critical.
- Preserve the command watchdog, explicit power gating, controller ownership, and stop-on-disconnect behavior unless there is a documented replacement.
- Test with the tracks lifted and motor power disconnected where possible before testing under load.
- Never rely on the software `POWER_PIN` as the only emergency stop; use a correctly fused physical motor-power cutoff.
- Keep the motor supply and regulated controller supply on separate branches with a common ground, as described in `README.md`.

## Change and verification guidance

- Keep ESP8266 and ESP32 behavior aligned when changing shared firmware logic.
- Validate malformed commands, out-of-range speeds, disconnects, watchdog expiry, power-off, and controller handover when touching control code.
- When changing `data/` assets, verify both the firmware build and the filesystem upload path.
- Prefer small, focused commits with a clear message. Do not rewrite existing history unless explicitly requested.