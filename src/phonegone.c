/* THE PHONE LEFT BEHIND: READY's edges, the walking window, the debt,
 * the raise's look, the air door's point. See phonegone.h. */
#include "phonegone.h"
#include <math.h>

static struct {
    int8_t   ready_prev;    /* the last call's READY, -1 = none yet (the first word is no edge) */
    bool     walked;        /* a stride has been seen (walk_ms holds) */
    uint32_t walk_ms;       /* the last call that saw the wrist walking */
    bool     owed;          /* he walked away from the phone; the raise has not come */
    uint32_t look_until_ms; /* the look's end, 0 = none */
    uint32_t last_ms;       /* the last call's clock (phonegone_left_s) */
    int      count, drops;
} P = { -1, false, 0, false, 0, 0, 0, 0 };

int phonegone_tick(const phonegone_in_t *in)
{
    int ev = PHONEGONE_EV_NONE;
    uint32_t now = in->now_ms;
    P.last_ms = now;
    if (in->walking) { P.walked = true; P.walk_ms = now; }
    bool walked_lately = in->walking || (P.walked && now - P.walk_ms <= PHONEGONE_WALK_MS);
    /* the look runs, or ends early: the glass gone, the house empty, a page */
    if (P.look_until_ms) {
        if (!in->lit || !in->holding || in->page || (int32_t)(P.look_until_ms - now) <= 0) P.look_until_ms = 0;
    }
    /* READY's edges (the first word after boot is no edge) */
    if (P.ready_prev >= 0) {
        bool was = P.ready_prev == 1;
        if (was && !in->ready) {
            if (in->up && walked_lately && !P.owed) { P.owed = true; P.drops++; ev = PHONEGONE_EV_GONE; }   /* the stack up, the phone gone, him on the move: left behind */
        } else if (!was && in->ready) {
            if (P.owed) { P.owed = false; ev = PHONEGONE_EV_BACK; }   /* the phone back before the raise: nothing owed */
        }
    }
    P.ready_prev = in->ready ? 1 : 0;
    /* the raise spends the debt: the look if the gel is here, nothing if not (the moment, not a debt kept) */
    if (in->raise && P.owed) {
        P.owed = false;
        if (in->holding && in->lit && !in->page) {
            P.look_until_ms = now + (uint32_t)(PHONEGONE_S * 1000.0f);
            if (!P.look_until_ms) P.look_until_ms = 1;   /* the clock's zero is "none" */
            P.count++;
            ev = PHONEGONE_EV_LOOK;
        }
    }
    return ev;
}

bool  phonegone_owed(void)  { return P.owed; }
bool  phonegone_on(void)    { return P.look_until_ms != 0; }
float phonegone_left_s(void)
{
    if (!P.look_until_ms) return 0;
    int32_t left = (int32_t)(P.look_until_ms - P.last_ms);
    return left > 0 ? left * 0.001f : 0;
}
int   phonegone_count(void) { return P.count; }
int   phonegone_drops(void) { return P.drops; }

void phonegone_point(float phi, float cx, float cy, float r, float *x, float *y)
{
    *x = cx - sinf(phi) * r;   /* wisp.cpp's door_xy: the rim opposite the floor */
    *y = cy - cosf(phi) * r;
}
