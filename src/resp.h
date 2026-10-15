/* THE BREATH READ (2026-10-14, his "oxygen levels and other stuff"; ruling
 * 154), pure (no Arduino): a RESPIRATION RATE off the IR slot's two slow
 * modulations over one read, fed a sample at a time from ppg.cpp's drain
 * beside spo2.h -- no buffer, a few dozen floats. The breath rides the
 * pulse wave two ways and both are read here: (1) THE BASELINE WANDER --
 * the venous return and the chest's pressure swing the whole trace up and
 * down at the breath's rate (the DC tracker's own slow drift, 0.1..0.5 Hz):
 * a four-pole tracker at RESP_BASE_LP_HZ holds the baseline, a one-pole at
 * RESP_SLOW_LP_HZ under it takes the longer drift away, and what sits
 * between is the wander; (2) THE AMPLITUDE MODULATION -- each beat's
 * peak-to-peak grows and shrinks with the breath (the stroke volume's
 * swing): a two-pole high-pass gives the beat, a hysteresis zero-crossing
 * cuts it into beats, each beat's peak-to-peak is one point of a slow
 * series. Each series goes through the same ZIGZAG (a turn is confirmed
 * when the signal has retreated a hysteresis from its running extreme; the
 * turn's time is the extreme's own, so the lag cancels), and the rate is
 * the turns' spacing: 30 x (turns - 1) / the span between the first and
 * the last turn, in breaths a minute. The two agree -> one number weighted
 * by their turns and a confidence from the cycles seen and the agreement;
 * they disagree -> the wander's number at a low confidence; one alone ->
 * its number, confidence capped. HONEST LIMITS, said in the line: a 10 s
 * read holds 2..4 breaths, so the number is COARSE (one turn more or
 * fewer moves it by a third); the 30 s raw stream (`ppg raw 30`) is the
 * bench's truth. GATES, in spo2.h's order and manner: the slot sees skin
 * (the DC over RESP_DC_MIN), the wrist still the WHOLE read (still_s at
 * the close under the read's length = moved), enough beats cut
 * (RESP_BEATS_MIN), a modulation deeper than the noise (RESP_TURNS_MIN
 * turns on at least one series, each slower than half the pulse -- the
 * pulse's own ripple through the trackers turns at the pulse's rate, and
 * that is never a breath), and the rate inside RESP_LO..RESP_HI. Never on
 * the glass, never a digit, never a word from the creature (ruling 154):
 * one log line per read, the night's MEDIAN once a night into the day
 * header's reserved[5] (daylog.h), and the last clean rate kept for
 * `status` and handed to rest_in_t.breath_bpm -- which the gel's breath
 * does NOT follow tonight (his ruling to take). tools/resp_test.cpp proves
 * it on the laptop. */
#ifndef RESP_H
#define RESP_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#define RESP_PULSE_LP_HZ  0.5f      /* the beat's high-pass corner, two poles (spo2.h's one-pole tracker is 0.5 too) */
#define RESP_BASE_LP_HZ   0.4f      /* the baseline's tracker corner, FOUR poles: a 0.8 Hz sleeper's pulse leaves 4 % ripple, a 0.25 Hz breath keeps half */
#define RESP_SLOW_LP_HZ   0.05f     /* under the baseline: the drift slower than any breath, taken away */
#define RESP_RMS_HZ       0.2f      /* the running RMS corners (the beat's, the wander's): the rulers settle inside the warm-up */
#define RESP_WARM_S       1.0f      /* the trackers' settle after a start: these samples cut no beat and turn no series (the rulers' 0.2 Hz corners settle inside it; a 10 s read cannot spare more) */
#define RESP_PERF_MIN     0.0003f   /* the beat's peak-to-peak over the DC: under 0.03 % the crossings are the sensor's noise, not beats */
#define RESP_PULSE_LO     30.0f     /* the beats' own rate must be a pulse: outside this band the cuts were noise */
#define RESP_PULSE_HI     200.0f
#define RESP_BEAT_HYST    0.25f     /* the beat's crossing: this much of the beat's running RMS above and below zero */
#define RESP_BASE_HYST    1.2f      /* a wander turn: a retreat of this many of the wander's own RMS (a sinusoid swings 2.83 RMS peak to peak) */
#define RESP_AM_HYST      0.08f     /* an amplitude turn: a retreat of this fraction of the mean beat amplitude (an 8 % modulation or deeper) */
#define RESP_AM_MEAN_K    0.2f      /* the mean amplitude's step per beat */
#define RESP_BEATS_MIN    6         /* fewer beats cut than this: no read */
#define RESP_TURNS_MIN    3         /* turns on a series before it says a rate (three turns = one whole cycle) */
#define RESP_PULSE_RATIO  2.0f      /* a series' rate must sit under the pulse's / this: the pulse's ripple is never a breath */
#define RESP_AGREE        0.30f     /* the two series agree when they differ by under this fraction of the larger */
#define RESP_CYCLES_FULL  6.0f      /* confidence 1.0 wants this many whole cycles across both series */
#define RESP_LO           4.0f      /* breaths a minute: the honest band */
#define RESP_HI           40.0f
#define RESP_DC_MIN       8000L     /* ppg.cpp's DC_MIN: under this the slot sees no skin */
#define RESP_PP_OF_RMS    2.8284271f

typedef enum { RESP_NONE = 0, RESP_OK, RESP_NO_SKIN, RESP_MOVED, RESP_FEW_BEATS, RESP_NO_PULSE, RESP_SHALLOW, RESP_OUT } resp_verdict_t;

/* THE ZIGZAG: a slow series' turns with hysteresis h; the turns' times are the extremes' own */
typedef struct {
    bool  started;
    int   dir;              /* 0 unknown yet, +1 rising (ext a running max), -1 falling (a running min) */
    float ext, ext_t;       /* the running extreme and its time */
    int   turns;
    float t_first, t_last;  /* the first and the last confirmed turn */
} resp_zig_t;

static inline void resp_zig_feed(resp_zig_t *z, float v, float t, float h)
{
    if (!z->started) { z->started = true; z->ext = v; z->ext_t = t; return; }
    if (z->dir == 0) {
        if (v > z->ext + h) { z->dir = +1; z->ext = v; z->ext_t = t; }
        else if (v < z->ext - h) { z->dir = -1; z->ext = v; z->ext_t = t; }
        return;
    }
    if (z->dir > 0 ? v > z->ext : v < z->ext) { z->ext = v; z->ext_t = t; return; }   /* still going: the extreme moves */
    if (z->dir > 0 ? v < z->ext - h : v > z->ext + h) {                               /* retreated the hysteresis: a turn, at the extreme's time */
        if (!z->turns) z->t_first = z->ext_t;
        z->t_last = z->ext_t;
        z->turns++;
        z->dir = -z->dir; z->ext = v; z->ext_t = t;
    }
}

/* a series' rate in turns a minute halved: 30 (turns - 1) / span; 0 under RESP_TURNS_MIN */
static inline float resp_zig_rate(const resp_zig_t *z)
{
    if (z->turns < RESP_TURNS_MIN) return 0;
    float span = z->t_last - z->t_first;
    return span > 0.1f ? 30.0f * (float)(z->turns - 1) / span : 0;
}

typedef struct {
    float    rate;              /* samples a second (the spot's 25, the raw stream's 100) */
    int      warm_n;
    float    k_pulse, k_base, k_slow, k_rms;
    uint32_t seen;
    /* the trackers */
    float    lp_a, lp_b;        /* the beat's two poles: hp1 = x - lp_a, beat = hp1 - lp_b */
    float    bl[4];             /* the baseline's four poles (bl[3] is the baseline) */
    float    lp_slow;           /* the drift under the baseline */
    float    beat_rms2, wander_rms2;
    /* the beats */
    int      beat_side;         /* -1 under the band, +1 over, 0 unknown */
    bool     beat_open;         /* inside a beat (an upstroke seen) */
    float    bmax, bmin;        /* the open beat's extremes */
    int      beats;
    float    t_beat_first, t_beat_last;
    float    amp_mean;          /* the beats' mean peak-to-peak, a one-pole per beat */
    resp_zig_t zb, za;          /* the baseline wander's turns, the amplitude's */
    /* the close */
    resp_verdict_t v;
    float    bpm;               /* breaths a minute (RESP_OK; RESP_OUT carries the out-of-band one) */
    float    conf;              /* 0..1 */
    float    r_base, r_am;      /* each series' own rate (0 = none) */
    int      breaths;           /* the whole cycles behind the number (the series that led) */
    float    secs;              /* the read's length */
    float    pulse_bpm;         /* the beats' rate, for the ratio gate and the line */
    float    dc, perf;          /* the baseline at the close; the beat's peak-to-peak over it */
    int      reads, oks;
} resp_t;

static inline float resp_k(float hz, float rate_hz) { return 1.0f - expf(-6.2831853f * hz / rate_hz); }

static inline void resp_begin(resp_t *s, float rate_hz)
{
    int reads = s->reads, oks = s->oks;
    if (rate_hz < 1.0f) rate_hz = 1.0f;
    resp_zig_t z0 = { false, 0, 0, 0, 0, 0, 0 };
    resp_t zero = { 0 };
    *s = zero;
    s->rate = rate_hz;
    s->warm_n = (int)(rate_hz * RESP_WARM_S + 0.5f);
    s->k_pulse = resp_k(RESP_PULSE_LP_HZ, rate_hz);
    s->k_base = resp_k(RESP_BASE_LP_HZ, rate_hz);
    s->k_slow = resp_k(RESP_SLOW_LP_HZ, rate_hz);
    s->k_rms = resp_k(RESP_RMS_HZ, rate_hz);
    s->zb = z0; s->za = z0;
    s->v = RESP_NONE;
    s->reads = reads; s->oks = oks;
}

/* one IR sample (or whichever one slot his asked read lit: the breath rides any of them) */
static inline void resp_feed(resp_t *s, int32_t ir)
{
    float x = (float)ir;
    if (s->seen == 0) { s->lp_a = x; s->lp_b = 0; s->bl[0] = s->bl[1] = s->bl[2] = s->bl[3] = x; s->lp_slow = x; }
    s->seen++;
    float t = (float)s->seen / s->rate;
    /* the beat: two poles of high-pass */
    s->lp_a += s->k_pulse * (x - s->lp_a);
    float hp1 = x - s->lp_a;
    s->lp_b += s->k_pulse * (hp1 - s->lp_b);
    float beat = hp1 - s->lp_b;
    /* the baseline: four poles, then the drift under it */
    s->bl[0] += s->k_base * (x - s->bl[0]);
    for (int i = 1; i < 4; i++) s->bl[i] += s->k_base * (s->bl[i - 1] - s->bl[i]);
    s->lp_slow += s->k_slow * (s->bl[3] - s->lp_slow);
    float wander = s->bl[3] - s->lp_slow;
    s->beat_rms2 += s->k_rms * (beat * beat - s->beat_rms2);
    s->wander_rms2 += s->k_rms * (wander * wander - s->wander_rms2);
    if (s->seen <= (uint32_t)s->warm_n) return;
    /* the beats: a hysteresis crossing cuts them; each one's peak-to-peak is one point of the amplitude series */
    float hb = RESP_BEAT_HYST * sqrtf(s->beat_rms2);
    if (s->beat_open) { if (beat > s->bmax) s->bmax = beat; if (beat < s->bmin) s->bmin = beat; }
    if (beat > hb) {
        if (s->beat_side < 0) {                                  /* the upstroke: a beat begins, the last one closes */
            if (s->beat_open) {
                float amp = s->bmax - s->bmin;
                if (!s->beats) { s->amp_mean = amp; s->t_beat_first = t; }
                else s->amp_mean += RESP_AM_MEAN_K * (amp - s->amp_mean);
                resp_zig_feed(&s->za, amp, t, RESP_AM_HYST * s->amp_mean);
                s->beats++; s->t_beat_last = t;
            }
            s->beat_open = true; s->bmax = s->bmin = beat;
        }
        s->beat_side = +1;
    } else if (beat < -hb) s->beat_side = -1;
    resp_zig_feed(&s->zb, wander, t, RESP_BASE_HYST * sqrtf(s->wander_rms2));
}

/* the read is over: the verdict. still_s = the wrist's still run at the close (ppg.cpp's s_quiet_s) */
static inline bool resp_close(resp_t *s, float still_s)
{
    s->secs = (float)s->seen / s->rate;
    s->dc = s->bl[3];
    s->bpm = 0; s->conf = 0; s->breaths = 0; s->pulse_bpm = 0;
    s->r_base = resp_zig_rate(&s->zb); s->r_am = resp_zig_rate(&s->za);
    if (s->beats >= 2 && s->t_beat_last > s->t_beat_first) s->pulse_bpm = 60.0f * (float)(s->beats - 1) / (s->t_beat_last - s->t_beat_first);
    s->reads++;
    if (s->dc < (float)RESP_DC_MIN) { s->v = RESP_NO_SKIN; return false; }
    if (still_s < s->secs - 0.5f) { s->v = RESP_MOVED; return false; }
    s->perf = s->dc > 0 ? sqrtf(s->beat_rms2) * RESP_PP_OF_RMS / s->dc : 0;
    if (s->perf < RESP_PERF_MIN || s->pulse_bpm < RESP_PULSE_LO || s->pulse_bpm > RESP_PULSE_HI) { s->v = RESP_NO_PULSE; return false; }
    if (s->beats < RESP_BEATS_MIN) { s->v = RESP_FEW_BEATS; return false; }
    float cap = s->pulse_bpm / RESP_PULSE_RATIO;
    bool ok_b = s->r_base > 0 && s->r_base < cap, ok_a = s->r_am > 0 && s->r_am < cap;
    if (!ok_b && !ok_a) { s->v = RESP_SHALLOW; return false; }
    float cyc_b = ok_b ? (float)(s->zb.turns - 1) * 0.5f : 0, cyc_a = ok_a ? (float)(s->za.turns - 1) * 0.5f : 0;
    float cycles = cyc_b + cyc_a, cyc_f = cycles / RESP_CYCLES_FULL;
    if (cyc_f > 1.0f) cyc_f = 1.0f;
    if (ok_b && ok_a) {
        float hi = s->r_base > s->r_am ? s->r_base : s->r_am;
        float diff = fabsf(s->r_base - s->r_am) / hi;
        if (diff <= RESP_AGREE) {
            s->bpm = (s->r_base * cyc_b + s->r_am * cyc_a) / cycles;
            s->conf = cyc_f * (1.0f - 0.5f * diff / RESP_AGREE);
            s->breaths = (int)(cyc_b + 0.5f);
        } else {                                                 /* they disagree: the wander's, said low */
            s->bpm = s->r_base; s->conf = (cyc_b / RESP_CYCLES_FULL > 1.0f ? 1.0f : cyc_b / RESP_CYCLES_FULL) * 0.3f; s->breaths = (int)(cyc_b + 0.5f);
        }
    } else {                                                     /* one series alone: its number, the confidence capped */
        s->bpm = ok_b ? s->r_base : s->r_am;
        s->conf = (ok_b ? cyc_b : cyc_a) / RESP_CYCLES_FULL * 0.6f;
        if (s->conf > 0.6f) s->conf = 0.6f;
        s->breaths = (int)((ok_b ? cyc_b : cyc_a) + 0.5f);
    }
    if (s->bpm < RESP_LO || s->bpm > RESP_HI) { s->v = RESP_OUT; s->conf = 0; return false; }
    s->v = RESP_OK; s->oks++;
    return true;
}

/* the log's line for the last closed read (never the glass) */
static inline int resp_word(const resp_t *s, char *out, int cap)
{
    switch (s->v) {
    case RESP_OK:        return snprintf(out, cap, "breath ~%.0f /min (confidence %.1f; %d breath%s in %.0f s%s)", (double)s->bpm, (double)s->conf,
                                         s->breaths, s->breaths == 1 ? "" : "s", (double)s->secs, s->secs < 20 ? ", coarse" : "");
    case RESP_NO_SKIN:   return snprintf(out, cap, "breath no number: nothing on the sensor (dc %.0f)", (double)s->dc);
    case RESP_MOVED:     return snprintf(out, cap, "breath no number: the wrist moved inside the read");
    case RESP_FEW_BEATS: return snprintf(out, cap, "breath no number: only %d beat%s cut in %.0f s (needs %d)", s->beats, s->beats == 1 ? "" : "s", (double)s->secs, RESP_BEATS_MIN);
    case RESP_NO_PULSE:  return snprintf(out, cap, "breath no number: no pulse under the breath (perfusion %.3f %%, the cuts at %.0f bpm)", (double)(s->perf * 100.0f), (double)s->pulse_bpm);
    case RESP_SHALLOW:   return snprintf(out, cap, "breath no number: no modulation deeper than the noise (wander %.0f, amplitude %.0f /min against a pulse of %.0f)",
                                         (double)s->r_base, (double)s->r_am, (double)s->pulse_bpm);
    case RESP_OUT:       return snprintf(out, cap, "breath no number: %.0f /min outside %.0f..%.0f", (double)s->bpm, (double)RESP_LO, (double)RESP_HI);
    default:             return snprintf(out, cap, "breath no read yet");
    }
}

/* THE NIGHT'S MEDIAN: the keeper -- a histogram over the honest band, one byte a bin (no list of the night's
 * numbers). Noted only with numbers taken while he sleeps (ppg.cpp asks rest); taken once at the span's end,
 * which clears it -- a nap's numbers are taken and dropped the same way. 0 = none. */
#define RESP_BINS ((int)RESP_HI - (int)RESP_LO + 1)
typedef struct { uint8_t bin[RESP_BINS]; uint16_t n; } resp_night_t;

static inline void resp_night_note(resp_night_t *k, float bpm)
{
    int b = (int)(bpm + 0.5f) - (int)RESP_LO;
    if (bpm <= 0 || b < 0 || b >= RESP_BINS) return;
    if (k->bin[b] == 255) return;
    k->bin[b]++; k->n++;
}

static inline int resp_night_take(resp_night_t *k, int *n_out)
{
    int n = k->n, med = 0;
    if (n_out) *n_out = n;
    if (n) {
        int half = (n + 1) / 2, acc = 0;
        for (int b = 0; b < RESP_BINS; b++) { acc += k->bin[b]; if (acc >= half) { med = b + (int)RESP_LO; break; } }
    }
    for (int b = 0; b < RESP_BINS; b++) k->bin[b] = 0;
    k->n = 0;
    return med;
}
#endif
