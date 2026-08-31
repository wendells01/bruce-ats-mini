# Bruce ATS Mini — Pentest Firmware for ATS Mini Radio

A pentest-only port of [Bruce Firmware](https://github.com/pr3y/Bruce) for the **ATS Mini** handheld radio. This build keeps the offensive-security toolbox (WiFi, BLE, WebUI, JS) and drops everything to do with radio. The ATS Mini's stock radio firmware stays on the device for actual RF use.

## Overview

The ATS Mini is a small ESP32-S3 radio. Stock firmware handles the radio side. This project is a **separate firmware** you flash when you want Bruce's pentest features instead. It installs over the radio firmware, so you pick one or the other, not both. There is no radio functionality here by design, and no CC1101, NRF24, PN532, IR, SD, or GPS support.

## Features

- **WiFi attacks**: Beacon Spam, Deauth, Evil Portal, ARP Spoof, Responder, Scan Hosts, Wardriving, WireGuard, Brucegotchi
- **BLE**: Scan, Spam (iOS / Windows / Android), BadBLE (DuckyScript)
- **WebUI** over the browser
- **JS Interpreter** (mQuickJS)
- **LittleFS File Manager** (list files, view JPG, QR / PIX codes)
- **Config** (brightness, dim time, orientation, UI color, sleep)
- **Clock / NTP** sync
- **ESPNOW** file and command transfer

## Hardware

**Supported: ATS Mini** (ESP32-S3-WROOM-1, 16MB Flash, 8MB PSRAM, ST7789/GC9307 170x320 display, rotary encoder).

**Not supported** (not built into this port):

- CC1101, NRF24, PN532 sub-GHz / RFID modules
- IR, RGB LED, SD card, GPS
- Radio (SI4732/4735) RX or TX
- BadUSB HID (the ATS Mini is USB CDC only)
- Battery fuel gauge (ADC percent only)

## Flashing

1. Download the latest `.bin` from [GitHub Releases](https://github.com/wendells01/bruce-ats-mini/releases).
2. Put the device into download mode: hold the **boot** button and press **reset**.
3. Flash with `esptool.py`:

```sh
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 Bruce-ats-mini-vX.Y.Z.bin
```

Or flash in the browser with the [Espressif Web Flasher](https://espressif.github.io/esptool-js/).

## Development Workflow

```sh
# Clone, branch, change
git clone https://github.com/wendells01/bruce-ats-mini.git
git checkout -b my-feature
# ... make changes ...

# Push — CI builds automatically
git push origin my-feature
gh run watch

# When ready, tag a release
git tag v0.1.0 && git push origin v0.1.0
gh run watch

# Binary appears in GitHub Releases
```

The CI builds each push and PR (`gh run watch` to monitor). Tagging `v*` triggers the release workflow, which builds the firmware and publishes the `.bin` to GitHub Releases.

## Credits

Based on [Bruce Firmware](https://github.com/pr3y/Bruce) by pr3y.

## License

AGPL-3.0 (same as Bruce).
