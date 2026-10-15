/* THE KNOCK FROM BEHIND -- knock.h says why; this is the small machine.
 *
 * Two states. QUIET: a frame whose normal part is over KNOCK_G opens an
 * excursion (OVER) with its dt as the span and `clean` = that frame
 * was along the normal. OVER: every further frame over KNOCK_G adds
 * its dt and may spoil `clean` (a plane part too large) or the span
 * (past KNOCK_SPAN_MS: a push); the first frame back under KNOCK_G is
 * the read -- a clean, short excursion with the gates open on every
 * frame of it is the knock: FIRST, or AGAIN when the last knock was
 * inside KNOCK_AGAIN_S. The eyes' clock runs from the read; the dead
 * clock keeps the ring's bounce from being a second knock. A gate shut
 * or a tick gap mid-excursion drops it silently. */
#include "knock.h"
#include "gel.h"            /* GEL_EYE_WIDE, GEL_EYE_SQUINT */
#include <math.h>
#include <string.h>

enum { S_QUIET = 0, S_OVER = 1 };

static struct {
    unsigned char state;    /* S_* */
    bool  clean;            /* every frame of the excursion was along the normal, the gates open */
    float span_s;           /* the excursion's length so far */
    float dead_s;           /* seconds left in which an excursion is the last rap's bounce */
    float since_s;          /* seconds since the last knock read (capped) */
    float eye_s;            /* seconds left of the eye kind; <= 0 none */
    unsigned char eye;      /* the eye kind while eye_s > 0 */
} K;

#define SINCE_CAP   1000.0f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void knock_begin(void)
{
    memset(&K, 0, sizeof K);
    K.since_s = SINCE_CAP;
}

static void drop(void) { K.state = S_QUIET; K.span_s = 0; K.clean = false; }

int knock_tick(const knock_in_t *in)
{
    if (!in) return KNOCK_EV_NONE;
    float dt = clampf(in->dt, 0.0f, 0.25f);
    if (in->touched) K.eye_s = 0;   /* the review: a rap then a tap -- the pet's face wins, the knock's eyes do not outlive the touch */
    bool open = in->lit && in->holding && !in->asleep && !in->falling && !in->weightless && !in->touched;
    int ev = KNOCK_EV_NONE;

    /* the clocks run whatever the gates say (a look already given plays out; the window and the dead time are time) */
    if (K.since_s < SINCE_CAP) K.since_s = clampf(K.since_s + dt, 0.0f, SINCE_CAP);
    if (K.dead_s > 0) K.dead_s -= dt;
    if (K.eye_s > 0) K.eye_s -= dt;

    if (!open || in->dt > KNOCK_GAP_S) {        /* a gate shut, or the dark lay between two ticks: nothing in progress, nothing said */
        drop();
        return KNOCK_EV_NONE;
    }

    float n = fabsf(in->jz);
    float p = sqrtf(in->jx * in->jx + in->jy * in->jy);
    bool over = n > KNOCK_G;
    bool along = over && p < KNOCK_PLANE_K * n;

    switch (K.state) {
    case S_QUIET:
        if (over) {
            if (K.dead_s > 0) break;            /* the last rap's bounce: the same knock */
            K.state = S_OVER; K.span_s = dt; K.clean = along;
        }
        break;
    case S_OVER:
        if (over) {                             /* still over: the span grows, a plane part spoils it */
            K.span_s += dt;
            if (!along) K.clean = false;
            if (K.span_s * 1000.0f + 0.5f > (float)KNOCK_SPAN_MS) K.clean = false;   /* a push, not a rap (half a ms of grace: the frames' sum against a round number) */
            break;
        }
        /* back under: the read */
        if (K.clean && K.span_s * 1000.0f + 0.5f <= (float)KNOCK_SPAN_MS) {
            bool again = K.since_s <= KNOCK_AGAIN_S;
            ev = again ? KNOCK_EV_AGAIN : KNOCK_EV_FIRST;
            K.eye = again ? GEL_EYE_SQUINT : GEL_EYE_WIDE;
            K.eye_s = again ? KNOCK_SQUINT_S : KNOCK_WIDE_S;
            K.since_s = 0;
            K.dead_s = KNOCK_DEAD_MS / 1000.0f;
        }
        drop();
        break;
    default:
        drop();
        break;
    }
    return ev;
}

int   knock_eye_kind(void) { return K.eye_s > 0 ? (int)K.eye : -1; }
float knock_since_s(void)  { return K.since_s; }
