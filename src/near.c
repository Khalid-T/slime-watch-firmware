/* THE DESK NEXT DOOR: the poll's cadence, the smoothing, the bands
 * with their hysteresis, the edge. See near.h. */
#include "near.h"

static struct {
    uint32_t last_ms;      /* the last sample's clock, 0 = none */
    bool     lit_prev;     /* last frame's lit (the lit edge asks at once) */
    float    s;            /* the smoothed RSSI */
    bool     have;         /* s holds a value */
    int      band;         /* NEAR_* */
    int      edges;        /* rises into AT HAND */
} N;

static const char *WORDS[4] = { "--", "FAR", "NEAR", "AT HAND" };

bool near_due(uint32_t now_ms, bool lit)
{
    bool edge = lit && !N.lit_prev;
    N.lit_prev = lit;
    if (!lit) {
        /* the dark asks nothing; a band with no sample for NEAR_STALE_MS is no band */
        if (N.band != NEAR_NONE && (uint32_t)(now_ms - N.last_ms) >= (uint32_t)NEAR_STALE_MS) { N.band = NEAR_NONE; N.have = false; N.s = 0; }
        return false;
    }
    if (edge || !N.last_ms || (uint32_t)(now_ms - N.last_ms) >= (uint32_t)NEAR_POLL_MS) {
        N.last_ms = now_ms ? now_ms : 1;
        return true;
    }
    return false;
}

int near_feed(int rssi_dbm)
{
    int was = N.band;
    if (rssi_dbm == 0) {                                   /* no desk on the air door: nothing to band */
        N.have = false; N.s = 0; N.band = NEAR_NONE;
        return was != NEAR_NONE ? NEAR_EV_BAND : NEAR_EV_NONE;
    }
    if (!N.have) { N.s = (float)rssi_dbm; N.have = true; }
    else N.s += NEAR_SMOOTH_K * ((float)rssi_dbm - N.s);
    /* the band: up at a line, down only NEAR_HYST_DBM under it */
    int b = was;
    if (was == NEAR_NONE) b = N.s >= ATHAND_DBM ? NEAR_ATHAND : N.s >= NEAR_DBM ? NEAR_NEAR : NEAR_FAR;
    else {
        if (b == NEAR_FAR && N.s >= NEAR_DBM) b = NEAR_NEAR;
        if (b == NEAR_NEAR && N.s >= ATHAND_DBM) b = NEAR_ATHAND;
        if (b == NEAR_ATHAND && N.s < ATHAND_DBM - NEAR_HYST_DBM) b = NEAR_NEAR;
        if (b == NEAR_NEAR && N.s < NEAR_DBM - NEAR_HYST_DBM) b = NEAR_FAR;
    }
    N.band = b;
    if (b == was) return NEAR_EV_NONE;
    if (b == NEAR_ATHAND && was != NEAR_NONE) { N.edges++; return NEAR_EV_ATHAND; }   /* the rise from a HELD lower band: he came to the desk */
    return NEAR_EV_BAND;                                                               /* a band first known, or a fall: the word only */
}

int         near_band(void)      { return N.band; }
bool        near_is_near(void)   { return N.band == NEAR_NEAR || N.band == NEAR_ATHAND; }
int         near_dbm(void)       { return N.have ? (int)(N.s < 0 ? N.s - 0.5f : N.s + 0.5f) : 0; }
const char *near_word(int band)  { return (band >= 0 && band < 4) ? WORDS[band] : "?"; }
int         near_glances(void)   { return N.edges; }
