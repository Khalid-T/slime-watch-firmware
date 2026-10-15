/* THE WEIGHTLESS BEAT (design/watch-night-2026-10-12.md C8, the
 * operator night). A bowl in free fall: the wrist flung down, the watch
 * dropped on a bed -- for a beat there is no floor, and what sits in
 * the bowl leaves it. The IMU's whole magnitude |a| UNDER FREEFALL_G
 * for FREEFALL_MS while the glass is lit is that beat: the gel LOFTS
 * off its wall along the normal (the toss's own lift road, FALL0_LIFT_PX
 * = TOSS_LIFT_PX's 8 px, up over FALL0_UP_S), the eyes WIDE, the breath
 * HELD (fall0_breath_k 1 -> 0 with the loft). Then one of two:
 *
 *   IMPACT  |a| over IMPACT_G inside the next IMPACT_WAIT_MS: it hit.
 *           The lift is gone at once and the fall's own splat runs
 *           (LAND_SQUASH easing off over LAND_S, walls.h's numbers:
 *           main adds the landing kick and the squint) -- and THE BRACE's
 *           minute (walls.h BRACE_S: it was dropped, it hugs the floor;
 *           main sets brace_until as fall_splat does, never on the shelf).
 *   CLEAR   no impact inside the second: it eases down over FALL0_EASE_S,
 *           the breath back, the ordinary face. (A hand that caught it;
 *           an impact too soft, or too quick for a frame's one sample.)
 *
 * The gates: lit only -- the dark house polls the IMU every 250 ms, and
 * a drop lasts 300-500 ms: one sample, maybe, under 0.3 g, never 120 ms
 * of them; a drop in the dark cannot be read and is not pretended --
 * never asleep (nobody tosses a sleeper), never on the empty glass (no
 * gel to loft), never inside a fall (the fall is the fall). A hard arm
 * swing reads under 0.3 g for a frame or two: the 120 ms keeps it from
 * lofting, and a loft that did happen ends by the impact-or-ease pair
 * either way, so the worst a swing can do is one loft that eases down.
 * After a CLEAR the magnitude must come back over FREEFALL_G before
 * another loft is read (a long weightlessness is one beat, not many);
 * a gap in the ticks over FALL0_GAP_S (the dark lay between) drops a
 * loft silently. The day log takes no mark; no sense goes up; nothing
 * lights the glass.
 *
 * Pure C, no Arduino: |a| and dt come in with the gates, the lift, the
 * splat's squash, the breath's factor and the eye kind go out;
 * tools/fall0_test.cpp runs it on the laptop. State: a dozen bytes. */
#pragma once
#include <stdbool.h>
#include "walls.h"          /* LAND_SQUASH, LAND_S: the fall's own splat */
#include "play.h"           /* TOSS_LIFT_PX, TOSS_UP_S: the toss's lift road */

#ifdef __cplusplus
extern "C" {
#endif

#define FREEFALL_G       0.30f    /* |a| under this is free fall (a bowl at rest reads 1 g; a hard swing dips under 0.3 for a frame or two) */
#define FREEFALL_MS      120u     /* ... for this long before it is read (five frames at 40 fps; a swing's dip is one or two) */
#define IMPACT_G         1.8f     /* |a| over this inside the second after the loft is the landing */
#define IMPACT_WAIT_MS   1000u    /* the impact is waited for this long; past it the gel eases down instead */
#define FALL0_LIFT_PX    TOSS_LIFT_PX   /* the loft: the toss's 8 px along the wall's normal, inside the disc */
#define FALL0_UP_S       TOSS_UP_S      /* the loft takes this (the toss's own 0.12 s) */
#define FALL0_EASE_S     0.30f    /* no impact: the lift and the breath back over this */
#define FALL0_GAP_S      0.50f    /* a tick this long after the last (the dark lay between) drops a loft silently */

enum { FALL0_EV_NONE = 0, FALL0_EV_LOFT = 1, FALL0_EV_IMPACT = 2, FALL0_EV_CLEAR = 3 };

typedef struct {
    float dt;          /* seconds since the last tick */
    float a_g;         /* the IMU sample's whole magnitude |a|, in g (the raw sample, not the low-passed one) */
    bool  lit;         /* the glass is on (main: state != DARK) */
    bool  holding;     /* the pet is here (house_holding) */
    bool  asleep;      /* rest's asleep */
    bool  falling;     /* main's fall_phase != FALL_NONE */
} fall0_in_t;

void  fall0_begin(void);                    /* a clean state (the statics are zero at boot; the test resets) */
int   fall0_tick(const fall0_in_t *in);     /* once a frame; the frame's event (LOFT / IMPACT / CLEAR) or NONE */
float fall0_lift(void);                     /* px off the wall along its normal (0..FALL0_LIFT_PX; 0 on the floor) */
float fall0_squash(void);                   /* the splat after an impact: LAND_SQUASH easing off over LAND_S; 0 otherwise */
float fall0_breath_k(void);                 /* the breath's factor: 1 on the floor, 0 held in the air, eased with the lift */
int   fall0_eye_kind(void);                 /* GEL_EYE_WIDE while in the air, -1 = the draw's own */
bool  fall0_on(void);                       /* in the air (lofting, or waiting on the impact) */
bool  fall0_easing(void);                   /* easing down after a CLEAR, or splatting after an IMPACT */

#ifdef __cplusplus
}
#endif
