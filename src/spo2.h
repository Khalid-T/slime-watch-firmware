/* THE RED AND THE INFRARED (the operator night 2026-10-12, T19; ruling
 * 152), pure (no Arduino): an UNCALIBRATED SpO2 off the two-slot stream
 * the AGC per LED already runs -- red and ir, one sample pair at a time,
 * no buffer. Per window of SPO2_WINDOW_S each slot's DC (a one-pole
 * tracker at SPO2_LP_HZ, the same on both so it cancels in the ratio) and
 * its AC (the RMS about the tracker, told as a sinusoid's peak-to-peak:
 * x 2 sqrt 2, the usual perfusion index), then the textbook ratio of
 * ratios R = (AC_red / DC_red) / (AC_ir / DC_ir) and the textbook line
 * SpO2 ~ 107.3 - 25 R (calibrated 2026-10-14, see SPO2_LINE_A). QUALITY-GATED, in this order: the red slot not dark
 * (the AGC's word at the window's start, or a DC under SPO2_DC_MIN), the
 * wrist still the whole window, both slots' perfusion over SPO2_PERF_MIN,
 * R inside SPO2_R_LO..SPO2_R_HI -- any of these fails and the window says
 * so instead of a number. HONEST: the line is nobody's calibration (a real
 * meter fits its own curve per device and per LED pair); the number is a
 * TREND for him and the box, never a measurement, never on the glass,
 * never a word from the creature. The night keeper holds the lowest
 * number seen while he sleeps for the day header's one byte
 * (daylog.h reserved[4]). tools/spo2_test.cpp proves it on the laptop. */
#ifndef SPO2_H
#define SPO2_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#define SPO2_WINDOW_S   4.0f        /* the window: four or five beats */
#define SPO2_WARM_S     1.0f        /* the tracker's settle after a (re)start: these samples count for nothing */
#define SPO2_LP_HZ      0.5f        /* the DC tracker's corner (the same filter on both slots: it cancels in R) */
#define SPO2_PERF_MIN   0.0015f     /* 0.15 % peak-to-peak over the DC, each slot, or no number (was 0.3 %: his fingertip at rest runs 0.2-0.36 % red on this plate, 2026-10-14) */
#define SPO2_R_LO       0.2f        /* R under this is no skin's (was 0.4: his fingertip at 99 % reads R 0.33-0.36 on this sensor, 2026-10-14) */
#define SPO2_R_HI       1.6f        /* R over this is noise or motion (the line would say under 70) */
#define SPO2_DC_MIN     8000L       /* ppg.cpp's DC_MIN: under this the slot sees no skin */
#define SPO2_LINE_A     107.3f      /* THE LINE, CALIBRATED 2026-10-14 against his hospital oximeter (three rest pairs: R 0.360/0.332/0.326 at 99/98.5/99 %; the slope held at the textbook 25 -- no desaturation point yet, a breath hold moved the meter 99 -> 97 but the watch's red was too thin): was the textbook 110 */
#define SPO2_LINE_B     25.0f
#define SPO2_PP_OF_RMS  2.8284271f  /* 2 sqrt 2: a sinusoid's peak-to-peak from its RMS */

typedef enum { SPO2_NONE = 0, SPO2_OK, SPO2_NO_RED, SPO2_MOVED, SPO2_LOW_PERF, SPO2_R_OUT } spo2_verdict_t;

typedef struct {
    float          rate;           /* samples a second (the spot's 25, the raw stream's 100) */
    int            win_n;          /* samples per window */
    int            warm_n;         /* samples thrown away after a start */
    float          k;              /* the tracker's step */
    bool           red_dark;       /* the AGC's word at the start: red at the ceiling and still under */
    uint32_t       seen;           /* samples since the start (the warm-up counts here) */
    int            n;              /* samples in the open window */
    float          lp[2];          /* the DC tracker per slot (0 red, 1 ir) */
    float          dcsum[2];       /* the tracker summed over the window */
    float          ac2[2];         /* the deviations squared, summed */
    /* the last closed window */
    spo2_verdict_t v;
    float          r;              /* the ratio of ratios (SPO2_OK / SPO2_R_OUT) */
    int            spo2;           /* the number, 0..100 (SPO2_OK only) */
    float          perf[2];        /* each slot's perfusion, a fraction */
    int            low_slot;       /* SPO2_LOW_PERF: which slot (0 red, 1 ir) */
    int            windows, oks;   /* closed windows since the start, and how many said a number */
} spo2_t;

static inline void spo2_begin(spo2_t *s, float rate_hz, bool red_dark)
{
    if (rate_hz < 1.0f) rate_hz = 1.0f;
    s->rate = rate_hz;
    s->win_n = (int)(rate_hz * SPO2_WINDOW_S + 0.5f);
    s->warm_n = (int)(rate_hz * SPO2_WARM_S + 0.5f);
    s->k = 1.0f - expf(-6.2831853f * SPO2_LP_HZ / rate_hz);
    s->red_dark = red_dark;
    s->seen = 0; s->n = 0;
    s->lp[0] = s->lp[1] = 0; s->dcsum[0] = s->dcsum[1] = 0; s->ac2[0] = s->ac2[1] = 0;
    s->v = SPO2_NONE; s->r = 0; s->spo2 = 0; s->perf[0] = s->perf[1] = 0; s->low_slot = 0;
    s->windows = 0; s->oks = 0;
}

/* one window's verdict from its sums; still_s = how long the wrist has been still at the close */
static inline void spo2_close(spo2_t *s, float still_s)
{
    float dc[2], ac[2];
    for (int i = 0; i < 2; i++) {
        dc[i] = s->dcsum[i] / (float)s->n;
        ac[i] = sqrtf(s->ac2[i] / (float)s->n) * SPO2_PP_OF_RMS;
        s->perf[i] = dc[i] > 0 ? ac[i] / dc[i] : 0;
    }
    s->r = 0; s->spo2 = 0; s->low_slot = 0;
    if (s->red_dark || dc[0] < (float)SPO2_DC_MIN) s->v = SPO2_NO_RED;
    else if (still_s < SPO2_WINDOW_S) s->v = SPO2_MOVED;
    else if (s->perf[0] < SPO2_PERF_MIN || s->perf[1] < SPO2_PERF_MIN) { s->v = SPO2_LOW_PERF; s->low_slot = s->perf[0] < SPO2_PERF_MIN ? 0 : 1; }
    else {
        s->r = s->perf[0] / s->perf[1];
        if (s->r < SPO2_R_LO || s->r > SPO2_R_HI) s->v = SPO2_R_OUT;
        else {
            float v = SPO2_LINE_A - SPO2_LINE_B * s->r;
            s->spo2 = (int)(v + 0.5f);
            if (s->spo2 > 100) s->spo2 = 100;
            if (s->spo2 < 0) s->spo2 = 0;
            s->v = SPO2_OK; s->oks++;
        }
    }
    s->windows++;
    s->n = 0; s->dcsum[0] = s->dcsum[1] = 0; s->ac2[0] = s->ac2[1] = 0;
}

/* one sample pair. True when a window closed: s->v and the rest are
 * that window's. still_s = the wrist's still run so far (ppg.cpp's
 * s_quiet_s): under the window's length at the close, it moved inside. */
static inline bool spo2_feed(spo2_t *s, int32_t red, int32_t ir, float still_s)
{
    float x[2] = { (float)red, (float)ir };
    if (s->seen == 0) { s->lp[0] = x[0]; s->lp[1] = x[1]; }
    s->seen++;
    for (int i = 0; i < 2; i++) {
        s->lp[i] += s->k * (x[i] - s->lp[i]);
        if (s->seen <= (uint32_t)s->warm_n) continue;
        float d = x[i] - s->lp[i];
        s->dcsum[i] += s->lp[i];
        s->ac2[i] += d * d;
    }
    if (s->seen <= (uint32_t)s->warm_n) return false;
    if (++s->n < s->win_n) return false;
    spo2_close(s, still_s);
    return true;
}

/* the log's line for the last closed window (never the glass) */
static inline int spo2_word(const spo2_t *s, char *out, int cap)
{
    switch (s->v) {
    case SPO2_OK:       return snprintf(out, cap, "spo2 ~%d (R %.2f, quality ok; the line of 2026-10-14)", s->spo2, (double)s->r);
    case SPO2_NO_RED:   return snprintf(out, cap, "spo2 no red");
    case SPO2_MOVED:    return snprintf(out, cap, "spo2 no number: the wrist moved inside the window");
    case SPO2_LOW_PERF: return snprintf(out, cap, "spo2 no number: %s perfusion %.2f %% (under %.1f)", s->low_slot ? "ir" : "red",
                                        (double)(s->perf[s->low_slot] * 100.0f), (double)(SPO2_PERF_MIN * 100.0f));
    case SPO2_R_OUT:    return snprintf(out, cap, "spo2 no number: R %.2f outside %.1f..%.1f", (double)s->r, (double)SPO2_R_LO, (double)SPO2_R_HI);
    default:            return snprintf(out, cap, "spo2 no window yet");
    }
}

/* THE NIGHT'S LOWEST: the keeper. Noted only with numbers taken while he
 * sleeps (ppg.cpp asks rest); taken once at the span's end, which clears
 * it -- a nap's numbers are taken and dropped the same way. 0 = none. */
typedef struct { uint8_t lo; uint8_t n; } spo2_night_t;

static inline void spo2_night_note(spo2_night_t *k, int spo2)
{
    if (spo2 <= 0 || spo2 > 100) return;
    if (!k->lo || spo2 < k->lo) k->lo = (uint8_t)spo2;
    if (k->n != 255) k->n++;
}

static inline int spo2_night_take(spo2_night_t *k, int *n_out)
{
    int lo = k->lo;
    if (n_out) *n_out = k->n;
    k->lo = 0; k->n = 0;
    return lo;
}
#endif
