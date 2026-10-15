#!/usr/bin/env bash
# THE PPG, from the bench (2026-09-12): the MAX30102 breakout on the
# second bus (VIN 3V3, GND, SDA GPIO 18, SCL GPIO 21 (crossed at the 2026-10-03 solder; was SDA 21 / SCL 18); INT/RD/IRD unwired).
# Every call opens the port, which resets the board (~2 s of boot first).
#
#   tools/ppg.sh scan            what answers on both I2C buses (0x57 = the sensor: the wires are right)
#   tools/ppg.sh lines           SDA/SCL read as inputs, bare and pulled -- floating / held low / pulled high (the meter, without the meter)
#   tools/ppg.sh try             bus 1 scanned slow (100 kHz) and with SDA/SCL SWAPPED: crossed wires or a marginal joint, named
#   tools/ppg.sh status          the driver's state / the last spot
#   tools/ppg.sh spot [secs]     a spot read (10 s; 5..30) -- finger or wrist ON the sensor, hold still; prints the result
#   tools/ppg.sh pulse           THE PULSE ROW's read (30 s), as a tap on SETTINGS 5's PULSE row starts it (`set pulse`)
#   tools/ppg.sh led ir|green|red   which LED the spot reads by (ir for the 30102; stored)
#   tools/ppg.sh rail 21|16|off  a header GPIO driven HIGH as the module's VIN (the adapter's 3V3/VSYS pins are dead, 2026-09-13; stored)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="${PIO_VENV:-$HOME/.local/share/pio-venv}/bin/python"
say() { "$PY" "$HERE/say.py" -t "${LISTEN:-2}" -g 1 -- "$@" 2>&1 | grep -E "^bus|^ppg|ppg:" || true; }
case "${1:-status}" in
  scan)   say "bus" ;;
  lines)  say "bus lines" ;;
  try)    say "bus try" ;;
  status) say "ppg" ;;
  spot)   LISTEN=$(( ${2:-10} + 8 )) say "ppg spot ${2:-}" ;;
  pulse)  "$PY" "$HERE/say.py" -t 42 -g 1 -- "set pulse" 2>&1 | grep -E "^settings|ppg:|pulse:" || true ;;
  led)    say "ppg led ${2:-}" ;;
  rail)   say "ppg rail ${2:-}" ;;
  *)      sed -n 2,13p "$0"; exit 1 ;;
esac
