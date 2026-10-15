/* THE HOUR'S GLANCE on the laptop (hourlook.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/hourlook_test tools/hourlook_test.cpp -x c src/hourlook.c -lm && /tmp/hourlook_test
 * The :00 edge and no other minute's; its 1.5 s then forward; the
 * clock first set at :00 is no turn; missed (not caught up) in the
 * dark, on the empty glass, under a page, with the body busy; the
 * glass going dark or the page mid-glance ends it; the rim point at
 * the hour tick's angle for the four quarters and 09:15's. */
#include "hourlook.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.05f;
static hourlook_in_t in;

static void reset_in(int minute) { in.dt = DT; in.minute = minute; in.lit = true; in.holding = true; in.busy = false; in.page = false; }
static int run(float s)
{
    int n = (int)(s / DT + 0.5f), evs = 0;
    for (int i = 0; i < n; i++) if (hourlook_tick(&in) == HOURLOOK_EV_GLANCE) evs++;
    return evs;
}

int main(void)
{
    puts("-- the :00 edge");
    reset_in(9 * 60 + 59); run(2.0f);
    CHECK(!hourlook_on(), "09:59 for 2 s: nothing");
    in.minute = 10 * 60;
    CHECK(run(DT) == 1 && hourlook_on(), "the minute turns 10:00: the glance");
    CHECK(fabsf(hourlook_left_s() - HOURLOOK_S) < 0.001f, "... %.2f s of it ahead (the edge's frame is whole)", hourlook_left_s());
    run(1.4f);
    CHECK(hourlook_on(), "at 1.45 s still on");
    run(0.1f);
    CHECK(!hourlook_on() && hourlook_left_s() == 0, "at %.1f s forward", HOURLOOK_S);
    CHECK(run(10.0f) == 0, "10 s more of 10:00: once");
    in.minute = 10 * 60 + 1;
    CHECK(run(1.0f) == 0 && !hourlook_on(), "10:01: no other minute's turn glances");
    in.minute = 10 * 60 + 30; run(1.0f); in.minute = 10 * 60 + 59; run(1.0f);
    CHECK(!hourlook_on(), "... nor :30 nor :59");
    in.minute = 11 * 60;
    CHECK(run(DT) == 1 && hourlook_count() == 2, "11:00: the next hour's (%d so far)", hourlook_count());

    puts("-- a clock set at :00 is no turn");
    reset_in(-1); run(1.0f); in.minute = 12 * 60;
    CHECK(run(1.0f) == 0 && !hourlook_on(), "no clock then 12:00: nothing (the hand did not move, it appeared)");
    in.minute = 12 * 60 + 1; run(0.5f); in.minute = 13 * 60;
    CHECK(run(DT) == 1, "... 13:00 after it: the glance");
    run(2.0f);

    puts("-- missed, not caught up");
    const char *names[] = { "the dark", "the empty glass", "a page", "the body busy" };
    for (int k = 0; k < 4; k++) {
        reset_in(14 * 60 + 59 + k * 60); run(0.5f);
        switch (k) { case 0: in.lit = false; break; case 1: in.holding = false; break; case 2: in.page = true; break; case 3: in.busy = true; break; }
        in.minute += 1;
        int evs = run(1.0f);
        reset_in(in.minute);
        evs += run(5.0f);
        CHECK(evs == 0 && !hourlook_on(), "%s at the turn: no glance then, none later", names[k]);
    }

    puts("-- ended early");
    reset_in(19 * 60 + 59); run(0.5f); in.minute = 20 * 60; run(0.3f);
    CHECK(hourlook_on(), "20:00's glance running");
    in.lit = false; run(DT);
    CHECK(!hourlook_on(), "the glass dark at 0.3 s: forward");
    reset_in(20 * 60 + 59); run(0.5f); in.minute = 21 * 60; run(0.3f); in.page = true; run(DT);
    CHECK(!hourlook_on(), "a page up at 0.3 s: forward");
    reset_in(21 * 60 + 59); run(0.5f); in.minute = 22 * 60; run(0.3f); in.busy = true; run(0.5f);
    CHECK(hourlook_on(), "the body busy at 0.3 s does NOT end it (a glance rides a settle; 0.8 s in, on)");

    puts("-- the rim point at the hour tick's angle (12 up, clockwise; r 110 from 120,120)");
    float x, y;
    hourlook_point(0, 120, 120, HOURLOOK_R, &x, &y);
    CHECK(fabsf(x - 120) < 0.01f && fabsf(y - 10) < 0.01f, "12 o'clock (0 deg): %.0f,%.0f", x, y);
    hourlook_point(90, 120, 120, HOURLOOK_R, &x, &y);
    CHECK(fabsf(x - 230) < 0.01f && fabsf(y - 120) < 0.01f, "3 o'clock (90 deg): %.0f,%.0f", x, y);
    hourlook_point(180, 120, 120, HOURLOOK_R, &x, &y);
    CHECK(fabsf(x - 120) < 0.01f && fabsf(y - 230) < 0.01f, "6 o'clock (180 deg): %.0f,%.0f", x, y);
    hourlook_point(270, 120, 120, HOURLOOK_R, &x, &y);
    CHECK(fabsf(x - 10) < 0.01f && fabsf(y - 120) < 0.01f, "9 o'clock (270 deg): %.0f,%.0f", x, y);
    hourlook_point(277, 120, 120, HOURLOOK_R, &x, &y);   /* 09:15: (555 % 720) / 2 = 277 */
    CHECK(x < 12 && x > 9 && y < 120 && y > 105, "09:15's hour tick (277 deg): %.0f,%.0f -- a hair past 9", x, y);

    printf("hourlook_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
