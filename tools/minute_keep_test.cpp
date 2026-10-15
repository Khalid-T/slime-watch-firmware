/* Host test of THE MINUTE KEPT ACROSS A RESET's decision (src/minutekeep.h):
 * a made-up RTC keep (the stamp around a minute's tally) against every
 * kind of boot -- the same minute, a crossed one, a power-on, no clock,
 * another day, a stamp ahead of the clock, a wrong magic, and every
 * field past its bound (garbage never files).
 * Build: c++ -std=gnu++17 -I src -o minute_keep_test tools/minute_keep_test.cpp
 * Run:   ./minute_keep_test
 */
#include "../src/minutekeep.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static const uint32_t TODAY = 20261012;

/* a plausible minute: 40 s of stillness, a short walk, a tap */
static mk_keep_t good(int minute)
{
    mk_keep_t k;
    memset(&k, 0, sizeof k);
    mk_stamp(&k, TODAY, minute);
    k.t.n = 2400; k.t.sumsq = 2400 * 0.0004;
    k.t.steps = 12; k.t.raises = 1; k.t.taps = 1;
    k.t.screen_s = 8; k.t.still_s = 40;
    k.t.posture_n[1] = 1800; k.t.posture_n[3] = 600;
    k.t.hr = 64; k.t.hrv = 38;
    return k;
}

static int decide(const mk_keep_t *k, bool soft, bool clock, int minute, int *why)
{
    return mk_decide(k, soft, clock, TODAY, minute, why);
}

int main(void)
{
    int why;
    /* the three verdicts */
    {
        mk_keep_t k = good(870);
        check("a soft reset in the same minute: SAME", decide(&k, true, true, 870, &why) == MK_SAME && why == MK_WHY_TAKEN);
        check("a soft reset one minute on: CROSSED", decide(&k, true, true, 871, &why) == MK_CROSSED && why == MK_WHY_TAKEN);
        check("a soft reset three minutes on (a hang): CROSSED", decide(&k, true, true, 873, &why) == MK_CROSSED);
        check("a power-on: DISCARD (the RTC memory is garbage)", decide(&k, false, true, 870, &why) == MK_DISCARD && why == MK_WHY_POWERON);
        check("no clock at boot: DISCARD", decide(&k, true, false, 870, &why) == MK_DISCARD && why == MK_WHY_NO_CLOCK);
        check("the clock set but no minute (-1): DISCARD", decide(&k, true, true, -1, &why) == MK_DISCARD && why == MK_WHY_NO_CLOCK);
        check("a stamp from yesterday (a reset across midnight): DISCARD", mk_decide(&k, true, true, 20261013, 0, &why) == MK_DISCARD && why == MK_WHY_OTHER_DAY);
        check("a stamp ahead of the clock: DISCARD", decide(&k, true, true, 869, &why) == MK_DISCARD && why == MK_WHY_AHEAD);
        check("why may be NULL", decide(&k, true, true, 870, NULL) == MK_SAME);
    }
    /* the magic */
    {
        mk_keep_t k = good(870);
        k.magic = 0;
        check("no magic (nothing kept): DISCARD", decide(&k, true, true, 870, &why) == MK_DISCARD && why == MK_WHY_MAGIC);
        k.magic = MK_MAGIC ^ 1;
        check("a wrong magic: DISCARD", decide(&k, true, true, 870, &why) == MK_DISCARD && why == MK_WHY_MAGIC);
        mk_keep_t c = good(870);
        mk_clear(&c);
        check("mk_clear leaves nothing to take (the flush filed it)", decide(&c, true, true, 870, &why) == MK_DISCARD && why == MK_WHY_MAGIC);
        check("a NULL keep: DISCARD", decide(NULL, true, true, 870, &why) == MK_DISCARD);
    }
    /* the bounds: one field at a time past its line, each a DISCARD with the bounds word */
    {
        struct { const char *what; void (*spoil)(mk_keep_t *); } cases[] = {
            { "minute 1440", [](mk_keep_t *k) { k->minute = 1440; } },
            { "minute -1", [](mk_keep_t *k) { k->minute = -1; } },
            { "date 20261300 (month 13)", [](mk_keep_t *k) { k->date = 20261300; } },
            { "date 20261032 (day 32)", [](mk_keep_t *k) { k->date = 20261032; } },
            { "date 19991231 (before the floor)", [](mk_keep_t *k) { k->date = 19991231; } },
            { "n past MK_MAX_N", [](mk_keep_t *k) { k->t.n = MK_MAX_N + 1; } },
            { "steps past MK_MAX_STEPS", [](mk_keep_t *k) { k->t.steps = MK_MAX_STEPS + 1; } },
            { "raises 256", [](mk_keep_t *k) { k->t.raises = 256; } },
            { "taps 256", [](mk_keep_t *k) { k->t.taps = 256; } },
            { "shakes past MK_MAX_SHAKES", [](mk_keep_t *k) { k->t.shakes = MK_MAX_SHAKES + 1; } },
            { "sumsq NaN", [](mk_keep_t *k) { k->t.sumsq = NAN; } },
            { "sumsq negative", [](mk_keep_t *k) { k->t.sumsq = -1.0; } },
            { "sumsq past every sample at 16 g", [](mk_keep_t *k) { k->t.sumsq = MK_MAX_SUMSQ_G2 * k->t.n + 2.0; } },
            { "sumsq infinite", [](mk_keep_t *k) { k->t.sumsq = INFINITY; } },
            { "screen_s past MK_MAX_SECS", [](mk_keep_t *k) { k->t.screen_s = MK_MAX_SECS + 1; } },
            { "still_s negative", [](mk_keep_t *k) { k->t.still_s = -5; } },
            { "a posture count past n", [](mk_keep_t *k) { k->t.posture_n[2] = (int)k->t.n + 1; } },
            { "the postures sum past n", [](mk_keep_t *k) { k->t.posture_n[0] = 100; } },
            { "a posture count negative", [](mk_keep_t *k) { k->t.posture_n[4] = -1; } },
            { "shaken 2 (not a bool)", [](mk_keep_t *k) { k->t.shaken = 2; } },
            { "up 255 (not a bool)", [](mk_keep_t *k) { k->t.up = 255; } },
            { "drinks 16", [](mk_keep_t *k) { k->t.drinks = 16; } },
            { "meals -1", [](mk_keep_t *k) { k->t.meals = -1; } },
            { "hr 256", [](mk_keep_t *k) { k->t.hr = 256; } },
            { "hrv -1", [](mk_keep_t *k) { k->t.hrv = -1; } },
        };
        for (auto &c : cases) {
            mk_keep_t k = good(870);
            c.spoil(&k);
            char what[96]; snprintf(what, sizeof what, "garbage: %s -> DISCARD", c.what);
            check(what, decide(&k, true, true, 870, &why) == MK_DISCARD && why == MK_WHY_BOUNDS);
        }
    }
    /* the edges that are NOT garbage */
    {
        mk_keep_t k = good(0);
        check("minute 0 is a minute", decide(&k, true, true, 0, &why) == MK_SAME);
        k = good(1439);
        check("minute 1439 is a minute", decide(&k, true, true, 1439, &why) == MK_SAME);
        k = good(870); memset(&k.t, 0, sizeof k.t);
        check("an empty tally (a reset at the minute's first sample) is taken", decide(&k, true, true, 870, &why) == MK_SAME);
        k = good(870); k.t.n = MK_MAX_N; k.t.sumsq = MK_MAX_SUMSQ_G2 * MK_MAX_N; k.t.posture_n[1] = MK_MAX_N; k.t.posture_n[3] = 0;
        check("every field AT its bound is taken", decide(&k, true, true, 870, &why) == MK_SAME);
        k = good(870); k.t.shaken = 1; k.t.up = 1; k.t.drinks = 15; k.t.meals = 15; k.t.raises = 255; k.t.taps = 255; k.t.hr = 255; k.t.hrv = 255;
        check("the capped fields at their caps are taken", decide(&k, true, true, 870, &why) == MK_SAME);
        /* a real-looking bit flip on a garbage boot: a magic that survives by chance is still bounded */
        k = good(870); memset(&k.t, 0xff, sizeof k.t);
        check("a tally of 0xff bytes with a good magic: DISCARD", decide(&k, true, true, 870, &why) == MK_DISCARD && why == MK_WHY_BOUNDS);
    }
    /* the stamp and the words */
    {
        mk_keep_t k; memset(&k, 0xa5, sizeof k);
        mk_stamp(&k, TODAY, 5);
        check("mk_stamp sets the magic, the date and the minute", k.magic == MK_MAGIC && k.date == TODAY && k.minute == 5);
        check("the size stays small (RTC slow memory, under 128 B)", sizeof(mk_keep_t) <= 128);
        check("every why has a word", strcmp(mk_why_word(MK_WHY_TAKEN), "?") && strcmp(mk_why_word(MK_WHY_POWERON), "?") && strcmp(mk_why_word(MK_WHY_NO_CLOCK), "?")
              && strcmp(mk_why_word(MK_WHY_MAGIC), "?") && strcmp(mk_why_word(MK_WHY_BOUNDS), "?") && strcmp(mk_why_word(MK_WHY_OTHER_DAY), "?") && strcmp(mk_why_word(MK_WHY_AHEAD), "?"));
    }
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
