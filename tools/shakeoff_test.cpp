/* THE SHAKE-OFF on the laptop (shakeoff.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/shakeoff_test tools/shakeoff_test.cpp -x c src/shakeoff.c -lm && /tmp/shakeoff_test
 * Nothing before the edge; the dried edge lit + holding + awake begins
 * it; the curve is the cold shiver's (zero at the start, 12 Hz, the
 * amplitude 0.08 fading linearly, never over the harmonic's clamp,
 * silent at 0.4 s); the squint's half second outlives the jiggle;
 * the edge dark / empty / on a sleeper is missed, not caught up; a
 * second edge restarts it. */
#include "shakeoff.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 1.0f / 240.0f;   /* fine steps: the 12 Hz crests are 1/48 s apart */

/* run s seconds; the biggest |a2| seen, the sign flips, the sum of a2 (the curve's balance) */
static void run(float s, float *peak, int *flips, float *sum)
{
    int n = (int)(s / DT + 0.5f); float last = 0; if (peak) *peak = 0; if (flips) *flips = 0; if (sum) *sum = 0;
    for (int i = 0; i < n; i++) {
        float a = shakeoff_tick(DT);
        if (peak && fabsf(a) > *peak) *peak = fabsf(a);
        if (flips && last != 0 && a != 0 && (a > 0) != (last > 0)) (*flips)++;
        if (sum) *sum += a;
        if (a != 0) last = a;
    }
}

int main(void)
{
    float peak; int flips; float sum;
    puts("-- nothing before the edge");
    run(2.0f, &peak, &flips, &sum);
    CHECK(!shakeoff_on() && !shakeoff_squint() && peak == 0, "2 s of frames with no edge: a2 0, nothing on");
    CHECK(shakeoff_count() == 0 && shakeoff_missed() == 0, "no shake, nothing missed");

    puts("-- the dried edge, lit, holding, awake");
    CHECK(shakeoff_trigger(true, true, false), "the glass dried: the shake begins");
    CHECK(shakeoff_on() && shakeoff_squint() && shakeoff_count() == 1, "the jiggle and the squint are on; one shake");
    CHECK(shakeoff_tick(DT) == 0 && shakeoff_a2() == 0, "the first frame is the curve's zero (sin 0)");
    run(1.0f / 48.0f, &peak, &flips, &sum);
    CHECK(peak > 0.070f && peak <= SHAKEOFF_A2, "the first crest near 1/48 s: |a2| %.3f (the cold shiver's 0.08, a hair of fade off)", peak);
    CHECK(shakeoff_a2() > 0, "... and it is the first lobe's sign (the cold shiver's)");
    run(SHAKEOFF_S - 7 * DT, &peak, &flips, &sum);                 /* 95 frames in all: a frame short of the 0.4 s */
    CHECK(shakeoff_on(), "at %.3f s still shaking", 95 * DT);
    CHECK(peak <= SHAKEOFF_A2 && peak < 0.10f, "never over the amplitude 0.08 (the harmonic's clamp 0.10 is never touched): peak %.3f", peak);
    CHECK(flips >= 6, "12 Hz through the rest of the 0.4 s: %d sign flips (a jiggle, not a sway)", flips);
    CHECK(fabsf(sum) * DT < 0.002f, "the curve balances (its integral %.5f): no lean left behind", sum * DT);
    run(2 * DT, &peak, &flips, &sum);
    CHECK(!shakeoff_on() && shakeoff_a2() == 0, "at %.1f s silent: a2 0", SHAKEOFF_S);
    CHECK(shakeoff_squint(), "... the squint outlives the jiggle");
    run(SHAKEOFF_SQUINT_S - SHAKEOFF_S, &peak, &flips, &sum);
    CHECK(!shakeoff_squint() && peak == 0, "at %.1f s the ordinary face; no a2 came back", SHAKEOFF_SQUINT_S);
    run(3.0f, &peak, &flips, &sum);
    CHECK(peak == 0 && shakeoff_count() == 1, "3 s more: nothing, once");

    puts("-- the fade");
    shakeoff_trigger(true, true, false);
    float early, late;
    run(SHAKEOFF_S * 0.25f, &early, &flips, &sum);
    run(SHAKEOFF_S * 0.50f, &peak, &flips, &sum);
    run(SHAKEOFF_S * 0.25f + DT, &late, &flips, &sum);
    CHECK(early > 0.06f && late < 0.025f && late > 0, "the first quarter's crests %.3f, the last quarter's %.3f: it fades out", early, late);
    run(1.0f, &peak, &flips, &sum);

    puts("-- the edge missed");
    int c = shakeoff_count();
    CHECK(!shakeoff_trigger(false, true, false) && !shakeoff_on() && !shakeoff_squint(), "the glass dried in the dark: nothing");
    CHECK(!shakeoff_trigger(true, false, false) && !shakeoff_on(), "the house empty: nothing");
    CHECK(!shakeoff_trigger(true, true, true) && !shakeoff_on(), "on a sleeper: nothing");
    CHECK(shakeoff_count() == c && shakeoff_missed() == 3, "three missed, none caught up");
    run(1.0f, &peak, &flips, &sum);
    CHECK(peak == 0, "... and the next second of frames owes nothing");

    puts("-- a second edge restarts it");
    shakeoff_trigger(true, true, false);
    run(0.2f, &peak, &flips, &sum);
    CHECK(shakeoff_trigger(true, true, false), "an edge mid-shake: begins again");
    run(0.3f, &peak, &flips, &sum);
    CHECK(shakeoff_on() && shakeoff_squint(), "0.3 s on: still shaking (the clock began over)");
    run(0.3f, &peak, &flips, &sum);
    CHECK(!shakeoff_on() && !shakeoff_squint(), "0.6 s on: done");

    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
