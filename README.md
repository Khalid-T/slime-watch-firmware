# Slime watch firmware

Firmware for a hand-built ESP32-S3 smartwatch. The watch hosts a small animated
creature ("the slime") that lives on the wrist, reacts to touch and motion,
reads the wearer's pulse, and syncs its day home over WiFi.

This repository contains the watch firmware only. The companion desktop
software, the art partition image, and the creature's assets are not included.

## Hardware

- **Board:** Waveshare ESP32-S3-Touch-LCD-1.28 (ESP32-S3R2, 16 MB flash,
  2 MB QSPI PSRAM): 1.28" 240x240 round GC9A01 over SPI, CST816S capacitive
  touch, QMI8658 IMU, one shared I2C bus
- **Added:** MAX30102 pulse oximeter (MH-ET module) on a second I2C bus,
  coin ERM haptic motor on a GPIO, 603040 LiPo cell
- **Case:** two-part PLA shell, parametric FreeCAD model, printed on a Bambu Lab

## What the firmware does

- **Rendering:** the creature is rastered on a small art cell and blitted into
  a 240x240 RGB565 buffer in internal DMA RAM, pushed whole by DMA at up to
  40 fps (LovyanGFX)
- **Motion:** IMU sampled at 224 Hz drives the creature's physics, tilt, and
  the gesture set (pet, drink, feed, knock, stroke, raise-to-wake, swipes)
- **Power:** sleeps when still and dark, wakes on raise; WiFi radio stays
  down unless the link has a reason to dial
- **Network:** WiFi credentials (PSK or WPA2-Enterprise PEAP) stored in NVS,
  fast rejoin by BSSID and channel, TLS link to a home server, ArduinoOTA
  updates, mDNS as `slime-watch.local`
- **Bluetooth:** NimBLE, up only while the WiFi radio is down; iPhone
  notifications over ANCS, a BLE UART link
- **Health:** pulse and HRV from the MAX30102 with per-LED AGC, periodic rest
  reads logged through the day; SpO2 estimate
- **Day log:** LittleFS partition holding the wearer's day (steps, rest reads,
  events), synced home every 10 minutes when the link is up
- **Diagnostics:** `doctor` over serial or HTTP prints heap, fps, clock, link,
  and day-file state with a one-line verdict

## Layout

```
platformio.ini   two envs: `watch` (USB) and `ota` (over WiFi)
partitions.csv   two 3 MB app slots, a 2 MB art partition, LittleFS for the day log
src/             the firmware (C and C++, Arduino-ESP32)
tools/           host-side unit tests (tools/check.sh), build-id stamping, PPG capture helpers
Makefile         `make check` runs every host test; `make build`, `make flash`, `make ota`
```

## Build and flash

PlatformIO with Python 3.11 (3.14 is refused by PlatformIO as of this writing).

```sh
pio run -e watch                 # build
pio run -e watch -t upload       # flash over USB (/dev/ttyACM0)
WATCH_OTA_PW=<pw> pio run -e ota -t upload   # flash over WiFi
make check                       # host tests
```

The art partition image (`wardrobe.bin`) is flashed separately and is not part
of this repository; the watch runs without it, with the creature's default
rendering.

## Development note

I designed and direct this project, own the hardware build, bring-up, wiring,
and debugging, and reviewed and tested every change on the device. Much of
the firmware code itself was written with AI-agent assistance under my
direction.
