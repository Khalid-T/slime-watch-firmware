/* THE EAR TO THE WRIST (design/watch-night-2026-10-12.md C19, THE
 * OPERATOR NIGHT): while the creature's OWN pulse read runs -- THE PULSE
 * IN THE DAY's 10 s spot, every five minutes at rest, never his asked
 * reads (the PULSE row's, the console's: those are the page's and get
 * no face) -- and the LEDs glow under the glass, the gel presses its ear
 * to him: the gaze DOWN toward the strap (LISTEN_GY, the lease's glance
 * px the other way), CONTENT eyes, for as long as the LEDs are on. A
 * clean read (one that landed in the ring) ends with one small settle
 * (idle's own: squash +LISTEN_SETTLE_SQUASH on a half sine over
 * LISTEN_SETTLE_S); a failed one (moved, nothing on the sensor, or
 * given way to his ask) with nothing -- the ear lifts, the ordinary
 * face. The LEDs' glow gets a meaning: it is listening.
 *
 * The look is only ever on the lit glass, HOLDING, no page, the body
 * free (a pet's face wins: the look pauses under it and comes back);
 * the read itself runs on in the dark as before, unlooked. The verdict
 * is the ring's: the result count (ppg_recent_seq) taken at the read's
 * start, compared at its end -- a push is a clean read. No number
 * anywhere; no bump; no light; no new shape -- one gaze px, an eye
 * kind and a few hundredths of squash, the draw's own deltas.
 *
 * Pure C, no Arduino: the frame's facts in, the deltas out.
 * tools/listen_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/listen_test tools/listen_test.cpp -x c src/listen.c -lm && /tmp/listen_test */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "gel.h"            /* GEL_EYE_CONTENT */

#ifdef __cplusplus
extern "C" {
#endif

#define LISTEN_GY             1       /* down, toward the strap (the lease's glance-up px, the other way) */
#define LISTEN_SETTLE_SQUASH  0.06f   /* the settle's depth after a clean read (idle's SETTLE_SQUASH, the stare's) */
#define LISTEN_SETTLE_S       1.5f    /* ... over a half sine (idle's SETTLE_S) */

typedef struct {
    float    dt;
    bool     reading;     /* the creature's OWN read is taken (waiting for the wrist, the agc choosing, or reading): ppg_own_left() */
    bool     glow;        /* ... and its LEDs are on (the read proper or the agc; dark while it waits for the wrist) */
    uint32_t seq;         /* ppg_recent_seq(): the ring's push count -- a change over the read is a clean one */
    bool     lit;         /* the glass is on */
    bool     holding;     /* house_holding(): a gel to look with */
    bool     busy;        /* idle's busy (a pet, a fall, a gesture face, the dim ...): the look pauses, the settle ends */
    bool     page;        /* THE WATCH PAGE up: no gel to look */
} listen_in_t;

typedef struct {
    int   gaze_y;         /* LISTEN_GY through the look, else 0 (added to the draw's gaze) */
    int   eye_kind;       /* GEL_EYE_CONTENT through the look, else -1 (leave the draw's) */
    float squash;         /* the settle's half sine, added */
} listen_fx_t;

enum {
    LISTEN_EV_NONE = 0,
    LISTEN_EV_EAR,        /* the look began (once per read: the first frame it shows) */
    LISTEN_EV_CLEAN,      /* the read ended clean (the settle begins if the look was showing) */
    LISTEN_EV_FAILED,     /* the read ended without a result: nothing plays */
};

int   listen_tick(const listen_in_t *in);    /* one call a frame; the event */
void  listen_fx(listen_fx_t *out);           /* what the draw folds in this frame */
bool  listen_on(void);                       /* the look is showing this frame (the breath may read it one day) */
bool  listen_settling(void);                 /* the settle in progress */
float listen_ear_s(void);                    /* seconds of this read's look so far (0 outside a read) */
bool  listen_heard(void);                    /* this read's look was shown at all (kept past its end until the next read: the end's log line is only for a read he saw) */
int   listen_clean(void);                    /* reads ended clean since boot (seen or not) */
int   listen_failed(void);                   /* reads ended without a result since boot */

#ifdef __cplusplus
}
#endif
