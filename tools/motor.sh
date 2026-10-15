#!/usr/bin/env bash
# THE BARE MOTOR, from the bench (2026-09-12): drive the watch's motor on
# GPIO 33 over USB without asking anyone. Every call opens the port,
# which resets the board (about 2 s of boot first), then sends.
#
#   tools/motor.sh hold [s]              the pin plain HIGH for s seconds (default 10, cap 60): the meter's test
#   tools/motor.sh buzz <ms> [pct] [hz]  one pulse: ms long, at pct % (default 100), at hz PWM (default 0 = plain DC)
#   tools/motor.sh bump|hop|purr [n]     a creature gesture, n times 0.9 s apart (default 5)
#   tools/motor.sh ladder                60 / 120 / 250 / 500 / 1000 ms plain DC pulses, 2 s apart
#   tools/motor.sh status                what the motor driver says
#   tools/motor.sh off                   stop it
#
# Run from anywhere. Needs the board on /dev/ttyACM0 and nobody else on
# the port (tools/monitor.py holds it; stop that first).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="${PIO_VENV:-$HOME/.local/share/pio-venv}/bin/python"
SAY="$HERE/say.py"
say() { "$PY" "$SAY" -t "${LISTEN:-2}" -g "${GAP:-0.9}" -- "$@" 2>&1 | grep -E "^haptic|^motor" || true; }
pauses() { local n=$1; local out=(); for ((i=0;i<n;i++)); do out+=(""); done; printf '%s\n' "${out[@]}"; }

cmd="${1:-status}"; shift || true
case "$cmd" in
  hold)   say "haptic hold ${1:-10}" ;;
  buzz)   [ -n "$1" ] || { echo "buzz <ms> [pct] [hz]"; exit 1; }
          say "haptic buzz $1 ${2:-100} ${3:-0}" ;;
  bump|hop|purr)
          n="${1:-5}"; lines=(); for ((i=0;i<n;i++)); do lines+=("haptic $cmd"); done
          say "${lines[@]}" ;;
  ladder) GAP=1.0 say "haptic buzz 60 100 0" "" "haptic buzz 120 100 0" "" "haptic buzz 250 100 0" "" "haptic buzz 500 100 0" "" "haptic buzz 1000 100 0" ;;
  status) say "haptic" ;;
  off)    say "haptic off" ;;
  *)      sed -n 2,16p "$0"; exit 1 ;;
esac
