/* THE WISP WATCHED: the gaze on the newest mote, its bow in and its
 * two seconds at the post; the blink on a swell. See wispeye.h. */
#include "wispeye.h"
#include <math.h>

enum { EYE_NONE = 0, EYE_FOLLOW, EYE_DWELL };

static uint32_t watched = 0;     /* the born number of the wisp last watched (a watch begins only for a newer one) */
static int      phase = EYE_NONE;
static float    dwell_t = -1;

void wispeye_gaze(int wx, int wy, float cx, float cy, float phi, int *gx, int *gy)
{
    float g_x = sinf(phi), g_y = cosf(phi);               /* gravity on the glass */
    float dx = (float)wx - cx, dy = (float)wy - cy;
    float right = dx * g_y - dy * g_x;                     /* along his floor, his right */
    float up = -(dx * g_x + dy * g_y);                     /* against gravity */
    *gx = right > WISPEYE_DEAD_PX ? WISPEYE_GX : right < -WISPEYE_DEAD_PX ? -WISPEYE_GX : 0;
    *gy = up > WISPEYE_DEAD_PX ? -WISPEYE_GY : up < -WISPEYE_DEAD_PX ? WISPEYE_GY : 0;
}

static void end_watch(void) { phase = EYE_NONE; dwell_t = -1; }

void wispeye_tick(const wispeye_in_t *in, wispeye_fx_t *fx)
{
    fx->gaze_x = 0; fx->gaze_y = 0; fx->watching = false; fx->began = false; fx->blink = false;
    if (!in->lit || !in->holding) { end_watch(); return; }   /* no face to turn: the dark glass, the empty house */
    /* the blink: a halo swelled (a second notice from the same app) -- one, however many swelled this frame */
    if (in->swelled > 0) fx->blink = true;
    if (!in->have || in->seq == 0) { end_watch(); return; }   /* none to watch (the last one fading, eaten, gone) */
    if (in->seq != watched) {
        if (in->seq < watched) { end_watch(); return; }      /* an older one newest again: it had its look */
        watched = in->seq;                                   /* a newer mote: a watch begins -- through its flight, or at once at its post */
        phase = in->flying ? EYE_FOLLOW : EYE_DWELL;
        dwell_t = in->flying ? -1 : 0;
        fx->began = true;
    }
    if (phase == EYE_FOLLOW && !in->flying) { phase = EYE_DWELL; dwell_t = 0; }   /* it landed: the rest on it begins */
    if (phase == EYE_DWELL) {
        if (dwell_t >= WISPEYE_DWELL_S) { end_watch(); return; }   /* the ordinary face */
        dwell_t += in->dt;
    }
    if (phase == EYE_NONE) return;
    wispeye_gaze(in->wx, in->wy, in->cx, in->cy, in->phi, &fx->gaze_x, &fx->gaze_y);
    fx->watching = true;
}

float    wispeye_dwell(void) { return phase == EYE_DWELL ? dwell_t : -1.0f; }
uint32_t wispeye_watched(void) { return watched; }
void     wispeye_reset(void) { watched = 0; end_watch(); }
