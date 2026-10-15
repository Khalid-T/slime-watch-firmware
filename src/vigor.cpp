/* VIGOR FROM THE CELL -- vigor.h says what; this is how. One eased
 * float, a few thresholds with hysteresis, the cable's settle as a
 * half-sine of squash. No Arduino. */
#include "vigor.h"
#include "gel.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static struct {
    bool  have;             /* a sample seen: the number snapped to it */
    float v, target;
    float level;            /* the cell's level the target came from, 0..1 */
    const char *source;
    uint16_t mv;            /* the last readings, for `vigor` */
    int   pct;
    bool  charging, low, spent;
    bool  thrift, lean;     /* BATTERY MANNERS: the cell under 20 % / 10 %, with the hysteresis */
    float spent_k;          /* the sleep look, eased */
    float lid_t;            /* the raise's droopy second left */
    /* the cable's settle */
    bool  settle_owed;      /* the cable arrived on a dark or empty glass: played when it can be seen */
    float owed_s;
    float settle_t;         /* > 0: playing (seconds into it); the eyes stay content a little past the squash */
    int   settles;
} V;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
/* the nominal creature until a sample says -- vigor_begin or not (the
 * host tests that link this and never tick it get the 1.0 creature, as
 * they get the grain's nominal knobs) */
static float cur(void) { return V.have ? V.v : 1.0f; }

float vigor_level_of_mv(uint16_t mv)
{
    if (mv >= VIGOR_MV_FLOAT) return 1.0f;
    return clampf((float)((int)mv - VIGOR_MV_EMPTY) / (float)(VIGOR_MV_FULL - VIGOR_MV_EMPTY), 0.0f, 1.0f);
}

/* the desk's staircase read as knees: 1.0 at full, VIGOR_AT_LOW at
 * VIGOR_LOW_PCT, VIGOR_AT_CRIT at VIGOR_CRIT_PCT, 0 at empty (the desk
 * has the mains to fall back on; a cell runs down to nothing) */
float vigor_of_level(float level)
{
    level = clampf(level, 0.0f, 1.0f);
    const float lo = VIGOR_LOW_PCT / 100.0f, cr = VIGOR_CRIT_PCT / 100.0f;
    if (level >= lo) return VIGOR_AT_LOW + (1.0f - VIGOR_AT_LOW) * (level - lo) / (1.0f - lo);
    if (level >= cr) return VIGOR_AT_CRIT + (VIGOR_AT_LOW - VIGOR_AT_CRIT) * (level - cr) / (lo - cr);
    return VIGOR_AT_CRIT * level / cr;
}

void vigor_begin(void)
{
    memset(&V, 0, sizeof V);
    V.v = V.target = V.level = 1.0f;        /* the nominal creature until a sample says */
    V.source = "nothing yet";
    V.pct = -1;
}

float vigor_tick(float dt, uint16_t batt_mv, bool charging, int gauge_pct, bool lit, bool holding)
{
    dt = dt > 0 ? (dt < 0.25f ? dt : 0.25f) : 0;
    /* the source and the target */
    if (gauge_pct >= 0) { V.level = clampf(gauge_pct / 100.0f, 0, 1); V.source = "the gauge"; }
    else if (batt_mv >= VIGOR_MV_FLOAT) { V.level = 1.0f; V.source = "the cable"; }
    else { V.level = vigor_level_of_mv(batt_mv); V.source = "the ADC"; }
    V.target = vigor_of_level(V.level);
    V.mv = batt_mv; V.pct = gauge_pct;
    /* the cable arriving: one contented settle (never at boot on the cable: it did not arrive) */
    if (V.have && charging && !V.charging) { V.settle_owed = true; V.owed_s = 0; }
    V.charging = charging;
    /* the ease; the first sample snaps */
    if (!V.have) { V.have = true; V.v = V.target; }
    else V.v += (V.target - V.v) * clampf(dt / (VIGOR_EASE_S / 3.0f), 0, 1);
    /* the thresholds, with their hysteresis */
    if (V.low ? V.v >= VIGOR_LOW + VIGOR_HYST : V.v < VIGOR_LOW) V.low = !V.low;
    if (V.spent ? V.v >= VIGOR_EMPTY + VIGOR_HYST : V.v < VIGOR_EMPTY) V.spent = !V.spent;
    /* BATTERY MANNERS: the cell's own percent (not the eased vigor: the
     * watch's thrift follows the cell as read, the creature's posture the
     * slow number), with its hysteresis on the way back up */
    {
        int pct = vigor_pct();
        if (V.thrift ? pct >= VIGOR_THRIFT_PCT + VIGOR_PCT_HYST : pct < VIGOR_THRIFT_PCT) V.thrift = !V.thrift;
        if (V.lean ? pct >= VIGOR_LEAN_PCT + VIGOR_PCT_HYST : pct < VIGOR_LEAN_PCT) V.lean = !V.lean;
    }
    /* the spent look eases in and out */
    float want = V.spent ? 1.0f : 0.0f;
    if (V.spent_k < want) V.spent_k = clampf(V.spent_k + dt / VIGOR_SPENT_EASE_S, 0, 1);
    else if (V.spent_k > want) V.spent_k = clampf(V.spent_k - dt / VIGOR_SPENT_EASE_S, 0, 1);
    /* the lids */
    if (V.lid_t > 0) V.lid_t -= dt;
    /* the settle: owed until the glass can show it, then played once */
    if (V.settle_owed) {
        V.owed_s += dt;
        if (lit && holding && V.settle_t <= 0 && !V.spent) { V.settle_owed = false; V.settle_t = 1e-4f; V.settles++; }
        else if (V.owed_s >= VIGOR_SETTLE_HOLD_S) V.settle_owed = false;
    }
    if (V.settle_t > 0) {
        V.settle_t += dt;
        if (V.settle_t >= VIGOR_SETTLE_EYE_S || !lit || !holding) V.settle_t = 0;
    }
    return V.v;
}

float vigor(void) { return cur(); }
float vigor_target(void) { return V.have ? V.target : 1.0f; }
const char *vigor_source(void) { return V.source ? V.source : "nothing yet"; }
bool  vigor_low(void) { return V.low; }
bool  vigor_spent(void) { return V.spent; }
int   vigor_pct(void) { return V.have ? (int)(V.level * 100.0f + 0.5f) : 100; }
bool  vigor_thrift(void) { return V.thrift; }
bool  vigor_lean(void) { return V.lean; }
float vigor_breath_hz_k(void) { return 0.7f + 0.3f * cur(); }
float vigor_breath_amp_k(void) { return 0.8f + 0.2f * cur(); }
float vigor_idle_k(void) { return 1.0f + (1.0f - cur()); }
void  vigor_raise(void) { if (V.low) V.lid_t = VIGOR_LID_S; }
bool  vigor_playing(void) { return V.settle_t > 0; }
int   vigor_settles(void) { return V.settles; }

void vigor_fx(vigor_fx_t *out)
{
    out->squash = 0; out->eye_kind = -1; out->spent_k = V.spent_k;
    if (V.settle_t > 0) {
        float ph = clampf(V.settle_t / VIGOR_SETTLE_S, 0, 1);
        out->squash = VIGOR_SETTLE_SQUASH * sinf((float)M_PI * ph);
        out->eye_kind = GEL_EYE_CONTENT;
    }
    if (V.lid_t > 0) out->eye_kind = GEL_EYE_DROOPY;
}

void vigor_status(char *out, int cap)
{
    char src[48];
    if (V.pct >= 0) snprintf(src, sizeof src, "the gauge %d %%", V.pct);
    else if (V.mv >= VIGOR_MV_FLOAT) snprintf(src, sizeof src, "the cable, %u mV (no cell, or full)", V.mv);
    else snprintf(src, sizeof src, "the ADC %u mV -> level %.2f", V.mv, V.level);
    snprintf(out, cap, "vigor: %.2f (%s; easing to %.2f%s) | breath rate x%.2f depth x%.2f | idle gap x%.2f | %s%s | timers %s | cable %s, settled %dx%s%s | manners: %d %% -- %s",
             cur(), V.have ? src : "nothing yet", vigor_target(), V.charging ? ", charging" : "",
             vigor_breath_hz_k(), vigor_breath_amp_k(), vigor_idle_k(),
             V.spent ? "SPENT: the sleep look, no idle things" : V.low ? "LOW: droopy on the raise" : "lids ordinary",
             V.lid_t > 0 ? " (drooping now)" : "",
             V.low ? "short (8/12 s)" : "12/20 s",
             V.charging ? "yes" : "no", V.settles,
             V.settle_owed ? " (one owed to the lit glass)" : "", V.settle_t > 0 ? " (settling now)" : "",
             vigor_pct(), V.lean ? "LEAN (under 10): the claim every 20 min, the cell in the shade, the glance short, the glass a step dimmer"
                                 : V.thrift ? "THRIFT (under 20): the glance short, the glass a step dimmer" : "none");
}
