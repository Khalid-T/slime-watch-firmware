/* THE DRUMMED TABLE (design/watch-night-2026-10-12.md C18, THE OPERATOR
 * NIGHT). At the desk the wrist carries his typing as a fine jitter --
 * DRUM_LO_G..DRUM_HI_G (0.02-0.08 g), DRUM_MIN_HZ..DRUM_MAX_HZ (4-10)
 * reversals a second -- that is neither a walk (its high-passed peaks
 * are 0.2-0.4 g, two or so strides a second) nor a jolt (one loud
 * sample, then nothing). A bowl on a drummed table ripples: while the
 * drumming lasts the gel clock runs FASTER -- the bubble's own road
 * (idle's gel_extra: laps added to the clock, never rewound), at the
 * bubble's own pace (DRUM_X 3 laps per resting lap, idle.cpp's
 * BUBBLE_X) scaled by the drumming's level 0..1 -- and nothing else.
 * No eye, no squash, no lean; never a sense, never a mark in the day
 * log, nothing on the wire, nothing lights the glass. It ends with the
 * typing.
 *
 * The read: sense()'s jerk (the raw sample less the low-passed one),
 * its three parts. Per axis a sign with a dead band: a part over
 * +DRUM_LO_G after one under -DRUM_LO_G (or the other way) is a
 * REVERSAL -- the jitter under the floor counts none, so a still
 * bowl's noise is silence. Each axis keeps a leaky rate (reversals a
 * second, time constant DRUM_TAU_S); the band is judged on the
 * busiest axis (typing shakes the wrist one way more than the others,
 * and three axes' crossings added would read a 5 Hz jitter as 30).
 * A sample with any part over DRUM_HI_G is LOUD (a walk's peaks, a
 * jolt, a flick): the frame is out of the band and so are the next
 * DRUM_LOUD_S (the ring after a bump is not typing).
 *
 * The dwell: the band held for DRUM_ON_S 3 s NET (in-band time less
 * out-of-band time: a pause between words costs its own time and no
 * more) starts the drumming (a few keys are not a drum roll); out of
 * the band for DRUM_OFF_S 2 s net ends it (a pause for thought is not
 * the end of the typing; the rate's own fall after the last key is
 * part of the 2 s). The
 * level eases toward 1 / 0 over DRUM_EASE_S, so the ripple quickens
 * and slows instead of switching.
 *
 * The gates: lit (the dark's 250 ms poll cannot read a 5 Hz jitter),
 * HOLDING (no gel, no ripple), never asleep (nothing shakes a
 * sleeper's clock), never walking (the day log's own verdict; the
 * loud line would catch it too), never inside a fall or THE
 * WEIGHTLESS BEAT (the clock is the fall's). A gate shut, or a tick
 * gap over DRUM_GAP_S, drops the read and lets the level ease out;
 * the laps already added stay (never rewound).
 *
 * Pure C, no Arduino: the jerk's three parts and dt come in with the
 * gates, the level and the clock's extra laps go out;
 * tools/drum_test.cpp runs it on the laptop. State: ~40 bytes. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRUM_LO_G        0.02f    /* the jitter band's floor: a part must pass this line on both sides for a reversal (the IMU's still noise is under it; main's MOTION_G is 0.05) */
#define DRUM_HI_G        0.08f    /* the band's ceiling: a part over it is LOUD (a walk's high-passed peaks are 0.2-0.4, the toss's line 0.6, a knock's 0.5; the springs' jolt is 0.06, inside the band) */
#define DRUM_MIN_HZ      4.0f     /* the busiest axis's reversals a second, at least this (a slow fidget, a breath's sway: fewer) */
#define DRUM_MAX_HZ      10.0f    /* ... and at most this (a buzz, an engine's hum under the wrist: more; the IMU is read at 40 fps, so 20 is the most it could see) */
#define DRUM_TAU_S       0.50f    /* the leaky rate's time constant: half a second of typing fills it, half a second of quiet empties it */
#define DRUM_SMOOTH_S    0.30f    /* the busiest axis's rate smoothed once more over this (the leaky rate's own ripple at the band's floor) */
#define DRUM_LOUD_S      0.50f    /* after a LOUD sample the band stays shut this long: a bump's ring is not typing */
#define DRUM_ON_S        3.0f     /* the band held this long starts the drumming (a few keys are not a drum roll) */
#define DRUM_OFF_S       2.0f     /* out of the band this long ends it (a pause for thought is not the end) */
#define DRUM_EASE_S      0.80f    /* the level's ease toward 1 / 0 (the ripple quickens, it does not switch) */
#define DRUM_X           3.0f     /* the gel clock's pace at a full drumming, in laps per resting lap: the bubble's own (idle.cpp BUBBLE_X) */
#define DRUM_GAP_S       0.50f    /* a tick this long after the last (the dark lay between) drops the read silently */

enum { DRUM_EV_NONE = 0, DRUM_EV_BEGIN = 1, DRUM_EV_END = 2 };

typedef struct {
    float dt;          /* seconds since the last tick */
    float jx, jy, jz;  /* sense()'s jerk this frame, in g: the raw sample less the low-passed one; x, y in the glass's plane, z its normal */
    bool  lit;         /* the glass is on (main: state != DARK) */
    bool  holding;     /* the pet is here (house_holding) */
    bool  asleep;      /* rest's asleep */
    bool  walking;     /* the day log's wrist_walking */
    bool  falling;     /* main's fall_phase != FALL_NONE, or THE WEIGHTLESS BEAT in the air or landing */
} drum_in_t;

void  drum_begin(void);                     /* a clean state (the statics are zero at boot; the test resets) */
int   drum_tick(const drum_in_t *in);       /* once a frame; the frame's event (BEGIN / END) or NONE */
float drum_level(void);                     /* the drumming 0..1 (eased; 0 when none) */
float drum_extra(void);                     /* laps added to the gel clock so far, wrapped to 0..1 (the bubble's gel_extra road); never rewinds */
bool  drum_on(void);                        /* the drumming is on (the dwell said so; the level may still be easing) */
float drum_rate_hz(void);                   /* the busiest axis's reversal rate, for the log and the test */

#ifdef __cplusplus
}
#endif
