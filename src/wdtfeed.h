/* THE LOOP'S PULSE (the operator night, 2026-10-12; T1): the rule of
 * the task watchdog's feeding, pure. The hardware half (wdt.cpp) arms
 * the IDF's task watchdog on the creature's own loopTask for
 * WDT_TIMEOUT_S and asks this header, at every place the thread
 * lingers, whether a feed is owed: the top of loop(), each pass of the
 * nap's awake-ear loop, each pass of a long serial aid, and the edge
 * of a light sleep (the sleep kind always feeds: the clock is freshest
 * going into the dark, and the MWDT does not count through it). A feed
 * is owed once per WDT_FEED_MS a kind, so the 40 fps loop does not
 * take the watchdog's spinlock every frame. The doctor's clause comes
 * from here too: `wdt 30 s, fed 12 ms ago`.
 * No Arduino: tools/wdt_test.cpp proves it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define WDT_TIMEOUT_S  30          /* the hang's bound: a dead loop reboots inside this, not when the cell dies (ruling 111) */
#define WDT_FEED_MS    1000        /* a feed is owed once a second a kind; the hardware call is a spinlock, not free at 40 fps */

/* where the thread is when it asks */
enum { WDT_SPELL_LOOP = 0, WDT_SPELL_EAR, WDT_SPELL_AID, WDT_SPELL_SLEEP, WDT_SPELL_N };

/* the gap a kind waits between feeds, ms; 0 = every time (sized by the enum) */
static const uint32_t WDT_GAP_MS[WDT_SPELL_N] = { WDT_FEED_MS, WDT_FEED_MS, WDT_FEED_MS, 0 };

typedef struct {
    uint32_t last_ms;              /* the last feed's clock */
    uint32_t feeds;                /* how many went to the hardware since boot */
    bool     fed;                  /* ever */
} wdt_track_t;

/* is a feed owed now, from this spell? */
static inline bool wdt_feed_due(const wdt_track_t *t, uint32_t now_ms, int spell)
{
    if (!t->fed) return true;
    if (spell < 0 || spell >= WDT_SPELL_N) spell = WDT_SPELL_LOOP;
    return (uint32_t)(now_ms - t->last_ms) >= WDT_GAP_MS[spell];
}

/* the feed taken: true when one is owed (the caller then resets the hardware) and the track moves */
static inline bool wdt_feed_take(wdt_track_t *t, uint32_t now_ms, int spell)
{
    if (!wdt_feed_due(t, now_ms, spell)) return false;
    t->last_ms = now_ms;
    t->fed = true;
    t->feeds++;
    return true;
}

/* the doctor's clause */
static inline int wdt_word(char *out, size_t cap, const wdt_track_t *t, uint32_t now_ms)
{
    if (!t->fed) return snprintf(out, cap, "wdt %d s, never fed", WDT_TIMEOUT_S);
    return snprintf(out, cap, "wdt %d s, fed %lu ms ago", WDT_TIMEOUT_S, (unsigned long)(now_ms - t->last_ms));
}
