#!/usr/bin/env bash
# ART OVER THE AIR: a wardrobe.bin into the slime watch's `art` partition
# through its HTTP door, no cable (README "THE WARDROBE" > "ART OVER THE
# AIR"; the protocol is src/artota.h's). curl in a loop over 4 KB chunks,
# the crcs and the signatures computed by python3 (zlib's crc32 -- the
# ROM computes the same; the knock's HMAC over "<stamp> art ...").
#
#   tools/art-push.sh [--dry] [--key <watch key>] [--chunk 4096] [<host>] [wardrobe.bin]
#
# <host> = the watch's address (the log's `net: JOINED ..., ip 10.0.0.42`,
# or slime-watch.local; default $WATCH_HOST or slime-watch.local); the
# image defaults to firmware/watch/wardrobe.bin (tools/wristbake.py --out).
# The key: --key, else $WATCH_KEY, else the desk's config (python3 -c
# "from config import Config" at the repo root: config.json's WATCH_KEY);
# none = the requests go unsigned (a bare board takes them, a provisioned
# one answers 403 `no key`). --dry prints the plan (the chunks, each crc,
# the image's crc, the three request shapes) and sends nothing.
#
# THE SEQUENCE ON THE WRIST (the door is up only while the radio is):
#   1. over serial `link hold` (or SETTINGS 3 > WIFI HOLD): the radio up
#      and held for the lit spell; wait for `net: JOINED ..., ip ...`
#   2. keep the glass lit: every /art request keeps it awake itself, but
#      start with a lit glass (a raise, a touch)
#   3. this script; ~40-60 s for a 670 KB image on the LAN (the erase of
#      each 64 KB sector is the slow part, ~10 chunks a second between)
#   4. `wardrobe` over serial: mounted, the count, the new hash; then
#      `link auto` to put the radio back on the budget.
# A `no wait` (409) is the veil still taking a costume off: the script
# retries it; a `no gap want=N` is answered by sending from N; any other
# `no` stops the script (the watch's log says why; a `no crc` means the
# old image is gone and `wardrobe` says "no image" until a whole push
# lands -- run the script again).
set -euo pipefail
dry=0; key=${WATCH_KEY:-}; chunk=4096; host=""; img=""
while [ $# -gt 0 ]; do
    case "$1" in
        --dry) dry=1 ;;
        --key) shift; key=${1:-} ;;
        --chunk) shift; chunk=${1:-4096} ;;
        -h|--help) sed -n '2,36p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) echo "art-push: unknown option $1 (see --help)" >&2; exit 2 ;;
        *) if [ -z "$host" ]; then host=$1; else img=$1; fi ;;
    esac
    shift
done
WATCH=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=$(cd "$WATCH/../.." && pwd)
host=${host:-${WATCH_HOST:-slime-watch.local}}
img=${img:-$WATCH/wardrobe.bin}
[ -f "$img" ] || { echo "art-push: no image at $img (tools/wristbake.py --out wardrobe.bin)" >&2; exit 1; }
[ "$chunk" -ge 1 ] && [ "$chunk" -le 4096 ] || { echo "art-push: --chunk must be 1..4096 (the watch's buffer)" >&2; exit 2; }
if [ -z "$key" ]; then
    key=$(cd "$REPO" && python3 -c 'from config import Config; print(Config.WATCH_KEY or "")' 2>/dev/null || true)
fi

# THE PLAN: one line a chunk -- off len crc -- then the image's crc and total
work=$(mktemp -d "${TMPDIR:-/tmp}/art-push.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$img" "$chunk" "$work" "$dry" <<'PY' > "$work/plan"
import os, sys, zlib
img, chunk, work, dry = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4] == "1"
data = open(img, "rb").read()
assert data[:4] == b"SLMA", "not an SLMA image (the head's magic)"
total = len(data)
n = 0
for off in range(0, total, chunk):
    part = data[off:off + chunk]
    print(f"chunk {off} {len(part)} {zlib.crc32(part) & 0xffffffff:08x}")
    if not dry:
        with open(os.path.join(work, f"c{n}"), "wb") as f:
            f.write(part)
    n += 1
print(f"image {total} {zlib.crc32(data) & 0xffffffff:08x} {n}")
PY
read -r _ total imgcrc nchunks < <(grep '^image ' "$work/plan")

# the signature: the knock's primitive over "<stamp> art <what>" (link.cpp's link_sign_check)
sign() {  # what -> "&stamp=N&mac=HEX" or ""
    [ -n "$key" ] || { printf ''; return; }
    python3 -c '
import hashlib, hmac, sys, time
key, what = sys.argv[1], sys.argv[2]
stamp = int(time.time() // 60)
mac = hmac.new(key.encode(), f"{stamp} {what}".encode(), hashlib.sha256).hexdigest()[:32]
print(f"&stamp={stamp}&mac={mac}")' "$key" "$1"
}

echo "art-push: $img -> http://$host/art: $total B in $nchunks chunks of $chunk, image crc $imgcrc, $( [ -n "$key" ] && echo signed || echo UNSIGNED )"
if [ "$dry" = 1 ]; then
    echo "  POST /art?begin=$total$(sign "art begin $total")"
    head -3 "$work/plan" | while read -r _ off len crc; do echo "  POST /art?off=$off&len=$len&crc=$crc$(sign "art chunk $off $crc")   (the body: $len B)"; done
    [ "$nchunks" -gt 4 ] && echo "  ... $((nchunks - 4)) more"
    tail -2 "$work/plan" | head -1 | while read -r _ off len crc; do echo "  POST /art?off=$off&len=$len&crc=$crc$(sign "art chunk $off $crc")   (the body: $len B)"; done
    echo "  POST /art?end=$imgcrc$(sign "art end $imgcrc")"
    echo "dry: nothing sent"
    exit 0
fi

# one request; the reply's first line in $reply, the status in $code
ask() {  # query [bodyfile]
    local q=$1 body=${2:-} out
    if [ -n "$body" ]; then
        out=$(curl -sS -m 15 -H 'Expect:' -H 'Content-Type: application/octet-stream' --data-binary "@$body" -w '\n%{http_code}' "http://$host/art?$q") || return 1
    else
        out=$(curl -sS -m 15 -X POST -H 'Expect:' -w '\n%{http_code}' "http://$host/art?$q") || return 1
    fi
    code=$(printf '%s' "$out" | tail -1)
    reply=$(printf '%s' "$out" | head -1)
    return 0
}

ask "begin=$total$(sign "art begin $total")" || { echo "art-push: no answer from $host (is the radio held? \`link hold\`)" >&2; exit 1; }
echo "  begin -> $reply"
case "$reply" in ok*) ;; *) exit 1 ;; esac

t0=$(date +%s); i=0; want=0
while read -r _ off len crc; do
    [ "$off" -lt "$want" ] && { i=$((i + 1)); continue; }   # already taken (a `no gap want=N` skipped us ahead)
    tries=0
    while :; do
        ask "off=$off&len=$len&crc=$crc$(sign "art chunk $off $crc")" "$work/c$i" || { echo "art-push: the door went away at $off (the hold ended? the glass dark?)" >&2; exit 1; }
        case "$reply" in
            ok*) want=$((off + len)); break ;;
            "no wait"*) tries=$((tries + 1)); [ $tries -le 20 ] || { echo "art-push: $reply (the veil never lifted)" >&2; exit 1; }; sleep 0.5 ;;
            "no gap want="*) want=${reply#no gap want=}; want=${want%% *}; echo "  $off -> $reply: sending from $want"; break ;;
            *) echo "  $off -> $reply" >&2; echo "art-push: stopped ($reply); the watch's log says why" >&2; exit 1 ;;
        esac
    done
    i=$((i + 1))
    if [ $((i % 16)) -eq 0 ] || [ "$i" -eq "$nchunks" ]; then printf '  %d/%d chunks (%d B, %d s)\n' "$i" "$nchunks" "$want" "$(( $(date +%s) - t0 ))"; fi
done < <(grep '^chunk ' "$work/plan")

ask "end=$imgcrc$(sign "art end $imgcrc")" || { echo "art-push: no answer to the end" >&2; exit 1; }
echo "  end -> $reply ($(( $(date +%s) - t0 )) s)"
case "$reply" in ok*) echo "art-push: landed; \`wardrobe\` over serial shows the new hash; \`link auto\` puts the radio back" ;; *) exit 1 ;; esac
