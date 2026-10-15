/* THE PERFUSION INDEX (2026-10-14, his "we have bpm now, we need oxygen
 * levels and other stuff" -- the read's own quality, said by name; ruling
 * 155), pure (no Arduino, no buffer): PI = AC / DC of the ir slot over
 * the read, in percent -- the AC as the beats' peak-to-peak over the
 * skin's steady reflection, THE SAME RULER as spo2.h's perf[] (the RMS
 * about the mean told as a sinusoid's peak-to-peak, x 2 sqrt 2). A
 * clinical finger clip reads 1-20 %; this plate under his loose strap
 * read 0.12-0.36 % on 10-14 (the calibration round), and that thinness
 * is why reads fail -- so the number is banded in words and the words
 * name the strap, not the heart:
 *   NONE  under PERF_THIN_PCT 0.1 %   nothing on the sensor
 *   THIN  0.1 .. 0.3 %                a loose strap -- a read may fail
 *   FAIR  0.3 .. 1 %
 *   GOOD  over 1 %
 * Where it goes: every read's log line carries it; a failed read's
 * verdict names it when it is the cause; the day's reads keep a tally
 * per band (`ppg`); the day's MEDIAN lands in the day header's one byte
 * (daylog.h reserved[6]) at the day's close. Never a number on the
 * glass; the PULSE row may say THIN where it said NONE. The desk's
 * morning gets one clause on a THIN or NONE day: the strap sat loose.
 * tools/perfusion_test.cpp proves it on the laptop. */
#ifndef PERFUSION_H
#define PERFUSION_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "spo2.h"                    /* SPO2_PP_OF_RMS: the one ruler */

#define PERF_THIN_PCT   0.1f         /* under this: nothing on the sensor */
#define PERF_FAIR_PCT   0.3f         /* under this: the strap */
#define PERF_GOOD_PCT   1.0f         /* over this: a clip's kind of number */
#define PERF_HIST_N     64           /* the day's histogram, a bin per tenth of a percent: 0.0 .. 6.3 %, the rest pooled in the last */
#define PERF_HDR_NONE   255          /* the header byte's word for "reads taken, the median under a tenth" (0 = no read that day) */
#define PERF_HDR_MAX    254          /* the byte's cap: 25.4 % (no wrist reads that; a clip barely) */

typedef enum { PERF_NONE = 0, PERF_THIN, PERF_FAIR, PERF_GOOD, PERF_BANDS } perf_band_t;

/* the index, in percent: the AC's RMS about the DC (the high-passed samples' RMS), the DC itself */
static inline float perf_index(float ac_rms, float dc)
{
    if (dc <= 0 || ac_rms < 0) return 0;
    return ac_rms * SPO2_PP_OF_RMS / dc * 100.0f;
}

static inline perf_band_t perf_band(float pct)
{
    return pct < PERF_THIN_PCT ? PERF_NONE : pct < PERF_FAIR_PCT ? PERF_THIN : pct < PERF_GOOD_PCT ? PERF_FAIR : PERF_GOOD;
}

static inline const char *perf_band_name(perf_band_t b)
{
    return b == PERF_NONE ? "NONE" : b == PERF_THIN ? "THIN" : b == PERF_FAIR ? "FAIR" : "GOOD";
}

/* the band's line, in the log's words */
static inline const char *perf_band_line(perf_band_t b)
{
    return b == PERF_NONE ? "nothing on the sensor" : b == PERF_THIN ? "a loose strap" : b == PERF_FAIR ? "fair" : "good";
}

/* every read's clause: `perfusion 0.28 % (THIN: a loose strap)` */
static inline int perf_word(float pct, char *out, int cap)
{
    perf_band_t b = perf_band(pct);
    return snprintf(out, cap, "perfusion %.2f %% (%s: %s)", (double)pct, perf_band_name(b), perf_band_line(b));
}

/* a failed read's verdict, when the perfusion is the cause (NONE or THIN):
 * `THIN perfusion (the strap)` -- the signal's own reason follows after a
 * colon so the log keeps it. False (and nothing written) when the
 * perfusion was FAIR or better: the cause is elsewhere. */
static inline bool perf_fail_word(float pct, const char *reason, char *out, int cap)
{
    perf_band_t b = perf_band(pct);
    if (b > PERF_THIN) return false;
    snprintf(out, cap, "%s perfusion (%s): %s", perf_band_name(b), b == PERF_NONE ? "nothing on the sensor" : "the strap", reason ? reason : "");
    return true;
}

/* THE DAY'S TALLY: each read's band counted, each read's index binned by
 * the tenth for the median -- 64 + 8 + 2 bytes, no buffer. Taken once at
 * the day's close (perf_day_take), which clears it. */
typedef struct {
    uint16_t n[PERF_BANDS];          /* reads per band today */
    uint8_t  hist[PERF_HIST_N];      /* reads per tenth of a percent (capped 255 a bin) */
    uint16_t count;                  /* reads today */
} perf_day_t;

static inline void perf_day_clear(perf_day_t *d)
{
    for (int i = 0; i < PERF_BANDS; i++) d->n[i] = 0;
    for (int i = 0; i < PERF_HIST_N; i++) d->hist[i] = 0;
    d->count = 0;
}

static inline void perf_day_note(perf_day_t *d, float pct)
{
    if (pct < 0 || isnan(pct)) return;
    perf_band_t b = perf_band(pct);
    if (d->n[b] != 65535) d->n[b]++;
    int bin = (int)(pct * 10.0f + 0.5f);
    if (bin >= PERF_HIST_N) bin = PERF_HIST_N - 1;
    if (d->hist[bin] != 255) d->hist[bin]++;
    if (d->count != 65535) d->count++;
}

/* the day's median, in tenths of a percent (the bin the middle read fell in); -1 with no read */
static inline int perf_day_median_tenths(const perf_day_t *d)
{
    uint32_t total = 0;
    for (int i = 0; i < PERF_HIST_N; i++) total += d->hist[i];
    if (!total) return -1;
    uint32_t half = (total + 1) / 2, seen = 0;
    for (int i = 0; i < PERF_HIST_N; i++) { seen += d->hist[i]; if (seen >= half) return i; }
    return PERF_HIST_N - 1;
}

/* the header's one byte from a median: 0 = no read; PERF_HDR_NONE 255 =
 * reads taken, the median under a tenth (nothing on the sensor); else the
 * tenths, capped PERF_HDR_MAX */
static inline uint8_t perf_hdr_byte(int median_tenths)
{
    if (median_tenths < 0) return 0;
    if (median_tenths == 0) return PERF_HDR_NONE;
    return (uint8_t)(median_tenths > PERF_HDR_MAX ? PERF_HDR_MAX : median_tenths);
}

/* the byte back into a band (the desk's wrist.py does the same) */
static inline perf_band_t perf_hdr_band(uint8_t byte)
{
    if (byte == PERF_HDR_NONE || byte == 0) return PERF_NONE;
    return perf_band(byte / 10.0f);
}

/* the day's close: the header byte, the count out, the tally cleared for tomorrow */
static inline uint8_t perf_day_take(perf_day_t *d, int *n_out)
{
    uint8_t b = perf_hdr_byte(perf_day_median_tenths(d));
    if (n_out) *n_out = d->count;
    perf_day_clear(d);
    return b;
}

/* `perfusion today: none 2 thin 9 fair 3 good 0` */
static inline int perf_tally_word(const perf_day_t *d, char *out, int cap)
{
    return snprintf(out, cap, "perfusion today: none %u thin %u fair %u good %u", d->n[PERF_NONE], d->n[PERF_THIN], d->n[PERF_FAIR], d->n[PERF_GOOD]);
}
#endif
