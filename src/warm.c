/* THE WARM SKIN: the proxy, the bands with their hysteresis, the two
 * debts and their moments. See warm.h. */
#include "warm.h"
#include <math.h>

static struct {
    float    s;            /* the proxy */
    float    last;         /* the last reading */
    bool     have;         /* s holds a value */
    int      band;         /* WARM_* */
    bool     settle_owed;  /* WARM's entry, unpaid */
    bool     shiver_owed;  /* COLD's entry, unpaid */
    int      reads, settles, shivers;
} W;

static const char *WORDS[4] = { "--", "plain", "WARM", "COLD" };

int warm_feed(float die_c)
{
    if (!(die_c >= WARM_DIE_MIN_C && die_c <= WARM_DIE_MAX_C)) return WARM_EV_NONE;   /* NAN or off the die's range: a bus fault, not a skin */
    W.reads++;
    W.last = die_c;
    if (!W.have) { W.s = die_c; W.have = true; }
    else W.s += (die_c - W.s) / (float)WARM_TAU;
    int was = W.band, band = was;
    switch (was) {
    case WARM_WARM: if (W.s < WARM_C - WARM_HYST_C) band = WARM_PLAIN; break;
    case WARM_COLD: if (W.s > COLD_C + WARM_HYST_C) band = WARM_PLAIN; break;
    default:        band = WARM_PLAIN; break;
    }
    if (band == WARM_PLAIN) {                              /* plain (or just let go): the lines proper */
        if (W.s >= WARM_C) band = WARM_WARM;
        else if (W.s <= COLD_C) band = WARM_COLD;
    }
    if (band == was) return WARM_EV_NONE;
    W.band = band;
    /* a debt the band leaves is forgiven; an entry leaves its own */
    W.settle_owed = band == WARM_WARM;
    W.shiver_owed = band == WARM_COLD;
    return WARM_EV_BAND;
}

int warm_tick(bool lit, bool holding, bool awake, bool raise, bool page)
{
    if (!lit || !holding || !awake || page) return WARM_ACT_NONE;
    if (W.shiver_owed) { W.shiver_owed = false; W.shivers++; return WARM_ACT_SHIVER; }
    if (W.settle_owed && raise) { W.settle_owed = false; W.settles++; return WARM_ACT_SETTLE; }
    return WARM_ACT_NONE;
}

int         warm_band(void)       { return W.band; }
float       warm_c(void)          { return W.have ? W.s : NAN; }
float       warm_last_c(void)     { return W.have ? W.last : NAN; }
const char *warm_word(int band)   { return (band >= 0 && band < 4) ? WORDS[band] : WORDS[0]; }
bool        warm_owed(int act)    { return act == WARM_ACT_SETTLE ? W.settle_owed : act == WARM_ACT_SHIVER ? W.shiver_owed : false; }
int         warm_reads(void)      { return W.reads; }
int         warm_settles(void)    { return W.settles; }
int         warm_shivers(void)    { return W.shivers; }
