/* THE SHAKE-OFF (design/watch-night-2026-10-12.md C16, THE OPERATOR
 * NIGHT): when the wet glass dries and the water lock lets go BY
 * ITSELF (gesture.cpp's GES_UNLOCKED with n 2: the phantom storm's
 * window passed and no drop read for AUTO_QUIET_S -- never the hand's
 * `unlock`, the page's row, the double shake or the 3-s hold; those are
 * a hand, and a hand freeing the glass is nothing to shake off), a
 * creature that was rained on shakes itself off: ONE jiggle of the
 * 2-lobe harmonic at SHAKEOFF_HZ 12 for SHAKEOFF_S 0.4 s, fading out --
 * the cold shiver's own shape and amplitude (rest.cpp's COLD_SHIVER_*,
 * the sky's SKY_SHIVER_*: a2 = A2 sin(2 pi f t) (left / S)) a third
 * longer -- and the SQUINT for SHAKEOFF_SQUINT_S 0.5 s, then the
 * ordinary face. Only on the dried edge; lit, HOLDING, not asleep
 * (a dried edge that comes dark, with the house empty or on a sleeper
 * is missed, not caught up: a moment, not a debt). No sound, no bump,
 * no light, no sense, no new shape: a few hundredths of a2 folded into
 * draw() beside cold_a2, and an eye kind main sets on its own timer.
 *
 * Pure C, no Arduino. tools/shakeoff_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/shakeoff_test tools/shakeoff_test.cpp -x c src/shakeoff.c -lm && /tmp/shakeoff_test */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHAKEOFF_S         0.4f    /* the jiggle, fading out (the cold shiver's 0.3 a third longer: water, not a chill) */
#define SHAKEOFF_HZ        12.0f   /* the cold shiver's own */
#define SHAKEOFF_A2        0.08f   /* the cold shiver's own amplitude (the harmonic's clamp is 0.10) */
#define SHAKEOFF_SQUINT_S  0.5f    /* the SQUINT through it and a hair past, then the ordinary face */

/* the dried edge: true = the shake began (lit, holding, not asleep);
 * false = missed, and nothing plays */
bool  shakeoff_trigger(bool lit, bool holding, bool asleep);
float shakeoff_tick(float dt);        /* one call a frame: advances, returns this frame's a2 (0 = none) */
float shakeoff_a2(void);              /* the last tick's a2, for the fold-in */
bool  shakeoff_on(void);              /* the jiggle runs */
bool  shakeoff_squint(void);          /* the squint's half second runs (main sets the eye kind on its own timer; this is the proof's clock) */
int   shakeoff_count(void);           /* shakes since boot */
int   shakeoff_missed(void);          /* dried edges that came dark / empty / asleep */

#ifdef __cplusplus
}
#endif
