/* Host test of the gesture detector (src/gesture.cpp): synthetic taps
 * and accelerations fed through gesture_tick at the loop's own cadence,
 * the events asserted. THE SWIPES (2026-09-23): the flick's 300 ms line
 * pinned both sides (250 / 290 ms a swipe, 310 / 350 a stroke), its
 * 40 px floor, the feeding axis never a swipe; THE GEL'S FRAME -- the
 * feed, the page's drag, the stroke and the swipe on each of the four
 * walls (gesture_set_wall), the rim's arc in the screen's frame still.
 * THE ECHOED RHYTHM (2026-10-09): four taps open it at the fourth (the
 * knocks still at the third), three are the knocks only, a late fourth
 * is a pet, the gap's close with the intervals and the age, the cap at
 * six closing at once, a seventh swallowed, a hold dropping it.
 * Build: c++ -std=gnu++17 -O1 -o gesture_test tools/gesture_test.cpp src/gesture.cpp -lm
 * Run:   ./gesture_test
 */
#include "../src/gesture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- the harness: a clock, a glass, a wrist ---------------------------------- */

static uint32_t now_ms = 1000;
static touch_t  glass = { false, 120, 120, 0 };
static vec3_t   wrist = { 0, 0, 1 };        /* at rest: 1 g on z */
static bool     screen_on = true;
static gesture_event_t got[64];
static int      ngot = 0;
static gesture_event_t got_last;            /* the last event of the last expect(), whole (dx, dy) */

static const int TICK_MS = 4;               /* the loop: IMU at 250 Hz, touch polled every pass */
/* THE ARC's live events are many (one a degree): counted aside, the last one kept */
static int arc_moves = 0;
static gesture_event_t arc_last_move;

static void tick(void)
{
    gesture_event_t ev[4];
    int n = gesture_tick(now_ms, &wrist, &glass, screen_on, ev, 4);
    for (int i = 0; i < n && i < 4; i++) {
        if (ev[i].kind == GES_ARC_MOVE) { arc_moves++; arc_last_move = ev[i]; continue; }
        if (ngot < 64) got[ngot++] = ev[i];
    }
    now_ms += TICK_MS;
    /* a resting IMU sample is never bit-identical twice: the last bit of noise */
    wrist.z += (float)((int)(now_ms % 3) - 1) * 1e-4f;
}

static void run_ms(uint32_t ms) { uint32_t end = now_ms + ms; while (now_ms < end) tick(); }

static void press(int x, int y, uint32_t held_ms)
{
    glass.down = true; glass.x = x; glass.y = y;
    run_ms(held_ms);
    glass.down = false;
    tick();
}

/* a drag: the finger lands at (x0, y0) and slides to (x1, y1) over ``ms``,
 * one sample a tick, then lifts */
static void drag(int x0, int y0, int x1, int y1, uint32_t ms)
{
    uint32_t start = now_ms, end = now_ms + ms;
    glass.down = true; glass.x = x0; glass.y = y0;
    while (now_ms < end) {
        float k = (float)(now_ms - start) / (float)ms;
        glass.x = x0 + (int)((x1 - x0) * k);
        glass.y = y0 + (int)((y1 - y0) * k);
        tick();
    }
    glass.x = x1; glass.y = y1; tick();
    glass.down = false;
    tick();
}

/* THE TICKLE: one finger landing at (x, y) and scrubbed ``legs`` times,
 * each leg ``px`` along (ax, ay) and back the next, ``leg_ms`` a leg,
 * one sample a tick, then lifted -- legs - 1 reversals in one touch */
static void scrub(int x, int y, int ax, int ay, int px, int legs, uint32_t leg_ms)
{
    glass.down = true; glass.x = x; glass.y = y; tick();
    for (int l = 0; l < legs; l++) {
        int sgn = (l & 1) ? -1 : 1;
        uint32_t start = now_ms, end = now_ms + leg_ms;
        while (now_ms < end) {
            float k = (float)(now_ms - start) / (float)leg_ms;
            int d = (int)(px * (sgn > 0 ? k : 1.0f - k));
            glass.x = x + ax * d; glass.y = y + ay * d;
            tick();
        }
    }
    glass.down = false;
    tick();
}

/* THE ARC: a finger drawn round the rim at radius ``r`` from angle
 * ``deg0`` to ``deg1`` (from 12 o'clock, clockwise) over ``ms`` */
static void arc_drag(float deg0, float deg1, float r, uint32_t ms)
{
    uint32_t start = now_ms, end = now_ms + ms;
    arc_moves = 0;
    glass.down = true;
    for (;;) {
        float k = now_ms >= end ? 1.0f : (float)(now_ms - start) / (float)ms;
        float a = (deg0 + (deg1 - deg0) * k) * (float)M_PI / 180.0f;
        glass.x = 120 + (int)lroundf(sinf(a) * r); glass.y = 120 - (int)lroundf(cosf(a) * r);
        tick();
        if (k >= 1.0f) break;
    }
    glass.down = false;
    tick();
}

/* a shake: excursions on the x axis, +G then -G then +G ... each ``half_ms``
 * long, ``halves`` of them, at ``g`` */
static void shake_x(int halves, uint32_t half_ms, float g)
{
    for (int h = 0; h < halves; h++) {
        float sgn = (h & 1) ? -1.0f : 1.0f;
        uint32_t end = now_ms + half_ms;
        while (now_ms < end) {
            /* a half sine of the excursion */
            float ph = 1.0f - (float)(end - now_ms) / (float)half_ms;
            wrist.x = sgn * g * sinf((float)M_PI * ph);
            tick();
        }
    }
    wrist.x = 0;
}

static int fails = 0, checks = 0;
static void expect(const char *what, const gesture_kind_t *kinds, int n)
{
    checks++;
    bool ok = ngot == n;
    for (int i = 0; ok && i < n; i++) ok = got[i].kind == kinds[i];
    printf("%s %-42s", ok ? "ok  " : "FAIL", what);
    printf(" got %d:", ngot);
    for (int i = 0; i < ngot; i++) printf(" %s(%d)", gesture_name(got[i].kind), got[i].n);
    if (ngot) got_last = got[ngot - 1];
    if (!ok) {
        printf("  wanted %d:", n);
        for (int i = 0; i < n; i++) printf(" %s", gesture_name(kinds[i]));
        fails++;
    }
    printf("\n");
    ngot = 0;
}
#define EXPECT(what, ...) do { gesture_kind_t k_[] = { __VA_ARGS__ }; expect(what, k_, (int)(sizeof k_ / sizeof k_[0])); } while (0)
#define EXPECT_NONE(what) do { expect(what, NULL, 0); } while (0)

/* how many of a kind came (for the long stretches under the lock, where
 * the note "touch under the lock" comes every 5 s and is not the point) */
static int count_kind(gesture_kind_t k)
{
    int c = 0;
    for (int i = 0; i < ngot; i++) if (got[i].kind == k) c++;
    return c;
}
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* A DRINK ON A HOT DAY: two taps, then a second of ticks reading the
 * settle's peak over the standing line (the double tap surfaces ~350 ms
 * after the second lift; its settle is 0.4 s) */
static float drink_peak(void)
{
    press(120, 120, 90); run_ms(150); press(122, 118, 80);
    float peak = 0;
    uint32_t end = now_ms + 1000;
    while (now_ms < end) { tick(); float s = gesture_squash() - gesture_drink_line(); if (s > peak) peak = s; }
    return peak;
}

/* a wet glass flickering: ``blips`` touches of 12-27 ms, 40 ms apart
 * (~0.75 s for eight; one PHANTOM burst is read at the 5th) */
static void flicker(int blips)
{
    for (int i = 0; i < blips; i++) { press(60 + i * 9, 80 + (i % 3) * 20, 12 + (i % 4) * 5); run_ms(40); }
}
/* a storm: ``bursts`` flickers with ``gap_ms`` of nothing between (past the 1.5 s deaf) */
static void storm(int bursts, uint32_t gap_ms)
{
    for (int b = 0; b < bursts; b++) { flicker(8); run_ms(gap_ms); }
}

int main(void)
{
    gesture_begin(now_ms);
    run_ms(2000);                                   /* the filters settle */
    EXPECT_NONE("rest");

    /* --- taps --------------------------------------------------------------- */
    press(120, 120, 90);
    run_ms(200);
    EXPECT_NONE("one tap: nothing inside the double window");
    run_ms(400);
    EXPECT("one tap: the pet after the window closes", GES_TAP);

    press(120, 120, 90); run_ms(150); press(122, 118, 80);
    run_ms(600);
    EXPECT("two taps: water", GES_DOUBLE_TAP);
    if (gesture_squash() <= 0) { printf("FAIL the drink's settle should be showing\n"); fails++; }
    run_ms(600);
    if (gesture_squash() != gesture_drink_line()) { printf("FAIL the drink's settle should be over (%.3f; THE DRINK LINE %.3f stays)\n", gesture_squash(), gesture_drink_line()); fails++; }

    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    EXPECT("three taps: the knocks, at once", GES_TRIPLE_TAP);
    if (gesture_lump() != 0) { printf("FAIL the knock is not the meal: no lump (%.2f)\n", gesture_lump()); fails++; }
    run_ms(600);
    EXPECT_NONE("three taps: nothing more after the window");

    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    run_ms(200);
    press(120, 120, 1200);
    run_ms(600);
    EXPECT("three taps then a 1.2 s hold: the knocks, then the lease's hold (main: release)", GES_TRIPLE_TAP, GES_LEASE_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    run_ms(200);
    press(120, 120, 1700);
    run_ms(600);
    EXPECT("three taps then a 1.7 s hold: the knocks, then the lease's hold too (the window reaches 1.9 s)", GES_TRIPLE_TAP, GES_LEASE_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    run_ms(200);
    press(120, 120, 1950);
    run_ms(600);
    EXPECT("three taps then a 1.95 s hold: the knocks, then the long press (past the window, lifted before 2 s)", GES_TRIPLE_TAP, GES_LONG_PRESS);

    press(120, 120, 15);
    run_ms(600);
    EXPECT_NONE("a 15 ms blip is not a tap");

    /* --- drags: the stroke and the feed ----------------------------------------- */
    drag(60, 120, 150, 120, 400);
    run_ms(600);
    EXPECT("a 90 px drag right in 400 ms: the stroke", GES_STROKE);
    if (got_last.dx != 90 || got_last.dy != 0) { printf("FAIL the stroke's direction should be +90,0 (got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    if (gesture_stroke() <= 0) { printf("FAIL the stroke's lean should be showing\n"); fails++; }
    if (gesture_squash() <= 0) { printf("FAIL the stroke's settle should be showing (%.3f)\n", gesture_squash()); fails++; }
    run_ms(1000);
    if (gesture_squash() != gesture_drink_line()) { printf("FAIL the stroke's settle should be over at 1.6 s (%.3f; the drink line %.3f stays)\n", gesture_squash(), gesture_drink_line()); fails++; }
    if (gesture_stroke() <= 0) { printf("FAIL the stroke's lean should still be easing back\n"); fails++; }
    run_ms(600);
    if (gesture_stroke() != 0) { printf("FAIL the stroke's lean should be gone at 2.2 s (%.3f)\n", gesture_stroke()); fails++; }

    drag(150, 120, 60, 120, 400);
    run_ms(600);
    EXPECT("a 90 px drag left: the stroke, the other way", GES_STROKE);
    if (got_last.dx != -90) { printf("FAIL the stroke's direction should be -90 (got %d)\n", got_last.dx); fails++; }

    drag(120, 120, 150, 120, 200);
    run_ms(600);
    EXPECT("a 30 px drag: a tap (a wobbly one)", GES_TAP);

    drag(60, 120, 150, 120, 2000);
    run_ms(600);
    EXPECT("a slow 2 s drag: the long press, as ever (a finger still on the glass)", GES_LONG_PRESS);

    drag(60, 120, 150, 120, 800);
    run_ms(600);
    EXPECT("a 90 px drag in 0.8 s: still the stroke (under 1.5 s)", GES_STROKE);
    if (gesture_stroke_ms() < 700 || gesture_stroke_ms() > 900) { printf("FAIL the stroke's length should be about 800 ms (got %lu; THE LULL reads it)\n", (unsigned long)gesture_stroke_ms()); fails++; }

    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("a drag down, top third to bottom third: the feed", GES_FEED);
    if (got_last.dy != 160) { printf("FAIL the feed's travel should be 0,+160 (got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    if (gesture_lump() < 0.97f) { printf("FAIL the lump should have risen (%.2f)\n", gesture_lump()); fails++; }
    if (gesture_squash() != gesture_drink_line()) { printf("FAIL the lump is the dome's own term, not a squash (%.3f; the drink line %.3f stays)\n", gesture_squash(), gesture_drink_line()); fails++; }

    drag(120, 200, 120, 40, 500);
    run_ms(600);
    EXPECT("a drag up, bottom third to top third: THE WATCH PAGE", GES_DRAG_UP);
    if (got_last.dy != -160) { printf("FAIL the page drag's travel should be 0,-160 (got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    if (gesture_lump() < 0.9f) { printf("FAIL the drag up is not a meal: the lump before it stays (%.2f)\n", gesture_lump()); fails++; }

    drag(120, 150, 120, 40, 500);
    run_ms(600);
    EXPECT_NONE("a drag up from the middle: not from the bottom, nothing");

    drag(120, 200, 120, 100, 500);
    run_ms(600);
    EXPECT_NONE("a drag up that stops in the middle: not to the top, nothing");

    drag(120, 100, 120, 200, 500);
    run_ms(600);
    EXPECT_NONE("a drag down from the middle: not from the top, nothing");

    drag(130, 40, 100, 200, 500);
    run_ms(600);
    EXPECT("a drag down, top to bottom, a little sideways: still the feed", GES_FEED);
    if (gesture_lump() < 0.97f) { printf("FAIL the feed: the lump at 1.0 (0.98 after 0.6 s: %.2f)\n", gesture_lump()); fails++; }
    run_ms(10000);
    {   /* the ease-out: (1 - t/60)^2 -- most of the bulge goes early, the last of it lingers */
        float l10 = gesture_lump();
        if (l10 > 0.72f || l10 < 0.66f) { printf("FAIL the lump at 10 s should be ~0.69 (%.2f)\n", l10); fails++; }
        run_ms(20000);
        float l30 = gesture_lump();
        if (l30 > 0.28f || l30 < 0.22f) { printf("FAIL the lump at 30 s should be ~0.25 (%.2f)\n", l30); fails++; }
        if ((1.0f - l10) / 10.0f <= (l10 - l30) / 20.0f) { printf("FAIL the sinking should be fastest first (%.2f -> %.2f -> %.2f)\n", 1.0f, l10, l30); fails++; }
    }
    run_ms(29000);
    if (gesture_lump() <= 0 || gesture_lump() > 0.01f) { printf("FAIL the lump at 59 s should be the last trace (%.3f)\n", gesture_lump()); fails++; }
    run_ms(2000);
    if (gesture_lump() != 0) { printf("FAIL the lump should be gone at 61 s (%.2f)\n", gesture_lump()); fails++; }

    /* the top-up: a second meal while one is sinking fills the belly back
     * to 1.0 -- max, not sum -- and the clock starts over */
    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("a feed", GES_FEED);
    run_ms(30000);
    if (gesture_lump() > 0.28f) { printf("FAIL half sunk (%.2f)\n", gesture_lump()); fails++; }
    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("a second feed while the first sinks: the top-up", GES_FEED);
    if (gesture_lump() < 0.97f || gesture_lump() > 1.0f) { printf("FAIL the top-up: back to 1.0, never over (%.2f)\n", gesture_lump()); fails++; }
    run_ms(30000);
    if (gesture_lump() > 0.28f || gesture_lump() < 0.22f) { printf("FAIL the top-up's own 60 s: ~0.25 at 30 s (%.2f)\n", gesture_lump()); fails++; }
    run_ms(31000);
    if (gesture_lump() != 0) { printf("FAIL the top-up gone at 61 s (%.2f)\n", gesture_lump()); fails++; }

    drag(60, 60, 160, 160, 400);
    run_ms(600);
    EXPECT("a diagonal drag: the stroke", GES_STROKE);
    if (got_last.dx != 100 || got_last.dy != 100) { printf("FAIL the diagonal's direction (got %d,%d)\n", got_last.dx, got_last.dy); fails++; }

    drag(60, 120, 150, 120, 200); drag(150, 120, 60, 120, 200);
    run_ms(600);
    EXPECT("two flicks, there and back, 200 ms each: two swipes (THE SWIPES: quick is a swipe, not a scrub)", GES_SWIPE, GES_SWIPE);
    drag(60, 120, 150, 120, 400); drag(150, 120, 60, 120, 400);
    run_ms(600);
    EXPECT("two strokes, there and back, 400 ms each: two strokes", GES_STROKE, GES_STROKE);

    /* --- THE SWIPES (design/watch-swipes.md, 2026-09-23): the flick's speed line, 300 ms ---- */
    gesture_unshow();                       /* the strokes above still easing back: a clean gel */
    drag(60, 120, 150, 120, 250);
    run_ms(600);
    EXPECT("a 90 px drag right in 250 ms: THE SWIPE (flicked, under 300 ms)", GES_SWIPE);
    if (got_last.dx != 90 || got_last.dy != 0) { printf("FAIL the swipe's travel should be +90,0 (got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    check("the swipe's n is the ms it took (~250)", got_last.n >= 240 && got_last.n <= 262);
    check("a swipe shows nothing of its own on the gel (no lean, no settle: the shove is main's)", gesture_stroke() == 0 && gesture_squash() == 0);
    drag(60, 120, 150, 120, 350);
    run_ms(600);
    EXPECT("the same 90 px drawn in 350 ms: THE STROKE (drawn, over 300 ms)", GES_STROKE);
    drag(60, 120, 150, 120, 290);
    run_ms(600);
    EXPECT("290 ms: the swipe (the line's near side)", GES_SWIPE);
    drag(60, 120, 150, 120, 310);
    run_ms(600);
    EXPECT("310 ms: the stroke (the line's far side)", GES_STROKE);
    drag(150, 120, 60, 120, 200);
    run_ms(600);
    EXPECT("a flick left: the swipe, the other way", GES_SWIPE);
    if (got_last.dx != -90) { printf("FAIL the swipe's direction should be -90 (got %d)\n", got_last.dx); fails++; }
    drag(120, 120, 165, 120, 200);
    run_ms(600);
    EXPECT("a 45 px flick in 200 ms: the swipe (the flick's floor is 40 px, under the stroke's 60)", GES_SWIPE);
    drag(120, 120, 155, 120, 200);
    run_ms(600);
    EXPECT("a 35 px flick: a tap (a wobbly one), under the flick's floor", GES_TAP);
    drag(120, 120, 165, 120, 350);
    run_ms(600);
    EXPECT("a 45 px drag in 350 ms: not a flick, under the stroke's 60: the wobbly tap it always was", GES_TAP);
    drag(120, 100, 120, 190, 200);
    run_ms(600);
    EXPECT_NONE("a 90 px flick DOWN from the middle: the feeding axis is not the swipe's -- nothing (not from the crown's third)");
    drag(120, 40, 120, 200, 250);
    run_ms(600);
    EXPECT("a fast drag down across the thirds, 250 ms: still the feed (the feeding axis is the feed's, quick or slow)", GES_FEED);
    gesture_unshow();
    drag(120, 200, 120, 40, 250);
    run_ms(600);
    EXPECT("a fast drag up across the thirds: still the page's drag", GES_DRAG_UP);
    drag(60, 60, 160, 160, 200);
    run_ms(600);
    EXPECT("a diagonal flick: the swipe (as a diagonal drag is the stroke)", GES_SWIPE);
    {   /* a quick scrub: 100 px out and most of the way back inside one touch (45 px net of 155 drawn): no direction */
        glass.down = true; glass.x = 60; glass.y = 120;
        for (int i = 0; i < 20; i++) { glass.x = 60 + i * 5; tick(); }
        for (int i = 20; i >= 9; i--) { glass.x = 60 + i * 5; tick(); }
        glass.down = false; tick(); run_ms(600);
        EXPECT_NONE("a quick scrub, out and back (130 ms, 45 px net): nothing, not a swipe");
    }

    /* --- THE TICKLE (THE OPERATOR NIGHT, watch-night-2026-10-12 C4): a scrub under one finger, three turns inside a second ---- */
    scrub(120, 160, 1, 0, 30, 5, 100);          /* out, back, out, back, out: four turns, 100 ms a leg */
    run_ms(600);
    EXPECT("a scrub of five 30 px legs, 100 ms each (four turns in 0.5 s): THE TICKLE", GES_TICKLE);
    check("... n = the turns, 4", got_last.n == 4);
    check("... x, y, dx, dy = the box it stayed in (120,160 30x0)", got_last.x == 120 && got_last.y == 160 && got_last.dx == 30 && got_last.dy == 0);
    check("a tickle shows nothing of its own on the gel (the wriggle is main's)", gesture_stroke() == 0 && gesture_squash() == 0);
    scrub(120, 160, 1, 0, 30, 3, 100);          /* out, back, out: two turns */
    run_ms(600);
    EXPECT("three legs (two turns, 300 ms, 30 px net): not the tickle -- the wobbly tap it always was", GES_TAP);
    scrub(120, 160, 1, 0, 30, 4, 340);          /* three turns 340 ms apart: 680 between the first and the third */
    run_ms(600);
    EXPECT("four slow legs, 340 ms each (the three turns 680 ms apart): still the tickle", GES_TICKLE);
    check("... n = 3", got_last.n == 3);
    scrub(120, 160, 1, 0, 30, 4, 510);          /* three turns 510 ms apart: 1020 between the first and the third */
    run_ms(600);
    EXPECT("four legs of 510 ms (the turns 1020 ms apart): not the tickle -- the hold it always was (the peekaboo at 2 s, the long press)", GES_PEEK_DOWN, GES_PEEK_UP, GES_LONG_PRESS);
    scrub(120, 160, 1, 1, 30, 4, 100);          /* a diagonal scrub: both axes turn on the same sample */
    run_ms(600);
    EXPECT("a diagonal scrub, four legs: the tickle, each turn counted once", GES_TICKLE);
    check("... n = 3, not 6", got_last.n == 3);
    check("... the box 30x30", got_last.dx == 30 && got_last.dy == 30);
    {   /* a still finger's jitter: the chip wobbles a px or two under it -- never a turn */
        glass.down = true; glass.x = 120; glass.y = 120;
        for (int i = 0; i < 250; i++) { glass.x = 120 + ((i >> 1) & 1) * 2; glass.y = 120 + (i & 1); tick(); }
        glass.down = false; tick(); run_ms(600);
        EXPECT("a finger held a second, jittering 2 px: the lease's hold, not a tickle (TICKLE_PX 6 of hysteresis)", GES_LEASE_HOLD);
    }
    scrub(120, 160, 1, 0, 100, 5, 100);         /* long legs: 100 px out and back */
    run_ms(600);
    EXPECT("a scrub of 100 px legs: the tickle, never the stroke or the swipe (the turns are read first)", GES_TICKLE);
    press(120, 120, 90); run_ms(200); press(120, 120, 90); run_ms(200); press(120, 120, 90);
    run_ms(200); scrub(120, 160, 1, 0, 30, 5, 100);
    run_ms(200); press(120, 120, 90);
    run_ms(600);
    EXPECT("three knocks, a scrub, a tap: the knocks, the tickle (the knocks' window closed by it), a pet -- never the rhythm", GES_TRIPLE_TAP, GES_TICKLE, GES_TAP);
    gesture_set_lock(true); tick();
    scrub(120, 160, 1, 0, 30, 5, 100);
    run_ms(600);
    EXPECT("a scrub under the lock: inert", GES_LOCKED, GES_TOUCH_LOCKED);
    gesture_set_lock(false); tick(); run_ms(100);
    EXPECT("... unlocked", GES_UNLOCKED);

    /* --- THE GEL'S FRAME (section 2): every drag turned by his wall ---------------- */
    check("a boot sits on the bottom wall (the screen's frame)", gesture_wall() == 0);
    gesture_set_wall(1);                    /* the right wall: his base at 3 o'clock, his crown toward 9 */
    drag(40, 120, 200, 120, 500);
    run_ms(600);
    EXPECT("RIGHT WALL: a drag screen-RIGHT, left third to right third (down onto him): the feed", GES_FEED);
    if (got_last.dx != 160 || got_last.dy != 0) { printf("FAIL the feed's travel rides in screen px (+160,0; got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    gesture_unshow();
    drag(200, 120, 40, 120, 500);
    run_ms(600);
    EXPECT("RIGHT WALL: a drag screen-LEFT, right third to left third (up off him): the page's drag", GES_DRAG_UP);
    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("RIGHT WALL: a drag screen-DOWN across him (along his wall): the stroke, not the feed", GES_STROKE);
    if (got_last.dy != 160) { printf("FAIL the stroke's travel rides in screen px (0,+160; got %d,%d)\n", got_last.dx, got_last.dy); fails++; }
    drag(120, 200, 120, 40, 500);
    run_ms(600);
    EXPECT("RIGHT WALL: a drag screen-UP across him: the stroke, not the page", GES_STROKE);
    drag(120, 60, 120, 180, 200);
    run_ms(600);
    EXPECT("RIGHT WALL: a flick screen-down along his wall: the swipe", GES_SWIPE);
    drag(40, 120, 200, 120, 250);
    run_ms(600);
    EXPECT("RIGHT WALL: a fast drag screen-right across the thirds: the feed (the feeding axis, his)", GES_FEED);
    gesture_unshow();
    drag(60, 120, 150, 120, 200);
    run_ms(600);
    EXPECT_NONE("RIGHT WALL: a 90 px flick screen-right from the middle: the feeding axis, not from the crown's third -- nothing");
    drag(40, 120, 130, 120, 500);
    run_ms(600);
    EXPECT_NONE("RIGHT WALL: a drag screen-right that stops in the middle: not into his base's third -- nothing");
    arc_drag(0, 90, 105, 600); run_ms(600);
    EXPECT("RIGHT WALL: a quarter turn round the rim: the arc, in the SCREEN's frame (the dial is the watch's)", GES_ARC);
    check("...ends at 90 with 90 swept, whatever the wall", got_last.n >= 89 && got_last.n <= 91 && got_last.dx >= 88 && got_last.dx <= 91);
    gesture_set_wall(2);                    /* the top wall: his base at 12 o'clock */
    drag(120, 200, 120, 40, 500);
    run_ms(600);
    EXPECT("TOP WALL: a drag screen-UP, bottom third to top third (down onto him): the feed", GES_FEED);
    gesture_unshow();
    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("TOP WALL: a drag screen-DOWN, top third to bottom third (up off him): the page's drag", GES_DRAG_UP);
    drag(60, 120, 150, 120, 400);
    run_ms(600);
    EXPECT("TOP WALL: a drag screen-right across him: the stroke", GES_STROKE);
    drag(60, 120, 150, 120, 200);
    run_ms(600);
    EXPECT("TOP WALL: a flick screen-right: the swipe", GES_SWIPE);
    gesture_set_wall(3);                    /* the left wall: his base at 9 o'clock */
    drag(200, 120, 40, 120, 500);
    run_ms(600);
    EXPECT("LEFT WALL: a drag screen-LEFT, right third to left third (down onto him): the feed", GES_FEED);
    gesture_unshow();
    drag(40, 120, 200, 120, 500);
    run_ms(600);
    EXPECT("LEFT WALL: a drag screen-RIGHT, left third to right third (up off him): the page's drag", GES_DRAG_UP);
    drag(120, 40, 120, 200, 400);
    run_ms(600);
    EXPECT("LEFT WALL: a drag screen-down across him: the stroke", GES_STROKE);
    drag(120, 200, 120, 40, 200);
    run_ms(600);
    EXPECT("LEFT WALL: a flick screen-up: the swipe", GES_SWIPE);
    gesture_set_wall(0);
    drag(120, 40, 120, 200, 500);
    run_ms(600);
    EXPECT("BOTTOM WALL again: the feed is a drag down the screen, as ever", GES_FEED);
    gesture_unshow();
    gesture_set_wall(2);
    gesture_begin(now_ms);
    check("a reboot sits on the bottom wall until main says", gesture_wall() == 0);
    run_ms(600); ngot = 0;

    /* a scrub: back and forth under one finger, net 70 px of a 250 px path -- five turns in 0.4 s: since THE TICKLE (C4) the detector names it, and main asks whether it lay on him (here beside him: nothing) */
    glass.down = true; glass.x = 60; glass.y = 120;
    for (int i = 0; i < 100; i++) { glass.x = 60 + (int)(90.0f * fabsf(sinf((float)i / 100.0f * 3.0f * (float)M_PI))) ; if (i > 85) glass.x = 130; tick(); }
    glass.down = false; tick();
    run_ms(600);
    EXPECT("a scrub back and forth: no one direction -- THE TICKLE (on him or beside him is main's)", GES_TICKLE);
    check("... its five turns", got_last.n == 5);

    /* a jump: two drops read as one finger, 100 px apart in two samples
     * (the chip's own cadence, ~40 Hz: the loop saw nothing between) */
    glass.down = true; glass.x = 60; glass.y = 120; tick(); now_ms += 36; glass.x = 160; tick(); glass.down = false; tick();
    run_ms(600);
    EXPECT_NONE("a 100 px jump in two samples 40 ms apart: not a finger, nothing");

    press(120, 120, 90); run_ms(100); drag(60, 120, 150, 120, 400);
    run_ms(600);
    EXPECT("a tap then a stroke: the stroke, then the pet (taps +0 for the stroke)", GES_STROKE, GES_TAP);

    press(120, 120, 1950);
    run_ms(600);
    EXPECT("a 1.95 s hold: the long press (main: nothing of its own)", GES_LONG_PRESS);

    press(120, 120, 800);
    run_ms(600);
    EXPECT("a 0.8 s hold: the lease's hold (the old window's floor, well inside the new)", GES_LEASE_HOLD);

    press(120, 120, 90); run_ms(150); press(120, 120, 1950);
    run_ms(600);
    EXPECT("a tap then a 1.95 s hold: the tap dropped, the long press", GES_TAPS_DROPPED, GES_LONG_PRESS);

    /* --- THE LEASE, EASY: one hold, always the same; the stopwatch moved.
     * THE HOLD'S WINDOW, WIDER (09-10 21:08): 0.5-1.9 s on the lift --- */
    press(120, 120, 1000);
    run_ms(600);
    EXPECT("a 1.0 s hold, nothing pending: THE LEASE toggles (main: come / release by the house)", GES_LEASE_HOLD);
    if (got_last.n < 990 || got_last.n > 1010) { printf("FAIL the hold's length should ride on the event (got %d)\n", got_last.n); fails++; }
    press(120, 120, 600);
    run_ms(600);
    EXPECT("a 0.6 s hold: the lease (inside the wider window)", GES_LEASE_HOLD);
    press(120, 120, 1800);
    run_ms(600);
    EXPECT("a 1.8 s hold: the lease (inside the wider window)", GES_LEASE_HOLD);
    press(120, 120, 1400);
    run_ms(600);
    EXPECT("a 1.4 s hold: still the lease (the old ceiling)", GES_LEASE_HOLD);
    press(120, 120, 500);
    run_ms(600);
    EXPECT("a 0.5 s hold: the lease (the window's floor)", GES_LEASE_HOLD);
    press(120, 120, 1900);
    run_ms(600);
    EXPECT("a 1.9 s hold: the lease (the window's ceiling)", GES_LEASE_HOLD);
    press(120, 120, 450);
    run_ms(600);
    EXPECT("a 0.45 s hold: nothing (the one gap, over a slow tap and under the window)", GES_HOLD);
    press(120, 120, 1950);
    run_ms(600);
    EXPECT("a 1.95 s hold: the long press (past the window, lifted before the peekaboo's 2 s)", GES_LONG_PRESS);
    drag(60, 120, 150, 120, 1700);
    run_ms(600);
    EXPECT("a 90 px drag held 1.7 s: the long press, not the lease (a finger drawn is not a hand resting)", GES_LONG_PRESS);
    press(120, 120, 90); run_ms(150); press(120, 120, 1000);
    run_ms(600);
    EXPECT("a tap then a 1.0 s hold: THE STOPWATCH (the double-tap-and-hold), the tap not dropped", GES_TAP_HOLD);
    if (got_last.n < 990 || got_last.n > 1010) { printf("FAIL the tap-hold's length should ride on the event (got %d)\n", got_last.n); fails++; }
    press(120, 120, 90); run_ms(150); press(120, 120, 90); run_ms(150); press(120, 120, 900);
    run_ms(600);
    EXPECT("two taps then a 0.9 s hold: the stopwatch too", GES_TAP_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 600);
    run_ms(600);
    EXPECT("a tap then a 0.6 s hold: the stopwatch (its window widened with the lease's)", GES_TAP_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 1800);
    run_ms(600);
    EXPECT("a tap then a 1.8 s hold: the stopwatch", GES_TAP_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 450);
    run_ms(600);
    EXPECT("a tap then a 0.45 s hold: the tap dropped, nothing", GES_TAPS_DROPPED, GES_HOLD);
    press(120, 120, 90); run_ms(150); press(120, 120, 1950);
    run_ms(600);
    EXPECT("a tap then a 1.95 s hold: past the window, the tap dropped, the long press", GES_TAPS_DROPPED, GES_LONG_PRESS);
    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    EXPECT("three knocks: still come (main)", GES_TRIPLE_TAP);
    run_ms(200);
    press(120, 120, 1000);
    run_ms(600);
    EXPECT("... and a hold after them: the lease's hold (the knocks' release, armed or not)", GES_LEASE_HOLD);
    drag(120, 200, 120, 40, 500);
    run_ms(600);
    EXPECT("the drag up: THE WATCH PAGE", GES_DRAG_UP);
    gesture_unshow();

    /* --- PEEKABOO: a plain hold past 2 s ------------------------------------------ */
    glass.down = true; glass.x = 120; glass.y = 120;
    run_ms(1900);
    EXPECT_NONE("a finger down 1.9 s: nothing yet");
    run_ms(200);
    EXPECT("...at 2 s, still down: a hand over the bowl (a 2.1 s hold is the peekaboo, never the lease)", GES_PEEK_DOWN);
    check("the hide carries the ms so far (~2000)", got_last.n >= 1990 && got_last.n <= 2010);
    run_ms(1000);
    EXPECT_NONE("...said once while it stays");
    glass.down = false; tick();
    run_ms(600);
    EXPECT("the lift at 3.1 s: the boo, then the long press as ever", GES_PEEK_UP, GES_LONG_PRESS);
    press(120, 120, 1950); run_ms(600);
    EXPECT("a 1.95 s hold: the long press alone (under 2 s: no peekaboo)", GES_LONG_PRESS);
    press(120, 120, 1700); run_ms(600);
    EXPECT("a 1.7 s hold: the lease's hold alone (no peekaboo either)", GES_LEASE_HOLD);
    drag(60, 120, 150, 120, 2500); run_ms(600);
    EXPECT("a 90 px drag held 2.5 s: the long press, no peekaboo (a finger drawn is not a hand over the bowl)", GES_LONG_PRESS);
    glass.down = true; glass.x = 120; glass.y = 120; run_ms(2200);
    for (int i = 0; i < 20; i++) { glass.x = 120 + i * 4; tick(); }          /* the hand slides off after the hide */
    glass.down = false; tick(); run_ms(600);
    EXPECT("hidden, then the finger slides 76 px and lifts: the boo still owed, then the long press", GES_PEEK_DOWN, GES_PEEK_UP, GES_LONG_PRESS);
    press(120, 120, 90); run_ms(150); press(120, 120, 80); run_ms(150); press(120, 120, 80);
    run_ms(200);
    press(120, 120, 2500); run_ms(600);
    EXPECT("three knocks then a 2.5 s hold: the detector says both (main gates: the knocks' release wins)", GES_TRIPLE_TAP, GES_PEEK_DOWN, GES_PEEK_UP, GES_LONG_PRESS);

    /* --- THE ARC: the finger round the rim (the alarm's dial) ---------------------- */
    arc_drag(0, 90, 105, 600); run_ms(600);
    EXPECT("a quarter turn round the rim, 12 to 3 o'clock: the arc", GES_ARC);
    check("the arc ends at 90 deg, swept +90", got_last.n >= 89 && got_last.n <= 91 && got_last.dx >= 88 && got_last.dx <= 91);
    check("the finger was followed: many moves, the last at ~90 deg", arc_moves > 25 && arc_last_move.n >= 89 && arc_last_move.n <= 91);
    check("the moves began only once 60 deg were swept", arc_moves < 40);
    arc_drag(0, 30, 105, 300); run_ms(600);
    EXPECT("a twelfth of a turn on the rim (54 px chord): a tap, a wobbly one -- not the arc", GES_TAP);
    arc_drag(0, 90, 60, 600); run_ms(600);
    EXPECT("a quarter turn INSIDE the band (r 60): the stroke", GES_STROKE);
    arc_drag(0, -90, 105, 600); run_ms(600);
    EXPECT("a quarter turn the other way, 12 to 9: the arc", GES_ARC);
    check("...ends at 270 deg, swept -90", got_last.n >= 269 && got_last.n <= 271 && got_last.dx <= -88 && got_last.dx >= -91);
    arc_drag(300, 420, 100, 2500); run_ms(600);
    EXPECT("a third of a turn across 12 o'clock, taken 2.5 s: still the arc (never the hold)", GES_ARC);
    check("...ends at 60 deg, swept +120 (no seam at 12)", got_last.n >= 59 && got_last.n <= 61 && got_last.dx >= 118 && got_last.dx <= 121);
    arc_drag(180, 540, 110, 1400); run_ms(600);
    EXPECT("a whole turn from 6 o'clock round to 6: the arc", GES_ARC);
    check("...ends at 180, swept +360", got_last.n >= 179 && got_last.n <= 181 && got_last.dx >= 358 && got_last.dx <= 361);
    {   /* the finger leaves the band mid-way: disqualified for good, judged as a drag (60 px to the right of where it began: the stroke) */
        glass.down = true;
        for (int i = 0; i <= 30; i++) { float a = i * 3.0f * (float)M_PI / 180.0f; glass.x = 120 + (int)(sinf(a) * 105); glass.y = 120 - (int)(cosf(a) * 105); tick(); }
        glass.x = 120; glass.y = 60; tick(); tick();                               /* a cut inside */
        for (int i = 30; i <= 90; i++) { float a = i * 3.0f * (float)M_PI / 180.0f; glass.x = 120 + (int)(sinf(a) * 105); glass.y = 120 - (int)(cosf(a) * 105); tick(); }
        glass.down = false; tick(); run_ms(600);
        EXPECT("a rim drag that cut inside the band: not the arc (judged as a drag: its path doubled back three times inside the second -- since C4 that scrub is THE TICKLE)", GES_TICKLE);
    }

    /* --- a phantom burst: a wet glass flickering ------------------------------- */
    for (int i = 0; i < 12; i++) { press(60 + i * 9, 80 + (i % 3) * 20, 12 + (i % 4) * 5); run_ms(40); }
    run_ms(2500);
    EXPECT("wet-glass flicker (12 blips): dropped, no pet", GES_PHANTOM);

    press(120, 120, 90); run_ms(50); press(120, 120, 90); run_ms(50); press(120, 120, 90); run_ms(50);
    press(120, 120, 90); run_ms(50); press(120, 120, 90); run_ms(50); press(120, 120, 90);
    run_ms(2500);
    EXPECT("six 90 ms touches: the knocks (three real taps), the 4th opens the rhythm, the rhythm dropped (4), the burst", GES_TRIPLE_TAP, GES_RHYTHM_START, GES_TAPS_DROPPED, GES_PHANTOM);

    press(120, 120, 90);
    run_ms(600);
    EXPECT("after the quiet: the glass listens again", GES_TAP);

    /* --- shakes ----------------------------------------------------------------- */
    /* a bump: one excursion and its rebound */
    shake_x(2, 60, 1.6f);
    run_ms(2000);
    EXPECT_NONE("a bump is not a shake");

    /* a shake: out-back-out-back, 4 halves of 70 ms at 1.8 g */
    shake_x(4, 70, 1.8f);
    run_ms(400);
    EXPECT_NONE("a shake: nothing until the double window closes");
    run_ms(1200);
    EXPECT("a shake: up", GES_SHAKE);

    /* a double shake: two shakes 300 ms apart, screen on: the lock */
    shake_x(4, 70, 1.8f); run_ms(300); shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("a double shake: the water lock ON", GES_DOUBLE_SHAKE, GES_LOCKED);
    if (!gesture_locked()) { printf("FAIL should be locked\n"); fails++; }

    /* under the lock: taps, doubles, holds are inert; the note once */
    press(120, 120, 90); run_ms(600);
    press(120, 120, 90); run_ms(150); press(120, 120, 90); run_ms(600);
    press(120, 120, 1700); run_ms(600);
    EXPECT("under the lock: every touch inert, one note", GES_TOUCH_LOCKED);
    if (gesture_touch_live()) { printf("FAIL a touch under the lock is not attention\n"); fails++; }
    for (int i = 0; i < 12; i++) { press(60 + i * 9, 80, 15); run_ms(40); }
    run_ms(2500);
    EXPECT_NONE("under the lock: a wet glass, silent (the note's 5 s not up)");
    run_ms(5000);
    drag(60, 120, 150, 120, 400); run_ms(600);
    drag(120, 40, 120, 200, 500); run_ms(600);
    EXPECT("under the lock: a stroke and a feed inert (the note again, 5 s on)", GES_TOUCH_LOCKED);
    if (gesture_stroke() != 0 || gesture_lump() != 0) { printf("FAIL nothing should show for a stroke or a feed under the lock (stroke %.3f lump %.3f)\n", gesture_stroke(), gesture_lump()); fails++; }
    press(120, 120, 2500); run_ms(600);
    EXPECT_NONE("under the lock: a 2.5 s hold is no peekaboo (the note 1 s ago)");

    /* a lone shake under the lock is still "up" */
    shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("under the lock: a shake is still up", GES_SHAKE);

    /* a double shake again: the lock OFF */
    shake_x(4, 70, 1.8f); run_ms(300); shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("a double shake: the water lock off", GES_DOUBLE_SHAKE, GES_UNLOCKED);
    if (gesture_locked()) { printf("FAIL should be unlocked\n"); fails++; }

    /* a double shake in the dark: no lock */
    screen_on = false;
    shake_x(4, 70, 1.8f); run_ms(300); shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("a double shake with the screen off: not the lock", GES_DOUBLE_SHAKE);
    screen_on = true;

    /* a run: stride-rate reversals that never go quiet */
    shake_x(40, 160, 1.3f);
    run_ms(2000);
    EXPECT_NONE("a run (6 s of 3 Hz reversals) is not a shake");

    /* a slow turn of the wrist: gravity moves, no excursion */
    for (int i = 0; i < 250; i++) { float a = (float)i / 250.0f * (float)M_PI / 2; wrist.x = sinf(a); wrist.z = cosf(a); tick(); }
    run_ms(2000);
    EXPECT_NONE("a slow turn is not a shake");
    wrist.x = 0; wrist.z = 1;
    run_ms(2000); ngot = 0;

    /* serial lock / unlock: the event surfaces next tick */
    gesture_set_lock(true); tick();
    EXPECT("`lock` over serial", GES_LOCKED);
    gesture_set_lock(true); tick();
    EXPECT_NONE("`lock` again: already");
    gesture_set_lock(false); tick();
    EXPECT("`unlock` over serial", GES_UNLOCKED);

    /* --- THE LOCK IS NEVER A TRAP (2026-09-10 night) ---------------------------- */
    /* the page's / serial's lock is RAM only: a reboot always unlocks */
    gesture_set_lock(true); tick();
    EXPECT("the page's lock (`set lock on`)", GES_LOCKED);
    gesture_begin(now_ms);
    check("rebooted: unlocked, whoever set it (there is no NVS key)", !gesture_locked());
    press(120, 120, 90); run_ms(600);
    EXPECT("rebooted: a tap is a tap", GES_TAP);
    /* the double shake clears a page-set lock */
    gesture_set_lock(true); tick(); ngot = 0;
    press(120, 120, 90); run_ms(600);
    EXPECT("under the page's lock: a tap is inert", GES_TOUCH_LOCKED);
    shake_x(4, 70, 1.8f); run_ms(300); shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("a double shake clears the page's lock", GES_DOUBLE_SHAKE, GES_UNLOCKED);
    check("... n = 1 (the hand's word)", got_last.n == 1 && !gesture_locked());
    /* a plain hold of 3 s clears any lock, while the finger still presses; its lift is inert */
    gesture_set_lock(true); tick(); ngot = 0;
    run_ms(6000);                                   /* the note's 5 s */
    press(120, 120, 2500); run_ms(600);
    EXPECT("under the lock: a 2.5 s hold is not enough (the note)", GES_TOUCH_LOCKED);
    check("... still locked", gesture_locked());
    run_ms(6000);
    glass.down = true; glass.x = 120; glass.y = 120;
    run_ms(2900);
    check("2.9 s into the hold: still locked", gesture_locked());
    run_ms(200);
    check("3 s into the hold, the finger still down: UNLOCKED", !gesture_locked() && !gesture_auto_locked());
    EXPECT("... the event, n = 3 (a long hold)", GES_TOUCH_LOCKED, GES_UNLOCKED);
    check("... n = 3", got_last.n == 3);
    run_ms(1500);
    glass.down = false; tick();
    run_ms(2000);
    EXPECT_NONE("the unlocking hold's lift: nothing (no long press, no peekaboo, no lease hold)");
    check("a touch is attention again", gesture_touch_live());
    press(120, 120, 90); run_ms(600);
    EXPECT("the glass listens again: a tap", GES_TAP);
    /* a hold that travels is not the unlocking hold (a wet sleeve dragged across) */
    gesture_set_lock(true); tick(); ngot = 0;
    run_ms(6000);
    drag(40, 120, 200, 120, 3500); run_ms(600);
    EXPECT("under the lock: a 3.5 s drag across the glass is inert (the note)", GES_TOUCH_LOCKED);
    check("... still locked: travel is not a hand resting", gesture_locked());
    /* the wet glass's own lock lets go to the hold too */
    gesture_set_lock(false); tick(); ngot = 0;
    run_ms(2000);
    storm(4, 1800);
    run_ms(100);
    check("a storm: the glass's own lock", gesture_auto_locked());
    ngot = 0;
    run_ms(6000);
    glass.down = true; glass.x = 120; glass.y = 120;
    run_ms(3200);
    check("a 3-s hold under the wet glass's lock: unlocked", !gesture_locked() && !gesture_auto_locked());
    EXPECT("... the event, n = 3", GES_TOUCH_LOCKED, GES_UNLOCKED);
    glass.down = false; tick(); run_ms(2000); ngot = 0;

    /* the empty glass: main.cpp declines the event and unshows what it set */
    gesture_begin(now_ms);
    drag(120, 40, 120, 200, 500); run_ms(100);
    EXPECT("a feed (main will decline it: the glass is empty)", GES_FEED);
    gesture_unshow();
    if (gesture_lump() != 0 || gesture_squash() != 0) { printf("FAIL unshow: the lump should be gone (%.2f)\n", gesture_lump()); fails++; }

    /* --- THE DRINK LINE (design/watch-ideas.md D30, 2026-09-23) -------------- */
    gesture_begin(now_ms);
    run_ms(2000); ngot = 0;
    check("at boot the meniscus is at its resting line", gesture_drink_line() == 0 && gesture_squash() == 0);
    press(120, 120, 90); run_ms(150); press(122, 118, 80); run_ms(600);
    EXPECT("a drink", GES_DOUBLE_TAP);
    check("one drink: the line a hair up (0.02)", fabsf(gesture_drink_line() - 0.02f) < 0.001f);
    run_ms(600);
    check("... the settle over, the hair stays in gesture_squash (0.02)", fabsf(gesture_squash() - 0.02f) < 0.001f);
    press(120, 120, 90); run_ms(150); press(122, 118, 80); run_ms(1200);
    EXPECT("a second drink", GES_DOUBLE_TAP);
    check("two drinks: two hairs (0.04)", fabsf(gesture_drink_line() - 0.04f) < 0.001f);
    press(120, 120, 90); run_ms(150); press(122, 118, 80); run_ms(1200);
    EXPECT("a third", GES_DOUBLE_TAP);
    check("three drinks: the cap (0.06)", fabsf(gesture_drink_line() - 0.06f) < 0.001f);
    press(120, 120, 90); run_ms(150); press(122, 118, 80); run_ms(1200);
    EXPECT("a fourth", GES_DOUBLE_TAP);
    check("a fourth drink: still the cap, never a puddle (0.06)", fabsf(gesture_drink_line() - 0.06f) < 0.001f);
    /* the sink: one hair per two hours -- the tick's dt is capped at 0.5 s,
     * so the hour is walked in 400 ms steps */
    {
        uint32_t t_end = now_ms + 3600u * 1000u;            /* an hour */
        while (now_ms < t_end) { now_ms += 400; glass.down = false; tick(); }
        check("an hour on: half a hair gone (0.05)", fabsf(gesture_drink_line() - 0.05f) < 0.002f);
        t_end = now_ms + 3600u * 1000u;
        while (now_ms < t_end) { now_ms += 400; tick(); }
        check("two hours on: one hair gone (0.04)", fabsf(gesture_drink_line() - 0.04f) < 0.003f);
        t_end = now_ms + 3u * 3600u * 1000u + 1800u * 1000u;
        while (now_ms < t_end) { now_ms += 400; tick(); }
        check("five and a half hours on: the last hair nearly gone (< 0.006)", gesture_drink_line() < 0.006f);
        t_end = now_ms + 1800u * 1000u;
        while (now_ms < t_end) { now_ms += 400; tick(); }
        check("six hours on: the resting line again (0 exactly), a dry afternoon", gesture_drink_line() == 0 && gesture_squash() == 0);
    }
    ngot = 0;
    /* the meal and the stroke ride over the line, never on it */
    press(120, 120, 90); run_ms(150); press(122, 118, 80); run_ms(1200); ngot = 0;
    check("a drink after the dry spell: a hair again", fabsf(gesture_drink_line() - 0.02f) < 0.001f);
    check("... the lump untouched by it", gesture_lump() == 0);
    /* the empty glass: main unshows what the tap set, the line too */
    gesture_unshow();
    check("unshow: the line is gone with the rest", gesture_drink_line() == 0 && gesture_squash() == 0);
    run_ms(2000); ngot = 0;

    /* --- A DRINK ON A HOT DAY (design/watch-ideas.md A9, 2026-10-08) ---------- */
    gesture_begin(now_ms);
    run_ms(2000); ngot = 0;
    check("at boot the gel is not warm", !gesture_warm());
    {   /* a cool drink first: the settle's peak is WATER_SQUASH's (0.08 at the half sine's top) */
        float peak = drink_peak();
        EXPECT("a cool drink", GES_DOUBLE_TAP);
        check("a cool drink's settle peaks at 0.08", fabsf(peak - 0.08f) < 0.004f);
        run_ms(1000);
        check("... and lifts one hair (0.02)", fabsf(gesture_drink_line() - 0.02f) < 0.001f);
    }
    gesture_set_warm(true);
    check("the sky says hot: warm", gesture_warm());
    {
        float peak = drink_peak();
        EXPECT("a drink on a hot day", GES_DOUBLE_TAP);
        check("the settle HOT_DRINK_SETTLE_K deeper: 0.12", fabsf(peak - 0.08f * HOT_DRINK_SETTLE_K) < 0.004f);
        run_ms(1000);
        check("... the settle over (the hair stands)", fabsf(gesture_squash() - gesture_drink_line()) < 1e-5f);
        check("the hair HOT_DRINK_LINE_K taller: the rain's own 0.04 on the 0.02 standing (0.06)", fabsf(gesture_drink_line() - 0.06f) < 0.001f);
        drink_peak();
        EXPECT("another hot drink", GES_DOUBLE_TAP);
        run_ms(1000);
        check("... the cap is the cap on a hot day too (0.06)", fabsf(gesture_drink_line() - 0.06f) < 0.001f);
    }
    gesture_set_warm(false);
    {   /* the sky cooling changes nothing already drunk: the line stands, the next settle is the cool one */
        check("the sky cooled: the line stands (0.06)", fabsf(gesture_drink_line() - 0.06f) < 0.001f);
        uint32_t t_end = now_ms + 4u * 3600u * 1000u;              /* four hours: a taller hair sinks at the one rate (0.02 / 2 h) */
        while (now_ms < t_end) { now_ms += 400; glass.down = false; tick(); }
        check("four hours on: two hairs gone (0.02), the sink one law", fabsf(gesture_drink_line() - 0.02f) < 0.003f);
        ngot = 0;
        float peak = drink_peak();
        EXPECT("a cool drink again", GES_DOUBLE_TAP);
        check("... its settle the cool 0.08 again", fabsf(peak - 0.08f) < 0.004f);
        run_ms(1000);
        check("... and one hair (0.04)", fabsf(gesture_drink_line() - 0.04f) < 0.003f);
    }
    gesture_unshow();
    run_ms(2000); ngot = 0;

    /* --- THE WET GLASS LOCKS ITSELF ------------------------------------------ */
    gesture_begin(now_ms);
    run_ms(2000); ngot = 0;
    storm(3, 1800);
    run_ms(100);
    EXPECT("three bursts in 8 s: three splashes, no lock yet", GES_PHANTOM, GES_PHANTOM, GES_PHANTOM);
    check("three bursts: not locked", !gesture_locked());
    flicker(8);
    EXPECT("the fourth burst inside 10 s: a storm -- the lock ON by itself", GES_PHANTOM, GES_LOCKED);
    check("the auto-lock's event carries n = 2 (main: not for NVS)", got_last.n == 2);
    check("locked, and the lock is the glass's own", gesture_locked() && gesture_auto_locked());
    check("a touch under the auto-lock is not attention", !gesture_touch_live());
    run_ms(5000);                                   /* the note's 5 s (the storm's own burst was the last note) */
    press(120, 120, 90); run_ms(600);
    EXPECT("under the auto-lock: a tap is inert, the note once", GES_TOUCH_LOCKED);
    {   /* still flickering all through the window and past it: still wet, still locked */
        uint32_t t0 = now_ms;
        while (now_ms - t0 < 130000) { flicker(8); run_ms(4000); }
        check("flicker through the 120 s window and 10 s past: still locked", gesture_locked() && gesture_auto_locked());
        check("... no unlock came", count_kind(GES_UNLOCKED) == 0);
        check("... the bursts under the lock are silent (no phantom event)", count_kind(GES_PHANTOM) == 0);
        ngot = 0;
    }
    run_ms(24000);                                  /* ~28 s since the last burst (the loop's own 4 s gap counts) */
    check("28 s of a dry glass: still locked (30 s of quiet needed)", gesture_locked() && count_kind(GES_UNLOCKED) == 0);
    ngot = 0;
    run_ms(3000);
    EXPECT("30 s of quiet past the window: the lock lets go by itself", GES_UNLOCKED);
    check("the auto-unlock's event carries n = 2", got_last.n == 2);
    check("unlocked", !gesture_locked() && !gesture_auto_locked());
    press(120, 120, 90); run_ms(600);
    EXPECT("the glass listens again", GES_TAP);

    /* a storm, then the double shake: off sooner */
    storm(4, 1800);
    run_ms(100);
    check("a second storm: locked by itself again", gesture_auto_locked() && count_kind(GES_LOCKED) == 1);
    ngot = 0;
    run_ms(5000);
    shake_x(4, 70, 1.8f); run_ms(300); shake_x(4, 70, 1.8f);
    run_ms(2000);
    EXPECT("a double shake under the auto-lock: off sooner", GES_DOUBLE_SHAKE, GES_UNLOCKED);
    check("the double shake's unlock carries n = 1 (the hand's word)", got_last.n == 1);
    check("unlocked, not the glass's own any more", !gesture_locked() && !gesture_auto_locked());
    run_ms(200000);
    check("... and nothing later", count_kind(GES_UNLOCKED) == 0 && count_kind(GES_LOCKED) == 0);
    ngot = 0;

    /* a quiet-time storm: the lock a moment after the window with a dry glass */
    storm(4, 1800);
    run_ms(100); ngot = 0;
    run_ms(115000);                                 /* ~117 s since the lock (the storm's trailing gap counts) */
    check("a dry glass at 117 s: the window not up, still locked", gesture_locked());
    run_ms(6000);
    EXPECT("at 123 s, dry for long: off", GES_UNLOCKED);
    check("off by itself", !gesture_locked());

    /* the manual lock is never auto-unlocked: not by the window, not by a dry glass */
    gesture_set_lock(true); tick();
    EXPECT("`lock` over serial", GES_LOCKED);
    check("a manual lock is not the glass's own", gesture_locked() && !gesture_auto_locked());
    storm(4, 1800);
    check("a storm under the manual lock: silent (the note aside), no second lock", count_kind(GES_PHANTOM) == 0 && count_kind(GES_LOCKED) == 0);
    ngot = 0;
    run_ms(200000);
    check("200 s later: the manual lock stays", gesture_locked() && count_kind(GES_UNLOCKED) == 0);
    ngot = 0;
    gesture_set_lock(false); tick();
    EXPECT("`unlock` over serial", GES_UNLOCKED);

    /* `lock` typed while the wet glass holds it: now a setting, and it stays */
    run_ms(2000); ngot = 0;
    storm(4, 1800);
    run_ms(100);
    check("a storm: the glass's own lock", gesture_auto_locked());
    ngot = 0;
    gesture_set_lock(true); tick();
    EXPECT("`lock` typed under the auto-lock: the event surfaces (n = 0, the hand's)", GES_LOCKED);
    check("... and it is the hand's now (no drying off; still RAM only)", gesture_locked() && !gesture_auto_locked() && got_last.n == 0);
    run_ms(200000);
    check("200 s later: it stays", gesture_locked() && count_kind(GES_UNLOCKED) == 0);
    ngot = 0;
    gesture_set_lock(false); tick(); ngot = 0;

    /* a boot forgets the auto-lock as it forgets every lock (THE LOCK IS NEVER A TRAP) */
    storm(4, 1800);
    run_ms(100);
    check("a storm before a reboot: locked", gesture_auto_locked());
    gesture_begin(now_ms);
    check("rebooted: the wet glass's lock is forgotten (no lock survives a boot)", !gesture_locked());
    ngot = 0;

    /* --- THE ECHOED RHYTHM (design/watch-ideas.md F51, 2026-10-09) ---------- */
    run_ms(2500);
    EXPECT_NONE("rest before the rhythm");
    press(120, 120, 90); run_ms(200); press(120, 120, 90); run_ms(200); press(120, 120, 90);
    EXPECT("three taps: the knocks at the third, as ever", GES_TRIPLE_TAP);
    run_ms(200); press(120, 120, 90);
    EXPECT("a fourth tap 200 ms after the third's lift: THE RHYTHM OPENS (4)", GES_RHYTHM_START);
    check("... n = 4", got_last.n == 4);
    run_ms(580);
    EXPECT_NONE("0.68 s after the fourth landed: still open -- no pet, no drink");
    run_ms(100);
    EXPECT("0.78 s after it: the gap passed, THE RHYTHM of four", GES_RHYTHM);
    check("... n = 4", got_last.n == 4);
    check("... dx = the ms since the last tap landed (~776)", got_last.dx >= 700 && got_last.dx <= 800);
    {
        uint32_t g[5]; int ng = gesture_rhythm_gaps(g, 5);
        check("three gaps of ~296 ms (the taps' landings)", ng == 3 && g[0] >= 290 && g[0] <= 302 && g[1] >= 290 && g[1] <= 302 && g[2] >= 290 && g[2] <= 302);
        printf("     the gaps: %u %u %u\n", (unsigned)g[0], (unsigned)g[1], (unsigned)g[2]);
    }
    run_ms(800);
    EXPECT_NONE("... and nothing after (the four taps were never pets)");

    press(120, 120, 90); run_ms(200); press(120, 120, 90); run_ms(200); press(120, 120, 90);
    run_ms(400); press(120, 120, 90);
    run_ms(600);
    EXPECT("three knocks, then a fourth tap 400 ms after (past the knocks' window): the knocks, then a pet", GES_TRIPLE_TAP, GES_TAP);

    press(120, 120, 90); run_ms(200); press(120, 120, 90); run_ms(200); press(120, 120, 90);
    run_ms(200); press(120, 120, 90); run_ms(400); press(120, 120, 90); run_ms(300); press(120, 120, 90);
    EXPECT("six taps (gaps ~296 296 296 496 396): the knocks, the rhythm opens, THE SIXTH CLOSES IT AT ONCE", GES_TRIPLE_TAP, GES_RHYTHM_START, GES_RHYTHM);
    check("... n = 6", got_last.n == 6);
    check("... dx = just lifted (~96)", got_last.dx >= 80 && got_last.dx <= 120);
    {
        uint32_t g[5]; int ng = gesture_rhythm_gaps(g, 5);
        check("five gaps: ~296 296 296 496 396", ng == 5 && g[3] >= 490 && g[3] <= 502 && g[4] >= 390 && g[4] <= 402 && g[0] >= 290 && g[0] <= 302);
        printf("     the gaps: %u %u %u %u %u\n", (unsigned)g[0], (unsigned)g[1], (unsigned)g[2], (unsigned)g[3], (unsigned)g[4]);
    }
    run_ms(200); press(120, 120, 90);
    run_ms(800);
    EXPECT_NONE("a seventh tap inside the full rhythm's gap: swallowed (no pet on top of the echo)");
    press(120, 120, 90);
    run_ms(600);
    EXPECT("a tap past that gap: a pet again", GES_TAP);

    press(120, 120, 90); run_ms(200); press(120, 120, 90); run_ms(200); press(120, 120, 90);
    run_ms(200); press(120, 120, 90);
    EXPECT("four taps: the rhythm opens ...", GES_TRIPLE_TAP, GES_RHYTHM_START);
    run_ms(200); press(120, 120, 1200);
    run_ms(600);
    EXPECT("... then a 1.2 s hold: the rhythm dropped (4), the hold the lease's (as after the knocks)", GES_TAPS_DROPPED, GES_LEASE_HOLD);
    check("... the dropped rhythm's taps: 4", got[0].kind == GES_TAPS_DROPPED && got[0].n == 4);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
