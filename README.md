# Bruce ATS Mini — Pentest Firmware for ATS Mini Radio

This is a fork of [pr3t3nd3r/Bruce](https://github.com/pr3y/Bruce) that
ports the Bruce pentest toolbox to one board only: the ATS Mini handheld
radio. It keeps the offensive-security modules (WiFi, BLE, WebUI, JS
interpreter, LittleFS file manager, ESPNOW, clock) and drops everything
the radio hardware cannot do. There is no radio functionality here by
design. A Launcher-based firmware switcher for the same radio lives in
[wendells01/Launcher](https://github.com/wendells01/Launcher).

## Hardware

ATS Mini: ESP32-S3-WROOM-1, 16 MB flash, 8 MB PSRAM, GC9307/ST7789
170x320 display on an 8-bit parallel bus, rotary encoder with push
button, no SD slot. Not supported in this port: CC1101, NRF24, PN532,
IR, RGB LED, SD card, GPS, SI4732 radio RX/TX, BadUSB HID (USB CDC
only), battery fuel gauge (ADC percent only).

## Display init

The panel runs on the ST7789 driver with INIT_SEQUENCE_3. Units shipped
with three panel variants, told apart at boot by the RDDID reply
(command 0x04), following esp32-si4732/ats-mini issue 41:

| RDDID | Panel | Fix applied at boot |
|---|---|---|
| 0x048181B3 | original | none, stock init |
| 0x04858552 | high gamma | gamma curve 3 plus content-adaptive brightness 0xB1 |
| 0x00009307 | inverted and mirrored | MADCTL 0xE8 |

## Flash mode

Flash mode must be DIO at 40 MHz. QIO at 80 MHz hangs the loader on
this board. PSRAM uses octal mode. These are pinned in the board
PlatformIO config, not negotiable.

## Partition table note

Releases ship a merged factory image (bootloader, partition table, app)
that flashes at `0x0`. The embedded table lays out NVS, otadata, two
4 MB OTA app slots, and a spiffs data partition. Do not flash the app
binary alone unless you know the partition offsets; the merged image is
the supported artifact.

## Flashing

1. Download the `.bin` from
   [Releases](https://github.com/wendells01/bruce-ats-mini/releases).
2. Hold boot, press reset to enter download mode.
3. `esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 Bruce-ats-mini-vX.Y.Z-merged.bin`

## Dev workflow

Pushes and pull requests build through GitHub Actions. Tagging `v*`
triggers the release workflow, which builds the firmware and publishes
the merged `.bin` to Releases:

```
git checkout -b my-feature
git push origin my-feature
gh run watch
git tag vX.Y.Z && git push origin vX.Y.Z
```

## Credits

Upstream: [pr3t3nd3r/Bruce](https://github.com/pr3y/Bruce).
User-contributed fixes in this port: RDDID panel detection for the
three GC9307 variants; encoder pins with internal pull-ups; DIO/40MHz
flash stability fix; octal PSRAM mode.
