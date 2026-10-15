/* THE DRUMMED TABLE -- drum.h says why; this is the small machine.
 *
 * Three axes, each a sign with a dead band and a leaky reversal rate;
 * the busiest axis's rate smoothed once more. A frame is IN THE BAND
 * when that rate sits between DRUM_MIN_HZ and DRUM_MAX_HZ and no LOUD
 * sample fell inside the last DRUM_LOUD_S. One dwell, a net count:
 * off, it climbs with in-band time and falls with out-of-band time
 * (floored at 0) to DRUM_ON_S = the drumming begins; on, it falls with
 * out-of-band time and climbs back with in-band time (capped at 0) to
 * -DRUM_OFF_S = it ends. A pause between words costs its own time. The
 * level eases toward on ? 1 : 0; the clock's extra laps grow by
 * (DRUM_X - 1) resting laps a second times the level, wrapped -- the
 * bubble's own sum. A gate shut or a tick gap drops the read (the
 * rates, the signs and the dwells to zero; an on drumming ends) and
 * the level eases out from where it is. */
#include "drum.h"
#include "gel.h"            /* GEL_GEL_HZ: the resting lap */
#include <string.h>

static struct {
    signed char sign[3];    /* -1 / 0 / +1: the side of the dead band each part was last seen past */
    float rate[3];          /* reversals a second, leaky, per axis */
    float busy;             /* the busiest axis's rate, smoothed once more (the leaky rate's own ripple at the band's floor would flicker it) */
    float loud_s;           /* seconds left of the shut band after a LOUD sample */
    float dwell_s;          /* the dwell: in-band time less out-of-band time -- 0..DRUM_ON_S while off, -DRUM_OFF_S..0 while on */
    bool  on;               /* the drumming is on */
    float level;            /* 0..1, eased */
    float extra;            /* laps added to the gel clock, wrapped 0..1 */
} D;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void drum_begin(void) { memset(&D, 0, sizeof D); }

static void drop_read(void)
{
    memset(D.sign, 0, sizeof D.sign);
    memset(D.rate, 0, sizeof D.rate);
    D.busy = 0; D.loud_s = 0; D.dwell_s = 0;
}

/* the level and the clock run whatever the gates say: a ripple already playing eases out, never snaps */
static void run_clock(float dt)
{
    float target = D.on ? 1.0f : 0.0f;
    float k = dt / DRUM_EASE_S;
    if (k > 1.0f) k = 1.0f;
    D.level += (target - D.level) * k;
    if (D.level < 0.01f && !D.on) D.level = 0.0f;
    if (D.level > 0.0f) {
        D.extra += (DRUM_X - 1.0f) * GEL_GEL_HZ * dt * D.level;
        while (D.extra >= 1.0f) D.extra -= 1.0f;
    }
}

int drum_tick(const drum_in_t *in)
{
    if (!in) return DRUM_EV_NONE;
    float dt = clampf(in->dt, 0.0f, 0.25f);
    bool open = in->lit && in->holding && !in->asleep && !in->walking && !in->falling;
    int ev = DRUM_EV_NONE;

    if (!open || in->dt > DRUM_GAP_S) {         /* a gate shut, or the dark lay between two ticks: the read dropped, an on drumming over */
        drop_read();
        if (D.on) { D.on = false; ev = DRUM_EV_END; }
        run_clock(dt);
        return ev;
    }

    /* the three parts: a reversal past the dead band, a loud sample */
    const float j[3] = { in->jx, in->jy, in->jz };
    float decay = dt / DRUM_TAU_S;
    if (decay > 1.0f) decay = 1.0f;
    bool loud = false;
    float busiest = 0.0f;
    for (int i = 0; i < 3; i++) {
        float v = j[i];
        if (v > DRUM_HI_G || v < -DRUM_HI_G) loud = true;
        int side = v > DRUM_LO_G ? 1 : v < -DRUM_LO_G ? -1 : 0;
        bool rev = side != 0 && D.sign[i] == -side;
        if (side != 0) D.sign[i] = (signed char)side;
        D.rate[i] -= D.rate[i] * decay;
        if (rev) D.rate[i] += 1.0f / DRUM_TAU_S;
        if (D.rate[i] > busiest) busiest = D.rate[i];
    }
    if (loud) D.loud_s = DRUM_LOUD_S;
    else if (D.loud_s > 0) D.loud_s -= dt;
    float ks = dt / DRUM_SMOOTH_S;
    if (ks > 1.0f) ks = 1.0f;
    D.busy += (busiest - D.busy) * ks;

    /* the dwell: a net count, so a pause between words costs its own time and no more (a reset on one
     * frame out would never let real typing reach 3 s); clamped at the resting end, so no credit is banked */
    bool in_band = D.loud_s <= 0 && D.busy >= DRUM_MIN_HZ && D.busy <= DRUM_MAX_HZ;
    D.dwell_s += in_band ? dt : -dt;
    if (!D.on) {
        if (D.dwell_s < 0) D.dwell_s = 0;
        if (D.dwell_s >= DRUM_ON_S) { D.on = true; D.dwell_s = 0; ev = DRUM_EV_BEGIN; }
    } else {
        if (D.dwell_s > 0) D.dwell_s = 0;
        if (D.dwell_s <= -DRUM_OFF_S) { D.on = false; D.dwell_s = 0; ev = DRUM_EV_END; }
    }

    run_clock(dt);
    return ev;
}

float drum_level(void)   { return D.level; }
float drum_extra(void)   { return D.extra; }
bool  drum_on(void)      { return D.on; }
float drum_rate_hz(void) { return D.busy; }
