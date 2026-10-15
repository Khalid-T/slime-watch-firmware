/* THE KNOCK FROM BEHIND (design/watch-night-2026-10-12.md C13, the
 * operator night). A rap on the back of the glass, or on the table it
 * rests on, is a jerk along the NORMAL -- the one direction a finger
 * on the glass does not take (a tap is a touch) and a flick does not
 * take (THE TOSS is in the plane). Until tonight it only kicked the
 * wobble spring. Someone knocked on the bowl: the eyes go WIDE for
 * KNOCK_WIDE_S, then the ordinary face; a second knock inside
 * KNOCK_AGAIN_S of the last gets the SQUINT for KNOCK_SQUINT_S -- a
 * calm "I heard you", never a sulk (every knock after the first inside
 * the window is the squint again).
 *
 * The read: the raw sample less the low-passed one (sense()'s jerk,
 * in g), its part along the normal |jz| OVER KNOCK_G while the in-plane
 * part sqrt(jx^2 + jy^2) stays UNDER KNOCK_PLANE_K of it (within ~22
 * deg of the normal: a shove or a flick leans into the plane and is
 * not a knock), for an excursion no longer than KNOCK_SPAN_MS (one
 * frame at 40 fps: a knuckle's rap rings for ~10 ms and lands in one
 * sample; a push holds for several and is not a knock). It is read the
 * frame the excursion ends, so every frame of it was judged. One rap
 * is one knock: KNOCK_DEAD_MS after a read the ring's bounce is not a
 * second.
 *
 * The gates: no touch down (a finger on the glass is the gesture's,
 * and its own tap would read along the normal), lit only (the dark's
 * 250 ms poll cannot read a 10 ms rap; the wake's dark "bump" verdict
 * is the nap's own), HOLDING, never asleep (the shaken sleeper is
 * rest's), never inside a fall or THE WEIGHTLESS BEAT (the impact is
 * a jerk along the normal too, and it is the drop's). A gate shut on
 * any frame of the excursion, or a tick gap over KNOCK_GAP_S, drops
 * it silently. Nothing on the wire, no bump, no mark in the day log,
 * nothing lights the glass, no new shape: two eye kinds that exist.
 *
 * Pure C, no Arduino: the jerk's three parts and dt come in with the
 * gates, the event and the eye kind go out; tools/knock_test.cpp runs
 * it on the laptop. State: a dozen bytes. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KNOCK_G          0.50f    /* the jerk's part along the normal over this is a rap (the toss's in-plane line is 0.6, the jolt's 0.06; a walk's high-passed peaks are 0.2-0.4 and mostly in the plane) */
#define KNOCK_PLANE_K    0.40f    /* the in-plane part under this much of the normal part (within ~22 deg of the normal): a shove or a flick leans into the plane */
#define KNOCK_SPAN_MS    40u      /* the excursion over KNOCK_G no longer than this (one frame at 40 fps; two frames are 50 and a push) */
#define KNOCK_DEAD_MS    150u     /* after a read, the ring's bounce inside this is the same rap, not a second */
#define KNOCK_WIDE_S     0.40f    /* the first knock: the eyes WIDE this long, then the ordinary face */
#define KNOCK_AGAIN_S    2.0f     /* a knock inside this of the last is the second: the squint */
#define KNOCK_SQUINT_S   0.60f    /* the second knock: the SQUINT this long (the fall's is 0.45, the toss's 1.0: a calm "I heard you") */
#define KNOCK_GAP_S      0.50f    /* a tick this long after the last (the dark lay between) drops an excursion silently */

enum { KNOCK_EV_NONE = 0, KNOCK_EV_FIRST = 1, KNOCK_EV_AGAIN = 2 };

typedef struct {
    float dt;          /* seconds since the last tick */
    float jx, jy, jz;  /* sense()'s jerk this frame, in g: the raw sample less the low-passed one; x, y in the glass's plane, z its normal */
    bool  lit;         /* the glass is on (main: state != DARK) */
    bool  holding;     /* the pet is here (house_holding) */
    bool  asleep;      /* rest's asleep */
    bool  falling;     /* main's fall_phase != FALL_NONE */
    bool  weightless;  /* THE WEIGHTLESS BEAT in the air or landing (fall0_on || fall0_easing) */
    bool  touched;     /* a touch down, one lifted inside the touch's hold, or a touch that cannot be vouched for (the water lock, a phantom storm) */
} knock_in_t;

void  knock_begin(void);                    /* a clean state (the statics are zero at boot; the test resets) */
int   knock_tick(const knock_in_t *in);     /* once a frame; the frame's event (FIRST / AGAIN) or NONE */
int   knock_eye_kind(void);                 /* GEL_EYE_WIDE through the first's KNOCK_WIDE_S, GEL_EYE_SQUINT through the second's KNOCK_SQUINT_S, -1 = the draw's own */
float knock_since_s(void);                  /* seconds since the last knock read (a large number when none) */

#ifdef __cplusplus
}
#endif
