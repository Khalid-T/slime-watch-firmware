/* THE WISP WATCHED (design/watch-night-2026-10-12.md C3, THE OPERATOR
 * NIGHT): a mote flies in through the air door and bobs beside him, and
 * until tonight the gel never looked at it. Now the gaze follows the
 * NEWEST wisp through its bow in (every frame, the mote's own point
 * read in the gel's frame the way the far tap reads a knock: along his
 * floor +-WISPEYE_GX past the dead band, across it +-WISPEYE_GY),
 * rests on it at its post for WISPEYE_DWELL_S, then the ordinary face
 * -- a snap back, the far tap's. A swelling halo (a second notice from
 * an app already hovering) gets ONE blink (main closes the eyes for
 * WISPEYE_BLINK_S, idle's own blink). Three wisps: the newest is
 * watched; an older one that becomes the newest again (the newer was
 * cleared at its source) is NOT watched twice -- a watch begins only
 * for a wisp born after the last one watched. The glance that sends a
 * wisp off is untouched: a fading mote is nobody's to watch, and the
 * watch ends with it.
 *
 * What it never does: a bump, a sense, a log in the day; nothing on
 * the empty glass or the dark one (no face to turn); nothing for a
 * wisp at the door (nobody home) or one being eaten (the feed's own
 * happy eyes). It only turns the pupils; the lids are whoever's they
 * were (the brace's squint, the drowse's droop ride over it).
 *
 * Pure C, no Arduino: the frame's facts in (wisp.cpp's wisp_eye_point /
 * wisp_eye_seq / wisp_take_swelled, the gel's centre as main's look_at
 * finds it), a gaze and a blink out. State: a born number, a phase and
 * a clock. tools/wispeye_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/wispeye_test tools/wispeye_test.cpp -x c src/wispeye.c -lm && /tmp/wispeye_test */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WISPEYE_DWELL_S   2.0f    /* the rest on it at its post, then the ordinary face */
#define WISPEYE_DEAD_PX   20.0f   /* the far tap's dead band (main's FAR_TAP_DEAD_PX): inside it no turn on that axis */
#define WISPEYE_GX        2       /* along his floor, the far tap's own px */
#define WISPEYE_GY        1       /* across it (negative = up, the glance's own sign) */
#define WISPEYE_BLINK_S   0.12f   /* the swell's blink: idle's BLINK_S */
#define WISPEYE_EYES_S    3.5f    /* main holds the eyes up this long from the watch's start: the flight (FLY_S 1.4) and the dwell */

/* what main hands over each frame */
typedef struct {
    float    dt;
    bool     lit;           /* the glass is on */
    bool     holding;       /* the pet is here (house_holding) */
    bool     have;          /* wisp_eye_point said true ... */
    int      wx, wy;        /* ... the newest mote's glass point (the bob aside) */
    bool     flying;        /* ... still on its bow in (wisp_eye_seq) */
    uint32_t seq;           /* ... its born number (0 = none) */
    float    cx, cy;        /* the gel's centre on the glass (look_at's) */
    float    phi;           /* the wall's angle (bowl_phi) */
    int      swelled;       /* halos that swelled this frame (wisp_take_swelled) */
} wispeye_in_t;

/* what draw() folds in beside the far tap's look */
typedef struct {
    int  gaze_x, gaze_y;    /* the look toward the mote, 0,0 when none */
    bool watching;          /* a watch runs (the flight or the dwell) */
    bool began;             /* a watch began this frame (main holds the eyes up, logs once) */
    bool blink;             /* a halo swelled this frame: one blink (main closes the eyes for WISPEYE_BLINK_S) */
} wispeye_fx_t;

/* the point -> the gaze in his frame: along his floor (cos phi, -sin phi)
 * is his right, against gravity (sin phi, cos phi) is up; each axis past
 * the dead band turns the pupils that way */
void wispeye_gaze(int wx, int wy, float cx, float cy, float phi, int *gx, int *gy);

/* one call a frame after wisp_tick */
void wispeye_tick(const wispeye_in_t *in, wispeye_fx_t *fx);

float    wispeye_dwell(void);     /* seconds into the dwell at the post, -1 = not dwelling */
uint32_t wispeye_watched(void);   /* the born number of the last wisp watched (0 = none yet) */
void     wispeye_reset(void);     /* the test's, and a boot's */

#ifdef __cplusplus
}
#endif
