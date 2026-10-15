/* THE PULSE's arithmetic on the laptop (pulse.h is header-only, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/pulse_test tools/pulse_test.cpp && /tmp/pulse_test
 * The honest band (35..190 bpm, the edges in); freshness (under 30 min,
 * a negative age never, an invalid pulse never); THE PULSE AS THE
 * BREATH's target (60 bpm = the resting rate handed in, in proportion
 * above and below -- 90 = the walk's 0.80 from a 0.55 rest, 45 = the
 * sit's 0.41; a quicker-grained rest keeps its proportion; the clamp
 * at the sleeper's 0.30 and the walk's 0.80; an invalid pulse or no
 * rest hands the rest back); THE HEARTBEAT HANDED BACK's plan (beats
 * whose onset lies inside the two seconds -- 2 at 60 bpm, 3 at 62, 4
 * at 120, 7 at 180 -- the period rounded to the ms, the cap at 8, the
 * edges of the band, no plan for an invalid pulse or no time). */
#include "pulse.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool near(float a, float b) { return fabsf(a - b) < 0.005f; }

int main(void)
{
    puts("-- the honest band: 35..190, the edges in");
    CHECK(pulse_valid(35.0f) && pulse_valid(190.0f), "35 and 190: in");
    CHECK(!pulse_valid(34.9f) && !pulse_valid(190.1f), "34.9 and 190.1: out");
    CHECK(!pulse_valid(0) && !pulse_valid(-60.0f), "0 and a negative: out (no pulse yet)");

    puts("-- fresh: in the band and under 30 min old");
    CHECK(pulse_fresh(62.0f, 0), "62 bpm, now: fresh");
    CHECK(pulse_fresh(62.0f, 29 * 60 + 59), "29:59 old: fresh");
    CHECK(!pulse_fresh(62.0f, 30 * 60), "30:00 old: stale");
    CHECK(!pulse_fresh(62.0f, 4 * 3600), "four hours: stale");
    CHECK(!pulse_fresh(62.0f, -1), "a negative age (a clock's wrap): never fresh");
    CHECK(!pulse_fresh(20.0f, 0) && !pulse_fresh(250.0f, 0), "out of the band, however young: never fresh");

    puts("-- THE PULSE AS THE BREATH: the target from a pulse, in the gel's own proportion");
    CHECK(near(pulse_breath_hz(60.0f, 0.55f), 0.55f), "60 bpm on a 0.55 rest: %.3f Hz -- the resting rate itself", pulse_breath_hz(60.0f, 0.55f));
    CHECK(near(pulse_breath_hz(90.0f, 0.55f), 0.80f) || fabsf(pulse_breath_hz(90.0f, 0.55f) - 0.80f) < 0.03f, "90 bpm: %.3f Hz -- the walk's", pulse_breath_hz(90.0f, 0.55f));
    CHECK(fabsf(pulse_breath_hz(45.0f, 0.55f) - 0.4125f) < 0.005f, "45 bpm: %.3f Hz -- the hour-still's 0.40, near enough", pulse_breath_hz(45.0f, 0.55f));
    CHECK(fabsf(pulse_breath_hz(72.0f, 0.55f) - 0.66f) < 0.005f, "72 bpm: %.3f Hz -- THE BUMPS' line (1.2 x): a pet past it is kept", pulse_breath_hz(72.0f, 0.55f));
    CHECK(near(pulse_breath_hz(60.0f, 0.594f), 0.594f), "a quicker-grained rest (0.594 = 1.08 x): 60 bpm keeps it, %.3f", pulse_breath_hz(60.0f, 0.594f));
    CHECK(fabsf(pulse_breath_hz(75.0f, 0.594f) - 0.7425f) < 0.005f, "... and 75 bpm is 1.25 x it, %.3f", pulse_breath_hz(75.0f, 0.594f));
    puts("-- the clamp: the sleeper's 0.30 under, the walk's 0.80 over");
    CHECK(near(pulse_breath_hz(140.0f, 0.55f), 0.80f), "140 bpm: %.2f -- the walk's, no quicker (a gel does not pant)", pulse_breath_hz(140.0f, 0.55f));
    CHECK(near(pulse_breath_hz(190.0f, 0.55f), 0.80f), "190 bpm: %.2f -- the same", pulse_breath_hz(190.0f, 0.55f));
    CHECK(near(pulse_breath_hz(35.0f, 0.55f), 0.3208f), "35 bpm on 0.55: %.3f -- above the floor on its own", pulse_breath_hz(35.0f, 0.55f));
    CHECK(near(pulse_breath_hz(35.0f, 0.385f), 0.30f), "35 bpm on a tired slime's 0.385 (vigor 0.7): %.2f -- the sleeper's floor", pulse_breath_hz(35.0f, 0.385f));
    puts("-- no pulse, no rest: the rest handed back");
    CHECK(near(pulse_breath_hz(0, 0.55f), 0.55f) && near(pulse_breath_hz(250.0f, 0.55f), 0.55f), "0 and 250 bpm: the rest as given");
    CHECK(pulse_breath_hz(60.0f, 0) == 0, "no rest (0): 0, nothing invented");

    puts("-- THE HEARTBEAT HANDED BACK: the plan, beats whose onset lies inside the two seconds");
    pulse_beats_t p;
    CHECK(pulse_beats_plan(60.0f, HEARTBEAT_S, &p) == 2 && p.n == 2 && p.period_ms == 1000, "60 bpm: %d beats, %u ms apart (0 and 1 s; the third would land at the end)", p.n, p.period_ms);
    CHECK(pulse_beats_plan(62.0f, HEARTBEAT_S, &p) == 3 && p.period_ms == 968, "62 bpm: %d beats, %u ms apart (0, 0.97, 1.94 s)", p.n, p.period_ms);
    CHECK(pulse_beats_plan(45.0f, HEARTBEAT_S, &p) == 2 && p.period_ms == 1333, "45 bpm: %d beats, %u ms apart", p.n, p.period_ms);
    CHECK(pulse_beats_plan(120.0f, HEARTBEAT_S, &p) == 4 && p.period_ms == 500, "120 bpm: %d beats, %u ms apart", p.n, p.period_ms);
    CHECK(pulse_beats_plan(180.0f, HEARTBEAT_S, &p) == 7 && p.period_ms == 333, "180 bpm: %d beats, %u ms apart (the seventh at 1.998 s)", p.n, p.period_ms);
    CHECK(pulse_beats_plan(190.0f, HEARTBEAT_S, &p) == 7 && p.period_ms == 316, "190 bpm (the band's top): %d beats, %u ms apart -- under the cap of %d", p.n, p.period_ms, HEARTBEAT_MAX);
    CHECK(pulse_beats_plan(35.0f, HEARTBEAT_S, &p) == 2 && p.period_ms == 1714, "35 bpm (the band's floor): %d beats, %u ms apart", p.n, p.period_ms);
    CHECK(pulse_beats_plan(190.0f, 5.0f, &p) == HEARTBEAT_MAX, "190 bpm over 5 s: capped at %d", HEARTBEAT_MAX);
    CHECK(pulse_beats_plan(60.0f, 0.5f, &p) == 1, "60 bpm inside half a second: the one beat at 0");
    CHECK(pulse_beats_plan(20.0f, HEARTBEAT_S, &p) == 0 && p.n == 0 && p.period_ms == 0, "20 bpm: no plan (the purr plays instead)");
    CHECK(pulse_beats_plan(60.0f, 0, &p) == 0, "no time: no plan");
    CHECK(pulse_beats_plan(60.0f, HEARTBEAT_S, NULL) == 2, "no plan asked for (NULL): still counts");
    CHECK(HEARTBEAT_KICK_MS + HEARTBEAT_TAP_MS < 316, "a beat (%d ms) fits inside the shortest period (316 ms at 190 bpm)", HEARTBEAT_KICK_MS + HEARTBEAT_TAP_MS);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
