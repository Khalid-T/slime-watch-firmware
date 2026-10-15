#!/usr/bin/env bash
# Load every WiFi network this laptop knows into the slime watch, over the
# USB serial cable -- Khalid's ask (2026-09-09): "share all wifi credentials
# this laptop has since it has all the main ones I use".
#
# Run it YOURSELF: it reads the saved secrets with NetworkManager as you
# (a polkit prompt or `sudo` may appear), and writes them ONLY to the
# watch's serial port as the firmware's provisioning lines
# (`wifi "<ssid>" "<psk>"` / `wifi-eap "<ssid>" "<identity>" "<password>"`,
# double-quoted because SSIDs and passwords may hold spaces); nothing
# is printed, logged or saved anywhere else. The watch keeps them in its
# own flash (NVS). Pass --dry-run to see the SSIDs and kinds only.
#
#   ./tools/wifi-export.sh [--dry-run] [--active] [/dev/ttyACM0]
# --active: only the network the laptop is on RIGHT NOW (the dock's
# hand-over: run it whenever the watch is plugged in somewhere new).
set -euo pipefail
dry=0; active=0; port=/dev/ttyACM0
for a in "$@"; do case "$a" in --dry-run) dry=1;; --active) active=1;; *) port="$a";; esac; done
if [ "$dry" = 0 ] && [ ! -w "$port" ]; then
    echo "no write access to $port (sudo chmod a+rw $port, or add yourself to uucp)"; exit 1
fi
if [ "$dry" = 0 ]; then
    stty -F "$port" 115200 raw -echo -hupcl
    printf '\r\n' > "$port"; sleep 0.6          # a dark watch loses the waking bytes: knock first
fi
q() { printf '"%s"' "${1//\"/\\\"}"; }   # a double-quoted serial argument
# Khalid's trim (2026-09-09): networks not worth the watch's slots
SKIP="Don'tTouchThis|iPhone|Iphone_EXT|Maison Bergevin|Vive-017f0013"
n=0
while IFS=: read -r name type; do
    [ "$type" = "802-11-wireless" ] || continue
    ssid=$(nmcli -g 802-11-wireless.ssid connection show "$name")
    case "|$SKIP|" in *"|$ssid|"*) echo "trim: $ssid"; continue;; esac
    mgmt=$(nmcli -g 802-11-wireless-security.key-mgmt connection show "$name" || true)
    case "$mgmt" in
        wpa-psk|sae)
            psk=$(nmcli -s -g 802-11-wireless-security.psk connection show "$name")
            [ -n "$psk" ] || { echo "skip $ssid: no psk stored"; continue; }
            line="wifi $(q "$ssid") $(q "$psk")";;
        wpa-eap)
            id=$(nmcli -g 802-1x.identity connection show "$name")
            pw=$(nmcli -s -g 802-1x.password connection show "$name")
            anon=$(nmcli -g 802-1x.anonymous-identity connection show "$name" || true)
            [ -n "$pw" ] || { echo "skip $ssid: enterprise password not stored (asked at connect time)"; continue; }
            line="wifi-eap $(q "$ssid") $(q "$id") $(q "$pw")${anon:+ $(q "$anon")}";;
        ""|none)
            line="wifi $(q "$ssid")";;         # open network
        *)  echo "skip $ssid: $mgmt not supported"; continue;;
    esac
    if [ "$dry" = 1 ]; then echo "would send: ${line%% *} $ssid (${mgmt:-open})"
    else printf '%s\r\n' "$line" > "$port"; sleep 0.4; echo "sent: $ssid (${mgmt:-open})"; fi
    n=$((n+1))
done < <(if [ "$active" = 1 ]; then nmcli -t -f NAME,TYPE connection show --active; else nmcli -t -f NAME,TYPE connection show; fi)
echo "$n network(s) $( [ "$dry" = 1 ] && echo listed || echo loaded ); the watch joins whichever it sees"
