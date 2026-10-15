// c++ -std=gnu++17 -I src -o /tmp/resp_test tools/resp_test.cpp -lm && /tmp/resp_test
/* THE BREATH READ (2026-10-14): a synthetic pulse at 1.1 Hz with a 0.25 Hz
 * amplitude + baseline modulation says ~15 a minute at the raw stream's 30 s
 * and, coarsely, at the creature's own 10 s; every gate refuses in its own
 * words; no modulation is no number; the night keeper takes the median once. */
#include "resp.h"
#include <cstdio>
#include <cstring>
#include <cmath>
static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* ir = dc x (1 + perf/2 x (1 + am sin wb) x sin wp) + base x sin(wb + phase): a pulse at fp with the breath at fb
 * riding it twice -- the beats' amplitude swings by am, the baseline by base counts (peak); white noise of nz counts */
struct skin_t { float dc, perf, fp, fb, am, base, nz; };
static uint32_t rng = 12345;
static float noise() { rng = rng * 1664525u + 1013904223u; return ((rng >> 8) & 0xFFFF) / 65535.0f - 0.5f; }
static int32_t sample(const skin_t &k, float rate, uint32_t i)
{
    float t = i / rate;
    float b = sinf(6.2831853f * k.fb * t);
    float p = sinf(6.2831853f * k.fp * t);
    float v = k.dc * (1.0f + 0.5f * k.perf * (1.0f + k.am * b) * p) + k.base * sinf(6.2831853f * k.fb * t + 1.0f) + k.nz * noise();
    return (int32_t)(v + 0.5f);
}
static void run(resp_t *s, const skin_t &k, float rate, float secs, float still_s)
{
    resp_begin(s, rate);
    uint32_t n = (uint32_t)(rate * secs + 0.5f);
    for (uint32_t i = 0; i < n; i++) resp_feed(s, sample(k, rate, i));
    resp_close(s, still_s);
}

int main()
{
    resp_t s; char w[160];
    memset(&s, 0, sizeof s);
    resp_begin(&s, 25.0f);
    CHECK(s.warm_n == 25 && s.v == RESP_NONE && s.reads == 0);
    resp_word(&s, w, sizeof w); CHECK(!strcmp(w, "breath no read yet"));
    /* THE BENCH'S TRUTH: the raw stream, 30 s at 100 Hz, a 1.1 Hz pulse (66 bpm) at 2 % perfusion on an ir dc of
     * 120000, the breath at 0.25 Hz (15 a minute) as a 20 % amplitude swing and a 300-count baseline swing */
    skin_t finger = { 120000, 0.02f, 1.1f, 0.25f, 0.20f, 300, 10 };
    run(&s, finger, 100.0f, 30.0f, 60.0f);
    printf("raw 30 s: v %d bpm %.1f conf %.2f base %.1f am %.1f turns %d/%d beats %d pulse %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am, s.zb.turns, s.za.turns, s.beats, s.pulse_bpm);
    CHECK(s.v == RESP_OK && s.bpm > 13.0f && s.bpm < 17.0f);
    CHECK(s.conf > 0.6f && s.conf <= 1.0f);
    CHECK(s.beats >= 28 && s.beats <= 32 && s.pulse_bpm > 63 && s.pulse_bpm < 69);
    CHECK(s.r_base > 13.0f && s.r_base < 17.0f && s.r_am > 12.0f && s.r_am < 18.0f);   /* both series heard the breath */
    CHECK(s.reads == 1 && s.oks == 1);
    resp_word(&s, w, sizeof w); printf("  %s\n", w);
    CHECK(!strncmp(w, "breath ~1", 9) && strstr(w, "/min (confidence ") && strstr(w, "breaths in 30 s)") && !strstr(w, "coarse"));
    /* THE CREATURE'S OWN 10 s at 25 Hz: a number, coarse, said so; the confidence lower (2..3 breaths) */
    run(&s, finger, 25.0f, 10.0f, 60.0f);
    printf("own 10 s: v %d bpm %.1f conf %.2f base %.1f am %.1f turns %d/%d beats %d\n", s.v, s.bpm, s.conf, s.r_base, s.r_am, s.zb.turns, s.za.turns, s.beats);
    CHECK(s.v == RESP_OK && s.bpm > 10.0f && s.bpm < 20.0f);
    CHECK(s.conf > 0.15f && s.conf < 0.8f);
    CHECK(s.breaths >= 1 && s.breaths <= 4);
    resp_word(&s, w, sizeof w); printf("  %s\n", w);
    CHECK(strstr(w, "in 10 s, coarse)"));
    /* a slower breath (0.2 Hz = 12) and a faster one (0.4 Hz = 24) on the 30 s stream */
    skin_t slow = finger; slow.fb = 0.2f;
    run(&s, slow, 100.0f, 30.0f, 60.0f);
    printf("slow: v %d bpm %.1f conf %.2f base %.1f am %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am);
    CHECK(s.v == RESP_OK && s.bpm > 10.0f && s.bpm < 14.0f);
    skin_t fast = finger; fast.fb = 0.4f;
    run(&s, fast, 100.0f, 30.0f, 60.0f);
    printf("fast: v %d bpm %.1f conf %.2f base %.1f am %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am);
    CHECK(s.v == RESP_OK && s.bpm > 21.0f && s.bpm < 27.0f);
    /* a sleeper: the pulse at 0.8 Hz (48 bpm), the breath at 0.22 Hz (13), the wrist's dimmer dc and thinner pulse */
    skin_t sleeper = { 60000, 0.01f, 0.8f, 0.22f, 0.20f, 100, 10 };
    run(&s, sleeper, 25.0f, 10.0f, 60.0f);
    printf("sleeper 10 s: v %d bpm %.1f conf %.2f base %.1f am %.1f turns %d/%d beats %d pulse %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am, s.zb.turns, s.za.turns, s.beats, s.pulse_bpm);
    CHECK(s.v == RESP_OK && s.bpm > 8.0f && s.bpm < 18.0f);
    /* THE GATES, each in its own words */
    /* nothing on the sensor */
    skin_t bare = { 3000, 0.02f, 1.1f, 0.25f, 0.2f, 10, 2 };
    run(&s, bare, 25.0f, 10.0f, 60.0f);
    CHECK(s.v == RESP_NO_SKIN && s.bpm == 0);
    resp_word(&s, w, sizeof w); CHECK(strstr(w, "nothing on the sensor"));
    /* the wrist moved inside the read: still for less than the read at its close */
    run(&s, finger, 25.0f, 10.0f, 4.0f);
    CHECK(s.v == RESP_MOVED);
    resp_word(&s, w, sizeof w); CHECK(!strcmp(w, "breath no number: the wrist moved inside the read"));
    /* no pulse under the breath: a flat line (the sensor's noise alone) is cut into nothing a breath could ride */
    skin_t flat = { 120000, 0.0f, 1.1f, 0.25f, 0.0f, 0, 3 };
    run(&s, flat, 25.0f, 10.0f, 60.0f);
    printf("flat: v %d perf %.4f %% beats %d pulse %.0f\n", s.v, s.perf * 100, s.beats, s.pulse_bpm);
    CHECK(s.v == RESP_NO_PULSE);
    resp_word(&s, w, sizeof w); CHECK(strstr(w, "no pulse under the breath"));
    /* a short read: 4 s is under six beats */
    run(&s, finger, 25.0f, 4.0f, 60.0f);
    CHECK(s.v == RESP_FEW_BEATS);
    resp_word(&s, w, sizeof w); CHECK(strstr(w, "beat") && strstr(w, "needs 6"));
    /* NO MODULATION: a clean pulse with nothing riding it -- the trackers' ripple turns at the pulse's rate, which
     * is never a breath; the amplitude series does not turn at all. No number. */
    skin_t steady = { 120000, 0.02f, 1.1f, 0.25f, 0.0f, 0, 10 };
    int oks_before = s.oks;
    run(&s, steady, 100.0f, 30.0f, 60.0f);
    printf("steady: v %d base %.1f am %.1f turns %d/%d pulse %.1f\n", s.v, s.r_base, s.r_am, s.zb.turns, s.za.turns, s.pulse_bpm);
    CHECK(s.v == RESP_SHALLOW && s.bpm == 0 && s.oks == oks_before);
    resp_word(&s, w, sizeof w); CHECK(strstr(w, "no modulation deeper than the noise"));
    run(&s, steady, 25.0f, 10.0f, 60.0f);
    CHECK(s.v == RESP_SHALLOW);
    /* one series alone: amplitude modulation without a baseline swing -> a number, the confidence capped at 0.6 */
    skin_t am_only = { 120000, 0.02f, 1.1f, 0.25f, 0.25f, 0, 10 };
    run(&s, am_only, 100.0f, 30.0f, 60.0f);
    printf("am only: v %d bpm %.1f conf %.2f base %.1f am %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am);
    CHECK(s.v == RESP_OK && s.bpm > 13.0f && s.bpm < 17.0f && s.conf <= 0.6f && s.r_am > 0);
    /* the baseline alone: a wander without an amplitude swing */
    skin_t base_only = { 120000, 0.02f, 1.1f, 0.25f, 0.0f, 400, 10 };
    run(&s, base_only, 100.0f, 30.0f, 60.0f);
    printf("base only: v %d bpm %.1f conf %.2f base %.1f am %.1f\n", s.v, s.bpm, s.conf, s.r_base, s.r_am);
    CHECK(s.v == RESP_OK && s.bpm > 13.0f && s.bpm < 17.0f && s.conf <= 0.6f && s.r_base > 0);
    /* out of the band: a 0.05 Hz swell (3 a minute) is no breath of his */
    skin_t swell = { 120000, 0.02f, 1.1f, 0.05f, 0.3f, 600, 10 };
    run(&s, swell, 100.0f, 60.0f, 90.0f);
    printf("swell: v %d bpm %.1f base %.1f am %.1f\n", s.v, s.bpm, s.r_base, s.r_am);
    CHECK(s.v == RESP_OUT || s.v == RESP_SHALLOW);
    if (s.v == RESP_OUT) { resp_word(&s, w, sizeof w); CHECK(strstr(w, "outside 4..40")); }
    /* THE ZIGZAG on its own: a turn is the extreme's time, the hysteresis holds the noise back */
    resp_zig_t z = { false, 0, 0, 0, 0, 0, 0 };
    float vals[] = { 0, 1, 2, 3, 2.9f, 2.8f, 2, 1, 0, -1, -0.9f, -0.8f, 0, 1, 2 };
    for (int i = 0; i < 15; i++) resp_zig_feed(&z, vals[i], (float)i, 0.5f);
    CHECK(z.turns == 2 && z.t_first == 3.0f && z.t_last == 9.0f);
    CHECK(resp_zig_rate(&z) == 0);                                            /* two turns are under RESP_TURNS_MIN */
    resp_zig_feed(&z, 3, 15, 0.5f); resp_zig_feed(&z, 2, 16, 0.5f);
    CHECK(z.turns == 3 && z.t_last == 15.0f && fabsf(resp_zig_rate(&z) - 30.0f * 2 / 12) < 0.01f);
    /* THE NIGHT KEEPER: the median of the night, taken once, then nothing */
    resp_night_t k; memset(&k, 0, sizeof k); int n = 0;
    CHECK(resp_night_take(&k, &n) == 0 && n == 0);
    resp_night_note(&k, 14); resp_night_note(&k, 12); resp_night_note(&k, 18); resp_night_note(&k, 13); resp_night_note(&k, 15.4f);
    resp_night_note(&k, 0); resp_night_note(&k, 60); resp_night_note(&k, 3.2f);     /* no number, out of the band: no note */
    CHECK(k.n == 5);
    CHECK(resp_night_take(&k, &n) == 14 && n == 5);
    CHECK(resp_night_take(&k, &n) == 0 && n == 0);
    resp_night_note(&k, 16); resp_night_note(&k, 12);
    CHECK(resp_night_take(&k, &n) == 12 && n == 2);                           /* an even count: the lower middle */
    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
