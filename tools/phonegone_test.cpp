/* THE PHONE LEFT BEHIND on the laptop (phonegone.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/phonegone_test tools/phonegone_test.cpp -x c src/phonegone.c -lm && /tmp/phonegone_test
 * The first word is no edge; a drop while never walking is nothing; a
 * drop while walking, or inside 60 s of the last stride, is the debt
 * (61 s after: nothing); a drop with the stack going down is the
 * radio's; the phone back before the raise clears it; the raise's
 * look 1.5 s then forward, once, never twice; an empty-house raise
 * spends it with no look; the dark / the house emptying / a page
 * mid-look ends it; the drop in the dark (the nap's polls, 250 ms ..
 * 1 s) is heard; the door point opposite the floor on four walls. */
#include "phonegone.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static phonegone_in_t in;
static uint32_t T = 1000;   /* the clock, ms */

static void reset_in(void) { in.now_ms = T; in.up = true; in.ready = true; in.walking = false; in.raise = false; in.holding = true; in.lit = true; in.page = false; }
/* run ms of frames at step ms; returns the last non-NONE event (or NONE) and counts them */
static int evs_n;
static int run(uint32_t ms, uint32_t step = 50)
{
    int last = PHONEGONE_EV_NONE; evs_n = 0;
    for (uint32_t t = 0; t < ms; t += step) {
        T += step; in.now_ms = T;
        int e = phonegone_tick(&in);
        if (e != PHONEGONE_EV_NONE) { last = e; evs_n++; }
        in.raise = false;   /* the cue is one frame's */
    }
    return last;
}
static int one(void) { return run(50); }
/* the scene: the phone ready, the wrist walking for walk_ms, then still for still_ms, then the phone drops */
static int walk_then_drop(uint32_t walk_ms, uint32_t still_ms)
{
    reset_in(); run(500);
    in.walking = true; run(walk_ms);
    in.walking = false; run(still_ms);
    in.ready = false;
    return one();
}
static void settle(void) { in.ready = true; run(200); }   /* the phone back: every debt gone before the next scene */

int main(void)
{
    puts("-- the first word, and a drop while never walking");
    reset_in(); in.ready = false; run(500);
    CHECK(!phonegone_owed(), "boot with no phone: nothing");
    in.ready = true;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_owed(), "the phone found: no edge of note");
    run(500); in.ready = false;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_owed(), "the phone drops with the wrist never walking: nothing (the phone left, not him)");
    in.ready = true; run(200);

    puts("-- the walk away");
    CHECK(walk_then_drop(3000, 0) == PHONEGONE_EV_GONE && phonegone_owed(), "walking as the phone drops: the debt");
    CHECK(phonegone_drops() == 1 && phonegone_count() == 0, "one walk-away, no look yet");
    in.ready = false; run(5000);
    CHECK(evs_n == 0 && phonegone_owed(), "5 s more of no phone: nothing more (one debt)");
    settle();
    CHECK(walk_then_drop(3000, 59000) == PHONEGONE_EV_GONE, "the drop 59 s after the last stride: inside the window, the debt");
    settle();
    CHECK(walk_then_drop(3000, 61000) == PHONEGONE_EV_NONE && !phonegone_owed(), "the drop 61 s after: outside it, nothing");
    settle();

    puts("-- the radio's own drops are not his");
    reset_in(); run(500); in.walking = true; run(2000);
    in.up = false; in.ready = false;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_owed(), "the stack goes down with the phone (a WiFi dial, the dark house): nothing");
    in.up = true; run(500); in.ready = true; run(500);
    CHECK(!phonegone_owed() && evs_n == 0, "the stack back and the phone with it: nothing owed, no BACK said");
    in.walking = false; run(200);

    puts("-- the phone back before the raise");
    CHECK(walk_then_drop(2000, 1000) == PHONEGONE_EV_GONE, "the debt");
    in.ready = true;
    CHECK(one() == PHONEGONE_EV_BACK && !phonegone_owed(), "the phone back (an iOS hiccup, or he went back for it): cleared");
    in.raise = true;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_on(), "the raise after: no look");
    run(200);

    puts("-- the raise's look");
    CHECK(walk_then_drop(2000, 1000) == PHONEGONE_EV_GONE, "the debt");
    in.walking = false; in.lit = false; run(30000);   /* the dark house: the debt waits */
    CHECK(phonegone_owed() && !phonegone_on(), "30 s dark: still owed, no look in the dark");
    in.lit = true; in.raise = true;
    CHECK(one() == PHONEGONE_EV_LOOK && phonegone_on() && !phonegone_owed(), "the raise: the look toward the door, the debt spent");
    CHECK(fabsf(phonegone_left_s() - PHONEGONE_S) < 0.001f, "... %.2f s of it ahead", phonegone_left_s());
    run(1400);
    CHECK(phonegone_on(), "at 1.45 s still on");
    run(100);
    CHECK(!phonegone_on() && phonegone_left_s() == 0, "at %.1f s forward (at him)", PHONEGONE_S);
    CHECK(phonegone_count() == 1, "one look");
    in.raise = true;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_on(), "a second raise: never twice");
    in.ready = true;
    CHECK(one() == PHONEGONE_EV_NONE, "the phone back after the look: nothing to clear, nothing said");
    run(200);

    puts("-- an empty-house raise spends it");
    CHECK(walk_then_drop(2000, 1000) == PHONEGONE_EV_GONE, "the debt");
    in.holding = false; in.raise = true;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_owed() && !phonegone_on(), "the raise with the house EMPTY: no look, the debt spent (the moment was the raise)");
    in.holding = true; in.raise = true;
    CHECK(one() == PHONEGONE_EV_NONE && !phonegone_on(), "... the next raise with him home: nothing (not a debt kept for days)");
    settle();

    puts("-- ended early");
    const char *names[] = { "the glass dark", "the house emptying", "a page up" };
    for (int k = 0; k < 3; k++) {
        CHECK(walk_then_drop(2000, 500) == PHONEGONE_EV_GONE, "the debt");
        in.raise = true; one(); run(300);
        CHECK(phonegone_on(), "the look running at 0.35 s");
        switch (k) { case 0: in.lit = false; break; case 1: in.holding = false; break; case 2: in.page = true; break; }
        one();
        CHECK(!phonegone_on(), "%s: forward", names[k]);
        reset_in(); settle();
    }

    puts("-- the drop in the dark (the nap's polls)");
    reset_in(); run(500);
    in.walking = true; run(3000);
    in.lit = false; in.holding = true;
    run(2000, 250);                                     /* the nap's 250 ms polls, still walking */
    in.walking = false; run(10000, 1000);               /* the slow polls, still now */
    in.ready = false;
    T += 1000; in.now_ms = T;
    CHECK(phonegone_tick(&in) == PHONEGONE_EV_GONE && phonegone_owed(), "the phone drops 11 s into the stillness, heard at a 1 s poll: the debt");
    run(5000, 1000);
    in.lit = true; in.raise = true;
    CHECK(one() == PHONEGONE_EV_LOOK, "the raise that lights the glass: the look");
    run(2000); settle();

    puts("-- the air door: the rim opposite the floor (r 116 from 120,120)");
    float x, y;
    phonegone_point(0.0f, 120, 120, PHONEGONE_R, &x, &y);
    CHECK(fabsf(x - 120) < 0.01f && fabsf(y - 4) < 0.01f, "the floor at the bottom (phi 0): the door at the top %.0f,%.0f", x, y);
    phonegone_point((float)M_PI, 120, 120, PHONEGONE_R, &x, &y);
    CHECK(fabsf(x - 120) < 0.01f && fabsf(y - 236) < 0.01f, "the floor at the top (phi pi): the door at the bottom %.0f,%.0f", x, y);
    phonegone_point((float)M_PI / 2, 120, 120, PHONEGONE_R, &x, &y);
    CHECK(fabsf(x - 4) < 0.01f && fabsf(y - 120) < 0.01f, "phi pi/2: the door at %.0f,%.0f (the far side)", x, y);
    phonegone_point(-(float)M_PI / 2, 120, 120, PHONEGONE_R, &x, &y);
    CHECK(fabsf(x - 236) < 0.01f && fabsf(y - 120) < 0.01f, "phi -pi/2: the door at %.0f,%.0f", x, y);

    printf("phonegone_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
