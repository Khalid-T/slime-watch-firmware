/* THE STARE RETURNED: the clock on the watched glass, the look, the
 * quiet. See stare.h. */
#include "stare.h"
#include <math.h>

static struct {
    bool  lit_prev;     /* the spell's edge */
    bool  spent;        /* this spell's look played */
    float held;         /* seconds the gates have held, continuous */
    float t;            /* the look's clock, -1 = none */
    float quiet;        /* the quiet after it, counting down */
    int   count;
} S = { false, true, 0, -1, 0, 0 };

static void end_look(void) { S.t = -1; }

int stare_tick(const stare_in_t *in)
{
    int ev = STARE_EV_NONE;
    if (in->lit && !S.lit_prev) { S.spent = false; S.held = 0; S.quiet = 0; }   /* a new spell owes the look */
    S.lit_prev = in->lit;
    if (S.quiet > 0) { S.quiet -= in->dt; if (S.quiet < 0) S.quiet = 0; }

    bool alive = in->lit && in->holding && !in->page;
    bool watched = alive && in->raised && in->still && !in->touched && !in->busy && !in->word;

    if (S.t >= 0) {   /* the look runs: anything taking the body ends it */
        if (!alive || in->touched || in->busy) end_look();
        else {
            S.t += in->dt;
            if (S.t + 1e-4f >= STARE_LOOK_S && S.t + 1e-4f >= STARE_SETTLE_S) end_look();   /* frames of float add up a hair short */
        }
    }

    if (!watched) S.held = 0;
    else if (!S.spent) {
        if (S.held < STARE_S) S.held += in->dt;
        if (S.held >= STARE_S) {
            S.held = STARE_S;                         /* held at the mark while idle plays */
            if (!in->idle_playing) {
                S.spent = true; S.t = 0; S.quiet = STARE_QUIET_S; S.count++; S.held = 0;
                ev = STARE_EV_LOOK;
            }
        }
    }
    else S.held = 0;
    return ev;
}

void stare_fx(stare_fx_t *out)
{
    out->gaze_y = 0; out->eye_kind = -1; out->squash = 0;
    if (S.t < 0) return;
    if (S.t < STARE_LOOK_S) { out->gaze_y = STARE_GY; out->eye_kind = GEL_EYE_CONTENT; }
    if (S.t < STARE_SETTLE_S) out->squash = STARE_SETTLE_SQUASH * sinf((float)M_PI * S.t / STARE_SETTLE_S);
}

bool  stare_playing(void) { return S.t >= 0; }
bool  stare_spent(void)   { return S.spent; }
float stare_held_s(void)  { return S.held; }
float stare_quiet_s(void) { return S.quiet; }
int   stare_count(void)   { return S.count; }
