/* THE EAR'S LEDGER (2026-10-09, the overhaul wave; THE DARK EAR's owed
 * number: "the cell cost of the awake nap is UNMEASURED"). Pure C, no
 * Arduino: the nap's two kinds tallied in minutes -- DEEP (light sleep,
 * the nap as it was) and EAR (the nap idling awake so the BLE stack
 * keeps listening, ruling 78) -- today and since boot, and the cell's
 * SLOPE in mV/h per kind over the last DRAIN_WINDOW_MIN closed minutes,
 * so the cost is visible over serial (`doctor`, `ear`) without a bench.
 *
 * Fed one closed minute at a time (drain_minute: the same road as
 * rest_minute, from daylog.cpp's finish_minute and the clockless
 * minute), each with the cell's mV, the cable's flag and the minute's
 * dominant nap kind -- which drain_kind_of decides from the ms the
 * minute spent in each kind (ear.cpp sums the spells wake.cpp reports).
 * The slope is a least-squares line through that kind's minutes in the
 * window, cable minutes left out, and speaks only past DRAIN_MIN_MIN
 * minutes of that kind (`--` until then). The day's tally turns over
 * with the date; the boot's never. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DRAIN_LIT = 0, DRAIN_DEEP = 1, DRAIN_EAR = 2, DRAIN_KINDS = 3 };

#define DRAIN_WINDOW_MIN  60   /* the slope's window: the last hour of closed minutes -- long enough to see ~20 mV/h over the ADC's noise, short enough to answer the same evening */
#define DRAIN_MIN_MIN     20   /* the fewest minutes of a kind before its slope speaks: under 20 the ADC's +-10 mV wobble is the whole line */
#define DRAIN_KIND_MIN_MS 30000 /* a minute is a nap kind's when it spent at least half the minute in it (else the glass was lit for most of it: DRAIN_LIT) */

typedef struct {
    uint16_t mv;        /* the minute's cell, mV; 0 = the slot is empty */
    uint8_t  kind;      /* DRAIN_* */
    uint8_t  cable;     /* the cable's minute: never on a slope */
} drain_row_t;

typedef struct {
    drain_row_t ring[DRAIN_WINDOW_MIN];
    uint32_t seq;                       /* minutes fed since boot (the ring's write head) */
    uint32_t date;                      /* the day the today tally counts (0 = none yet) */
    uint32_t today_min[DRAIN_KINDS];    /* minutes by kind today */
    uint32_t boot_min[DRAIN_KINDS];     /* ... and since boot */
    uint8_t  last_kind;                 /* the kind of the last minute fed (the status line's "now") */
} drain_t;

void drain_begin(drain_t *d);
/* the minute's dominant kind from the ms it spent in each nap kind */
int  drain_kind_of(uint32_t deep_ms, uint32_t ear_ms);
/* one closed minute: ``date`` YYYYMMDD (0 = no clock: the today tally
 * does not turn), the cell, the cable, the kind */
void drain_minute(drain_t *d, uint32_t date, uint16_t batt_mv, bool cable, int kind);
/* the slope of ``kind`` over the window, mV per hour (negative = draining);
 * ``n_out`` = that kind's off-cable minutes in the window; false (and
 * ``--``) under DRAIN_MIN_MIN of them */
bool drain_slope(const drain_t *d, int kind, float *mv_per_h, int *n_out);
/* the ledger's one line: "ear: deep 412 min / awake 96 min today (since boot
 * 412 / 96); drain deep -18 mV/h over 58 min, awake -41 mV/h over 24 min" */
int  drain_text(const drain_t *d, char *out, int cap);
const char *drain_kind_name(int kind);   /* "lit" / "deep" / "awake" */

#ifdef __cplusplus
}
#endif
