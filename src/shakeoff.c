/* THE SHAKE-OFF: the dried edge's gates and the jiggle's curve. See
 * shakeoff.h. */
#include "shakeoff.h"
#include <math.h>

static struct {
    float left;      /* the jiggle's seconds left, 0 = none */
    float squint;    /* the squint's seconds left */
    float a2;        /* the last tick's */
    int   count, missed;
} S = { 0, 0, 0, 0, 0 };

bool shakeoff_trigger(bool lit, bool holding, bool asleep)
{
    if (!lit || !holding || asleep) { S.missed++; return false; }
    S.left = SHAKEOFF_S; S.squint = SHAKEOFF_SQUINT_S; S.count++;
    return true;
}

float shakeoff_tick(float dt)
{
    S.a2 = 0;
    if (S.left > 0) {
        float t = SHAKEOFF_S - S.left;                                                   /* time into the shake */
        S.a2 = SHAKEOFF_A2 * sinf(2.0f * (float)M_PI * SHAKEOFF_HZ * t) * (S.left / SHAKEOFF_S);   /* the cold shiver's curve */
        S.left -= dt; if (S.left < 1e-4f) S.left = 0;                                   /* frames of float add up a hair short */
    }
    if (S.squint > 0) { S.squint -= dt; if (S.squint < 1e-4f) S.squint = 0; }
    return S.a2;
}

float shakeoff_a2(void)     { return S.a2; }
bool  shakeoff_on(void)     { return S.left > 0; }
bool  shakeoff_squint(void) { return S.squint > 0; }
int   shakeoff_count(void)  { return S.count; }
int   shakeoff_missed(void) { return S.missed; }
