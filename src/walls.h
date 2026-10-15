/* THE FOUR WALLS: the tunables of where the gel sits and how it falls,
 * in ONE place -- main.cpp (the body), tools/preview.c (the laptop's
 * frames) and the desk's watchface.py / test_watch.py (which reads the
 * numbers off this file) all take them from here. Plain C, no Arduino.
 *
 * Khalid, 2026-09-10 17:10, reopening the continuous bowl of 09-09:
 * "I don't like how the slime shape is when it's between angles like
 * top-left and so on" -- "is the rotation nice? there must be a better
 * way for the watch and the slime without using too much resources,
 * keeping it lean." -> "go with the four walls".
 *
 * Pixel art does not rotate: at 15 / 30 / 45 deg the outline goes
 * jagged and the sprite stops being the sprite. So the gel sits at one
 * of FOUR rim places only -- 6, 3, 12, 9 o'clock, THE WALL -- and the
 * drawn angle AND the place on the rim are that wall's: the raster's
 * sin / cos snap to 0 / 1 at the quarters, so every frame is the quarter
 * blit, crisp, the base bent to the bowl at that wall exactly as before.
 *
 * Gravity's angle past the wall's is THE RESIDUAL, and the residual is
 * WORN, never rotated: the lean (a1, the old term, capped), a SLOSH of
 * the mass to the low shoulder (a2 from the residual's sine, eased) and
 * a SLIDE of a few px along the wall (the compose's slide, the same
 * sine). A wall is held until the pull passes the diagonal by
 * WALL_HYST_DEG and stays there WALL_HOLD_S; then the wall CHANGES, and
 * every change is THE FALL that already existed: PEEL, the SWAP on the
 * puddle's centroid, FLIGHT under FALL_G along an arc to the new wall
 * (a short arc across the face for a quarter turn, through the centre
 * for a half), IMPACT, SPLAT, the SLUMP (the lean and the slosh easing
 * in at the new wall). No slow roll round the bowl: `BOWL_MAX_DPS`,
 * `SLIDE_EASE`, the flip test (`FALL_FLIP_*`, `FALL_GAP_DEG`, the ring)
 * are gone -- the hysteresis and the hold are the whole rule.
 */
#ifndef WALLS_H
#define WALLS_H
#include <math.h>

/* level, and home */
#define FLAT_G        0.20f    /* in-plane g below which the face is level (no pull along the glass) */
#define FLAT_HOLD_S   1.0f     /* level this long and the pull is HOME -- the bottom wall (a watch lying on the desk shows him at 6 o'clock; from another wall that is a wall change, the fall home) */
/* the residual, worn */
#define LEAN_G        0.30f    /* a1 per g along the wall (the pull's part along the floor), toward the low side */
#define LEAN_CAP      0.22f
#define LEAN_EASE     6.0f     /* 1/s: the lean's ease toward its target */
#define TILT_EASE     4.0f     /* 1/s: the slosh and the slide follow the residual's sine at this ease (the old SLIDE_EASE's number: the same spring feel) x the grain's slide knob */
#define SLOSH_A2      0.08f    /* a2 per unit of the residual's sine: the mass to the low shoulder (negative a2 bulks the right shoulder) */
#define SLIDE_PX      8.0f     /* face px along the wall per unit of the residual's sine (LEAN_PX x 2); at the 60 deg edge ~7 px, the base's corners still on the bowl's curve within a block */
#define LEAN_PX       4.0f     /* the small lateral offset per unit sway (the springs' part) */
/* the wall change */
#define WALL_HYST_DEG 15.0f    /* the pull must pass the diagonal by this much: 45 + 15 = 60 deg from the wall it sits on ... */
#define WALL_HOLD_S   0.35f    /* ... and stay past it this long (a fidget through the diagonal is not a turn) before the wall changes */
/* the fall (the theatre of 09-09, unchanged but for the arc) */
#define FALL_G        900.0f   /* face px / s^2 */
#define PEEL_S        0.24f
#define PEEL_SQUASH   0.50f    /* was 0.62: at 0.62 the leaning puddle was wider than the glass (its flank 125 px out, the rim at 120); 0.50 (a total of 0.35 under the wrist's posture) is the widest the disc holds at any lean -- THE DISC IS THE GLASS */
#define PEEL_LEAN     0.30f    /* a1 toward the pull while it peels */
#define PEEL_CREEP_DEG 4.0f    /* how far along the wall the puddle creeps toward the pull (degrees of the rim, as px along the wall: x GEL_FLOOR_DROP = 7 px; was 8 = 15 px, more than the glass leaves beside a puddle) */
#define DROP_SQUASH   -0.50f
#define FLIGHT_SQUASH -0.22f
#define IMPACT_S      0.12f    /* the arrival: long against the glass, compressing into the splat */
#define IMPACT_SQUASH -0.25f
#define LAND_S        0.38f
#define LAND_SQUASH   0.48f
/* THE BRACED FALL (design/watch-ideas.md A7, 2026-09-23): for BRACE_S
 * after a fall THROUGH THE CENTRE (a half turn: the drop straight
 * through the middle of the glass, never a quarter turn's short arc)
 * the gel is warier -- the splat's squint held, the lean's cap halved
 * (LEAN_CAP x BRACE_LEAN_K), the slosh damped (SLOSH_A2 x BRACE_SLOSH_K):
 * it lands, then hugs the floor. main.cpp keeps the clock (a wall-clock
 * minute, the nap included); never on the shelf. */
#define BRACE_S       60.0f
#define BRACE_LEAN_K  0.5f
#define BRACE_SLOSH_K 0.5f

/* the wall's angle on the face (rad from straight down, + toward the
 * right edge): 0 bottom, pi/2 right, pi top, -pi/2 left */
static inline float walls_phi(int wall)
{
    static const float q[4] = { 0.0f, (float)M_PI / 2.0f, (float)M_PI, -(float)M_PI / 2.0f };
    return q[wall & 3];
}

/* what the residual settles to at rest: the lean (a1), the slosh (a2)
 * and the slide (face px along the wall, + toward the body's right) for
 * a pull ``res`` rad past the wall with ``g`` of in-plane gravity --
 * the targets main.cpp eases toward, and the preview's resting frames */
static inline void walls_rest(float res, float g, float *a1, float *a2, float *slide_px)
{
    float k = sinf(res);                       /* the pull's part along the wall, per g */
    float lean = g * k * LEAN_G;
    if (lean > LEAN_CAP) lean = LEAN_CAP;
    if (lean < -LEAN_CAP) lean = -LEAN_CAP;
    *a1 = lean;
    *a2 = -SLOSH_A2 * k;
    *slide_px = SLIDE_PX * k;
}

#endif
