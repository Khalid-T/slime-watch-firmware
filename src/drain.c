/* THE EAR'S LEDGER, pure (drain.h). tools/drain_test.cpp pins it. */
#include "drain.h"
#include <string.h>
#include <stdio.h>

void drain_begin(drain_t *d) { memset(d, 0, sizeof *d); }

int drain_kind_of(uint32_t deep_ms, uint32_t ear_ms)
{
    if (ear_ms >= DRAIN_KIND_MIN_MS && ear_ms >= deep_ms) return DRAIN_EAR;
    if (deep_ms >= DRAIN_KIND_MIN_MS) return DRAIN_DEEP;
    return DRAIN_LIT;
}

void drain_minute(drain_t *d, uint32_t date, uint16_t batt_mv, bool cable, int kind)
{
    if (kind < 0 || kind >= DRAIN_KINDS) kind = DRAIN_LIT;
    if (date && date != d->date) {                      /* the date's turn: today's tally starts over */
        d->date = date;
        memset(d->today_min, 0, sizeof d->today_min);
    }
    d->today_min[kind]++;
    d->boot_min[kind]++;
    d->last_kind = (uint8_t)kind;
    drain_row_t *r = &d->ring[d->seq % DRAIN_WINDOW_MIN];
    r->mv = batt_mv ? batt_mv : 1;                      /* 0 marks an empty slot; a dead reading still fills its minute */
    r->kind = (uint8_t)kind;
    r->cable = cable ? 1 : 0;
    d->seq++;
}

bool drain_slope(const drain_t *d, int kind, float *mv_per_h, int *n_out)
{
    /* least squares through (minute index, mV) over the kind's off-cable
     * minutes in the window: the slope in mV per minute, times 60 */
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    uint32_t have = d->seq < DRAIN_WINDOW_MIN ? d->seq : DRAIN_WINDOW_MIN;
    for (uint32_t i = 0; i < have; i++) {
        uint32_t idx = d->seq - have + i;               /* the minute's index since boot */
        const drain_row_t *r = &d->ring[idx % DRAIN_WINDOW_MIN];
        if (!r->mv || r->cable || r->kind != kind) continue;
        double x = (double)i, y = (double)r->mv;
        sx += x; sy += y; sxx += x * x; sxy += x * y;
        n++;
    }
    if (n_out) *n_out = n;
    if (mv_per_h) *mv_per_h = 0;
    if (n < DRAIN_MIN_MIN) return false;
    double den = (double)n * sxx - sx * sx;
    if (den <= 0) return false;                         /* every point on one minute: no line */
    double slope = ((double)n * sxy - sx * sy) / den;   /* mV per minute */
    if (mv_per_h) *mv_per_h = (float)(slope * 60.0);
    return true;
}

const char *drain_kind_name(int kind)
{
    return kind == DRAIN_DEEP ? "deep" : kind == DRAIN_EAR ? "awake" : "lit";
}

static int slope_word(const drain_t *d, int kind, char *out, int cap)
{
    float s; int n;
    if (drain_slope(d, kind, &s, &n)) return snprintf(out, (size_t)cap, "%s %+.0f mV/h over %d min", drain_kind_name(kind), s, n);
    return snprintf(out, (size_t)cap, "%s -- (%d of %d min)", drain_kind_name(kind), n, DRAIN_MIN_MIN);
}

int drain_text(const drain_t *d, char *out, int cap)
{
    char deep[48], ear[48];
    slope_word(d, DRAIN_DEEP, deep, sizeof deep);
    slope_word(d, DRAIN_EAR, ear, sizeof ear);
    return snprintf(out, (size_t)cap, "ear: deep %lu min / awake %lu min today (since boot %lu / %lu, lit %lu); drain %s, %s",
                    (unsigned long)d->today_min[DRAIN_DEEP], (unsigned long)d->today_min[DRAIN_EAR],
                    (unsigned long)d->boot_min[DRAIN_DEEP], (unsigned long)d->boot_min[DRAIN_EAR],
                    (unsigned long)d->boot_min[DRAIN_LIT], deep, ear);
}
