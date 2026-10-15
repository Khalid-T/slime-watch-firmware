/* THE SKY (queue item 3): the weather outside, told to the gel in a
 * word. A new fetch-only verb on the wire -- `sky` -> `ok sky <word>
 * [<word>]` / `ok none` -- asked ONCE per link after `word`, never a
 * stream; the box will speak it one day from what it knows of the
 * house's town; the desk answers `ok none` today. This is the
 * firmware's half.
 *
 * By the material: the gel is translucent, lit by the light through
 * the glass, and it feels the room -- so the sky reaches it as LIGHT
 * (the tint) and as a few small habits of the body, never as a mark,
 * never as a number (design-zoom-out: no numbers in the creature's
 * language; nothing shows when no sky is known). The vocabulary is
 * tiny, each word one thing the gel can carry:
 *
 *   rain   a wet day: the gel FULLER (the meniscus a little higher --
 *          +SKY_RAIN_SQUASH resting; the body bends the water line up
 *          with it) under a greyer light
 *   snow   a white day: the light brighter and whiter
 *   cold   the shine COOLED (bluer, less red), and one SHIVER on each
 *          raise (a 0.3 s jiggle of the 2-lobe harmonic)
 *   hot    a slower breath, and DROOPY lids for a few seconds on the
 *          raise (heavy, in the heat)
 *   clear  a bright day: the shine a touch brighter
 *   storm  the bubble clock QUICKER (the rim's ripple, the bubbles
 *          inside: the gel restless with the pressure) under a dark light
 *   wind   the surface stirred: the shine a touch up, the shade a touch
 *          less (tint only)
 *   dark   an overcast day: the light dimmed (tint only)
 *
 * THE SKY'S WEIGHT (design/watch-ideas.md E38, 2026-09-23): with rain
 * or storm held the gel is HEAVIER -- fuller with water, it moves
 * slower: the springs' damping x SKY_HEAVY_K (the desk's c 9 -> 11)
 * and the lean's and the tilt's eases divided by the same factor
 * (the bowl's ease slower). One factor, and a WEIGHT that ramps in
 * and out over SKY_HEAVY_EASE_S (a minute) so a sky change is never a
 * jump: main.cpp reads sky_heavy() each frame and scales by
 * sky_heavy_k(w). Snow, fog and the other words leave the springs as
 * they are.
 *
 * Up to two words hold at once (`rain cold`); their tints multiply.
 * Held SKY_HOLD_S then FORGOTTEN (a sky is not a fact for ever: the
 * next link asks again); a reboot forgets. `sky` on the serial port
 * prints the held words and their age; `sky <word> [<word>]` sets them
 * by hand (a test aid; the caller logs it). No Arduino in here: dt in,
 * a few deltas out; tools/sky_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gel.h"

#define SKY_WORDS 2

/* what the draw folds in this frame (all zero / -1 when no sky is known) */
typedef struct {
    float squash;       /* rain: the resting fullness, added */
    float a2;           /* cold: the shiver's jiggle, added to the 2-lobe harmonic */
    int   eye_kind;     /* hot: GEL_EYE_DROOPY for a few seconds after a raise; -1 = leave the draw's */
    float gel_extra;    /* storm: laps added to the gel clock (never rewinds, kept in [0, 1)) */
    float breath_k;     /* hot: the breath's rate factor (1 = rest's own) */
    float heavy;        /* rain / storm: THE SKY'S WEIGHT, 0..1 ramping over SKY_HEAVY_EASE_S (= sky_heavy()) */
} sky_fx_t;

void sky_begin(void);
/* the words from the wire (`ok sky rain cold` -> "rain cold") or by
 * hand; unknown words are refused (false) and nothing changes; "" or
 * "none" forgets. `who` is kept for the status line ("the box", "by hand") */
bool sky_set(const char *words, const char *who);
void sky_forget(void);
/* one call a frame; returns a log line to print (the hold's end), or NULL */
const char *sky_tick(float dt);
/* the body raised to look (the `wrist:raised` sense): cold shivers, hot droops */
void sky_raise(void);
void sky_fx(sky_fx_t *out);
/* THE SKY'S WEIGHT: 0 dry .. 1 heavy (rain / storm held a minute); the
 * factor main.cpp puts on the springs' damping and under the eases */
float sky_heavy(void);
/* the breath followed at the sky's rate: rest's integrated phase in,
 * a phase that advances at breath_k times its rate out (no jump when
 * the rate changes, no jump when the sky is forgotten) */
float sky_breath_phase(float rest_phase);
/* the light: the hour's palette's body, shade and shine multiplied by
 * the sky's factors (the outline and the eye are ink: untouched);
 * identity when no sky is known */
void sky_tint(gel_pal_t *pal);
bool sky_known(void);
/* `hot` is held (A DRINK ON A HOT DAY, gesture.h: main hands it to the
 * detector each frame, and holds the drink's content eyes longer by it) */
bool sky_hot(void);
const char *sky_word(int i);            /* "" past the held ones */
float sky_age_s(void);                  /* seconds since the sky was told; 0 when none */
bool sky_valid_word(const char *w);
void sky_status(char *out, int cap);    /* `sky` over serial */

/* the tunables, for the test and the README */
#define SKY_HOLD_S          3600.0f
#define SKY_RAIN_SQUASH     0.04f
#define SKY_SHIVER_S        0.3f
#define SKY_SHIVER_A2       0.08f
#define SKY_SHIVER_HZ       12.0f
#define SKY_HOT_LID_S       4.0f
#define SKY_HOT_BREATH_K    0.7f
#define SKY_STORM_LAPS_HZ   0.10f       /* laps a second added to the gel clock (GEL_GEL_HZ is 0.12: not quite twice) */
/* THE SKY'S WEIGHT (E38): the one factor -- the springs' damping c 9 -> 11
 * (x 11/9) and the lean's / tilt's eases slowed by the same; the weight
 * ramps 0 -> 1 over a minute when rain or storm arrives, 1 -> 0 over a
 * minute when it goes */
#define SKY_HEAVY_K         (11.0f / 9.0f)
#define SKY_HEAVY_EASE_S    60.0f
static inline float sky_heavy_k(float w) { return 1.0f + (SKY_HEAVY_K - 1.0f) * (w < 0 ? 0 : w > 1 ? 1 : w); }
