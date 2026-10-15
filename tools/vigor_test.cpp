/* VIGOR FROM THE CELL on the laptop (vigor.cpp has no Arduino in it;
 * the stub dir is on the line for the pattern, gel.h only for the eye
 * kinds):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/vigor_test tools/vigor_test.cpp src/vigor.cpp -lm && /tmp/vigor_test
 * The ADC's map at 4150 / 3700 / 3400 / 3300 mV through the desk's
 * shape (the knees at 25 % and 10 %); the cable with no cell = 1.0;
 * the gauge's percent winning over the mV; the 60-s ease (95 % by 60 s,
 * the first sample snapping); what each threshold moves (the breath's
 * factors, the idle gap, the lids on a raise only when low, the spent
 * look eased in, and back over with the hysteresis); the cable's settle
 * once (a one-shot: not at boot on the cable, not a second time while
 * it stays; owed to a dark glass and played when lit; dropped after
 * five minutes dark); the vigor rising with the mV on the cable. */
#include "../src/vigor.h"
#include "../src/gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool near(float a, float b, float eps) { return fabsf(a - b) <= eps; }

/* seconds of frames at one reading */
static float run(float seconds, uint16_t mv, bool charging, int pct, bool lit, bool holding)
{
    float v = vigor();
    for (float t = 0; t < seconds; t += 0.05f) v = vigor_tick(0.05f, mv, charging, pct, lit, holding);
    return v;
}
/* the fx over a run: the peak squash, whether droopy / content / the sleep bar were worn */
static float peak_squash, max_spent;
static bool saw_droopy, saw_content;
static void run_fx(float seconds, uint16_t mv, bool charging, int pct, bool lit, bool holding)
{
    peak_squash = 0; max_spent = 0; saw_droopy = saw_content = false;
    for (float t = 0; t < seconds; t += 0.05f) {
        vigor_tick(0.05f, mv, charging, pct, lit, holding);
        vigor_fx_t fx; vigor_fx(&fx);
        if (fx.squash > peak_squash) peak_squash = fx.squash;
        if (fx.spent_k > max_spent) max_spent = fx.spent_k;
        if (fx.eye_kind == GEL_EYE_DROOPY) saw_droopy = true;
        if (fx.eye_kind == GEL_EYE_CONTENT) saw_content = true;
    }
}

int main(void)
{
    char st[300];

    printf("THE MAP: the ADC's mV through the desk's shape\n");
    CHECK(vigor_level_of_mv(4150) == 1.0f && vigor_level_of_mv(3300) == 0.0f, "3300 mV = level 0, 4150 = level 1");
    CHECK(near(vigor_level_of_mv(3700), 0.4706f, 0.001f), "3700 mV = level %.3f (400 of 850)", vigor_level_of_mv(3700));
    CHECK(near(vigor_level_of_mv(3400), 0.1176f, 0.001f), "3400 mV = level %.3f", vigor_level_of_mv(3400));
    CHECK(vigor_level_of_mv(4100) == 1.0f && vigor_level_of_mv(4130) == 1.0f, "4100 and above (the float) = level 1");
    CHECK(vigor_of_level(1.0f) == 1.0f && vigor_of_level(0.0f) == 0.0f, "the desk's shape: 1 at full, 0 at empty");
    CHECK(near(vigor_of_level(0.25f), 0.7f, 1e-6f) && near(vigor_of_level(0.10f), 0.4f, 1e-6f),
          "the knees are the desk's: 0.7 at 25 %%, 0.4 at 10 %% (vigor_for's steps)");
    float v4150 = vigor_of_level(vigor_level_of_mv(4150)), v3700 = vigor_of_level(vigor_level_of_mv(3700));
    float v3400 = vigor_of_level(vigor_level_of_mv(3400)), v3300 = vigor_of_level(vigor_level_of_mv(3300));
    printf("       4150 -> %.3f, 3700 -> %.3f, 3400 -> %.3f, 3300 -> %.3f\n", v4150, v3700, v3400, v3300);
    CHECK(v4150 == 1.0f && near(v3700, 0.788f, 0.002f) && near(v3400, 0.435f, 0.002f) && v3300 == 0.0f,
          "4150 / 3700 / 3400 / 3300 mV -> 1.00 / 0.79 / 0.44 / 0.00");
    CHECK(v4150 > v3700 && v3700 > v3400 && v3400 > v3300, "monotone");
    {   /* no step under the float: 1 mV at a time; the one seam is the
         * float itself (4099 -> 4100: 0.98 -> 1.0), which the 60-s ease hides */
        float prev = vigor_of_level(vigor_level_of_mv(3300)), biggest = 0, seam = 0;
        for (int mv = 3301; mv < VIGOR_MV_FLOAT; mv++) { float v = vigor_of_level(vigor_level_of_mv((uint16_t)mv)); if (v - prev > biggest) biggest = v - prev; prev = v; }
        seam = vigor_of_level(vigor_level_of_mv(VIGOR_MV_FLOAT)) - prev;
        CHECK(biggest < 0.006f && seam < 0.03f, "no step in the map under the float (the largest move for 1 mV %.4f); the float's seam %.3f, eased", biggest, seam);
    }
    CHECK(v3400 < VIGOR_LOW + 0.2f && v3400 > VIGOR_LOW, "3400 mV is above LOW %.2f still (%.2f): the lids wait for ~3370", VIGOR_LOW, v3400);

    printf("THE CABLE WITH NO CELL (today's board): 1.0, the source named\n");
    vigor_begin();
    CHECK(vigor() == 1.0f && !strcmp(vigor_source(), "nothing yet"), "before a sample: the nominal 1.0, \"nothing yet\"");
    run(1, 4130, true, -1, true, true);
    CHECK(vigor() == 1.0f && !strcmp(vigor_source(), "the cable"), "4130 mV on USB, no cell: 1.0 from \"%s\"", vigor_source());
    CHECK(vigor_settles() == 0, "at boot on the cable no settle: it did not arrive");
    CHECK(near(vigor_breath_hz_k(), 1.0f, 1e-6f) && near(vigor_breath_amp_k(), 1.0f, 1e-6f) && near(vigor_idle_k(), 1.0f, 1e-6f),
          "the factors all x1 at full");
    vigor_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "the cable") && strstr(st, "1.00") && strstr(st, "lids ordinary"), "`vigor` says so");

    printf("THE GAUGE wins over the mV when it answers\n");
    vigor_begin();
    run(0.05f, 3700, false, 60, true, true);
    CHECK(!strcmp(vigor_source(), "the gauge") && near(vigor(), vigor_of_level(0.60f), 1e-5f), "gauge 60 %% at 3700 mV: %.3f from the gauge, not the ADC's 0.79", vigor());
    run(0.05f, 3700, false, -1, true, true);
    CHECK(!strcmp(vigor_source(), "the ADC"), "the gauge silent: the ADC again");

    printf("THE EASE: the first sample snaps, then 95 %% in 60 s\n");
    vigor_begin();
    run(0.05f, 3700, false, -1, true, true);
    CHECK(near(vigor(), v3700, 1e-5f), "the first sample snaps to %.3f (no fade-in from 1.0)", vigor());
    run(0.05f, 3300, false, -1, true, true);
    CHECK(vigor() > v3700 - 0.01f, "a drop to 3300 mV: one frame later still %.3f (the target %.2f)", vigor(), vigor_target());
    run(30, 3300, false, -1, true, true);
    CHECK(near(vigor(), v3700 * expf(-30.0f / 20.0f), 0.01f), "after 30 s %.3f (a 20-s time constant: %.3f)", vigor(), v3700 * expf(-1.5f));
    run(30, 3300, false, -1, true, true);
    CHECK(vigor() < v3700 * 0.06f, "after 60 s %.3f: 95 %% of the way", vigor());

    printf("THE THRESHOLDS: what each moves\n");
    vigor_begin();
    run(0.05f, 3700, false, -1, true, true);
    CHECK(!vigor_low() && !vigor_spent(), "at 3700 mV (%.2f) neither low nor spent", vigor());
    CHECK(near(vigor_breath_hz_k(), 0.7f + 0.3f * vigor(), 1e-6f) && near(vigor_breath_amp_k(), 0.8f + 0.2f * vigor(), 1e-6f),
          "the breath's rate x%.3f depth x%.3f (0.7 + 0.3 v, 0.8 + 0.2 v)", vigor_breath_hz_k(), vigor_breath_amp_k());
    CHECK(near(vigor_idle_k(), 2.0f - vigor(), 1e-6f), "the idle gap x%.3f (1 + (1 - v))", vigor_idle_k());
    vigor_raise(); run_fx(1.5f, 3700, false, -1, true, true);
    CHECK(!saw_droopy, "a raise at 0.79: no droopy lids");
    /* down to low: a gauge at 8 % (0.32, under 0.35) */
    vigor_begin();
    run(0.05f, 3500, false, 8, true, true);
    CHECK(vigor_low() && !vigor_spent(), "gauge 8 %%: %.2f, LOW (under %.2f), not spent", vigor(), VIGOR_LOW);
    vigor_raise(); run_fx(VIGOR_LID_S + 0.05f, 3500, false, 8, true, true);
    CHECK(saw_droopy, "a raise when low: droopy lids");
    run_fx(1.0f, 3500, false, 8, true, true);
    CHECK(!saw_droopy, "...for a second only (VIGOR_LID_S %.0f): ordinary after", VIGOR_LID_S);
    CHECK(near(vigor_breath_hz_k(), 0.7f + 0.3f * 0.32f, 1e-3f) && near(vigor_idle_k(), 1.68f, 1e-3f),
          "low: the breath's rate x%.2f, the idle gap x%.2f", vigor_breath_hz_k(), vigor_idle_k());
    vigor_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "LOW") && strstr(st, "8/12"), "`vigor` says LOW and the short timers");
    /* the hysteresis: back over 0.35 is not back; 0.40 is */
    run(90, 3500, false, 9, true, true);   /* 9 % = 0.36 */
    CHECK(vigor_low() && vigor() > VIGOR_LOW, "back to %.2f (over %.2f) is still low: the hysteresis", vigor(), VIGOR_LOW);
    run(90, 3500, false, 11, true, true);  /* 11 % = 0.42 */
    CHECK(!vigor_low(), "at %.2f (over %.2f + %.2f) low is off", vigor(), VIGOR_LOW, VIGOR_HYST);
    /* spent: a gauge at 2 % (0.08) */
    vigor_begin();
    run(0.05f, 3320, false, 2, true, true);
    CHECK(vigor_spent() && vigor_low(), "gauge 2 %%: %.2f, SPENT (under %.2f)", vigor(), VIGOR_EMPTY);
    run_fx(1.0f, 3320, false, 2, true, true);
    CHECK(max_spent > 0.4f && max_spent < 0.6f, "the sleep look easing in: %.2f after 1 s (VIGOR_SPENT_EASE_S %.0f)", max_spent, VIGOR_SPENT_EASE_S);
    run_fx(2.0f, 3320, false, 2, true, true);
    CHECK(max_spent == 1.0f, "...full at 2 s");
    vigor_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "SPENT"), "`vigor` says SPENT");
    run(90, 3400, false, 4, true, true);   /* 4 % = 0.16: over 0.12 but under 0.17 */
    CHECK(vigor_spent(), "back to %.2f is still spent: the hysteresis", vigor());
    run(90, 3400, false, 5, true, true);   /* 5 % = 0.20 */
    CHECK(!vigor_spent() && vigor_low(), "at %.2f spent is off, low stays", vigor());
    run_fx(3.0f, 3400, false, 5, true, true);
    vigor_fx_t fx; vigor_fx(&fx);
    CHECK(fx.spent_k == 0.0f, "the sleep look eased back out");

    printf("THE CABLE ARRIVING: one contented settle, and the vigor rising with the mV\n");
    vigor_begin();
    run(1, 3700, false, -1, true, true);
    run_fx(0.05f, 4060, true, -1, true, true);
    CHECK(vigor_settles() == 1 && vigor_playing(), "the cable arrives (charging): the settle begins, once");
    run_fx(1.5f, 4060, true, -1, true, true);
    CHECK(near(peak_squash, VIGOR_SETTLE_SQUASH, 0.003f) && saw_content, "the settle: squash to %.3f on a half sine, content eyes", peak_squash);
    run_fx(1.0f, 4060, true, -1, true, true);
    CHECK(!vigor_playing(), "over after %.0f s (the content eyes %.1f s past the squash)", VIGOR_SETTLE_EYE_S, VIGOR_SETTLE_EYE_S - VIGOR_SETTLE_S);
    run_fx(0.5f, 4060, true, -1, true, true);
    CHECK(!saw_content && peak_squash == 0, "...and the ordinary face");
    run_fx(5, 4060, true, -1, true, true);
    CHECK(vigor_settles() == 1 && peak_squash == 0, "still on the cable: no second settle");
    float before = vigor();
    run(60, 4090, true, -1, true, true);
    CHECK(vigor() > before && near(vigor(), vigor_of_level(vigor_level_of_mv(4090)), 0.06f), "the vigor rises with the mV: %.2f -> %.2f at 4090", before, vigor());
    run(60, 4130, true, -1, true, true);
    CHECK(near(vigor(), 1.0f, 0.06f) && !strcmp(vigor_source(), "the cable"), "at the float: %.2f, \"the cable\"", vigor());
    run(1, 3900, false, -1, true, true);
    run_fx(0.05f, 4060, true, -1, true, true);
    CHECK(vigor_settles() == 2, "off and on again: a second arrival settles again");
    /* owed to a dark glass */
    vigor_begin();
    run(1, 3700, false, -1, false, true);
    run_fx(3, 4060, true, -1, false, true);
    CHECK(vigor_settles() == 0 && peak_squash == 0, "the cable on a dark glass: nothing played yet");
    run_fx(1.5f, 4060, true, -1, true, true);
    CHECK(vigor_settles() == 1 && peak_squash > 0.05f, "...played when the glass lights");
    vigor_begin();
    run(1, 3700, false, -1, false, true);
    run(VIGOR_SETTLE_HOLD_S + 1, 4060, true, -1, false, true);
    run_fx(2, 4060, true, -1, true, true);
    CHECK(vigor_settles() == 0 && peak_squash == 0, "owed %.0f s to a dark glass: dropped, not played late", VIGOR_SETTLE_HOLD_S);
    vigor_begin();
    run(1, 3700, false, -1, true, false);
    run_fx(3, 4060, true, -1, true, false);
    CHECK(vigor_settles() == 0, "on the empty glass: no gel to settle (owed until it is home)");

    printf("BATTERY MANNERS: the cell's percent, thrift under 20, lean under 10, 2 back over; none on the cable\n");
    vigor_begin();
    run(1.0f, 3700, false, -1, true, true);
    CHECK(vigor_pct() == 47 && !vigor_thrift() && !vigor_lean(), "3700 mV: 47 %%, no manners (%d)", vigor_pct());
    run(1.0f, 3700, false, 19, true, true);
    CHECK(vigor_pct() == 19 && vigor_thrift() && !vigor_lean(), "the gauge says 19 %%: thrift, not lean");
    run(1.0f, 3700, false, 21, true, true);
    CHECK(vigor_thrift(), "21 %%: still thrift (the hysteresis: back over at 22)");
    run(1.0f, 3700, false, 22, true, true);
    CHECK(!vigor_thrift(), "22 %%: thrift off");
    run(1.0f, 3700, false, 9, true, true);
    CHECK(vigor_thrift() && vigor_lean(), "9 %%: lean (and thrift)");
    run(1.0f, 3700, false, 11, true, true);
    CHECK(vigor_lean(), "11 %%: still lean");
    run(1.0f, 3700, false, 12, true, true);
    CHECK(!vigor_lean() && vigor_thrift(), "12 %%: lean off, thrift on");
    run(1.0f, 3380, false, -1, true, true);
    CHECK(vigor_pct() == 9 && vigor_lean(), "the ADC alone: 3380 mV is 9 %%, lean");
    run(1.0f, 4130, true, -1, true, true);
    CHECK(vigor_pct() == 100 && !vigor_thrift() && !vigor_lean(), "the cable: 100 %%, no manners while it drinks");
    { char l[400]; vigor_status(l, sizeof l); CHECK(strstr(l, "manners: 100 % -- none") != NULL, "status: %s", strstr(l, "manners") ? strstr(l, "manners") : l); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
