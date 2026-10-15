# The slime watch, from the repo root: `make -C firmware/watch check-all`.
#
#   check      every host test + the preview's md5 (tools/check.sh)
#   check-all  ... + `pio run` for the two envs + the repo's test_watch.py
#   preview    the preview's 18 frames and a contact sheet (strip.png)
#   build      pio run -e watch (no port touched)
#   flash      pio run -e watch -t upload -- NEEDS THE PORT: /dev/ttyACM0
#              rw (`sudo chmod a+rw /dev/ttyACM0` per plug-in), the board
#              on USB and nobody else holding it (tools/monitor.py does);
#              opening the port resets the board. Never from a build-only
#              agent. NEVER erase_flash (NVS holds his WiFi, doors, key).
#   ota        pio run -e ota -t upload over WiFi: WATCH_OTA_PW=<the link
#              key's first 8> is required; OTA_PORT=<ip> when mDNS is not
#              on your LAN (slime-watch.local is the default)
#   (the `ancs` target is gone with the watch-ancs env, 09-10 21:50: BLE
#   and the iPhone's notices are in the default build, up only while the
#   WiFi radio is down -- README "BLUETOOTH")
#
# PlatformIO lives in ~/.local/share/pio-venv (python 3.11; platformio
# refuses 3.14): PIO_VENV=... to point elsewhere.

PIO_VENV ?= $(HOME)/.local/share/pio-venv
PIO       = PATH=$(PIO_VENV)/bin:$$PATH pio
CHECK     = tools/check.sh

.PHONY: check check-all preview build flash ota

check:
	$(CHECK)

check-all:
	$(CHECK) --pio --desk

preview:
	$(CHECK) --only preview --strip

build:
	$(PIO) run -e watch

flash:
	@test -w /dev/ttyACM0 || { echo "flash: /dev/ttyACM0 is not writable (sudo chmod a+rw /dev/ttyACM0), or the board is not on USB"; exit 1; }
	$(PIO) run -e watch -t upload

ota:
	@test -n "$(WATCH_OTA_PW)" || { echo "ota: WATCH_OTA_PW=<the link key's first 8 characters> is required"; exit 1; }
	$(PIO) run -e ota -t upload $(if $(OTA_PORT),--upload-port $(OTA_PORT),)
