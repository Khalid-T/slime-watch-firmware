/* THE WARM SKIN on the laptop (warm.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/warm_test tools/warm_test.cpp -x c src/warm.c -lm && /tmp/warm_test
 * No reading = no band, NAN; the first reading sets the proxy, the
 * next move it a third (tau 3); a bus fault (NAN, 0xFF) is refused;
 * the walk up into WARM, the hysteresis on both lines (no flap on a
 * wobble), the walk down into COLD; the debts: WARM's settle waits
 * for the RAISE (lit, holding, awake, no page) and plays once; COLD's
 * shiver plays on the first lit frame, once; a debt the band leaves
 * unpaid is forgiven; a re-entry owes again; the words. */
#include "warm.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int feed_n(float c, int n) { int evs = 0; for (int i = 0; i < n; i++) if (warm_feed(c) == WARM_EV_BAND) evs++; return evs; }
/* a few lit, holding, awake frames with no raise: what acts came */
static int frames(int n, bool lit, bool holding, bool awake, bool raise, bool page)
{
    int act = WARM_ACT_NONE;
    for (int i = 0; i < n; i++) { int a = warm_tick(lit, holding, awake, raise, page); if (a) act = a; }
    return act;
}

int main(void)
{
    printf("THE WARM SKIN\n");
    printf("-- nothing yet\n");
    CHECK(warm_band() == WARM_NONE, "no reading: the band is NONE");
    CHECK(isnan(warm_c()) && isnan(warm_last_c()), "no reading: the proxy and the last are NAN");
    CHECK(!strcmp(warm_word(warm_band()), "--"), "the word is --");
    CHECK(frames(10, true, true, true, true, false) == WARM_ACT_NONE, "no debt: a raise plays nothing");

    printf("-- a bus fault is refused\n");
    CHECK(warm_feed(NAN) == WARM_EV_NONE && warm_band() == WARM_NONE && warm_reads() == 0, "NAN: refused, not counted");
    CHECK(warm_feed(-1.0f + 15 * 0.0625f) == WARM_EV_BAND, "-0.06 C (0xFF 0xFF would read so) is inside the die's range: accepted (the proxy, not the band, is the guard)");
    CHECK(warm_feed(120.0f) == WARM_EV_NONE && warm_reads() == 1, "120 C: off the die's range, refused");

    printf("-- the proxy: the first sets it, the next move a third\n");
    /* start again from a plain room: the first real reading sets the proxy whatever came before? no -- the -0.06 set it. Walk it up */
    float s0 = warm_c();
    warm_feed(29.0f);
    CHECK(fabsf(warm_c() - (s0 + (29.0f - s0) / 3.0f)) < 1e-4f, "a reading moves the proxy a third of the way (%.2f -> %.2f)", s0, warm_c());
    CHECK(warm_last_c() == 29.0f, "the last reading is kept as it came");
    feed_n(29.0f, 20);
    CHECK(fabsf(warm_c() - 29.0f) < 0.05f, "twenty readings at 29: the proxy is there (%.2f)", warm_c());
    CHECK(warm_band() == WARM_PLAIN, "29 C is plain");
    CHECK(!warm_owed(WARM_ACT_SETTLE) && !warm_owed(WARM_ACT_SHIVER), "plain owes nothing");

    printf("-- the walk up into WARM\n");
    int ev = warm_feed(34.0f);
    CHECK(ev == WARM_EV_NONE && warm_band() == WARM_PLAIN, "one 34 C reading: the proxy at %.2f, under 32 -- no band yet (one spike is not a skin)", warm_c());
    ev = feed_n(34.0f, 3);
    CHECK(ev == 1 && warm_band() == WARM_WARM, "three more at 34: WARM (%.2f), one event", warm_c());
    CHECK(warm_owed(WARM_ACT_SETTLE), "WARM's entry owes the settle");
    CHECK(frames(20, true, true, true, false, false) == WARM_ACT_NONE, "lit frames with no raise: the settle waits");
    CHECK(frames(1, false, true, true, true, false) == WARM_ACT_NONE, "a raise in the dark: waits");
    CHECK(frames(1, true, false, true, true, false) == WARM_ACT_NONE, "a raise on the empty glass: waits");
    CHECK(frames(1, true, true, false, true, false) == WARM_ACT_NONE, "a raise while dozing: waits");
    CHECK(frames(1, true, true, true, true, true) == WARM_ACT_NONE, "a raise under a page: waits");
    CHECK(frames(1, true, true, true, true, false) == WARM_ACT_SETTLE && warm_settles() == 1, "the raise, lit, holding, awake, no page: the settle");
    CHECK(frames(5, true, true, true, true, false) == WARM_ACT_NONE && warm_settles() == 1, "the next raises: nothing (once per entry)");
    CHECK(feed_n(34.0f, 5) == 0 && warm_band() == WARM_WARM, "more warm readings: no new event, no new debt");

    printf("-- the hysteresis on the warm line\n");
    feed_n(31.5f, 30);
    CHECK(warm_band() == WARM_WARM && fabsf(warm_c() - 31.5f) < 0.05f, "the proxy at 31.5 (under 32, over 31): still WARM");
    feed_n(30.5f, 30);
    CHECK(warm_band() == WARM_PLAIN, "the proxy at 30.5 (under 31): plain again");
    CHECK(!warm_owed(WARM_ACT_SETTLE), "leaving WARM owes nothing");
    feed_n(31.8f, 30);
    CHECK(warm_band() == WARM_PLAIN, "31.8 from below: not WARM (the line is 32)");
    feed_n(32.2f, 30);
    CHECK(warm_band() == WARM_WARM && warm_owed(WARM_ACT_SETTLE), "32.2: WARM again, the settle owed again (a re-entry)");

    printf("-- a debt the band leaves is forgiven\n");
    feed_n(28.0f, 30);
    CHECK(warm_band() == WARM_PLAIN && !warm_owed(WARM_ACT_SETTLE), "plain before any raise: the settle forgiven");
    CHECK(frames(3, true, true, true, true, false) == WARM_ACT_NONE && warm_settles() == 1, "the raise after: nothing");

    printf("-- the walk down into COLD\n");
    ev = feed_n(24.0f, 1);
    CHECK(ev == 0 && warm_band() == WARM_PLAIN, "one 24 C reading: %.2f, over 26 -- no band yet", warm_c());
    ev = feed_n(24.0f, 4);
    CHECK(ev == 1 && warm_band() == WARM_COLD, "four more at 24: COLD (%.2f), one event", warm_c());
    CHECK(warm_owed(WARM_ACT_SHIVER) && !warm_owed(WARM_ACT_SETTLE), "COLD's entry owes the shiver, not the settle");
    CHECK(frames(4, false, true, true, false, false) == WARM_ACT_NONE, "dark frames: the shiver waits (nothing lights the glass)");
    CHECK(frames(1, true, true, false, false, false) == WARM_ACT_NONE, "lit but dozing: waits");
    CHECK(frames(1, true, false, true, false, false) == WARM_ACT_NONE, "lit but empty: waits");
    CHECK(frames(1, true, true, true, false, true) == WARM_ACT_NONE, "lit under a page: waits");
    CHECK(frames(1, true, true, true, false, false) == WARM_ACT_SHIVER && warm_shivers() == 1, "the first lit, holding, awake frame (no raise needed): the shiver");
    CHECK(frames(10, true, true, true, true, false) == WARM_ACT_NONE && warm_shivers() == 1, "after: nothing, a raise included (once per entry)");

    printf("-- the hysteresis on the cold line\n");
    feed_n(26.5f, 30);
    CHECK(warm_band() == WARM_COLD, "the proxy at 26.5 (over 26, under 27): still COLD");
    feed_n(27.5f, 30);
    CHECK(warm_band() == WARM_PLAIN && !warm_owed(WARM_ACT_SHIVER), "27.5: plain, nothing owed");
    feed_n(25.9f, 30);
    CHECK(warm_band() == WARM_COLD && warm_owed(WARM_ACT_SHIVER), "25.9 (under the line): COLD again, the shiver owed again");
    feed_n(35.0f, 30);
    CHECK(warm_band() == WARM_WARM && !warm_owed(WARM_ACT_SHIVER) && warm_owed(WARM_ACT_SETTLE), "straight up to WARM: the shiver forgiven, the settle owed");

    printf("-- the words\n");
    CHECK(!strcmp(warm_word(WARM_PLAIN), "plain") && !strcmp(warm_word(WARM_WARM), "WARM") && !strcmp(warm_word(WARM_COLD), "COLD"), "plain / WARM / COLD");
    CHECK(!strcmp(warm_word(99), "--"), "an unknown band is --");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
