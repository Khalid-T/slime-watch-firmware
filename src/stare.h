/* THE STARE RETURNED (design/watch-night-2026-10-12.md C12, THE
 * OPERATOR NIGHT): the glass held toward the eyes well past the hour's
 * six seconds with no touch is him looking at it. After STARE_S of lit,
 * raised, still, untouched glass -- HOLDING, the body free (no touch,
 * no pet, no fall, no page, no word, no costume ...), the wrist still
 * -- once per lit spell: the gaze up (STARE_GY, idle's own glance-up)
 * with CONTENT eyes for STARE_LOOK_S and one soft settle (idle's own
 * settle: squash +STARE_SETTLE_SQUASH on a half sine over
 * STARE_SETTLE_S). It noticed being watched.
 *
 * Deterministic where idle's GLANCE UP is a die. The two glances never
 * stack: the stare waits while idle plays a thing (in->idle_playing),
 * its clock held at the mark, and fires the frame idle is done; after
 * it STARE_QUIET_S of quiet (main hands idle the same quiet it gives a
 * pet: idle_pet(), PET_QUIET_S 10). The clock is continuous: a touch, a
 * move, a page, a word, anything taking the body puts it back to zero;
 * the same anything mid-look ends the look. Never on the empty glass,
 * never in the dark; no bump (THE BID is the cramp's), no sense, no
 * light, no new shape -- one gaze px, an eye kind and a few hundredths
 * of squash, the draw's own deltas.
 *
 * Pure C, no Arduino: the frame's facts in, the deltas out.
 * tools/stare_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/stare_test tools/stare_test.cpp -x c src/stare.c -lm && /tmp/stare_test
 * (clockwork.cpp's STARE_S 60 is THE LONG STARE's, static inside it; no
 * collision -- main.cpp sees only this one.) */
#pragma once
#include <stdbool.h>
#include "gel.h"            /* GEL_EYE_CONTENT */

#ifdef __cplusplus
extern "C" {
#endif

#define STARE_S              8.0f    /* lit, raised, still, untouched this long = he is looking at it (past the hour's 6 s) */
#define STARE_LOOK_S         1.5f    /* the glance up, held (idle's GLANCE_S) */
#define STARE_GY             -1      /* up, the glance's own px */
#define STARE_SETTLE_SQUASH  0.06f   /* the soft settle's depth (idle's SETTLE_SQUASH) */
#define STARE_SETTLE_S       1.5f    /* ... over a half sine (idle's SETTLE_S) */
#define STARE_QUIET_S        10.0f   /* the quiet after it (idle's PET_QUIET_S, handed over by main) */
#define STARE_FACE           0.35f   /* "raised": gravity out of the glass this much or more (~70 deg toward the eyes: squarely at him, not the raise's 0.15 glance) */

typedef struct {
    float dt;
    bool  lit;            /* the glass is on: the spell (the rising edge owes the look again) */
    bool  holding;        /* house_holding(): a gel to look at all */
    bool  raised;         /* the face toward the eyes (the raise's own face threshold on the pose) */
    bool  still;          /* the gel at rest and the wrist still (idle's !moving) */
    bool  touched;        /* a finger on the glass, or one just lifted: the clock back to zero */
    bool  busy;           /* idle's busy: something else has the body (a costume, a fall, a gesture face ...) */
    bool  page;           /* THE WATCH PAGE up: no gel to look */
    bool  word;           /* the box's word showing */
    bool  idle_playing;   /* idle_playing(): a thing of idle's in progress -- the stare waits for it */
} stare_in_t;

typedef struct {
    int   gaze_y;         /* STARE_GY through the look, else 0 (added to the draw's gaze) */
    int   eye_kind;       /* GEL_EYE_CONTENT through the look, else -1 (leave the draw's) */
    float squash;         /* the settle's half sine, added */
} stare_fx_t;

enum { STARE_EV_NONE = 0, STARE_EV_LOOK };   /* the look began this frame */

int   stare_tick(const stare_in_t *in);      /* one call a frame; the event */
void  stare_fx(stare_fx_t *out);             /* what the draw folds in this frame */
bool  stare_playing(void);                   /* the look in progress */
bool  stare_spent(void);                     /* this spell's look played (or no spell) */
float stare_held_s(void);                    /* the clock: seconds of lit, raised, still, untouched glass so far */
float stare_quiet_s(void);                   /* seconds of the quiet after the look left, 0 = none */
int   stare_count(void);                     /* looks since boot */

#ifdef __cplusplus
}
#endif
