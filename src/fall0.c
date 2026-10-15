/* THE WEIGHTLESS BEAT -- fall0.h says why; this is the small machine.
 *
 * Three states. FLOOR: |a| under FREEFALL_G runs a clock (low_ms); a
 * sample at or over it zeroes the clock and re-arms the read; the clock
 * at FREEFALL_MS is the LOFT. AIR: the loft's level climbs to 1 over
 * FALL0_UP_S (the lift is FALL0_LIFT_PX x smoothstep of it; the breath's
 * factor is 1 minus it); a sample over IMPACT_G is the IMPACT (the
 * level to 0 at once, the splat's clock started); air_ms at
 * IMPACT_WAIT_MS is the CLEAR (the level eases to 0 over FALL0_EASE_S).
 * DOWN: the splat's squash (LAND_SQUASH x (1 - smoothstep) over LAND_S)
 * or the ease; back to FLOOR when both are spent. A gate closing or a
 * tick gap in the AIR clears silently, nothing left. */
#include "fall0.h"
#include "gel.h"            /* GEL_EYE_WIDE */
#include <string.h>

enum { S_FLOOR = 0, S_AIR = 1, S_DOWN = 2 };

static struct {
    unsigned char state;    /* S_* */
    bool  armed;            /* |a| seen at or over FREEFALL_G since the last loft (a long weightlessness is one beat) */
    float low_s;            /* seconds of |a| under FREEFALL_G in a row */
    float air_s;            /* seconds since the loft */
    float level;            /* 0..1 the loft's height */
    float splat_s;          /* seconds into the splat; < 0 = none */
} F;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smoothf(float t) { return t * t * (3.0f - 2.0f * t); }

void fall0_begin(void)
{
    memset(&F, 0, sizeof F);
    F.splat_s = -1.0f;
}

static void clear_silent(void)
{
    F.state = S_FLOOR;
    F.low_s = 0;
    F.air_s = 0;
    F.level = 0;
    F.splat_s = -1.0f;
    F.armed = false;
}

/* DOWN: the ease and the splat run out; the floor when both are spent */
static void step_down(float dt, float a_g)
{
    if (F.level > 0) F.level = clampf(F.level - (FALL0_EASE_S > 0 ? dt / FALL0_EASE_S : 1.0f), 0.0f, 1.0f);
    if (F.splat_s >= 0) { F.splat_s += dt; if (F.splat_s >= LAND_S) F.splat_s = -1.0f; }
    if (F.level <= 0 && F.splat_s < 0) { F.state = S_FLOOR; F.low_s = 0; F.armed = a_g >= FREEFALL_G; }
}

int fall0_tick(const fall0_in_t *in)
{
    if (!in) return FALL0_EV_NONE;
    float dt = clampf(in->dt, 0.0f, 0.25f);
    bool open = in->lit && in->holding && !in->asleep && !in->falling;
    int ev = FALL0_EV_NONE;

    if (!open || in->dt > FALL0_GAP_S) {        /* a gate shut, or the dark lay between two ticks: nothing in the air, nothing said */
        if (F.state == S_AIR) clear_silent();
        else if (F.state == S_FLOOR) { F.low_s = 0; F.armed = false; }
        if (!open) {                            /* the splat or the ease still runs out on its own (a squash on an empty glass is nothing worse) */
            if (F.state == S_DOWN) step_down(dt, in->a_g);
            return FALL0_EV_NONE;
        }
    }

    switch (F.state) {
    case S_FLOOR:
        if (in->a_g >= FREEFALL_G) { F.low_s = 0; F.armed = true; }
        else if (F.armed) {
            F.low_s += dt;
            if (F.low_s * 1000.0f + 0.5f >= (float)FREEFALL_MS) {   /* (half a ms of grace: the frames' sum against a round number) */
                F.state = S_AIR; F.air_s = 0; F.low_s = 0; F.armed = false;
                F.level = clampf(FALL0_UP_S > 0 ? dt / FALL0_UP_S : 1.0f, 0.0f, 1.0f);   /* the loft begins on the frame it is read */
                ev = FALL0_EV_LOFT;
            }
        }
        break;
    case S_AIR:
        F.air_s += dt;
        F.level = clampf(F.level + (FALL0_UP_S > 0 ? dt / FALL0_UP_S : 1.0f), 0.0f, 1.0f);
        if (in->a_g > IMPACT_G) {               /* it hit: the lift gone at once, the fall's own splat */
            F.state = S_DOWN; F.level = 0; F.splat_s = 0;
            ev = FALL0_EV_IMPACT;
        } else if (F.air_s * 1000.0f + 0.5f >= (float)IMPACT_WAIT_MS) {   /* nothing came: it eases down */
            F.state = S_DOWN; F.splat_s = -1.0f;
            ev = FALL0_EV_CLEAR;
        }
        break;
    case S_DOWN:
        step_down(dt, in->a_g);
        break;
    default:
        clear_silent();
        break;
    }
    return ev;
}

float fall0_lift(void)     { return FALL0_LIFT_PX * smoothf(F.level); }
float fall0_squash(void)   { return F.splat_s >= 0 ? LAND_SQUASH * (1.0f - smoothf(clampf(F.splat_s / LAND_S, 0.0f, 1.0f))) : 0.0f; }
float fall0_breath_k(void) { return 1.0f - smoothf(F.level); }
int   fall0_eye_kind(void) { return F.state == S_AIR ? GEL_EYE_WIDE : -1; }
bool  fall0_on(void)       { return F.state == S_AIR; }
bool  fall0_easing(void)   { return F.state == S_DOWN; }
