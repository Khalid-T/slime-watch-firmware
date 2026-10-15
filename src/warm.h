/* THE WARM SKIN (design/watch-night-2026-10-12.md C20, THE OPERATOR
 * NIGHT): the pulse sensor has a thermometer in it. The MAX30102's die
 * temperature (TINT 0x1F a signed integer C, TFRAC 0x20 in sixteenths)
 * is read ONCE at the end of each spot read (ppg.cpp: one conversion,
 * ~30 ms, before the LEDs' shutdown; no buffer) and handed here as one
 * number per spot. A slow one-pole proxy for the skin (tau WARM_TAU
 * readings: the first sets it, every next moves it a third of the way)
 * is banded WARM (at or over WARM_C) / COLD (at or under COLD_C) /
 * plain, each band letting go only WARM_HYST_C past its line (no flap
 * on a wobble). Each ENTRY into a band leaves one debt, paid once:
 *   WARM  on his NEXT raise (lit, holding, awake, no page): one content
 *         settle -- WARM_SETTLE_SQUASH over WARM_SETTLE_S + CONTENT eyes
 *         the same second and a half (SHOWING SOMEONE's own settle).
 *   COLD  on the first lit, holding, awake frame (at once if the glass
 *         is lit): the cold sky's shiver once + the breath snapped slow,
 *         warming over its 30 s -- PUT ON COLD's road (rest_in_t.chilled).
 * A debt the band leaves unpaid is forgiven (the skin is no longer what
 * it said). Never a number, never a mark, never a word on the glass.
 *
 * HONEST: his v0 back plate is thick -- the die sits off the skin and
 * reads the case more than him; a watch lying on the desk reads the
 * room. The two lines are GUESSES until the v0.2 floor (ruling 148).
 * The readings come only when a spot runs: the creature's own every
 * 5 min at rest (never on the shelf, never on the cable), or his asked
 * one -- the proxy moves slowly, by design.
 *
 * Pure C, no Arduino. tools/warm_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/warm_test tools/warm_test.cpp -x c src/warm.c -lm && /tmp/warm_test */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WARM_TAU            3        /* readings: s += (t - s) / WARM_TAU per reading; the first sets it */
#define WARM_C              32.0f    /* the proxy at or over this = WARM (guessed; a wrist under a thin floor reads 32-34) */
#define COLD_C              26.0f    /* ...at or under this = COLD (guessed; a case just put on from a 22 C desk) */
#define WARM_HYST_C         1.0f     /* a band lets go only this far past its line (WARM ends under 31, COLD over 27) */
#define WARM_SETTLE_SQUASH  0.06f    /* the content settle on the raise: SHOWING SOMEONE's own */
#define WARM_SETTLE_S       1.5f
#define WARM_DIE_MIN_C     -40.0f    /* the die's own range: a reading outside it is a bus fault, refused */
#define WARM_DIE_MAX_C      85.0f

enum { WARM_NONE = 0, WARM_PLAIN, WARM_WARM, WARM_COLD };    /* NONE = no reading yet */
enum { WARM_EV_NONE = 0, WARM_EV_BAND };                     /* the band changed (main logs its word) */
enum { WARM_ACT_NONE = 0,
       WARM_ACT_SETTLE,     /* the raise after WARM's entry: one content settle (main's timer + eyes) */
       WARM_ACT_SHIVER };   /* the first lit frame after COLD's entry: the shiver + the slow breath (rest_in_t.chilled this frame) */

int         warm_feed(float die_c);           /* one reading per spot: the smoothing, the band, the debts; the event */
/* one call a frame: pays a debt when its moment comes -- raise = the raise's cue this frame (TELL_CUE_RAISE),
 * awake = not dozing (asleep or stirring), page = a page is up; the act is returned once */
int         warm_tick(bool lit, bool holding, bool awake, bool raise, bool page);
int         warm_band(void);                  /* WARM_* now */
float       warm_c(void);                     /* the proxy, NAN with no reading */
float       warm_last_c(void);                /* the last reading as it came, NAN with none */
const char *warm_word(int band);              /* "--" / "plain" / "WARM" / "COLD" */
bool        warm_owed(int act);               /* a debt waiting: WARM_ACT_SETTLE / WARM_ACT_SHIVER */
int         warm_reads(void);                 /* readings taken since boot */
int         warm_settles(void);               /* settles played */
int         warm_shivers(void);               /* shivers played */

#ifdef __cplusplus
}
#endif
