// c++ -std=gnu++17 -I src -o /tmp/perfusion_test tools/perfusion_test.cpp -lm && /tmp/perfusion_test
/* THE PERFUSION INDEX: the ruler is spo2.h's, the four bands fall at
 * 0.1 / 0.3 / 1 %, each band says its line, a failed read names the
 * strap only when the strap is the cause, the day's tally counts and
 * finds its median without a buffer, the header byte says 0 / 255 /
 * tenths and comes back as a band. */
#include "perfusion.h"
#include <cstdio>
#include <cstring>
#include <cmath>
static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main()
{
    char w[96];
    /* THE RULER: a sinusoid of peak-to-peak p over a dc has an RMS of p / (2 sqrt 2): the index is p / dc in percent */
    float dc = 120000, pp = 0.02f * dc;                                      /* 2 % peak-to-peak: a fingertip in a clip */
    float pi = perf_index(pp / SPO2_PP_OF_RMS, dc);
    CHECK(fabsf(pi - 2.0f) < 0.001f);
    CHECK(perf_index(100, 0) == 0 && perf_index(-1, dc) == 0);              /* no dc, no index; no negative ac */
    /* THE BANDS, their edges (10-14: his loose fingertip's red ran 0.12-0.36 %) */
    CHECK(perf_band(0.0f) == PERF_NONE && perf_band(0.09f) == PERF_NONE);
    CHECK(perf_band(0.1f) == PERF_THIN && perf_band(0.12f) == PERF_THIN && perf_band(0.29f) == PERF_THIN);
    CHECK(perf_band(0.3f) == PERF_FAIR && perf_band(0.36f) == PERF_FAIR && perf_band(0.99f) == PERF_FAIR);
    CHECK(perf_band(1.0f) == PERF_GOOD && perf_band(2.0f) == PERF_GOOD && perf_band(20.0f) == PERF_GOOD);
    CHECK(!strcmp(perf_band_name(PERF_NONE), "NONE") && !strcmp(perf_band_name(PERF_THIN), "THIN"));
    CHECK(!strcmp(perf_band_name(PERF_FAIR), "FAIR") && !strcmp(perf_band_name(PERF_GOOD), "GOOD"));
    CHECK(!strcmp(perf_band_line(PERF_NONE), "nothing on the sensor") && !strcmp(perf_band_line(PERF_THIN), "a loose strap"));
    /* EVERY READ'S CLAUSE */
    perf_word(0.28f, w, sizeof w);  CHECK(!strcmp(w, "perfusion 0.28 % (THIN: a loose strap)"));
    perf_word(0.05f, w, sizeof w);  CHECK(!strcmp(w, "perfusion 0.05 % (NONE: nothing on the sensor)"));
    perf_word(0.5f, w, sizeof w);   CHECK(!strcmp(w, "perfusion 0.50 % (FAIR: fair)"));
    perf_word(1.6f, w, sizeof w);   CHECK(!strcmp(w, "perfusion 1.60 % (GOOD: good)"));
    /* A FAILED READ'S VERDICT: the strap named only when it is the cause */
    w[0] = 0;
    CHECK(perf_fail_word(0.22f, "irregular (cv 0.32)", w, sizeof w) && !strcmp(w, "THIN perfusion (the strap): irregular (cv 0.32)"));
    CHECK(perf_fail_word(0.04f, "3 beats found, need 6", w, sizeof w) && !strcmp(w, "NONE perfusion (nothing on the sensor): 3 beats found, need 6"));
    strcpy(w, "untouched");
    CHECK(!perf_fail_word(0.5f, "irregular (cv 0.32)", w, sizeof w) && !strcmp(w, "untouched"));   /* FAIR: the cause is elsewhere, the words stay */
    CHECK(!perf_fail_word(3.0f, "x", w, sizeof w));
    CHECK(perf_fail_word(0.2f, NULL, w, sizeof w) && !strcmp(w, "THIN perfusion (the strap): "));
    /* THE DAY'S TALLY: the bands counted, the median off the histogram */
    perf_day_t d; perf_day_clear(&d);
    CHECK(perf_day_median_tenths(&d) == -1 && d.count == 0);
    CHECK(perf_hdr_byte(perf_day_median_tenths(&d)) == 0);                   /* no read today: the byte stays 0 */
    perf_tally_word(&d, w, sizeof w); CHECK(!strcmp(w, "perfusion today: none 0 thin 0 fair 0 good 0"));
    float reads[] = { 0.05f, 0.08f, 0.12f, 0.15f, 0.18f, 0.2f, 0.22f, 0.25f, 0.26f, 0.28f, 0.29f, 0.31f, 0.4f, 0.8f };
    for (float p : reads) perf_day_note(&d, p);
    CHECK(d.count == 14 && d.n[PERF_NONE] == 2 && d.n[PERF_THIN] == 9 && d.n[PERF_FAIR] == 3 && d.n[PERF_GOOD] == 0);
    perf_tally_word(&d, w, sizeof w); CHECK(!strcmp(w, "perfusion today: none 2 thin 9 fair 3 good 0"));
    CHECK(perf_day_median_tenths(&d) == 2);                                  /* the 7th of 14 (the lower middle) is 0.22 -> the 0.2 bin */
    perf_day_note(&d, -1.0f); perf_day_note(&d, NAN);                        /* no index is no note */
    CHECK(d.count == 14);
    int n = -1; uint8_t b = perf_day_take(&d, &n);
    CHECK(b == 2 && n == 14);
    CHECK(d.count == 0 && d.n[PERF_THIN] == 0 && perf_day_median_tenths(&d) == -1);   /* taken: tomorrow starts clean */
    CHECK(perf_hdr_band(b) == PERF_THIN);
    /* a NONE day: reads taken, the middle one under a tenth -> the byte says 255, never 0 (0 is no read) */
    perf_day_note(&d, 0.02f); perf_day_note(&d, 0.04f); perf_day_note(&d, 0.5f);
    CHECK(perf_day_median_tenths(&d) == 0);
    b = perf_day_take(&d, &n);
    CHECK(b == PERF_HDR_NONE && n == 3 && perf_hdr_band(b) == PERF_NONE);
    CHECK(perf_hdr_band(0) == PERF_NONE);
    /* a FAIR day, a GOOD day: the tenths, the cap */
    perf_day_note(&d, 0.3f); perf_day_note(&d, 0.5f); perf_day_note(&d, 0.6f);
    CHECK(perf_day_take(&d, NULL) == 5 && perf_hdr_band(5) == PERF_FAIR);
    perf_day_note(&d, 1.4f); perf_day_note(&d, 1.5f);
    CHECK(perf_day_take(&d, NULL) == 14 && perf_hdr_band(14) == PERF_GOOD);  /* two reads: the lower middle */
    perf_day_note(&d, 30.0f); perf_day_note(&d, 40.0f); perf_day_note(&d, 50.0f);
    CHECK(perf_day_median_tenths(&d) == PERF_HIST_N - 1);                    /* past the histogram: pooled in the last bin */
    CHECK(perf_day_take(&d, NULL) == PERF_HIST_N - 1 && perf_hdr_band(PERF_HIST_N - 1) == PERF_GOOD);
    CHECK(perf_hdr_byte(300) == PERF_HDR_MAX && perf_hdr_byte(254) == 254);
    /* a single read is its own median */
    perf_day_note(&d, 0.28f);
    CHECK(perf_day_median_tenths(&d) == 3);                                  /* 0.28 rounds to the 0.3 bin */
    CHECK(perf_day_take(&d, NULL) == 3);
    /* the bin's cap: 300 reads in one bin stay 255, the median still there */
    for (int i = 0; i < 300; i++) perf_day_note(&d, 0.2f);
    CHECK(d.hist[2] == 255 && d.count == 300 && perf_day_median_tenths(&d) == 2);
    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
