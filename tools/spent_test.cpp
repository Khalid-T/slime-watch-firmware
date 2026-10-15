/* THE LAST DROP'S MANNERS (src/spent.h) on the laptop: the spent line's
 * band and settle, vigor's band OR'ed in, and the glass's two clamps.
 * Build: c++ -std=gnu++17 -I src -o spent_test tools/spent_test.cpp
 * Run:   ./spent_test
 * A boot at 3500 is spent from its first sample; 3540 held is spent
 * after 10 s and not at 9.5; a burst's dip of 4 s is not the line; the
 * way back needs 3630 (the hysteresis), held; vigor's spent band is the
 * line whatever the mV; the LOW step and the SHORT spell clamp, never
 * lengthen. */
#include "spent.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static uint32_t now = 1000;
static bool feed(spent_t *s, uint16_t mv, bool vs, uint32_t ms)   /* a reading every 100 ms for ``ms`` */
{
    bool last = false;
    uint32_t end = now + ms;
    while (now < end) { now += 100; last = spent_sense(s, mv, vs, now); }
    return last;
}

int main(void)
{
    spent_t s;
    /* a boot under the line snaps */
    memset(&s, 0, sizeof s);
    check("a boot at 3500 mV: spent from the first sample", spent_sense(&s, 3500, false, now) == true);
    check("3600 (inside the band) for a minute: still spent", feed(&s, 3600, false, 60000) == true);
    feed(&s, 3640, false, 9500);
    check("3640 for 9.5 s: not yet over the line", s.mv_on == true);
    check("3640 for 10.5 s: over the line, not spent", feed(&s, 3640, false, 1000) == false);
    /* a boot over the line; the line reached, held */
    memset(&s, 0, sizeof s);
    check("a boot at 3800: not spent", spent_sense(&s, 3800, false, now) == false);
    check("3560 (just over) for a minute: not spent", feed(&s, 3560, false, 60000) == false);
    feed(&s, 3540, false, 9500);
    check("3540 for 9.5 s: not yet", s.mv_on == false && s.cand);
    check("3540 for 10.5 s: spent", feed(&s, 3540, false, 1000) == true);
    /* a burst's dip is seconds: never the line */
    memset(&s, 0, sizeof s);
    spent_sense(&s, 3700, false, now);
    for (int i = 0; i < 20; i++) { feed(&s, 3500, false, 4000); feed(&s, 3700, false, 20000); }
    check("a 4 s dip to 3500 every 24 s for 8 min: never spent (the settle restarts)", s.mv_on == false);
    /* vigor's band is the line whatever the ADC says */
    memset(&s, 0, sizeof s);
    check("3800 with vigor spent: spent", spent_sense(&s, 3800, true, now) == true && s.mv_on == false);
    check("... vigor back: not spent, no settle (vigor has its own)", spent_sense(&s, 3800, false, now + 100) == false);
    /* the clamps */
    check("LOW step: 255 -> 120 while spent", spent_level(true, 255, 120) == 120);
    check("LOW step: 200 -> 120 while spent", spent_level(true, 200, 120) == 120);
    check("LOW step: 120 stays 120", spent_level(true, 120, 120) == 120);
    check("not spent: 255 stays", spent_level(false, 255, 120) == 255);
    check("SHORT spell: 60 s -> 12 s while spent", spent_ms(true, 60000, 12000) == 12000);
    check("the low cell's 8 s is already shorter: kept", spent_ms(true, 8000, 12000) == 8000);
    check("not spent: 60 s stays", spent_ms(false, 60000, 12000) == 60000);
    check("the line is 3550, the band 80, the settle 10 s", DROP_MV == 3550 && DROP_HYST_MV == 80 && DROP_SETTLE_MS == 10000);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
