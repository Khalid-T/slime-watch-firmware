/* THE HONEST BEAT (2026-10-14; ruling 158), pure (no Arduino): the pulse
 * read's beat counter, taken out of ppg.cpp's finish() so the laptop can
 * drive it with a synthetic waveform (tools/beat_test.cpp). THE EVIDENCE:
 * his true resting pulse is ~60 (the hospital oximeter 59-62; the desk's
 * own peak count on the watch's 30 s raw stream from his fingertip 60-64),
 * but THE PULSE IN THE DAY's own 10 s reads this week were filed CLEAN at
 * 139 / 141 / 112 / 130 / 138 bpm with an RMSSD of 146 / 129 / 180 / 150 /
 * 154 ms -- twice the true rate, a spread three or four times any resting
 * value: on a weak wrist signal (a loose strap, the thick v0 plate:
 * perfusion 0.1-0.3 %) the DICROTIC NOTCH (the aortic valve's closing,
 * 0.2-0.4 s after the beat, a third to a half of its height) was counted
 * as a second beat, and the cv gate (0.25) did not catch the short-long
 * alternation. Those numbers fed the breath, the night ring, the heartbeat
 * handed back, the gentle wake and the desk's week. THREE RULES, in order:
 * (1) THE REFRACTORY -- after each accepted beat the next may come no sooner
 *     than BEAT_REFRACT_K 0.55 of the running median interval (the last
 *     BEAT_MED_N accepted, once BEAT_MED_MIN are in; never under the fixed
 *     BEAT_REFRACT_S floor of 180 bpm): a notch 0.2-0.4 s after a 1 s beat
 *     is never a beat;
 * (2) THE ALTERNATION -- a read whose intervals swing short / long (the
 *     ratio of consecutive intervals over BEAT_ALT_RATIO 1.25 both ways, at
 *     least BEAT_ALT_MIN swings each way and half the diffs swinging) has
 *     counted every notch: the pairs are merged (short + long = one beat to
 *     the next), the refractory is taken from the MERGED median, and the
 *     peaks are counted again under it -- the second pass is the honest
 *     series, its RMSSD included;
 * (3) THE REST GATE (ppg.cpp, the creature's OWN reads only -- his asked
 *     reads keep the 40..180 band): a rest read over BEAT_REST_BPM_MAX 110
 *     or an RMSSD over BEAT_REST_RMSSD_MAX 120 ms is refused `doubled
 *     beats?` and never filed; the day's tally in `ppg`.
 * The intervals are kept in samples (uint16_t: 1.5 s at 100 Hz is 150), at
 * most BEAT_IV_MAX -- a 30 s read at 180 bpm is 90 -- on the caller's
 * stack, ~300 B for the read's moment; no heap. The peak rule is the one
 * finish() always had: a local maximum of the high-passed trace over
 * BEAT_THR_FRAC of the tallest. */
#ifndef BEAT_H
#define BEAT_H
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define BEAT_THR_FRAC       0.4f    /* a peak: over this much of the tallest */
#define BEAT_REFRACT_S      0.33f   /* the floor: 180 bpm (ppg.cpp's old REFRACT_S) */
#define BEAT_INTERVAL_MAX_S 1.5f    /* 40 bpm: a longer gap is a missed beat, the chain breaks */
#define BEAT_REFRACT_K      0.55f   /* rule 1: the refractory after a beat, of the running median interval */
#define BEAT_MED_N          8       /* the running median: over the last this many accepted intervals */
#define BEAT_MED_MIN        3       /* ...once this many are in (before: the floor alone) */
#define BEAT_ALT_RATIO      1.25f   /* rule 2: consecutive intervals swinging over this, both ways (the week's own rows work out at 1.36-1.43: the brief's 1.4 would have caught half of them; a breath's sinus swing between CONSECUTIVE beats is under 1.1) */
#define BEAT_ALT_MIN        2       /* swings each way before the pairs merge */
#define BEAT_IV_MAX         128     /* intervals kept (30 s at 180 bpm = 90) */
#define BEAT_REST_BPM_MAX   110.0f  /* rule 3: the creature's own rest read, over this = doubled? */
#define BEAT_REST_RMSSD_MAX 120.0f  /* ...or an RMSSD over this (ms) */

typedef struct {
    uint16_t iv[BEAT_IV_MAX];   /* the intervals between accepted peaks, in samples; 0 = a chain break (a gap over the max) */
    int      n;                 /* entries kept (breaks included) */
    int      beats, diffs;      /* intervals in the human range; successive pairs inside a chain */
    float    mean, cv, bpm, rmssd;   /* the series' mean (s), spread over mean, 60 / mean, the RMSSD (ms) */
    int      ups, downs;        /* consecutive-interval ratios over BEAT_ALT_RATIO / under its inverse */
    int      merged;            /* pairs merged by rule 2 (0 = the series never alternated) */
    int      refused;           /* peaks turned away by the refractory in the final pass */
    float    refract_s;         /* the refractory the final pass began with (the floor, or the merged median's share) */
    int      passes;            /* 1; 2 = counted again under the merged median's refractory; 3 = the merged pairs' own numbers (the second pass did worse) */
} beat_t;

static inline void beat_push(beat_t *b, uint16_t v)
{
    if (b->n >= BEAT_IV_MAX) return;
    if (v == 0 && (b->n == 0 || b->iv[b->n - 1] == 0)) return;   /* one break at a time, none at the start */
    b->iv[b->n++] = v;
}

/* the median of the last k non-zero intervals (k <= BEAT_MED_N), 0 under BEAT_MED_MIN of them */
static inline float beat_median_last(const beat_t *b, int k)
{
    uint16_t v[BEAT_MED_N]; int m = 0;
    for (int i = b->n - 1; i >= 0 && m < k && m < BEAT_MED_N; i--) if (b->iv[i]) v[m++] = b->iv[i];
    if (m < BEAT_MED_MIN) return 0;
    for (int i = 1; i < m; i++) { uint16_t t = v[i]; int j = i; while (j > 0 && v[j - 1] > t) { v[j] = v[j - 1]; j--; } v[j] = t; }
    return (m & 1) ? v[m / 2] : 0.5f * (v[m / 2 - 1] + v[m / 2]);
}

/* the median of every non-zero interval (the merged series' word for the second pass) */
static inline float beat_median_all(const beat_t *b)
{
    uint16_t v[BEAT_IV_MAX]; int m = 0;
    for (int i = 0; i < b->n; i++) if (b->iv[i]) v[m++] = b->iv[i];
    if (!m) return 0;
    for (int i = 1; i < m; i++) { uint16_t t = v[i]; int j = i; while (j > 0 && v[j - 1] > t) { v[j] = v[j - 1]; j--; } v[j] = t; }
    return (m & 1) ? v[m / 2] : 0.5f * (v[m / 2 - 1] + v[m / 2]);
}

/* one pass over the high-passed trace: the peaks over thr, each at least the refractory after the last accepted
 * (refract_s, raised to BEAT_REFRACT_K x the running median when `adaptive`); the intervals into b->iv. With
 * start_s > 0 (the second pass: the period is known) the chain BEGINS on the tallest peak inside the first
 * start_s seconds -- a trace that opens on a notch would otherwise start its series one short interval early */
static inline void beat_pass(beat_t *b, const int32_t *hp, int n, float rate, int32_t thr, float refract_s, bool adaptive, float start_s)
{
    b->n = 0; b->refused = 0; b->refract_s = refract_s;
    int maxgap = (int)(BEAT_INTERVAL_MAX_S * rate + 0.5f);
    int floor_n = (int)(refract_s * rate + 0.5f);
    int last = -floor_n, from = 1;
    if (start_s > 0) {                                      /* the tallest peak inside one period of the FIRST peak */
        int start_n = (int)(start_s * rate + 0.5f), best = -1, first = -1;
        for (int i = 1; i + 1 < n; i++) {
            if (hp[i] <= thr || hp[i] <= hp[i - 1] || hp[i] < hp[i + 1]) continue;
            if (first < 0) first = i;
            if (i > first + start_n) break;
            if (best < 0 || hp[i] > hp[best]) best = i;
        }
        if (best >= 0) { last = best; from = best + 1; }
    }
    for (int i = from; i + 1 < n; i++) {
        if (hp[i] <= thr || hp[i] <= hp[i - 1] || hp[i] < hp[i + 1]) continue;
        int refract = floor_n;
        if (adaptive) {
            float med = beat_median_last(b, BEAT_MED_N);
            int r = (int)(BEAT_REFRACT_K * med + 0.5f);
            if (r > refract) refract = r;
        }
        if (i - last < refract) { b->refused++; continue; }
        if (last >= 0) {
            if (i - last <= maxgap) beat_push(b, (uint16_t)(i - last));
            else beat_push(b, 0);
        }
        last = i;
    }
}

/* the series' numbers: the mean, the spread, the RMSSD over successive intervals inside a chain, the swings */
static inline void beat_stats(beat_t *b, float rate)
{
    float isum = 0, isum2 = 0, dsum2 = 0, prev = -1;
    b->beats = 0; b->diffs = 0; b->ups = 0; b->downs = 0;
    for (int i = 0; i < b->n; i++) {
        if (!b->iv[i]) { prev = -1; continue; }
        float iv = b->iv[i] / rate;
        isum += iv; isum2 += iv * iv; b->beats++;
        if (prev > 0) {
            float d = iv - prev; dsum2 += d * d; b->diffs++;
            float r = iv / prev;
            if (r > BEAT_ALT_RATIO) b->ups++;
            else if (r < 1.0f / BEAT_ALT_RATIO) b->downs++;
        }
        prev = iv;
    }
    if (!b->beats) { b->mean = 0; b->cv = 1; b->bpm = 0; b->rmssd = 0; return; }
    b->mean = isum / b->beats;
    float var = isum2 / b->beats - b->mean * b->mean;
    b->cv = b->mean > 0 ? sqrtf(var > 0 ? var : 0) / b->mean : 1;
    b->bpm = b->mean > 0 ? 60.0f / b->mean : 0;
    b->rmssd = b->diffs > 0 ? sqrtf(dsum2 / b->diffs) * 1000.0f : 0;
}

/* rule 2's test: the intervals alternate short / long */
static inline bool beat_alternating(const beat_t *b)
{
    return b->ups >= BEAT_ALT_MIN && b->downs >= BEAT_ALT_MIN && 2 * (b->ups + b->downs) >= b->diffs;
}

/* rule 2's merge: consecutive intervals inside a chain summed in pairs (a chain's odd last one dropped) */
static inline void beat_merge(beat_t *b)
{
    int out = 0, pairs = 0;
    uint16_t held = 0;
    for (int i = 0; i < b->n; i++) {
        uint16_t v = b->iv[i];
        if (!v) { held = 0; if (out && b->iv[out - 1]) b->iv[out++] = 0; continue; }
        if (held) { uint32_t s = (uint32_t)held + v; b->iv[out++] = s > 65535u ? 65535u : (uint16_t)s; held = 0; pairs++; }
        else held = v;
    }
    while (out && !b->iv[out - 1]) out--;
    b->n = out;
    b->merged = pairs;
}

/* the numbers of one counting, kept while another is tried */
typedef struct { int beats, diffs, ups, downs, passes; float mean, cv, bpm, rmssd; } beat_word_t;
static inline beat_word_t beat_word(const beat_t *b) { beat_word_t w = { b->beats, b->diffs, b->ups, b->downs, b->passes, b->mean, b->cv, b->bpm, b->rmssd }; return w; }
static inline void beat_take(beat_t *b, const beat_word_t *w) { b->beats = w->beats; b->diffs = w->diffs; b->ups = w->ups; b->downs = w->downs; b->passes = w->passes; b->mean = w->mean; b->cv = w->cv; b->bpm = w->bpm; b->rmssd = w->rmssd; }

/* THE HONEST BEAT, one call: a first count under the floor (the running median's refractory as it grows, rule 1);
 * the PERIOD from it -- the intervals' median, or, when they alternate short / long (rule 2), the merged pairs'
 * median; then the peaks counted AGAIN under a refractory of BEAT_REFRACT_K x that period, the chain begun on
 * the tallest peak of the first period. The count with the smallest spread is the read's word (a second pass
 * under a wrong period -- a merged median past the true one -- counts fewer and worse, and the earlier word
 * stands). The rest gate (3) is beat_rest_doubtful, the caller's to apply (his asked reads skip it). */
static inline void beat_run(beat_t *b, const int32_t *hp, int n, float rate, int32_t thr)
{
    b->merged = 0;
    beat_pass(b, hp, n, rate, thr, BEAT_REFRACT_S, true, 0);
    b->passes = 1;
    beat_stats(b, rate);
    beat_word_t best = beat_word(b);
    float period = beat_median_all(b) / rate;
    if (beat_alternating(b)) {
        beat_merge(b);
        beat_stats(b, rate);
        b->passes = 3;
        period = beat_median_all(b) / rate;
        if (b->cv < best.cv) best = beat_word(b);
    }
    float refract = BEAT_REFRACT_K * period;
    if (refract > BEAT_REFRACT_K * BEAT_INTERVAL_MAX_S) refract = BEAT_REFRACT_K * BEAT_INTERVAL_MAX_S;
    if (period <= 0 || refract <= BEAT_REFRACT_S) { beat_take(b, &best); return; }   /* no period, or a quick heart the floor already covers */
    int merged = b->merged;
    beat_pass(b, hp, n, rate, thr, refract, true, period);
    b->passes = 2;
    b->merged = merged;
    beat_stats(b, rate);
    if (b->beats < BEAT_MED_MIN || b->cv > best.cv + 0.02f) beat_take(b, &best);
}

/* the counter as it was before 2026-10-14 (the fixed floor alone, no merge): the test's "before" */
static inline void beat_run_plain(beat_t *b, const int32_t *hp, int n, float rate, int32_t thr)
{
    b->merged = 0;
    beat_pass(b, hp, n, rate, thr, BEAT_REFRACT_S, false, 0);
    b->passes = 1;
    beat_stats(b, rate);
}

/* rule 3: a rest read that reads like doubled beats (over the rest band, or a spread no resting heart has) */
static inline bool beat_rest_doubtful(float bpm, float rmssd_ms)
{
    return bpm > BEAT_REST_BPM_MAX || rmssd_ms > BEAT_REST_RMSSD_MAX;
}
#endif
