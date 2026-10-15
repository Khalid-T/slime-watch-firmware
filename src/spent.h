/* THE LAST DROP'S MANNERS (2026-10-12, the operator night, T7): what
 * the body may do under the SPENT LINE.
 *
 * The rail: a WiFi burst draws ~300 mA; a cell near empty sags under it
 * past the S3's brownout (2.43 V) and the body dies mid-dial -- the
 * minute in progress lost, the day's row with it, and the dial itself
 * for nothing. Vigor's `spent` band (under VIGOR_EMPTY, ~3330 mV) is
 * far under where the sag kills: the line is drawn at the mV, DROP_MV
 * 3550 (Khalid's number to rule on: ruling 118), and vigor's own spent
 * band is OR'ed in (a gauge that says 3 % is spent whatever the ADC
 * says). The mV half has a band and a settle like cable.h's: ON under
 * DROP_MV, OFF only over DROP_MV + DROP_HYST_MV, each edge held
 * DROP_SETTLE_MS -- a burst's dip of a few seconds is not the line,
 * and the first sample snaps.
 *
 * Under the line, nothing new is shown and nothing is said -- the body
 * is already droopy there (vigor) -- but four things give way:
 *   the radio   refuses every WiFi dial unless ON THE CABLE (the rail
 *               is fed there): radio.cpp's RADIO_SPENT, before FORCED
 *               and the rest; the day's sync and the honest claim wait
 *               for the cable; BLE stays up (the ear draws nothing like
 *               a dial, and BLE is up only while WiFi is down anyway)
 *   the glass   holds the LOW brightness step whatever the row says
 *   the spell   is the SHORT lit spell whatever the row says (and the
 *               low cell's own shorter timers if those are shorter)
 * `vigor` and doctor's radio field say `spent: the radio waits for the
 * cable`. Pure: no Arduino; tools/spent_test.cpp walks it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DROP_MV         3550    /* the spent line on the rail: under this a dial's sag reaches the brownout */
#define DROP_HYST_MV    80      /* back over the line only this far above it (3630): a rebound after a load is not a charge */
#define DROP_SETTLE_MS  10000   /* an edge is believed once the reading has held it this long (a burst's dip is seconds) */

typedef struct {
    bool     have;      /* a first sample was read */
    bool     mv_on;     /* the mV half, settled: under the line */
    bool     cand;      /* the reading has crossed the far edge; the settle runs */
    uint32_t cand_ms;
} spent_t;

/* one call a frame (more is harmless): the ADC's mV against the band,
 * each edge after the settle, OR vigor's own spent band. Returns the
 * spent line now. */
static inline bool spent_sense(spent_t *s, uint16_t mv, bool vigor_spent, uint32_t now_ms)
{
    if (!s->have) { s->have = true; s->mv_on = mv < DROP_MV; s->cand = false; }
    else {
        bool raw = s->mv_on ? mv < DROP_MV + DROP_HYST_MV : mv < DROP_MV;
        if (raw == s->mv_on) s->cand = false;
        else if (!s->cand) { s->cand = true; s->cand_ms = now_ms; }
        else if (now_ms - s->cand_ms >= DROP_SETTLE_MS) { s->mv_on = raw; s->cand = false; }
    }
    return s->mv_on || vigor_spent;
}

/* the glass's clamps: the level never over `low` (the LOW step) while
 * spent; a timer never longer than `short_ms` (the SHORT spell) */
static inline uint8_t  spent_level(bool spent, uint8_t lv, uint8_t low)            { return spent && lv > low ? low : lv; }
static inline uint32_t spent_ms(bool spent, uint32_t ms, uint32_t short_ms)        { return spent && ms > short_ms ? short_ms : ms; }
