// c++ -std=gnu++17 -I src -o /tmp/beat_test tools/beat_test.cpp -lm && /tmp/beat_test
/* THE HONEST BEAT (2026-10-14): a synthetic 1.0 Hz pulse with a dicrotic
 * notch (0.45 of the beat's height, 0.3 s after it) and noise, high-passed
 * the way finish() does it, at the spot's 25 Hz and the raw stream's 100 Hz
 * -> 60 bpm with a small RMSSD; the counter as it was before (the fixed
 * 0.33 s floor alone, no merge) counts the notch and says ~120 with an
 * RMSSD in the hundreds -- the week's 139 / 141 / 138 rows; the
 * alternation merge on a bare interval series; the rest gate's lines; a
 * real 120 bpm pulse without a notch still passes. */
#include "beat.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* the skin: a pulse at fp Hz as a systolic bump (a gaussian, sigma 0.06 s) and a dicrotic bump notch x as tall
 * (sigma 0.05 s: a sharp one -- a broad one at 0.09 s is half swallowed by finish()'s 0.5 s moving mean and
 * counts only now and then, a partial doubling the rest gate alone catches), notch_s after it, riding dc at perf
 * peak-to-peak; a breath's baseline swing; white noise of nz counts */
struct skin_t { float dc, perf, fp, notch, notch_s, base, nz; };
static uint32_t rng = 777;
static float noise() { rng = rng * 1664525u + 1013904223u; return ((rng >> 8) & 0xFFFF) / 65535.0f - 0.5f; }
static float shape(const skin_t &k, float t)
{
    float T = 1.0f / k.fp, tau = fmodf(t, T);
    float s = 0;
    for (int m = -1; m <= 1; m++) {                          /* the bumps of this beat and its neighbours (a tail crosses the period) */
        float u = tau - m * T;
        s += expf(-(u * u) / (2 * 0.06f * 0.06f));
        float v = u - k.notch_s;
        s += k.notch * expf(-(v * v) / (2 * 0.05f * 0.05f));
    }
    return s;
}
static void synth(const skin_t &k, float rate, float secs, std::vector<int32_t> &x)
{
    int n = (int)(rate * secs + 0.5f);
    x.resize(n);
    for (int i = 0; i < n; i++) {
        float t = i / rate;
        float v = k.dc * (1.0f + k.perf * (shape(k, t) - 0.5f)) + k.base * sinf(6.2831853f * 0.25f * t) + k.nz * noise();
        x[i] = (int32_t)(v + 0.5f);
    }
}
/* finish()'s high-pass: x minus the mean of the 13 around it at 25 Hz (w = 6 each side); the same 0.24 s each
 * side at 100 Hz (finish() itself only ever runs at the spot's 25: the raw stream is the laptop's) */
static int32_t highpass(const std::vector<int32_t> &x, float rate, std::vector<int32_t> &hp)
{
    int n = (int)x.size(), w = (int)(0.24f * rate + 0.5f);
    hp.resize(n);
    int32_t hpmax = 0;
    for (int i = 0; i < n; i++) {
        int a = i - w < 0 ? 0 : i - w, b = i + w >= n ? n - 1 : i + w;
        int64_t s = 0;
        for (int j = a; j <= b; j++) s += x[j];
        hp[i] = x[i] - (int32_t)(s / (b - a + 1));
        if (hp[i] > hpmax) hpmax = hp[i];
    }
    return hpmax;
}
static void run(beat_t *b, const skin_t &k, float rate, float secs, bool plain)
{
    std::vector<int32_t> x, hp;
    synth(k, rate, secs, x);
    int32_t hpmax = highpass(x, rate, hp);
    int32_t thr = (int32_t)(hpmax * BEAT_THR_FRAC);
    if (plain) beat_run_plain(b, hp.data(), (int)hp.size(), rate, thr);
    else beat_run(b, hp.data(), (int)hp.size(), rate, thr);
}
static void say(const char *what, const beat_t &b)
{
    printf("%-34s %6.1f bpm  rmssd %5.1f ms  %2d beats  cv %.2f  ups %d downs %d  merged %d  refused %d  refract %.2f s  pass %d\n",
           what, b.bpm, b.rmssd, b.beats, b.cv, b.ups, b.downs, b.merged, b.refused, b.refract_s, b.passes);
}

int main()
{
    beat_t b;
    /* THE WEEK'S WRIST: a 60 bpm pulse, a notch 0.5 as tall 0.4 s after each beat, 0.25 % perfusion on a dc of
     * 90000 (THIN, the plate), 30 counts of noise, a 200-count breath swing. (The brief said 0.3 s: at the spot's
     * 25 Hz the old fixed 0.33 s floor already covers a notch at 0.30 -- `early` below proves it, and that the
     * rules do it no harm. The week's 112-141 bpm / 129-180 ms rows need the notch PAST the floor, 0.35-0.45 s
     * after a resting beat, which is where a slow heart's notch sits; 0.4 s is the case.) */
    skin_t wrist = { 90000, 0.0025f, 1.0f, 0.5f, 0.40f, 200, 30 };
    /* BEFORE (the fixed 0.33 s floor alone): the notch counted -- ~120 bpm, an RMSSD in the hundreds.
     * This run printed: 25 Hz 10 s 120 bpm rmssd 200 ms (cv 0.20: UNDER the cv gate, so it was FILED -- the week's
     * rows); 100 Hz 30 s 120 bpm rmssd 188 ms. */
    run(&b, wrist, 25.0f, 10.0f, true);  say("before: 25 Hz 10 s (the own read)", b);
    CHECK(b.bpm > 100 && b.rmssd > 120);
    CHECK(b.cv <= 0.25f);                                                        /* the old cv gate let it through */
    CHECK(beat_rest_doubtful(b.bpm, b.rmssd));                                  /* rule 3 alone would have refused the week's rows */
    run(&b, wrist, 100.0f, 30.0f, true); say("before: 100 Hz 30 s (the raw)", b);
    CHECK(b.bpm > 100 && b.rmssd > 120);
    skin_t early = { 90000, 0.0025f, 1.0f, 0.5f, 0.30f, 200, 30 };
    run(&b, early, 25.0f, 10.0f, true);  say("before: notch at 0.30 s", b);
    CHECK(b.bpm > 90 && b.cv > 0.25f);                                          /* the floor sits ON a 0.30 s notch: half of them counted, and the old cv gate refused the whole read -- no number, where the heart was plainly there */
    run(&b, early, 25.0f, 10.0f, false); say("after: notch at 0.30 s", b);
    CHECK(b.bpm > 57 && b.bpm < 63 && b.rmssd < 40 && b.merged == 0 && b.refused > 0);   /* rule 1's refractory alone (the period's median), no merge needed */
    /* AFTER: 60, the RMSSD small, the notch turned away (refused) or merged */
    run(&b, wrist, 25.0f, 10.0f, false); say("after: 25 Hz 10 s", b);
    CHECK(b.bpm > 57 && b.bpm < 63);
    CHECK(b.rmssd < 40);
    CHECK(b.beats >= 6 && b.cv < 0.25f);
    CHECK(b.refused > 0 || b.merged > 0);
    CHECK(!beat_rest_doubtful(b.bpm, b.rmssd));
    run(&b, wrist, 25.0f, 30.0f, false); say("after: 25 Hz 30 s (the row)", b);
    CHECK(b.bpm > 57 && b.bpm < 63 && b.rmssd < 40 && b.beats >= 25);
    run(&b, wrist, 100.0f, 10.0f, false); say("after: 100 Hz 10 s", b);
    CHECK(b.bpm > 57 && b.bpm < 63 && b.rmssd < 80);                            /* 100 Hz unaveraged: the noise jitters each peak a few samples */
    run(&b, wrist, 100.0f, 30.0f, false); say("after: 100 Hz 30 s", b);
    CHECK(b.bpm > 57 && b.bpm < 63 && b.rmssd < 80 && b.beats >= 25);
    /* a later notch (0.4 s, a slower heart's) and a taller one (0.6) at 55 bpm: still one beat a second */
    skin_t late = { 90000, 0.0025f, 55.0f / 60.0f, 0.6f, 0.40f, 200, 30 };
    run(&b, late, 25.0f, 10.0f, true);   say("before: 55 bpm, notch 0.6 at 0.4 s", b);
    CHECK(b.bpm > 90);
    run(&b, late, 25.0f, 10.0f, false);  say("after: 55 bpm, notch 0.6 at 0.4 s", b);
    CHECK(b.bpm > 52 && b.bpm < 58 && b.rmssd < 50);
    /* a clean 60 without a notch: untouched by the rules (nothing refused, nothing merged) */
    skin_t clean = { 90000, 0.0025f, 1.0f, 0.0f, 0.30f, 200, 30 };
    run(&b, clean, 25.0f, 10.0f, false); say("clean 60, no notch", b);
    CHECK(b.bpm > 57 && b.bpm < 63 && b.rmssd < 40 && b.merged == 0 && b.refused == 0);
    /* a real 120 bpm pulse, no notch (his asked read after the stairs): the band 40..180 is his, not rule 3's --
     * the counter says 120, rule 1's refractory (0.55 x 0.5 s = 0.275 s, under the 0.33 floor) turns nothing away */
    skin_t quick = { 90000, 0.0025f, 2.0f, 0.0f, 0.15f, 200, 30 };
    run(&b, quick, 25.0f, 10.0f, false); say("a real 120, no notch", b);
    CHECK(b.bpm > 115 && b.bpm < 125 && b.rmssd < 40 && b.merged == 0 && b.refused == 0);
    CHECK(beat_rest_doubtful(b.bpm, b.rmssd));                                  /* ...but as a REST read of the creature's own it is refused */
    run(&b, quick, 100.0f, 30.0f, false); say("a real 120 at 100 Hz", b);
    CHECK(b.bpm > 115 && b.bpm < 125 && b.merged == 0);
    /* a real 120 WITH a notch (0.15 s after the beat): the floor alone covers it, the rules do no harm */
    skin_t quick_notch = { 90000, 0.0025f, 2.0f, 0.4f, 0.15f, 200, 30 };
    run(&b, quick_notch, 25.0f, 10.0f, false); say("a real 120 with a notch", b);
    CHECK(b.bpm > 115 && b.bpm < 125 && b.rmssd < 60);
    /* noise alone (no pulse on a dc): the old gates or the rest gate refuse it -- never a clean resting number.
     * (A quick, half-regular noise count CAN pass the old cv gate: this run printed 137 bpm at cv 0.22. His asked
     * read has only the perfusion's word against that -- pre-existing, THE PERFUSION INDEX's business.) */
    skin_t none = { 90000, 0.0f, 1.0f, 0.0f, 0.3f, 0, 30 };
    run(&b, none, 25.0f, 10.0f, false);  say("noise alone", b);
    CHECK(b.beats < 6 || b.cv > 0.25f || b.bpm > 180 || b.bpm < 40 || beat_rest_doubtful(b.bpm, b.rmssd));
    /* THE ALTERNATION on a bare series (25 Hz): 8 / 17 / 8 / 17 ... = the notch at 0.32 s of a 1 s beat */
    memset(&b, 0, sizeof b);
    for (int i = 0; i < 12; i++) beat_push(&b, i & 1 ? 17 : 8);
    beat_stats(&b, 25.0f);
    CHECK(b.beats == 12 && b.diffs == 11 && b.ups == 6 && b.downs == 5 && beat_alternating(&b));
    CHECK(fabsf(b.bpm - 120.0f) < 1.0f && b.rmssd > 300);
    beat_merge(&b); beat_stats(&b, 25.0f);
    CHECK(b.merged == 6 && b.beats == 6 && b.n == 6);
    CHECK(fabsf(b.bpm - 60.0f) < 0.1f && b.rmssd == 0);
    /* a chain break inside, an odd one at a chain's end: the break kept, the odd one dropped */
    memset(&b, 0, sizeof b);
    beat_push(&b, 8); beat_push(&b, 17); beat_push(&b, 8); beat_push(&b, 0); beat_push(&b, 0); beat_push(&b, 17); beat_push(&b, 8); beat_push(&b, 17); beat_push(&b, 8);
    CHECK(b.n == 8);                                                                /* one break at a time */
    beat_merge(&b);
    CHECK(b.n == 4 && b.iv[0] == 25 && b.iv[1] == 0 && b.iv[2] == 25 && b.iv[3] == 25 && b.merged == 3);
    beat_stats(&b, 25.0f);
    CHECK(b.beats == 3 && b.diffs == 1);
    /* a steady series never alternates; a single swing (one missed beat) is not an alternation either */
    memset(&b, 0, sizeof b);
    for (int i = 0; i < 10; i++) beat_push(&b, 25);
    beat_stats(&b, 25.0f); CHECK(!beat_alternating(&b) && b.ups == 0 && b.downs == 0);
    memset(&b, 0, sizeof b);
    for (int i = 0; i < 10; i++) beat_push(&b, i == 5 ? 37 : 25);
    beat_stats(&b, 25.0f); CHECK(!beat_alternating(&b) && b.ups == 1 && b.downs == 1);
    /* the running median: the floor alone until three are in, then 0.55 of the median */
    memset(&b, 0, sizeof b);
    CHECK(beat_median_last(&b, BEAT_MED_N) == 0);
    beat_push(&b, 25); beat_push(&b, 26); CHECK(beat_median_last(&b, BEAT_MED_N) == 0);
    beat_push(&b, 24); CHECK(beat_median_last(&b, BEAT_MED_N) == 25);
    beat_push(&b, 40); CHECK(beat_median_last(&b, BEAT_MED_N) == 25.5f);
    CHECK(beat_median_all(&b) == 25.5f);
    /* RULE 3, the rest gate: the week's rows refused, a resting heart passed, the lines themselves */
    CHECK(beat_rest_doubtful(139, 146) && beat_rest_doubtful(141, 129) && beat_rest_doubtful(112, 180) && beat_rest_doubtful(130, 150) && beat_rest_doubtful(138, 154));
    CHECK(beat_rest_doubtful(120, 40));                                             /* quick alone */
    CHECK(beat_rest_doubtful(70, 150));                                             /* a wild spread alone */
    CHECK(!beat_rest_doubtful(65, 50) && !beat_rest_doubtful(110, 120) && !beat_rest_doubtful(48, 90));
    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
