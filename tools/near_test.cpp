/* THE DESK NEXT DOOR on the laptop (near.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/near_test tools/near_test.cpp -x c src/near.c && /tmp/near_test
 * The poll's cadence (every 2 s lit, at once on the lit edge, never
 * in the dark); no desk = no band; a band first known is no edge; the
 * walk in FAR -> NEAR -> AT HAND with the one edge; the hysteresis on
 * both lines (no flap on a wobble); the smoothing (one spike is not a
 * band); the fall back out; the dark ages the band to NONE so the
 * next light's first sample is no edge; the desk leaving; the words. */
#include "near.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* a lit stretch of `secs` at one steady RSSI, polled as the frame would: the events counted by kind */
static uint32_t T = 1000;
static int evs_band = 0, evs_athand = 0;
static void lit_s(float secs, int rssi)
{
    int frames = (int)(secs * 20.0f + 0.5f);               /* 20 Hz is enough: the poll is 2 s */
    for (int i = 0; i < frames; i++) {
        T += 50;
        if (near_due(T, true)) { int ev = near_feed(rssi); if (ev == NEAR_EV_BAND) evs_band++; else if (ev == NEAR_EV_ATHAND) evs_athand++; }
    }
}
static int dark_s(float secs)                              /* the dark's 250 ms; how many frames asked (must be 0) */
{
    int frames = (int)(secs * 4.0f + 0.5f), asked = 0;
    for (int i = 0; i < frames; i++) { T += 250; if (near_due(T, false)) asked++; }
    return asked;
}
static void reset_evs(void) { evs_band = evs_athand = 0; }

int main(void)
{
    puts("-- the poll's cadence");
    {
        int asked = 0;
        for (int i = 0; i < 40; i++) { T += 50; if (near_due(T, true)) { asked++; near_feed(0); } }   /* 2 s lit */
        CHECK(asked == 1, "the first lit frame asks at once, then nothing for 2 s: asked %d", asked);
        asked = 0;
        for (int i = 0; i < 200; i++) { T += 50; if (near_due(T, true)) { asked++; near_feed(0); } }   /* 10 s more */
        CHECK(asked == 5, "10 s lit = five asks: %d", asked);
        asked = dark_s(5);
        CHECK(asked == 0, "5 s dark = no ask: %d", asked);
        T += 250; CHECK(near_due(T, true), "the lit edge asks at once");
        near_feed(0);
    }

    puts("-- no desk on the air door = no band");
    CHECK(near_band() == NEAR_NONE, "band NONE with rssi 0");
    CHECK(!near_is_near(), "not near");
    CHECK(near_dbm() == 0, "no dBm");

    puts("-- the walk in: FAR, NEAR, AT HAND -- one edge");
    reset_evs();
    lit_s(10, -85);
    CHECK(near_band() == NEAR_FAR, "-85 dBm = FAR (%s)", near_word(near_band()));
    CHECK(evs_band == 1 && evs_athand == 0, "a band first known is the word, no edge (band %d, athand %d)", evs_band, evs_athand);
    CHECK(near_dbm() == -85, "smoothed -85: %d", near_dbm());
    reset_evs();
    lit_s(12, -66);
    CHECK(near_band() == NEAR_NEAR, "-66 dBm (over -70) = NEAR (%s)", near_word(near_band()));
    CHECK(near_is_near(), "near: the cramp's word");
    CHECK(evs_band == 1 && evs_athand == 0, "FAR -> NEAR is the word only (band %d, athand %d)", evs_band, evs_athand);
    reset_evs();
    lit_s(12, -50);
    CHECK(near_band() == NEAR_ATHAND, "-50 dBm (over -55) = AT HAND (%s)", near_word(near_band()));
    CHECK(evs_athand == 1 && evs_band == 0, "NEAR -> AT HAND is THE EDGE, once (athand %d, band %d)", evs_athand, evs_band);
    CHECK(near_glances() == 1, "one rise counted");
    reset_evs();
    lit_s(30, -50);
    CHECK(evs_athand == 0 && evs_band == 0, "held AT HAND: nothing more (athand %d, band %d)", evs_athand, evs_band);

    puts("-- the hysteresis: a wobble on the line never flaps");
    reset_evs();
    for (int i = 0; i < 10; i++) { lit_s(2, -58); lit_s(2, -53); }   /* across -55, never under -60 */
    CHECK(near_band() == NEAR_ATHAND && evs_band == 0 && evs_athand == 0, "-58 / -53 wobble: AT HAND held (band %d, athand %d)", evs_band, evs_athand);
    lit_s(12, -62);
    CHECK(near_band() == NEAR_NEAR, "-62 (under -60) = NEAR again (%s)", near_word(near_band()));
    CHECK(evs_band == 1 && evs_athand == 0, "the fall is the word only");
    reset_evs();
    lit_s(12, -52);
    CHECK(near_band() == NEAR_ATHAND && evs_athand == 1, "back over -55 = AT HAND, the edge again (athand %d)", evs_athand);
    reset_evs();
    lit_s(12, -62);
    reset_evs();
    for (int i = 0; i < 10; i++) { lit_s(2, -73); lit_s(2, -68); }
    CHECK(near_band() == NEAR_NEAR && evs_band == 0, "-73 / -68 wobble (across -70, never under -75): NEAR held (band %d)", evs_band);
    lit_s(12, -78);
    CHECK(near_band() == NEAR_FAR, "-78 (under -75) = FAR (%s)", near_word(near_band()));
    reset_evs();
    for (int i = 0; i < 10; i++) { lit_s(2, -73); lit_s(2, -68); }
    CHECK(near_band() == NEAR_NEAR && evs_band == 1, "-73 / -68 wobble from FAR: up to NEAR once, then held (band %d)", evs_band);

    puts("-- the smoothing: one spike is not a band");
    lit_s(12, -80); reset_evs();
    CHECK(near_band() == NEAR_FAR, "settled FAR at -80");
    { T += 2000; if (near_due(T, true)) near_feed(-60); }       /* one reflection, 20 dB up */
    CHECK(near_band() == NEAR_FAR, "a single -60 sample from -80 moves the smoothed to %d: still FAR (%s)", near_dbm(), near_word(near_band()));
    lit_s(2.05f, -80);
    CHECK(near_dbm() <= -70, "and it is back under the line next poll: %d", near_dbm());
    CHECK(evs_band == 0 && evs_athand == 0, "no event from the spike");
    lit_s(8, -50);
    CHECK(near_band() == NEAR_ATHAND, "but a HELD -50 settles AT HAND inside 8 s (%d dBm)", near_dbm());

    puts("-- the dark ages the band; the next light's first sample is no edge");
    reset_evs();
    CHECK(dark_s(5) == 0, "5 s dark asks nothing");
    CHECK(near_band() == NEAR_ATHAND, "5 s dark: the band still held (%s)", near_word(near_band()));
    CHECK(dark_s(6) == 0, "6 s more dark asks nothing");
    CHECK(near_band() == NEAR_NONE, "11 s dark: the band is NONE (%s)", near_word(near_band()));
    CHECK(near_dbm() == 0, "and the smoothing forgotten");
    reset_evs();
    lit_s(1, -50);
    CHECK(near_band() == NEAR_ATHAND, "lit again at the desk: AT HAND at the first sample");
    CHECK(evs_band == 1 && evs_athand == 0, "...the word, NOT the edge (band %d, athand %d)", evs_band, evs_athand);
    CHECK(near_glances() == 3, "the rises still three (the walk in, the wobble's return, the smoothing's held -50)");

    puts("-- the desk leaves, comes back at arm's length: no edge; walks in from far: the edge");
    reset_evs();
    lit_s(4, 0);
    CHECK(near_band() == NEAR_NONE && evs_band == 1, "rssi 0 = NONE, the word once (band %d)", evs_band);
    reset_evs();
    lit_s(4, -45);
    CHECK(near_band() == NEAR_ATHAND && evs_athand == 0 && evs_band == 1, "back AT HAND from NONE: the word only (athand %d)", evs_athand);
    reset_evs();
    lit_s(4, 0); lit_s(12, -90); lit_s(12, -45);
    CHECK(near_band() == NEAR_ATHAND && evs_athand == 1, "NONE -> FAR -> AT HAND: the edge (athand %d)", evs_athand);
    CHECK(near_glances() == 4, "four rises");

    puts("-- the words");
    CHECK(!strcmp(near_word(NEAR_NONE), "--"), "NONE is --");
    CHECK(!strcmp(near_word(NEAR_FAR), "FAR") && !strcmp(near_word(NEAR_NEAR), "NEAR") && !strcmp(near_word(NEAR_ATHAND), "AT HAND"), "FAR / NEAR / AT HAND");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
