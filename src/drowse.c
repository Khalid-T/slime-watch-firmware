/* THE DIM'S DROWSE -- drowse.h says why; this is the small machine.
 *
 * One bool (drowsing) and one level (0..1): the level climbs over
 * DROWSE_IN_S while drowsing and falls over DROWSE_OUT_S after; the
 * lids are DROOPY while drowsing and the draw's own the instant it
 * ends (up at once), the slump and the breath follow the level. The
 * breath's factor is computed per frame from the rates main hands
 * over: the target rate is DROWSE_BREATH_K x the current rate, floored
 * at DROWSE_BREATH_K x the resting rate and capped at the current rate
 * (a breath already slower than the drowsy rest -- the hour's sit, a
 * cold warming -- is left alone; the drowse never quickens), and the
 * rate this frame is the current one eased toward that target by the
 * level. The phase chain (drowse_breath_phase) integrates rest's phase
 * delta under that factor, the sky's own road, so the notch has no
 * edge of its own; with the notch off (every wake: the glass was dark)
 * the chain IS rest's phase a turn apart at most, so THE WAKING
 * BREATH's snapped inhale lands on the chain's inhale too. */
#include "drowse.h"
#include <math.h>
#include <string.h>

static struct {
    bool  on;               /* drowsing */
    float level;            /* 0..1 */
    float k;                /* the breath's factor this frame */
    float phase, last_in;   /* the chained phase and the last input */
    bool  phase_init;
} D;

void drowse_begin(void)
{
    memset(&D, 0, sizeof D);
    D.k = 1.0f;
    D.phase_init = false;   /* the review (2026-10-13): re-seed the chained breath phase on the next lit frame -- the dim's shortfall never paid back left the drawn inhale an arbitrary turn from rest's, defeating THE WAKING BREATH and the purr's inhale */
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

int drowse_tick(const drowse_in_t *in)
{
    if (!in) return DROWSE_EV_NONE;
    float dt = clampf(in->dt, 0.0f, 0.25f);
    bool want = in->lit && in->dim && in->holding && !in->dozing && !in->page && !in->touch;
    int ev = DROWSE_EV_NONE;
    if (want && !D.on) { D.on = true; ev = DROWSE_EV_DOWN; }
    else if (!want && D.on) {
        D.on = false;
        if (!in->lit) D.level = 0;                                   /* the dark: taken silently, nothing left for the next lit frame */
        else if (in->touch || !in->dim) ev = DROWSE_EV_UP;           /* his hand or his arm ended the dim: the lids up, main's engage */
        /* else: the pet left, a page came up, the night began -- the look eases out, no event */
    }
    if (D.on) D.level = clampf(D.level + (DROWSE_IN_S > 0 ? dt / DROWSE_IN_S : 1.0f), 0.0f, 1.0f);
    else      D.level = clampf(D.level - (DROWSE_OUT_S > 0 ? dt / DROWSE_OUT_S : 1.0f), 0.0f, 1.0f);
    /* the breath's factor */
    float hz = in->breath_hz > 0.0f ? in->breath_hz : 0.0f;
    if (hz > 0.0f && D.level > 0.0f) {
        float target = hz * DROWSE_BREATH_K;
        float floor_hz = in->breath_rest_hz > 0.0f ? in->breath_rest_hz * DROWSE_BREATH_K : target;
        if (target < floor_hz) target = floor_hz;
        if (target > hz) target = hz;                                /* never quicker */
        float drowsy = hz + (target - hz) * D.level;
        D.k = drowsy / hz;
    } else D.k = 1.0f;
    return ev;
}

void drowse_dark(void)
{
    D.on = false;
    D.level = 0.0f;
    D.k = 1.0f;
}

void drowse_fx(drowse_fx_t *out)
{
    if (!out) return;
    out->eye_kind = D.on ? GEL_EYE_DROOPY : -1;
    out->eye_alpha = D.on ? D.level : 0.0f;
    out->squash = DROWSE_SQUASH * D.level;
    out->breath_k = D.k;
    out->level = D.level;
}

float drowse_breath_phase(float phase_in)
{
    if (!D.phase_init) { D.phase = phase_in; D.last_in = phase_in; D.phase_init = true; return D.phase; }
    float d = phase_in - D.last_in;
    D.last_in = phase_in;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;                /* the input may wrap (or snap: THE WAKING BREATH's inhale is a forward step) */
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    D.phase += d * (D.k > 0.0f ? D.k : 1.0f);
    if (D.phase > 1000.0f) D.phase -= 2.0f * (float)M_PI * 159.0f;   /* kept small; a whole number of turns */
    return D.phase;
}

bool drowse_on(void) { return D.on; }
