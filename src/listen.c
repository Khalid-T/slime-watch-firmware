/* THE EAR TO THE WRIST: the look through the creature's own read, the
 * verdict at its end, the settle. See listen.h. */
#include "listen.h"
#include <math.h>

static struct {
    bool     reading_prev;  /* the read's edges */
    uint32_t seq0;          /* the ring's count at the read's start */
    bool     on;            /* the look showing this frame */
    bool     said;          /* this read's LISTEN_EV_EAR given */
    float    ear;           /* seconds of the look this read */
    float    settle;        /* the settle's clock, -1 = none */
    int      clean, failed;
} S = { false, 0, false, false, 0, -1, 0, 0 };

int listen_tick(const listen_in_t *in)
{
    int ev = LISTEN_EV_NONE;
    bool visible = in->lit && in->holding && !in->page && !in->busy;

    if (in->reading && !S.reading_prev) { S.seq0 = in->seq; S.said = false; S.ear = 0; }   /* a read begins: the count to judge it by */

    if (S.settle >= 0) {   /* the settle runs; the body taken ends it */
        if (!visible) S.settle = -1;
        else { S.settle += in->dt; if (S.settle + 1e-4f >= LISTEN_SETTLE_S) S.settle = -1; }
    }

    S.on = in->reading && in->glow && visible;
    if (S.on) {
        S.ear += in->dt;
        if (!S.said) { S.said = true; ev = LISTEN_EV_EAR; }
    }

    if (!in->reading && S.reading_prev) {   /* the read is over, one way or the other */
        bool ok = in->seq != S.seq0;
        if (ok) { S.clean++; ev = LISTEN_EV_CLEAN; if (visible) S.settle = 0; }
        else    { S.failed++; ev = LISTEN_EV_FAILED; }
        S.on = false;
    }
    if (!in->reading) S.ear = 0;
    S.reading_prev = in->reading;
    return ev;
}

void listen_fx(listen_fx_t *out)
{
    out->gaze_y = 0; out->eye_kind = -1; out->squash = 0;
    if (S.on) { out->gaze_y = LISTEN_GY; out->eye_kind = GEL_EYE_CONTENT; }
    if (S.settle >= 0) out->squash = LISTEN_SETTLE_SQUASH * sinf((float)M_PI * S.settle / LISTEN_SETTLE_S);
}

bool  listen_on(void)       { return S.on; }
bool  listen_settling(void) { return S.settle >= 0; }
float listen_ear_s(void)    { return S.ear; }
bool  listen_heard(void)    { return S.said; }
int   listen_clean(void)    { return S.clean; }
int   listen_failed(void)   { return S.failed; }
