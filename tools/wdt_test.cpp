/* THE LOOP'S PULSE (src/wdtfeed.h) on the laptop: when a feed is owed
 * from each spell, and the doctor's clause.
 * Build: c++ -std=gnu++17 -I src -o wdt_test tools/wdt_test.cpp
 * Run:   ./wdt_test
 * A track never fed feeds at once; the loop, the ear and an aid feed
 * once a second and not between; the edge of a light sleep feeds every
 * time; the clock's wrap-around is a gap like any other; a kind off the
 * enum is judged as the loop; the word says the timeout and the age. */
#include "wdtfeed.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main(void)
{
    wdt_track_t t; memset(&t, 0, sizeof t);
    char w[64];

    check("the timeout is 30 s", WDT_TIMEOUT_S == 30);
    check("the gap table is sized by the enum", sizeof WDT_GAP_MS / sizeof WDT_GAP_MS[0] == WDT_SPELL_N);
    check("never fed: owed at once", wdt_feed_due(&t, 5, WDT_SPELL_LOOP));
    wdt_word(w, sizeof w, &t, 5);
    check("never fed: the word says so", !strcmp(w, "wdt 30 s, never fed"));

    check("the first feed goes", wdt_feed_take(&t, 100, WDT_SPELL_LOOP) && t.feeds == 1 && t.last_ms == 100);
    check("the loop 500 ms later: not owed", !wdt_feed_due(&t, 600, WDT_SPELL_LOOP));
    check("the loop 999 ms later: not owed", !wdt_feed_take(&t, 1099, WDT_SPELL_LOOP) && t.feeds == 1);
    check("the loop 1000 ms later: owed", wdt_feed_take(&t, 1100, WDT_SPELL_LOOP) && t.feeds == 2 && t.last_ms == 1100);
    check("the ear 25 ms later: not owed", !wdt_feed_due(&t, 1125, WDT_SPELL_EAR));
    check("the ear a second later: owed", wdt_feed_due(&t, 2100, WDT_SPELL_EAR));
    check("an aid 5 ms later: not owed", !wdt_feed_due(&t, 1105, WDT_SPELL_AID));
    check("an aid a second later: owed", wdt_feed_due(&t, 2100, WDT_SPELL_AID));
    check("the sleep's edge: owed at once", wdt_feed_take(&t, 1101, WDT_SPELL_SLEEP) && t.feeds == 3);
    check("the sleep's edge again 1 ms on: owed again", wdt_feed_take(&t, 1102, WDT_SPELL_SLEEP) && t.feeds == 4);
    check("a kind off the enum is judged as the loop", !wdt_feed_due(&t, 1500, 99) && wdt_feed_due(&t, 2102, -1));

    wdt_word(w, sizeof w, &t, 1114);
    check("the word: `wdt 30 s, fed 12 ms ago`", !strcmp(w, "wdt 30 s, fed 12 ms ago"));

    /* millis wraps after 49.7 days: the gap is unsigned */
    t.last_ms = 0xFFFFFF00u; t.fed = true;
    check("across the clock's wrap, 256 ms on: not owed", !wdt_feed_due(&t, 0, WDT_SPELL_LOOP));
    check("across the wrap, a second on: owed", wdt_feed_due(&t, 744, WDT_SPELL_LOOP));
    wdt_word(w, sizeof w, &t, 744);
    check("the word across the wrap counts right", !strcmp(w, "wdt 30 s, fed 1000 ms ago"));

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
