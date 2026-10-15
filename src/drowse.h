/* THE DIM'S DROWSE (design/watch-night-2026-10-12.md C7, the operator
 * night). The lit spell has a DIM stage before the dark (main.cpp:
 * still for 60 s on LONG / 12 s on SHORT, the backlight down to BL_DIM)
 * and until tonight only the light changed: the creature sat in its
 * dusk with the same open face as under the full glass. Now the DIM
 * edge is a LOOK -- the lids go DROOPY, the stance slumps a hair
 * (DROWSE_SQUASH), the breath eases a notch toward the resting rate
 * (DROWSE_BREATH_K: a quick breath comes down toward it, a resting one
 * a notch under it, a slower one is left alone) -- settling over
 * DROWSE_IN_S. A touch or a raise (the only things that end the dim on
 * a lit glass) brings the lids up AT ONCE (DROWSE_EV_UP: main gives
 * the pet's own engage, the open eyes held and then melting as after a
 * pet), the slump and the breath back over DROWSE_OUT_S. The dark takes
 * it silently: no event, nothing left. Refused asleep or stirring (THE
 * NIGHT LIGHT owns that look), on the empty glass (no face to droop),
 * over a page (his tool page is not the pet's dusk); a page or the
 * pet's leaving mid-drowse ends it with no event.
 *
 * A look, never Zs (ruling 123): the module has no Z in it; on the
 * LONG preset the dim lasts 30 s and the lids stay DROOPY the whole of
 * it, the same face at the 30th second as at the 2nd.
 *
 * Pure C, no Arduino: the frame's facts come in, a few numbers go out;
 * tools/drowse_test.cpp runs the edges on the laptop. State: a handful
 * of floats. The breath's phase goes through drowse_breath_phase the
 * way it goes through the sky's hot breath (sky_breath_phase): the
 * delta integrated under the factor, no jump at either edge. */
#pragma once
#include <stdbool.h>
#include "gel.h"            /* GEL_EYE_DROOPY */

#ifdef __cplusplus
extern "C" {
#endif

#define DROWSE_SQUASH    0.03f    /* the slump at the full drowse, added to the stance */
#define DROWSE_BREATH_K  0.85f    /* the drowsy rate: this x the current rate, never under this x the resting rate, never quicker */
#define DROWSE_IN_S      2.0f     /* the lids and the slump settle over this */
#define DROWSE_OUT_S     0.4f     /* the slump and the breath back over this (the lids are up at once) */

/* what main hands over each frame */
typedef struct {
    float dt;               /* seconds since the last tick */
    bool  lit;              /* the glass is on (awake or dim) */
    bool  dim;              /* the lit spell's DIM stage */
    bool  touch;            /* a finger on the glass */
    bool  holding;          /* the pet is here (house_holding) */
    bool  dozing;           /* rest's asleep or stirring */
    bool  page;             /* the watch page is up */
    float breath_hz;        /* rest's rate this frame ... */
    float breath_rest_hz;   /* ... and its resting rate (the grain's), for the notch */
} drowse_in_t;

/* what draw() folds in */
typedef struct {
    int   eye_kind;         /* GEL_EYE_DROOPY while drowsing, -1 = leave the draw's */
    float eye_alpha;        /* the lids' presence 0..1 (the drowse brings the eyes in to droop them; 0 when not drowsing) */
    float squash;           /* the slump, added */
    float breath_k;         /* the breath's rate factor this frame, 1 = untouched */
    float level;            /* 0..1 the drowse's depth (for `drowse` and the test) */
} drowse_fx_t;

enum { DROWSE_EV_NONE = 0, DROWSE_EV_DOWN = 1, DROWSE_EV_UP = 2 };

void  drowse_begin(void);                        /* a clean state (the statics are zero at boot; the test resets) */
int   drowse_tick(const drowse_in_t *in);        /* once a frame; the frame's event: DOWN at the dim edge, UP for the touch / raise that ends it, NONE otherwise (the dark, a page, the pet gone) */
void  drowse_dark(void);                         /* the glass went dark (main's go_dark): taken silently, nothing left -- no frame runs between the dark and the wake, so the dark is told by hand */
void  drowse_fx(drowse_fx_t *out);               /* this frame's look */
float drowse_breath_phase(float phase_in);       /* the breath's phase through the notch: the delta integrated under breath_k, no jump at either edge */
bool  drowse_on(void);                           /* drowsing now */

#ifdef __cplusplus
}
#endif
