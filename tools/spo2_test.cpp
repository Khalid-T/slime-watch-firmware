// c++ -std=gnu++17 -I src -o /tmp/spo2_test tools/spo2_test.cpp -lm && /tmp/spo2_test
/* THE RED AND THE INFRARED (T19): a synthetic two-slot pulse with a known
 * R gives the textbook number; every quality gate refuses in its own
 * words; the night keeper takes the lowest once. */
#include "spo2.h"
#include <cstdio>
#include <cstring>
#include <cmath>
static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* a pulse at f Hz on each slot: dc x (1 + perf/2 sin), perf the peak-to-peak over the dc */
struct skin_t { float dc_r, dc_i, perf_r, perf_i, f; };
static void sample(const skin_t &k, float rate, uint32_t i, int32_t *red, int32_t *ir)
{
    float t = i / rate, s = sinf(6.2831853f * k.f * t);
    *red = (int32_t)(k.dc_r * (1.0f + 0.5f * k.perf_r * s) + 0.5f);
    *ir = (int32_t)(k.dc_i * (1.0f + 0.5f * k.perf_i * s) + 0.5f);
}
/* feed until a window closes (or give up after max samples); how many it took */
static int run(spo2_t *s, const skin_t &k, float rate, float still_s, int max, uint32_t *i)
{
    int32_t r, ir; int n = 0;
    while (n < max) {
        sample(k, rate, (*i)++, &r, &ir); n++;
        if (spo2_feed(s, r, ir, still_s)) return n;
    }
    return -1;
}

int main()
{
    spo2_t s; char w[96]; uint32_t i;
    /* THE KNOWN R: ir 2 % perfusion, red 1.12 % -> R 0.56 -> 107.3 - 25 x 0.56 = 93.3 -> 93 (the line calibrated 2026-10-14); a 1.2 Hz pulse at the spot's 25 Hz */
    skin_t finger = { 50000, 120000, 0.0112f, 0.02f, 1.2f };
    spo2_begin(&s, 25.0f, false); i = 0;
    CHECK(s.win_n == 100 && s.warm_n == 25 && s.v == SPO2_NONE);
    spo2_word(&s, w, sizeof w); CHECK(!strcmp(w, "spo2 no window yet"));
    int took = run(&s, finger, 25.0f, 60.0f, 1000, &i);
    CHECK(took == 125);                                                      /* the 1 s warm-up, then the 4 s window */
    CHECK(s.v == SPO2_OK && s.spo2 == 93 && fabsf(s.r - 0.56f) < 0.02f && s.windows == 1 && s.oks == 1);
    CHECK(s.perf[1] > 0.017f && s.perf[1] < 0.021f);                       /* the ir's 2 %, read through the tracker's attenuation */
    spo2_word(&s, w, sizeof w); CHECK(!strncmp(w, "spo2 ~93 (R 0.5", 15) && strstr(w, "quality ok") && strstr(w, "the line of 2026-10-14"));
    /* the next window follows without a warm-up, the same number */
    took = run(&s, finger, 25.0f, 60.0f, 1000, &i);
    CHECK(took == 100 && s.v == SPO2_OK && s.spo2 == 93 && s.windows == 2 && s.oks == 2);
    /* the raw stream's 100 Hz: the same R, the same number, 400 a window */
    spo2_begin(&s, 100.0f, false); i = 0;
    CHECK(s.win_n == 400 && s.warm_n == 100);
    took = run(&s, finger, 100.0f, 60.0f, 2000, &i);
    CHECK(took == 500 && s.v == SPO2_OK && s.spo2 == 93 && fabsf(s.r - 0.56f) < 0.02f);
    /* a slower sleeper's pulse (0.8 Hz) and a wrist's dim dc: the ratio still cancels the tracker */
    skin_t wrist = { 20000, 90000, 0.0056f, 0.01f, 0.8f };                 /* R 0.56 again */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, wrist, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_OK && s.spo2 >= 92 && s.spo2 <= 94);
    /* another R: red 2 %, ir 2 % -> R 1.0 -> 85 */
    skin_t low = { 50000, 120000, 0.02f, 0.02f, 1.2f };
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, low, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_OK && s.spo2 == 82);   /* R 1.0: 107.3 - 25 */
    /* THE GATES, each in its own words */
    /* the red slot dark by the AGC's word: no red, whatever the signal */
    spo2_begin(&s, 25.0f, true); i = 0;
    run(&s, finger, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_NO_RED && s.spo2 == 0 && s.oks == 0);
    spo2_word(&s, w, sizeof w); CHECK(!strcmp(w, "spo2 no red"));
    /* the red slot's dc under the floor: no red */
    skin_t nored = { 5000, 120000, 0.02f, 0.02f, 1.2f };
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, nored, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_NO_RED);
    /* the wrist moved inside the window (still for less than the window at its close) */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, finger, 25.0f, 2.5f, 1000, &i);
    CHECK(s.v == SPO2_MOVED && s.spo2 == 0);
    spo2_word(&s, w, sizeof w); CHECK(strstr(w, "the wrist moved"));
    /* still again for the next window: a number */
    run(&s, finger, 25.0f, 6.5f, 1000, &i);
    CHECK(s.v == SPO2_OK && s.spo2 == 93);
    /* a thin red (0.1 %): the perfusion gate names the slot */
    skin_t thin = { 50000, 120000, 0.001f, 0.02f, 1.2f };
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, thin, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_LOW_PERF && s.low_slot == 0);
    spo2_word(&s, w, sizeof w); CHECK(strstr(w, "red perfusion 0.") && strstr(w, "under 0.2"));
    skin_t thin_ir = { 50000, 120000, 0.02f, 0.001f, 1.2f };   /* 0.1 %: under the 0.15 floor */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, thin_ir, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_LOW_PERF && s.low_slot == 1);
    spo2_word(&s, w, sizeof w); CHECK(strstr(w, "ir perfusion"));
    /* a flat line (no pulse at all): the perfusion gate, not a number */
    skin_t flat = { 50000, 120000, 0.0f, 0.0f, 1.2f };
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, flat, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_LOW_PERF);
    /* R out of the band both ways */
    skin_t big_r = { 50000, 120000, 0.04f, 0.02f, 1.2f };                  /* R 2.0 */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, big_r, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_R_OUT && fabsf(s.r - 2.0f) < 0.05f && s.spo2 == 0);
    spo2_word(&s, w, sizeof w); CHECK(strstr(w, "R 2.0") && strstr(w, "outside 0.2..1.6"));
    skin_t small_r = { 50000, 120000, 0.006f, 0.02f, 1.2f };               /* R 0.3: his own fingertip's R (2026-10-14) -- inside the band now, the line's 99.8 -> 100 */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, small_r, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_OK && s.spo2 == 100 && s.r < 0.4f);
    skin_t tiny_r = { 50000, 120000, 0.003f, 0.02f, 1.2f };                /* R 0.15: under SPO2_R_LO 0.2, no skin's */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, tiny_r, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_R_OUT && s.r < 0.2f);
    /* the band's edge: R 0.4 is the line's 100, never over */
    skin_t edge = { 50000, 120000, 0.0082f, 0.02f, 1.2f };                 /* R 0.41 -> 97.05 -> 97 on the calibrated line */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, edge, 25.0f, 60.0f, 1000, &i);
    CHECK(s.v == SPO2_OK && s.spo2 == 97);
    /* a restart throws the window away: begin again, the warm-up again */
    spo2_begin(&s, 25.0f, false); i = 0;
    run(&s, finger, 25.0f, 60.0f, 50, &i);                                 /* half a window in */
    CHECK(s.n == 25 && s.windows == 0);
    spo2_begin(&s, 25.0f, false); i = 0;
    CHECK(s.n == 0 && s.seen == 0);
    took = run(&s, finger, 25.0f, 60.0f, 1000, &i);
    CHECK(took == 125 && s.v == SPO2_OK);
    /* THE NIGHT KEEPER: the lowest of the night, taken once, then nothing */
    spo2_night_t k = { 0, 0 }; int n = 0;
    CHECK(spo2_night_take(&k, &n) == 0 && n == 0);
    spo2_night_note(&k, 97); spo2_night_note(&k, 94); spo2_night_note(&k, 96);
    spo2_night_note(&k, 0); spo2_night_note(&k, 120);                        /* no number is no note */
    CHECK(k.lo == 94 && k.n == 3);
    CHECK(spo2_night_take(&k, &n) == 94 && n == 3);
    CHECK(spo2_night_take(&k, &n) == 0 && n == 0 && k.lo == 0);
    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
