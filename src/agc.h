/* THE PPG'S AGC, pure (no Arduino): Maxim AN6409's rule -- the steady
 * level (the DC) must sit in 0.25..0.75 of the 18-bit range, and the LED
 * current is what puts it there. A finger, a pale wrist and a darker one
 * need very different currents (the Uno bench, 2026-09-23: 25 mA clipped
 * under a finger, 6 mA read 2 %). Start at AGC_PA_START, read the DC over
 * ~30 samples, halve when over the band, step up in proportion (capped at
 * x2) when under; AGC_PA_MAX still dark = nothing on the window. */
#ifndef AGC_H
#define AGC_H
#include <stdint.h>
#include <stdbool.h>

#define AGC_FULL      262143L
#define AGC_DC_LO     (AGC_FULL / 4)         /* 65535 */
#define AGC_DC_HI     (AGC_FULL * 3 / 4)     /* 196607 */
#define AGC_PA_START  0x1F                   /* ~6.2 mA, the datasheet's default */
#define AGC_PA_MIN    0x08                   /* ~1.6 mA: under this it is pressed flat or lit by the room */
#define AGC_PA_MAX    0xFF                   /* 50 mA */
#define AGC_STEPS_MAX 8                      /* 6 mA -> 50 mA is four doublings; four more to settle */

typedef enum { AGC_AGAIN = 0, AGC_SETTLED, AGC_DARK, AGC_BRIGHT } agc_verdict_t;

/* One step: the DC read at pa -> the next pa in *pa, and the verdict.
 * AGC_AGAIN: *pa changed, read again. AGC_SETTLED: in the band, keep *pa.
 * AGC_DARK: at the ceiling and still under the band (nothing on the
 * window). AGC_BRIGHT: at the floor and still over (pressed, or the room
 * lights the diode). */
static inline agc_verdict_t agc_step(long dc, uint8_t *pa)
{
    if (dc >= AGC_DC_LO && dc <= AGC_DC_HI) return AGC_SETTLED;
    if (dc > AGC_DC_HI) {
        if (*pa <= AGC_PA_MIN) return AGC_BRIGHT;
        unsigned v = *pa / 2u; if (v < AGC_PA_MIN) v = AGC_PA_MIN; *pa = (uint8_t)v;
        return AGC_AGAIN;
    }
    if (*pa >= AGC_PA_MAX) return AGC_DARK;
    long want = dc > 0 ? (long)*pa * ((AGC_DC_LO + AGC_DC_HI) / 2) / dc : (long)*pa * 2;
    if (want > (long)*pa * 2) want = (long)*pa * 2;
    if (want <= *pa) want = *pa + 4;
    if (want > AGC_PA_MAX) want = AGC_PA_MAX;
    *pa = (uint8_t)want;
    return AGC_AGAIN;
}

static inline float agc_ma(uint8_t pa) { return pa * 0.2f; }

/* THE AGC PER LED (2026-10-08, THE PULSE, THREE WAYS BETTER; the wrist
 * read of 09-25: the current pinned at 0xF8 because the ONE channel
 * judged was the one the skin gives least back on, while the other sat
 * at 75 % of the range already). Two channels, each its own current,
 * each judged against the band on its own: a channel that has settled
 * (or given up) is left where it is while the other keeps stepping. The
 * spot's single LED is agc_step above, untouched; the raw stream's two
 * slots (red, ir) are this. */
#define AGC_CH 2

typedef struct {
    uint8_t       pa[AGC_CH];      /* the current each channel is at */
    agc_verdict_t v[AGC_CH];       /* AGC_AGAIN while still choosing; the channel's verdict once it stopped */
    int           steps;           /* rounds taken (one round = every unsettled channel stepped once) */
} agc2_t;

static inline void agc2_begin(agc2_t *a, uint8_t pa0, uint8_t pa1)
{
    a->pa[0] = pa0; a->pa[1] = pa1;
    a->v[0] = a->v[1] = AGC_AGAIN;
    a->steps = 0;
}

/* one round: each channel's DC read at its pa -> its next pa, its own
 * verdict kept once it is not AGC_AGAIN. Returns AGC_AGAIN while any
 * channel is still choosing (and under AGC_STEPS_MAX rounds), else the
 * pair's word: AGC_SETTLED when both sit in the band, AGC_DARK when
 * either is dark at the ceiling (nothing on the window: no channel can
 * save that), AGC_BRIGHT when one is pressed flat and none dark. Past
 * AGC_STEPS_MAX a channel still choosing is left at its last pa and
 * read as settled enough (its verdict stays AGC_AGAIN for the log). */
static inline agc_verdict_t agc2_step(agc2_t *a, long dc0, long dc1)
{
    long dc[AGC_CH] = { dc0, dc1 };
    bool again = false;
    for (int i = 0; i < AGC_CH; i++) {
        if (a->v[i] != AGC_AGAIN) continue;
        uint8_t pa = a->pa[i];
        agc_verdict_t v = agc_step(dc[i], &pa);
        if (v == AGC_AGAIN) { a->pa[i] = pa; again = true; }
        else a->v[i] = v;
    }
    a->steps++;
    if (again && a->steps < AGC_STEPS_MAX) return AGC_AGAIN;
    for (int i = 0; i < AGC_CH; i++) if (a->v[i] == AGC_DARK) return AGC_DARK;
    for (int i = 0; i < AGC_CH; i++) if (a->v[i] == AGC_BRIGHT) return AGC_BRIGHT;
    return AGC_SETTLED;
}

static inline bool agc2_choosing(const agc2_t *a) { return (a->v[0] == AGC_AGAIN || a->v[1] == AGC_AGAIN) && a->steps < AGC_STEPS_MAX; }
#endif
