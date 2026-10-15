/* THE CABLE SENSE, SETTLED (2026-09-10 night, from the serial log of
 * 21:28:46-47: `link: off the cable -- the honest claim is back`, then
 * `link: on the cable -- the honest claim is off` one second later --
 * VBAT sat at the 4050 mV threshold on the campus dock and the claim
 * rule flapped with it).
 *
 * The board has no VBUS pin: a cable is INFERRED from the ETA6096
 * holding VBAT at its float (~4.12 V measured on USB with no cell; a
 * weak port sags toward 4.05). One threshold on a sagging reading is a
 * coin at the edge, so the sense is a BAND with a settle: ON at or
 * above CABLE_ON_MV (the number the claim rule, the day log's CHARGING
 * flag and the drop always read), OFF only under CABLE_OFF_MV (70 mV
 * lower: a sag never crosses it, a cell off the cable falls through
 * it), and EACH EDGE must hold CABLE_SETTLE_MS before it is believed --
 * a reading that crosses back inside the settle resets it, so a flap
 * settles nowhere. The first sample snaps (a boot on the cable is on
 * the cable from its first line). Everything that reads charging() --
 * THE CABLE NEVER CLAIMS, THE SHELF RULE, the day log's flag, vigor's
 * settle, the drop, the page -- reads this one settled value.
 *
 * THE RISING CELL (2026-10-12, the operator night, T8): the float is
 * blind to the first hour of a charge from empty -- a cell at 3500 mV
 * on the charger reads "off the cable" until it has climbed to 4050,
 * and for that hour the docked watch claims him, bumps on the dock,
 * the dock hand sleeps. So a SECOND WITNESS: a cell NEVER RISES BY
 * ITSELF. main.cpp feeds cable_minute() the closed minute's mV (the
 * day log's own cadence); a cell that ROSE CABLE_RISE_MV 40 over the
 * last CABLE_RISE_MIN 4 closed minutes -- steadily: each half of the
 * window at least half the rise, so a one-minute rebound after a load
 * (the radio's sag letting go) is not a charge -- while under
 * CABLE_ON_MV is on the cable. The witness lapses when the window's
 * rise falls under half (the climb stopped: unplugged mid-charge, or
 * the float has it from here); the edge it raises is held
 * CABLE_SETTLE_MS like the rest and let go by the same band (on: stays
 * on while at or over CABLE_OFF_MV OR still rising). The ring is
 * CABLE_RISE_MIN + 1 readings: a dozen bytes.
 *
 * The honesty gap is the same as before, only wider on one side: with
 * a cell in, a FULL cell freshly unplugged rests above CABLE_OFF_MV
 * (vigor.h: VIGOR_MV_FULL 4150, the float 4100) and reads "on the
 * cable" until it has drained under 3980 -- the hold still claims; the
 * day the cell is in, the log's edge lines say how long, and a VBUS
 * divider on a reserved pin (HARDWARE ROOM) would make it exact.
 *
 * Pure: mv and the clock are parameters; tools/cable_test.cpp runs it
 * on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CABLE_ON_MV      4050   /* on: at the charger's float or above (the number since THE SHELF RULE) */
#define CABLE_OFF_MV     3980   /* off: only under this -- the band a sag at the float never crosses */
#define CABLE_SETTLE_MS  10000  /* an edge is believed once the reading has held it this long */
#define CABLE_RISE_MV    40     /* THE RISING CELL: a climb of this over the window is a charger (a cell never rises by itself) */
#define CABLE_RISE_MIN   4      /* ... the window, in closed minutes (the day log's cadence) */

typedef struct {
    bool     have;      /* a first sample was read */
    bool     on;        /* the settled sense: on the cable */
    bool     cand;      /* the reading has crossed the band's far edge; the settle is running */
    uint32_t cand_ms;   /* when it crossed */
    /* THE RISING CELL */
    uint16_t ring[CABLE_RISE_MIN + 1];  /* the closed minutes' mV, oldest first once full */
    uint8_t  n;         /* readings held, up to CABLE_RISE_MIN + 1 */
    bool     rising;    /* the witness: the cell climbs as only a charger makes it */
} cable_t;

/* THE RISING CELL: one call per closed minute with that minute's mV.
 * The witness comes on when both halves of the window rose at least
 * CABLE_RISE_MV / 2 (a steady climb; the whole at least CABLE_RISE_MV),
 * and goes off when the whole window's rise is under CABLE_RISE_MV / 2.
 * Only under the float: at CABLE_ON_MV the float is the witness. */
static inline void cable_minute(cable_t *c, uint16_t mv)
{
    const uint8_t N = CABLE_RISE_MIN + 1;
    if (c->n < N) c->ring[c->n++] = mv;
    else { for (uint8_t i = 1; i < N; i++) c->ring[i - 1] = c->ring[i]; c->ring[N - 1] = mv; }
    if (c->n < N) return;
    int whole = (int)c->ring[N - 1] - (int)c->ring[0];
    int first = (int)c->ring[CABLE_RISE_MIN / 2] - (int)c->ring[0];
    int second = (int)c->ring[N - 1] - (int)c->ring[CABLE_RISE_MIN / 2];
    if (!c->rising) { if (mv < CABLE_ON_MV && first >= CABLE_RISE_MV / 2 && second >= CABLE_RISE_MV / 2) c->rising = true; }
    else if (whole < CABLE_RISE_MV / 2) c->rising = false;
}

/* one call whenever the sense is asked (twice a second is plenty; more
 * is harmless -- same now, same answer): the raw reading against the
 * band's far edge (or the rising witness), and the edge believed only
 * after the settle */
static inline bool cable_sense(cable_t *c, uint16_t mv, uint32_t now_ms)
{
    if (!c->have) { c->have = true; c->on = mv >= CABLE_ON_MV; c->cand = false; return c->on; }
    bool raw = c->rising || (c->on ? mv >= CABLE_OFF_MV : mv >= CABLE_ON_MV);
    if (raw == c->on) { c->cand = false; return c->on; }
    if (!c->cand) { c->cand = true; c->cand_ms = now_ms; return c->on; }
    if (now_ms - c->cand_ms >= CABLE_SETTLE_MS) { c->on = raw; c->cand = false; }
    return c->on;
}
