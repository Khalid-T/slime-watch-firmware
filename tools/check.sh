#!/usr/bin/env bash
# THE ONE COMMAND for the slime watch's proofs on the laptop (no board,
# no port -- nothing here opens /dev/ttyACM0):
#
#   tools/check.sh                 every host test + the preview's md5
#   tools/check.sh --pio           ... and `pio run` for the two envs (flash / RAM lines)
#   tools/check.sh --desk          ... and the repo's test_watch.py (the desk's parity tests)
#   tools/check.sh --pio --desk    everything (= `make -C firmware/watch check-all`)
#   tools/check.sh --only NAME,..  a few of the host tests (names as in the table)
#   tools/check.sh --strip         also write the preview's contact sheet (strip.png)
#
# Every host test is built into ${TMPDIR:-/tmp}/slime-watch-check with
# the SAME compile line its header carries (tools/*_test.cpp,
# tools/preview.c, ../../tools/gel_dump.c) -- only the -o path differs.
# When a source grows a new dependency, fix the header AND the line
# here; the README's "Proven on the laptop" lines copy the headers.
#
# One line per test: the name, what the test itself printed as its tally
# ("82 checks, 0 failed", "ALL GOOD", ...), PASS or FAIL (the exit code:
# every test exits non-zero on a failed check or a tripped assert). The
# preview's 21 frames are md5-summed (md5sum *.ppm | awk '{print $1}' |
# md5sum -- the strip's md5s' md5, the pixel-identity anchor of THE
# OPTIMIZATION PASS) and compared with the anchor below: a gel change
# that moves a pixel shows here as a FAIL until the anchor (and the
# README's copy of it) is updated on purpose. A summary table ends the
# run; the exit code is 1 if anything failed.
set -euo pipefail

# The anchor: the preview's md5 as recorded in the README ("One command:
# tools/check.sh"). Update both when the gel is meant to change.
PREVIEW_MD5_ANCHOR=6d5bb8c61e8c10c291e6b219e774ce7d   # 2026-09-10 THE LUMP IS A BELLY: 21 frames (19-21 the meal, GEL_BELLY_AMP 0.25 / POW 1.5; the 18 before untouched, their own md5 still 830c64b661ff7f4cd41442a13df64f7b); was 830c64b661ff7f4cd41442a13df64f7b for THE DISC IS THE GLASS (gel_fit_inside): the fall's drop hung no lower than the rim allows (08, 13), the peel's creep and the slide kept inside (07, 09, 12), PEEL_SQUASH 0.50 / PEEL_CREEP_DEG 4 (the puddle no wider than the glass); was 2b51b8389d170c698d39cda863d6b3a7 (THE FOUR WALLS, the drop clipped by the disc), 15c7a6237ac6743d636ab966c7cc71a1 (the continuous bowl)

WATCH=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=$(cd "$WATCH/../.." && pwd)
OUT=${TMPDIR:-/tmp}/slime-watch-check
PIO_VENV=${PIO_VENV:-$HOME/.local/share/pio-venv}

do_pio=0 do_desk=0 do_strip=0 only=""
while [ $# -gt 0 ]; do
    case "$1" in
        --pio)  do_pio=1 ;;
        --desk) do_desk=1 ;;
        --strip) do_strip=1 ;;
        --only) shift; only=${1:-} ;;
        --only=*) only=${1#--only=} ;;
        -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "check.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done

mkdir -p "$OUT"
cd "$WATCH"

# the table: one row per step, "name|result|detail"
rows=()
failed=0
row() {  # name result detail
    rows+=("$1|$2|$3")
    printf '%-14s %-36s %s\n' "$1" "$3" "$2"
    [ "$2" = PASS ] || failed=$((failed + 1))
}

wanted() {  # is this test asked for (--only)?
    [ -z "$only" ] && return 0
    case ",$only," in *",$1,"*) return 0 ;; esac
    return 1
}

# what the test printed as its own tally, in one short phrase
tally() {  # logfile
    local log=$1 line
    if line=$(grep -E '^[0-9]+ checks, [0-9]+ failed' "$log" | tail -1) && [ -n "$line" ]; then
        printf '%s' "$line"
    elif line=$(grep -E '^ALL GOOD' "$log" | tail -1) && [ -n "$line" ]; then
        printf '%s' "$line"
    elif grep -qE '^[a-z_]+: ok' "$log"; then
        printf 'ok (asserts, none tripped)'
    elif grep -qE 'written, [0-9]+ B in [0-9]+ chunks' "$log"; then
        printf '%s' "$(grep -oE '[0-9]+ B in [0-9]+ chunks' "$log" | tail -1) (asserts)"
    else
        printf '%s' "$(tail -1 "$log" | cut -c1-36)"
    fi
}

# host NAME -- COMPILE... -- RUN...   (the compile line verbatim from
# the source's header, its -o pointed at $OUT; run from $OUT so a test
# that drops files beside itself drops them there)
host() {
    local name=$1; shift
    wanted "$name" || return 0
    local compile=() run=()
    [ "$1" = -- ] && shift
    while [ "$1" != -- ]; do compile+=("$1"); shift; done
    shift
    run=("$@")
    local log=$OUT/$name.log
    if ! "${compile[@]}" >"$log" 2>&1; then
        row "$name" FAIL "did not build: see $log"
        return 0
    fi
    local rc=0
    (cd "$OUT" && "${run[@]}" >"$log" 2>&1) || rc=$?
    if [ $rc -eq 0 ]; then
        row "$name" PASS "$(tally "$log")"
    else
        row "$name" FAIL "$(tally "$log") (exit $rc) -- see $log"
    fi
}

echo "== host tests -> $OUT"
host gesture   -- c++ -std=gnu++17 -O1 -o "$OUT/gesture_test" tools/gesture_test.cpp src/gesture.cpp -lm -- "$OUT/gesture_test"
host play      -- c++ -std=gnu++17 -I src -o "$OUT/play_test" tools/play_test.cpp src/play.cpp -lm -- "$OUT/play_test"
host cramp     -- c++ -std=gnu++17 -I src -o "$OUT/cramp_test" tools/cramp_test.cpp -x c src/cramp.c -lm -- "$OUT/cramp_test"
host rest      -- c++ -O1 -I src -o "$OUT/rest_test" tools/rest_test.cpp src/rest.cpp src/grain.cpp src/tell.cpp src/vigor.cpp -lm -- "$OUT/rest_test"
host wake      -- c++ -std=gnu++17 -I src -o "$OUT/wake_test" tools/wake_test.cpp src/raise.cpp -lm -- "$OUT/wake_test"
host tell      -- c++ -std=gnu++17 -I src -o "$OUT/tell_test" tools/tell_test.cpp src/tell.cpp src/grain.cpp -lm -- "$OUT/tell_test"
host idle      -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/idle_test" tools/idle_test.cpp src/idle.cpp src/grain.cpp src/vigor.cpp -lm -- "$OUT/idle_test"
host wardrobe  -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/wardrobe_test" tools/wardrobe_test.cpp src/bored.cpp src/grain.cpp src/vigor.cpp -lm -- "$OUT/wardrobe_test"
host hours     -- c++ -std=gnu++17 -I src -o "$OUT/hours_test" tools/hours_test.cpp src/hours.cpp src/theme.cpp -x c src/gel.c -lm -- "$OUT/hours_test"
host house     -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/house_test" tools/house_test.cpp src/house.cpp -- "$OUT/house_test"
host radio     -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/radio_test" tools/radio_test.cpp src/radio.cpp -- "$OUT/radio_test"
host cable     -- c++ -std=gnu++17 -I src -o "$OUT/cable_test" tools/cable_test.cpp -- "$OUT/cable_test"
host spent     -- c++ -std=gnu++17 -I src -o "$OUT/spent_test" tools/spent_test.cpp -- "$OUT/spent_test"
host unstick   -- c++ -std=gnu++17 -I src -o "$OUT/unstick_test" tools/unstick_test.cpp -- "$OUT/unstick_test"
host blegate   -- c++ -std=gnu++17 -I src -o "$OUT/blegate_test" tools/blegate_test.cpp src/blegate.cpp -- "$OUT/blegate_test"
host blepair   -- c++ -std=gnu++17 -I src -o "$OUT/blepair_test" tools/blepair_test.cpp -x c src/blepair.c -- "$OUT/blepair_test"
host bleline   -- c++ -std=gnu++17 -I src -o "$OUT/bleline_test" tools/bleline_test.cpp -x c src/bleline.c -- "$OUT/bleline_test"
host cts       -- c++ -std=gnu++17 -I src -o "$OUT/cts_test" tools/cts_test.cpp -- "$OUT/cts_test"
host face      -- c++ -std=gnu++17 -I src -o "$OUT/face_test" tools/face_test.cpp src/face.cpp src/theme.cpp -x c src/gel.c -lm -- "$OUT/face_test"
host page      -- c++ -std=gnu++17 -I src -o "$OUT/page_test" tools/page_test.cpp src/page.cpp src/theme.cpp -x c src/gel.c -lm -- "$OUT/page_test"
host place     -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/place_test" tools/place_test.cpp src/place.cpp src/word.cpp src/glance.cpp src/theme.cpp -x c src/gel.c -lm -- "$OUT/place_test"
# THE BUBBLE READS: the place test's four frames (the real gel on each
# wall, the hour, the word "the desk waves from across the room") as one
# PNG to look at before a flash -- the word's legibility is a thing for
# an eye, not a check
if wanted place && [ -f "$OUT/place_bottom.ppm" ] \
   && python3 tools/ppm2png.py --cols 4 "$OUT/word-preview.png" "$OUT"/place_bottom.ppm "$OUT"/place_right.ppm "$OUT"/place_top.ppm "$OUT"/place_left.ppm 2>>"$OUT/place.log"; then
    echo "             the bubble: $OUT/word-preview.png"
fi
host sky       -- c++ -std=gnu++17 -I src -o "$OUT/sky_test" tools/sky_test.cpp src/sky.cpp -x c src/gel.c -lm -- "$OUT/sky_test"
host clockwork -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/clockwork_test" tools/clockwork_test.cpp src/clockwork.cpp src/face.cpp src/theme.cpp -x c src/gel.c src/gentle.c -lm -- "$OUT/clockwork_test"
host gentle    -- c++ -std=gnu++17 -I src -o "$OUT/gentle_test" tools/gentle_test.cpp -x c src/gentle.c -- "$OUT/gentle_test"
host drowse    -- c++ -std=gnu++17 -I src -o "$OUT/drowse_test" tools/drowse_test.cpp -x c src/drowse.c -lm -- "$OUT/drowse_test"
host fall0     -- c++ -std=gnu++17 -I src -o "$OUT/fall0_test" tools/fall0_test.cpp -x c src/fall0.c -lm -- "$OUT/fall0_test"
host grain     -- c++ -std=gnu++17 -I src -o "$OUT/grain_test" tools/grain_test.cpp src/grain.cpp -- "$OUT/grain_test"
host vigor     -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/vigor_test" tools/vigor_test.cpp src/vigor.cpp -lm -- "$OUT/vigor_test"
host wisp      -- c++ -std=gnu++17 -I src -o "$OUT/wisp_test" tools/wisp_test.cpp src/wisp.cpp src/theme.cpp -x c src/gel.c src/notice.c -lm -- "$OUT/wisp_test"
host wispeye   -- c++ -std=gnu++17 -I src -o "$OUT/wispeye_test" tools/wispeye_test.cpp -x c src/wispeye.c -lm -- "$OUT/wispeye_test"
host dirty     -- c++ -std=gnu++17 -I src -o "$OUT/dirty_test" tools/dirty_test.cpp -x c src/dirty.c -- "$OUT/dirty_test"
host daykeep   -- c++ -std=gnu++17 -I src -o "$OUT/daykeep_test" tools/daykeep_test.cpp src/daykeep.cpp -- "$OUT/daykeep_test"
host nightsum  -- c++ -std=gnu++17 -I src -o "$OUT/nightsum_test" tools/nightsum_test.cpp -x c src/nightsum.c -- "$OUT/nightsum_test"
host bmp       -- c++ -std=gnu++17 -I src -o "$OUT/bmp_test" tools/bmp_test.cpp -- "$OUT/bmp_test" "$OUT/screen_test.bmp"
host bumps     -- c++ -std=gnu++17 -I src -o "$OUT/bumps_test" tools/bumps_test.cpp -- "$OUT/bumps_test"
host pulse     -- c++ -std=gnu++17 -I src -o "$OUT/pulse_test" tools/pulse_test.cpp -- "$OUT/pulse_test"
host notice    -- c++ -std=gnu++17 -I src -o "$OUT/notice_test" tools/notice_test.cpp -x c src/notice.c -- "$OUT/notice_test"
host wdt       -- c++ -std=gnu++17 -I src -o "$OUT/wdt_test" tools/wdt_test.cpp -- "$OUT/wdt_test"
host death     -- c++ -std=gnu++17 -I src -o "$OUT/death_test" tools/death_test.cpp -- "$OUT/death_test"
host question  -- c++ -std=gnu++17 -I src -o "$OUT/question_test" tools/question_test.cpp -x c src/question.c -- "$OUT/question_test"
host tone      -- c++ -std=gnu++17 -I src -o "$OUT/tone_test" tools/tone_test.cpp -x c src/tone.c -lm -- "$OUT/tone_test"
host petplace  -- c++ -std=gnu++17 -I src -o "$OUT/petplace_test" tools/petplace_test.cpp -x c src/petplace.c -lm -- "$OUT/petplace_test"
host lookback  -- c++ -std=gnu++17 -I src -o "$OUT/lookback_test" tools/lookback_test.cpp -x c src/lookback.c -- "$OUT/lookback_test"
host stare     -- c++ -std=gnu++17 -I src -o "$OUT/stare_test" tools/stare_test.cpp -x c src/stare.c -lm -- "$OUT/stare_test"
host listen    -- c++ -std=gnu++17 -I src -o "$OUT/listen_test" tools/listen_test.cpp -x c src/listen.c -lm -- "$OUT/listen_test"
host hourlook  -- c++ -std=gnu++17 -I src -o "$OUT/hourlook_test" tools/hourlook_test.cpp -x c src/hourlook.c -lm -- "$OUT/hourlook_test"
host shakeoff  -- c++ -std=gnu++17 -I src -o "$OUT/shakeoff_test" tools/shakeoff_test.cpp -x c src/shakeoff.c -lm -- "$OUT/shakeoff_test"
host knock     -- c++ -std=gnu++17 -I src -o "$OUT/knock_test" tools/knock_test.cpp -x c src/knock.c -lm -- "$OUT/knock_test"
host phonegone -- c++ -std=gnu++17 -I src -o "$OUT/phonegone_test" tools/phonegone_test.cpp -x c src/phonegone.c -lm -- "$OUT/phonegone_test"
host lull      -- c++ -std=gnu++17 -I src -o "$OUT/lull_test" tools/lull_test.cpp -x c src/lull.c -lm -- "$OUT/lull_test"
host near      -- c++ -std=gnu++17 -I src -o "$OUT/near_test" tools/near_test.cpp -x c src/near.c -- "$OUT/near_test"
host drum      -- c++ -std=gnu++17 -I src -o "$OUT/drum_test" tools/drum_test.cpp -x c src/drum.c -lm -- "$OUT/drum_test"
host warm      -- c++ -std=gnu++17 -I src -o "$OUT/warm_test" tools/warm_test.cpp -x c src/warm.c -lm -- "$OUT/warm_test"
host drain     -- c++ -std=gnu++17 -I src -o "$OUT/drain_test" tools/drain_test.cpp -x c src/drain.c -- "$OUT/drain_test"
host dockhand  -- c++ -std=gnu++17 -I src -o "$OUT/dockhand_test" tools/dockhand_test.cpp -x c src/dockhand.c -- "$OUT/dockhand_test"
host agc       -- c++ -std=gnu++17 -I src -o "$OUT/agc_test" tools/agc_test.cpp -- "$OUT/agc_test"
host spo2      -- c++ -std=gnu++17 -I src -o "$OUT/spo2_test" tools/spo2_test.cpp -lm -- "$OUT/spo2_test"
host perfusion -- c++ -std=gnu++17 -I src -o "$OUT/perfusion_test" tools/perfusion_test.cpp -lm -- "$OUT/perfusion_test"
host resp      -- c++ -std=gnu++17 -I src -o "$OUT/resp_test" tools/resp_test.cpp -lm -- "$OUT/resp_test"
host beat      -- c++ -std=gnu++17 -I src -o "$OUT/beat_test" tools/beat_test.cpp -lm -- "$OUT/beat_test"
host still     -- c++ -std=gnu++17 -I src -o "$OUT/still_test" tools/still_test.cpp -x c src/still.c -- "$OUT/still_test"
host artota    -- c++ -std=gnu++17 -I src -o "$OUT/artota_test" tools/artota_test.cpp src/artota.cpp -- "$OUT/artota_test"
host minute_keep -- c++ -std=gnu++17 -I src -o "$OUT/minute_keep_test" tools/minute_keep_test.cpp -- "$OUT/minute_keep_test"
host fsguard   -- c++ -std=gnu++17 -I src -o "$OUT/fsguard_test" tools/fsguard_test.cpp -- "$OUT/fsguard_test"
host minuteflags -- c++ -std=gnu++17 -I src -o "$OUT/minuteflags_test" tools/minuteflags_test.cpp -- "$OUT/minuteflags_test"
host alarmask  -- c++ -std=gnu++17 -I src -o "$OUT/alarmask_test" tools/alarmask_test.cpp -- "$OUT/alarmask_test"
host holdask   -- c++ -std=gnu++17 -I src -o "$OUT/holdask_test" tools/holdask_test.cpp -- "$OUT/holdask_test"
host refusal   -- c++ -std=gnu++17 -I src -o "$OUT/refusal_test" tools/refusal_test.cpp -- "$OUT/refusal_test"
host proof     -- c++ -std=gnu++17 -I src -o "$OUT/proof_test" tools/proof_test.cpp -- "$OUT/proof_test"
host ancsact   -- c++ -std=gnu++17 -I src -o "$OUT/ancsact_test" tools/ancsact_test.cpp -- "$OUT/ancsact_test"
host httpgate  -- c++ -std=gnu++17 -I tools/stub -I src -o "$OUT/httpgate_test" tools/httpgate_test.cpp src/radio.cpp -- "$OUT/httpgate_test"

# gel_dump: the desk's parity harness (test_watch.py drives it under
# --desk); here only that it builds and one cell is the cell's size
if wanted gel_dump; then
    log=$OUT/gel_dump.log
    if cc -O2 -std=gnu99 -o "$OUT/gel_dump" "$REPO/tools/gel_dump.c" src/gel.c -lm >"$log" 2>&1; then
        want=$((64 * 64 * 2))
        got=$("$OUT/gel_dump" 37 -0.3 0.2 -0.2 0 1.0 0.5 2 0 0.5 2>>"$log" | wc -c)
        if [ "$got" -eq "$want" ]; then row gel_dump PASS "one cell = $got B (64x64 RGB565)"
        else row gel_dump FAIL "one cell = $got B, wanted $want"; fi
    else
        row gel_dump FAIL "did not build: see $log"
    fi
fi

# the preview: 21 frames, their md5 against the anchor
if wanted preview; then
    log=$OUT/preview.log
    pv=$OUT/preview-frames
    rm -rf "$pv"; mkdir -p "$pv"
    if cc -O2 -std=gnu99 -o "$OUT/preview" tools/preview.c src/gel.c -lm >"$log" 2>&1 \
       && "$OUT/preview" "$pv" >>"$log" 2>&1; then
        n=$(ls "$pv"/*.ppm 2>/dev/null | wc -l)
        md5=$(cd "$pv" && md5sum *.ppm | awk '{print $1}' | md5sum | awk '{print $1}')
        if [ "$n" -ne 21 ]; then
            row preview FAIL "$n frames, wanted 21"
        elif [ "$md5" = "$PREVIEW_MD5_ANCHOR" ]; then
            row preview PASS "21 frames, md5 $md5"
        else
            row preview FAIL "21 frames, md5 $md5 != anchor $PREVIEW_MD5_ANCHOR"
        fi
        if [ $do_strip -eq 1 ] && python3 tools/ppm2png.py --cols 7 "$pv/strip.png" "$pv"/*.ppm >>"$log" 2>&1; then
            echo "             strip: $pv/strip.png"
        fi
        # THE LUMP IS A BELLY: the meal's four side by side (empty, half, full, full on the right wall) -- a thing for an eye
        if [ $do_strip -eq 1 ] && python3 tools/ppm2png.py --cols 4 "$pv/meal-preview.png" "$pv"/01-rest-bottom.ppm "$pv"/19-meal-bottom-half.ppm "$pv"/20-meal-bottom-full.ppm "$pv"/21-meal-right-full.ppm >>"$log" 2>&1; then
            echo "             the meal: $pv/meal-preview.png"
        fi
    else
        row preview FAIL "did not build or run: see $log"
    fi
fi

# --pio: the two envs (the watch-ancs env is gone: BLE is in the default build, 09-10 21:50), each one's flash / RAM line (no upload: the port stays shut)
if [ $do_pio -eq 1 ]; then
    echo "== pio run (PATH=$PIO_VENV/bin:\$PATH)"
    for env in watch ota; do
        log=$OUT/pio-$env.log
        if PATH="$PIO_VENV/bin:$PATH" pio run -e "$env" >"$log" 2>&1; then
            ram=$(grep -oE 'RAM: +\[[^]]*\] +[0-9.]+% \(used [0-9]+ bytes' "$log" | tail -1 | sed -E 's/RAM: +\[[^]]*\] +/RAM /; s/ \(used / /; s/ bytes//')
            flash=$(grep -oE 'Flash: +\[[^]]*\] +[0-9.]+% \(used [0-9]+ bytes' "$log" | tail -1 | sed -E 's/Flash: +\[[^]]*\] +/flash /; s/ \(used / /; s/ bytes//')
            row "pio:$env" PASS "$flash B, $ram B"
        else
            row "pio:$env" FAIL "$(grep -E 'error:|Error' "$log" | head -1 | cut -c1-60) -- see $log"
        fi
    done
fi

# --desk: the repo's test_watch.py (the watchface parity tests, gel_dump among them)
if [ $do_desk -eq 1 ]; then
    echo "== python -m pytest -q test_watch.py (in $REPO)"
    log=$OUT/desk.log
    if (cd "$REPO" && python -m pytest -q test_watch.py >"$log" 2>&1); then
        row desk PASS "$(grep -oE '[0-9]+ passed[^=]*' "$log" | tail -1 | sed 's/ *$//')"
    else
        row desk FAIL "$(grep -oE '[0-9]+ failed[^=]*|[0-9]+ error[^=]*' "$log" | tail -1 | sed 's/ *$//') -- see $log"
    fi
fi

echo
echo "== summary"
printf '%-14s %-6s %s\n' step result detail
printf '%-14s %-6s %s\n' ---- ------ ------
for r in "${rows[@]}"; do
    IFS='|' read -r n s d <<<"$r"
    printf '%-14s %-6s %s\n' "$n" "$s" "$d"
done
echo
if [ $failed -eq 0 ]; then
    echo "all ${#rows[@]} steps PASS"
else
    echo "$failed of ${#rows[@]} steps FAILED (logs in $OUT)"
    exit 1
fi
