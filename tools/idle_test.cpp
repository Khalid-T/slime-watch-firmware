/* IDLE LIFE on the laptop (idle.cpp has no Arduino in it; the stub is
 * there for the pattern):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/idle_test tools/idle_test.cpp src/idle.cpp src/grain.cpp src/vigor.cpp -lm && /tmp/idle_test
 * Nothing on the empty glass, nothing asleep, nothing in the dark;
 * the first thing after IDLE_MIN_S at rest; the cadence 60-120 s
 * between things; never two of a kind in a row; nothing within 10 s of
 * a pet (and a pet ends a thing in progress); a blink only with the
 * eyes up; the look's sweep reaches both ends with the eyes eased up,
 * not snapped; the bubble's laps; motion resets the rest clock. THE OLD
 * HABIT (2026-09-23): the grain's favourite plays bigger once a lit
 * spell -- the settle deeper (seed 123456789's favourite), the glance
 * up longer (seed 2's), never with no grain. */
#include "../src/idle.h"
#include "../src/gel.h"
#include "../src/grain.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now_ms = 1000;
static idle_in_t in;
static idle_fx_t fx;
/* what a run saw */
static int starts = 0, repeats = 0;
static uint32_t start_ms[400];
static int start_kind[400];
static int last_seen = IDLE_NONE;
static int min_gx = 0, max_gx = 0, min_gy = 0;
static float max_eye_step = 0, prev_eye = 0, max_eye = 0;
static int blinks = 0;
static float max_squash = 0, max_a1 = 0, max_gel = 0;

static void reset_seen(void)
{
    starts = repeats = 0; last_seen = idle_last(); min_gx = max_gx = min_gy = 0;
    max_eye_step = 0; prev_eye = fx.eye_alpha; max_eye = 0; blinks = 0; max_squash = max_a1 = max_gel = 0;
}
static void run(float seconds)
{
    for (float t = 0; t < seconds; t += 0.025f) {
        in.dt = 0.025f; in.now_ms = now_ms;
        int before = idle_count();
        idle_tick(&in);
        idle_fx(&fx);
        if (idle_count() != before) {
            int k = idle_last();
            if (k == last_seen) repeats++;
            last_seen = k;
            if (starts < 400) { start_ms[starts] = now_ms; start_kind[starts] = k; }
            starts++;
            if (k == IDLE_BLINK) blinks++;
        }
        if (fx.gaze_x < min_gx) min_gx = fx.gaze_x;
        if (fx.gaze_x > max_gx) max_gx = fx.gaze_x;
        if (fx.gaze_y < min_gy) min_gy = fx.gaze_y;
        if (fabsf(fx.eye_alpha - prev_eye) > max_eye_step) max_eye_step = fabsf(fx.eye_alpha - prev_eye);
        prev_eye = fx.eye_alpha;
        if (fx.eye_alpha > max_eye) max_eye = fx.eye_alpha;
        if (fx.squash > max_squash) max_squash = fx.squash;
        if (fabsf(fx.a1) > max_a1) max_a1 = fabsf(fx.a1);
        if (fx.gel_extra > max_gel) max_gel = fx.gel_extra;
        now_ms += 25;
    }
}
static void home(void)   /* the ordinary evening: lit, held, awake, nothing on, still, eyes down */
{
    memset(&in, 0, sizeof in);
    in.lit = true; in.holding = true; in.asleep = false; in.busy = false; in.moving = false; in.eye_alpha = 0;
}

int main(void)
{
    char st[240];
    idle_begin(0x5EED1234u);

    puts("-- nothing where nothing should be");
    home(); in.holding = false; reset_seen(); run(600);
    CHECK(starts == 0, "the empty glass, 10 min: nothing (%d)", starts);
    home(); in.asleep = true; reset_seen(); run(600);
    CHECK(starts == 0, "asleep, 10 min: nothing (%d)", starts);
    home(); in.lit = false; reset_seen(); run(600);
    CHECK(starts == 0, "the dark house, 10 min: nothing (%d)", starts);
    home(); in.moving = true; reset_seen(); run(600);
    CHECK(starts == 0, "the wrist never still, 10 min: nothing (%d)", starts);
    home(); in.busy = true; reset_seen(); run(600);
    CHECK(starts == 0, "something else on the body, 10 min: nothing (%d)", starts);

    puts("-- the first thing, the cadence, no repeats");
    idle_begin(0x5EED1234u);
    home(); reset_seen();
    uint32_t t0 = now_ms;
    run(24.5f);
    CHECK(starts == 0, "24.5 s at rest: not yet");
    run(130);
    CHECK(starts == 1, "one thing within 25..150 s of rest (%s at +%.1f s)", idle_name(start_kind[0]), (start_ms[0] - t0) / 1000.0f);
    CHECK(start_ms[0] - t0 >= 25000, "not before 25 s at rest (+%.1f s)", (start_ms[0] - t0) / 1000.0f);
    run(1800);
    CHECK(starts >= 14 && starts <= 32, "30 more minutes: %d things (a cadence of 60-120 s)", starts);
    {
        uint32_t lo = 999999, hi = 0;
        for (int i = 1; i < starts && i < 400; i++) {
            uint32_t g = start_ms[i] - start_ms[i - 1];
            if (g < lo) lo = g;
            if (g > hi) hi = g;
        }
        CHECK(lo >= 60000 && hi <= 124000, "the gaps between starts %.1f..%.1f s (60 s + a thing's own length at most)", lo / 1000.0f, hi / 1000.0f);
    }
    CHECK(repeats == 0, "never two of a kind in a row");
    CHECK(blinks == 0, "eyes down all along: no blink (%d)", blinks);
    {
        int seen[IDLE_KINDS] = { 0 };
        for (int i = 0; i < starts && i < 400; i++) seen[start_kind[i]]++;
        printf("       looks %d, blinks %d, settles %d, bubbles %d, glances up %d\n", seen[IDLE_LOOK], seen[IDLE_BLINK], seen[IDLE_SETTLE], seen[IDLE_BUBBLE], seen[IDLE_GLANCE_UP]);
        CHECK(seen[IDLE_LOOK] > 0 && seen[IDLE_SETTLE] > 0 && seen[IDLE_BUBBLE] > 0 && seen[IDLE_GLANCE_UP] > 0, "every kind but the blink came round");
    }
    CHECK(min_gx == -2 && max_gx == 2, "the look reached both ends (%d..%d)", min_gx, max_gx);
    CHECK(min_gy == -1, "the glance up lifted the gaze (%d)", min_gy);
    CHECK(max_eye > 0.99f, "the eyes came up for it (%.2f)", max_eye);
    CHECK(max_eye_step < 0.1f, "eased, never snapped: the largest step a frame %.3f", max_eye_step);
    CHECK(fabsf(max_squash - 0.06f) < 0.005f, "the settle's squash peaked at %.3f", max_squash);
    CHECK(fabsf(max_a1 - 0.04f) < 0.005f, "the settle's lean %.3f", max_a1);
    {
        /* one bubble on its own: x3 for 3 s at GEL_GEL_HZ 0.12 = 0.72 extra laps, and never a rewind */
        int c = idle_count();
        for (int guard = 0; guard < 400000; guard++) {
            run(0.025f);
            if (idle_count() != c && idle_last() == IDLE_BUBBLE) break;
        }
        float g0 = fx.gel_extra; bool rewound = false; float prev = g0;
        for (int i = 0; i < 130; i++) { run(0.025f); float d = fx.gel_extra - prev; if (d < 0 && d > -0.9f) rewound = true; prev = fx.gel_extra; }
        float laps = fx.gel_extra - g0; if (laps < 0) laps += 1.0f;
        CHECK(fabsf(laps - 0.72f) < 0.03f && !rewound, "a bubble: %.2f extra laps over 3 s (x3), never a rewind", laps);
        CHECK(!idle_playing(), "and over");
    }
    idle_status(st, sizeof st); printf("       %s\n", st);

    puts("-- a pet: nothing for 10 s, and a thing in progress ends");
    {
        /* wait for a thing to start, then pet during it */
        int c = idle_count();
        while (idle_count() == c) run(0.025f);
        CHECK(idle_playing(), "a thing in progress (%s)", idle_name(idle_last()));
        in.busy = true; idle_pet(); run(0.3f); in.busy = false;   /* the tap: a touch down, then the pet */
        CHECK(!idle_playing(), "the pet ended it");
        uint32_t pet_at = now_ms;
        reset_seen(); run(9.5f);
        CHECK(starts == 0, "9.5 s after the pet: nothing");
        /* the rest clock was reset by the touch: 25 s more, and the gap */
        run(200);
        CHECK(starts >= 1 && start_ms[0] - pet_at >= 25000, "then a thing, %.1f s after the pet (>= 10 s quiet, >= 25 s at rest)", (start_ms[0] - pet_at) / 1000.0f);
        /* an HTTP pet: no touch, still the quiet */
        c = idle_count();
        while (idle_count() == c) run(0.025f);
        idle_pet(); run(0.025f);
        CHECK(!idle_playing(), "an HTTP pet (no touch on the glass) ends the thing too");
        reset_seen(); run(9.5f);
        CHECK(starts == 0, "and the 10 s quiet holds");
    }

    puts("-- the blink needs the eyes up");
    idle_begin(0x0BADF00Du);
    home(); in.eye_alpha = 1.0f; reset_seen(); run(1800);
    CHECK(blinks > 0, "eyes up (a finger on the glass) for 30 min: %d blinks of %d things", blinks, starts);
    CHECK(repeats == 0, "still never two of a kind");
    {
        /* a blink is a closed eye for ~120 ms and nothing else */
        int c = idle_count();
        for (int guard = 0; guard < 400000; guard++) {
            run(0.025f);
            if (idle_count() != c && idle_last() == IDLE_BLINK) break;
        }
        CHECK(idle_last() == IDLE_BLINK && fx.eye_kind == GEL_EYE_CLOSED, "a blink: eye_kind closed");
        run(0.15f);
        CHECK(!idle_playing() && fx.eye_kind == -1, "and over within 150 ms, the kind handed back");
    }

    puts("-- motion resets the rest clock");
    home(); reset_seen(); run(200);
    in.moving = true; run(2); in.moving = false;
    uint32_t moved = now_ms;
    reset_seen(); run(24);
    CHECK(starts == 0, "24 s after a move: nothing");
    run(150);
    CHECK(starts >= 1 && start_ms[0] - moved >= 25000, "then a thing %.1f s after the move", (start_ms[0] - moved) / 1000.0f);

    puts("-- the empty glass ends a thing");
    {
        int c = idle_count();
        while (idle_count() == c) run(0.025f);
        in.holding = false; run(0.025f);
        CHECK(!idle_playing() && fx.gaze_x == 0 && fx.gaze_y == 0 && fx.eye_kind == -1, "the departure: the thing ended, the gaze home");
        run(2);
        CHECK(fx.eye_alpha == 0, "the eyes down");
    }

    puts("-- THE OLD HABIT: the favourite, once a lit spell, a touch bigger");
    {
        /* the nominal knobs (no grain_begin, no line: favourite 0 -- the
         * state every check above ran under): nothing owed, and the 30
         * minutes above saw every settle at its plain 0.06 (max_squash) */
        CHECK(grain_knobs()->favourite == 0 && !idle_habit_owed(), "the nominal grain: no favourite, no habit owed");
        idle_status(st, sizeof st);
        CHECK(strstr(st, "the habit none (no grain)") != NULL, "`idle` says so: %s", strstr(st, "| the habit"));

        /* seed 123456789: the favourite is the settle (grain_test), settle knob 0.900 */
        grain_update(123456789, 0.0f, 0, "-");
        CHECK(grain_knobs()->favourite == IDLE_SETTLE, "seed 123456789's favourite is the settle");
        idle_begin(0x5EED1234u); home(); reset_seen();
        CHECK(idle_habit_owed(), "the boot's spell owes the habit");
        float deep = 0, after1 = 0, after2 = 0; bool big_seen = false;
        int n_settle = 0;
        for (int i = 0; i < 60 && n_settle < 3; i++) {
            int c = idle_count();
            while (idle_count() == c) run(0.025f);
            if (idle_last() != IDLE_SETTLE) { CHECK(!idle_habit_playing(), "%s is not the habit's", idle_name(idle_last())); continue; }
            n_settle++;
            if (n_settle == 1) big_seen = idle_habit_playing();
            float mx = 0;
            while (idle_playing()) { run(0.025f); if (fx.squash > mx) mx = fx.squash; }
            if (n_settle == 1) deep = mx; else if (n_settle == 2) after1 = mx; else after2 = mx;
        }
        CHECK(big_seen, "the spell's first settle is the habit's");
        CHECK(fabsf(deep - 0.06f * 0.9f * 1.5f) < 0.006f, "... deeper: 0.06 x 0.900 x 1.5 = 0.081 (%.3f)", deep);
        CHECK(!idle_habit_owed(), "... and the habit is spent for the spell");
        CHECK(fabsf(after1 - 0.06f * 0.9f) < 0.006f && fabsf(after2 - 0.06f * 0.9f) < 0.006f, "the next two settles plain: 0.054 (%.3f, %.3f)", after1, after2);

        /* the glass goes dark and lights again: a new spell owes it again */
        in.lit = false; run(5); in.lit = true; run(0.025f);
        CHECK(idle_habit_owed(), "a new lit spell: the habit owed again");
        home(); reset_seen();
        float again = 0; int seen = 0;
        for (int i = 0; i < 60 && !seen; i++) {
            int c = idle_count();
            while (idle_count() == c) run(0.025f);
            if (idle_last() != IDLE_SETTLE) continue;
            seen = 1;
            while (idle_playing()) { run(0.025f); if (fx.squash > again) again = fx.squash; }
        }
        CHECK(fabsf(again - 0.081f) < 0.006f, "... its first settle deep again (%.3f)", again);
        CHECK(!idle_habit_owed(), "... spent again");

        /* seed 2: the favourite is the glance up -- longer (1.5 -> 2.25 s), the gaze lifted the while */
        grain_update(2, 0.0f, 0, "-");
        CHECK(grain_knobs()->favourite == IDLE_GLANCE_UP, "seed 2's favourite is the glance up");
        idle_begin(0x5EED1234u); home(); reset_seen();
        float held_big = 0, held_plain = 0; int glances = 0;
        for (int i = 0; i < 120 && glances < 2; i++) {
            int c = idle_count();
            while (idle_count() == c) run(0.025f);
            if (idle_last() != IDLE_GLANCE_UP) continue;
            glances++;
            float held = 0;
            while (idle_playing()) { run(0.025f); if (fx.gaze_y == -1) held += 0.025f; }
            if (glances == 1) held_big = held; else held_plain = held;
        }
        CHECK(fabsf(held_big - 2.25f) < 0.1f, "the spell's first glance up held 2.25 s (%.2f)", held_big);
        CHECK(fabsf(held_plain - 1.5f) < 0.1f, "the next one 1.5 s (%.2f)", held_plain);
        idle_status(st, sizeof st);
        CHECK(strstr(st, "the habit spent this spell") != NULL, "`idle` says the habit is spent: %s", st);
        grain_begin();
    }

    puts("-- THE SMALL HOUSE (2026-10-09): the roam is on the table only while the cramp is on");
    {
        grain_begin();
        idle_begin(0x5EED1234u); home(); reset_seen(); in.cramp = 0; run(3600);
        int roams = 0; for (int i = 0; i < starts && i < 400; i++) if (start_kind[i] == IDLE_ROAM) roams++;
        CHECK(starts > 10 && roams == 0, "no cramp, an hour: %d things, no roam", starts);
        idle_begin(0x5EED1234u); home(); reset_seen(); in.cramp = 1.0f; run(3600);
        roams = 0; for (int i = 0; i < starts && i < 400; i++) if (start_kind[i] == IDLE_ROAM) roams++;
        CHECK(roams >= 3 && roams < starts, "a full cramp, an hour: %d of %d things are the roam (weight 30 beside the look's 30)", roams, starts);
        CHECK(!strcmp(idle_name(IDLE_ROAM), "the roam round the rim"), "its name");
        /* the roam's shape: the gaze walks the rim, up at the top */
        idle_begin(0x5EED1234u); home(); reset_seen(); in.cramp = 1.0f;
        int lo = 9, hi = -9, ups = 0; bool seen = false;
        for (int i = 0; i < 200 && !seen; i++) {
            int c = idle_count();
            while (idle_count() == c) run(0.025f);
            if (idle_last() != IDLE_ROAM) continue;
            seen = true;
            while (idle_playing()) { run(0.025f); if (fx.gaze_x < lo) lo = fx.gaze_x; if (fx.gaze_x > hi) hi = fx.gaze_x; if (fx.gaze_y == -1) ups++; }
        }
        CHECK(seen && lo == -2 && hi == 2 && ups > 20 && ups < 80, "the roam sweeps -2..2 and looks up through half the turn (%d..%d, up %d frames)", lo, hi, ups);
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
