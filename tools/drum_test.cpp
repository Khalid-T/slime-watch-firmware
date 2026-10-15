/* THE DRUMMED TABLE on the laptop (drum.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/drum_test tools/drum_test.cpp -x c src/drum.c -lm && /tmp/drum_test
 * Silence and the IMU's noise (nothing); a typing jitter (a 3 Hz sine
 * of 0.05 g on one axis = 6 reversals a second: the rate read near 6,
 * BEGIN after 3 s and not before, the level eased toward 1, the clock's
 * laps growing at the bubble's pace and never rewinding); the typing
 * stopped (END 2 s after the rate falls out, the level eased to 0, the
 * laps kept); the band's edges (0.03 g in, 0.015 under the floor, 0.1
 * loud, a 1 Hz fidget too slow, an 8 Hz buzz too fast); a walk's
 * 0.3 g strides (loud: never on); a jolt alone (never on) and a jolt
 * inside the typing (the ring's half second is forgiven, the drumming
 * stays); the jitter across three axes (judged on the busiest, not
 * their sum); the gates (the dark, the empty glass, asleep, walking, a
 * fall: no BEGIN; the dark mid-drum ends it at once, the laps kept);
 * the tick gap; dt clamped; the numbers. */
#include "drum.h"
#include "gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.025f;
static drum_in_t in;
static float t = 0;                     /* the generator's clock */
static int begins = 0, ends = 0;
static float last_begin_t = -1, last_end_t = -1;

static void reset(void)
{
    drum_begin();
    memset(&in, 0, sizeof in);
    in.dt = DT; in.lit = true; in.holding = true;
    t = 0; begins = 0; ends = 0; last_begin_t = -1; last_end_t = -1;
}

/* n frames of a sine of `amp` g at `hz` on axis `ax` (0 x, 1 y, 2 z), the others a 0.005 g noise */
static void sine(int n, int ax, float amp, float hz)
{
    for (int i = 0; i < n; i++) {
        float v = amp * sinf(2.0f * (float)M_PI * hz * t);
        float noise = 0.005f * sinf(37.0f * t);
        in.jx = ax == 0 ? v : noise; in.jy = ax == 1 ? v : noise; in.jz = ax == 2 ? v : noise;
        int ev = drum_tick(&in);
        if (ev == DRUM_EV_BEGIN) { begins++; last_begin_t = t; }
        if (ev == DRUM_EV_END)   { ends++; last_end_t = t; }
        t += in.dt;
    }
}
static void typing(int n) { sine(n, 0, 0.05f, 3.0f); }     /* 6 reversals a second at 0.05 g */
static void quiet(int n)  { sine(n, 0, 0.0f, 0.0f); }
static void frame(float jx, float jy, float jz)
{
    in.jx = jx; in.jy = jy; in.jz = jz;
    int ev = drum_tick(&in);
    if (ev == DRUM_EV_BEGIN) { begins++; last_begin_t = t; }
    if (ev == DRUM_EV_END)   { ends++; last_end_t = t; }
    t += in.dt;
}

int main(void)
{
    reset();

    puts("-- silence");
    quiet(400);
    CHECK(begins == 0 && drum_level() == 0.0f && drum_extra() == 0.0f, "10 s of a still bowl (0.005 g noise): nothing, the level 0, no laps");
    CHECK(drum_rate_hz() == 0.0f, "the noise under the floor counts no reversal (rate 0)");

    puts("-- the typing");
    reset();
    typing(40);
    CHECK(begins == 0 && !drum_on(), "1 s of typing: not yet (a few keys are not a drum roll; the rate warming, %.1f)", drum_rate_hz());
    typing(79);
    CHECK(drum_rate_hz() > 5.0f && drum_rate_hz() < 7.0f, "the rate reads near 6 a second (%.1f)", drum_rate_hz());
    CHECK(begins == 0, "just under 3 s in the band: not yet (%.2f s)", t);
    typing(61);
    CHECK(begins == 1 && drum_on() && last_begin_t >= 3.0f && last_begin_t <= 4.5f, "BEGIN once the band held 3 s (at %.2f s; the rate's warm-up is part of it)", last_begin_t);
    float lv0 = drum_level(), ex0 = drum_extra();
    typing(80);
    CHECK(drum_level() > 0.85f && drum_level() <= 1.0f, "2 s on: the level eased toward 1 (%.2f)", drum_level());
    CHECK(drum_extra() > ex0, "the clock's laps grow (%.3f -> %.3f)", ex0, drum_extra());
    CHECK(lv0 < 0.5f, "... and were still easing in at the begin (%.2f)", lv0);
    typing(160);
    CHECK(drum_level() > 0.98f, "6 s on: the level at 1 (%.3f)", drum_level());
    {   /* the pace: at a full level (DRUM_X - 1) x GEL_GEL_HZ laps a second = 0.24; never a step back */
        float before = drum_extra(); bool monotone = true; float gained = 0;
        for (int i = 0; i < 40; i++) {
            typing(1);
            float d = drum_extra() - before; if (d < 0) d += 1.0f;
            if (d < 0 || d > 0.5f) monotone = false;
            gained += d; before = drum_extra();
        }
        CHECK(monotone, "the laps never rewind (every frame's step forward, wrapped)");
        CHECK(fabsf(gained - (DRUM_X - 1.0f) * GEL_GEL_HZ) < 0.02f, "a second at full level adds %.3f laps (the bubble's x3: %.3f)", gained, (DRUM_X - 1.0f) * GEL_GEL_HZ);
    }
    CHECK(begins == 1 && ends == 0, "one BEGIN, no END through 10 s of typing");

    puts("-- the typing stops");
    float stop_t = t, ex1 = drum_extra();
    quiet(60);
    CHECK(drum_on() && ends == 0, "1.5 s of quiet: still on (a pause for thought)");
    quiet(60);
    CHECK(ends == 1 && !drum_on() && last_end_t - stop_t >= 2.0f && last_end_t - stop_t <= 3.0f, "END %.2f s after the last key (the rate's fall + the 2 s)", last_end_t - stop_t);
    quiet(200);
    CHECK(drum_level() == 0.0f, "5 s later the level is 0");
    { float d = drum_extra() - ex1; if (d < 0) d += 1.0f;
      CHECK(d >= 0.5f && d < 0.9f, "the laps kept: grown through the dwell's 2 s and the ease-out (%.3f), never rewound", d); }

    puts("-- the band's edges");
    reset(); sine(200, 0, 0.03f, 3.0f);
    CHECK(begins == 1, "0.03 g at 6 a second: in (on at %.2f s)", last_begin_t);
    reset(); sine(200, 0, 0.015f, 3.0f);
    CHECK(begins == 0 && drum_rate_hz() == 0.0f, "0.015 g: under the floor, no reversal, nothing");
    reset(); sine(200, 0, 0.10f, 3.0f);
    CHECK(begins == 0, "0.10 g at 6 a second: LOUD, nothing");
    reset(); sine(200, 0, 0.05f, 1.0f);
    CHECK(begins == 0 && drum_rate_hz() < 4.0f, "a 1 Hz fidget (2 a second): too slow (%.1f)", drum_rate_hz());
    reset(); sine(200, 0, 0.05f, 8.0f);
    CHECK(begins == 0 && drum_rate_hz() > 10.0f, "an 8 Hz buzz (16 a second): too fast (%.1f)", drum_rate_hz());
    reset(); sine(200, 0, 0.05f, 2.2f);
    CHECK(begins == 1, "2.2 Hz (4.4 a second): just inside the floor");
    reset(); sine(200, 0, 0.05f, 4.8f);
    CHECK(begins == 1, "4.8 Hz (9.6 a second): just inside the ceiling (%.1f)", drum_rate_hz());

    puts("-- a walk, a jolt");
    reset(); sine(400, 1, 0.30f, 2.0f);
    CHECK(begins == 0 && drum_level() == 0.0f, "10 s of strides at 0.3 g: loud every frame, never on");
    reset(); quiet(40); frame(0.02f, 0.0f, 0.5f); quiet(200);
    CHECK(begins == 0, "a knock's jolt alone, then quiet: nothing");
    reset(); typing(200);
    CHECK(begins == 1, "typing on (%.2f s)", last_begin_t);
    frame(0.4f, 0.1f, 0.2f);
    CHECK(drum_on() && ends == 0, "a bump mid-typing: the drumming stays");
    typing(20);
    CHECK(drum_on() && ends == 0, "... its half second of shut band inside the 2 s: forgiven");
    typing(100);
    CHECK(drum_on() && ends == 0 && begins == 1, "... and the typing goes on: still one drumming");
    reset(); typing(200); frame(0.4f, 0.1f, 0.2f); quiet(120);
    CHECK(ends == 1, "a bump then quiet: END by the 2 s (at %.2f s after)", last_end_t - 5.0f);

    puts("-- the pauses between words");
    reset();
    for (int w = 0; w < 12; w++) { typing(24); quiet(8); }     /* 0.6 s of keys, 0.2 s of nothing, twelve times: 9.6 s */
    CHECK(begins == 1 && drum_on(), "typing with 0.2 s pauses between words: on (at %.2f s; a pause costs its time, not the count)", last_begin_t);
    reset();
    for (int w = 0; w < 8; w++) { typing(40); quiet(40); }     /* a second of keys, a second of nothing: never net 3 s */
    CHECK(begins == 0, "a second on, a second off, for 16 s: never on (the net never reaches 3 s)");

    puts("-- three axes");
    reset();
    for (int i = 0; i < 200; i++) {     /* the same 3 Hz jitter on all three parts: 6 a second on each, judged on the busiest */
        float v = 0.05f * sinf(2.0f * (float)M_PI * 3.0f * t);
        frame(v, -v, v * 0.8f);
    }
    CHECK(begins == 1 && drum_rate_hz() < 8.0f, "the jitter on all three axes reads as 6 a second (%.1f), not 18: on", drum_rate_hz());
    reset();
    for (int i = 0; i < 200; i++) {     /* 2 a second on each of three axes: never 6 */
        float v = 0.05f * sinf(2.0f * (float)M_PI * 1.0f * t);
        frame(v, -v, v * 0.8f);
    }
    CHECK(begins == 0, "a 1 Hz sway on all three axes: still too slow (their sum is not the rate)");

    puts("-- the gates");
    reset(); in.lit = false;     typing(200); CHECK(begins == 0 && drum_rate_hz() == 0.0f, "the dark: nothing read");
    reset(); in.holding = false; typing(200); CHECK(begins == 0, "the empty glass: nothing");
    reset(); in.asleep = true;   typing(200); CHECK(begins == 0, "asleep: nothing");
    reset(); in.walking = true;  typing(200); CHECK(begins == 0, "walking (the day log's word): nothing");
    reset(); in.falling = true;  typing(200); CHECK(begins == 0, "a fall / the weightless beat: nothing");
    reset(); typing(200);
    CHECK(begins == 1 && drum_level() > 0.5f, "on, the level up");
    { float ex = drum_extra(); in.lit = false; typing(1);
      CHECK(ends == 1 && !drum_on(), "the dark mid-drum: END at once");
      CHECK(drum_level() > 0.0f && drum_level() < 1.0f, "... the level eases out (%.2f), not a snap", drum_level());
      typing(200);
      CHECK(drum_level() == 0.0f && begins == 1, "... and in the dark nothing restarts; the level 0");
      float d = drum_extra() - ex; if (d < 0) d += 1.0f;
      CHECK(d >= 0.0f && d < 0.3f, "... the laps kept and only grown by the ease-out (%.3f)", d); }
    reset(); typing(200); in.lit = false; typing(1); in.lit = true; typing(180);
    CHECK(begins == 2 && last_begin_t - 5.025f >= 3.0f, "lit again under the typing: a fresh 3 s, then on again (%.2f s after)", last_begin_t - 5.025f);

    puts("-- the gap, the clamp, the numbers");
    reset(); typing(200); in.dt = 0.6f; typing(1); in.dt = DT;
    CHECK(ends == 1 && !drum_on() && drum_rate_hz() == 0.0f, "a tick gap over 0.5 s: the read dropped, the drumming over");
    reset(); typing(200); in.dt = 5.0f; typing(1); in.dt = DT;
    CHECK(drum_level() >= 0.0f && drum_level() <= 1.0f && drum_extra() >= 0.0f && drum_extra() < 1.0f, "dt 5 s clamped: the level and the laps stay in bounds");
    CHECK(DRUM_LO_G == 0.02f && DRUM_HI_G == 0.08f, "the jitter band 0.02-0.08 g");
    CHECK(DRUM_MIN_HZ == 4.0f && DRUM_MAX_HZ == 10.0f, "4-10 reversals a second");
    CHECK(DRUM_ON_S == 3.0f && DRUM_OFF_S == 2.0f, "3 s to start, 2 s to end");
    CHECK(DRUM_X == 3.0f, "the bubble's pace, x3");
    CHECK(DRUM_HI_G < 0.18f && DRUM_HI_G < 0.6f, "the ceiling under the wake's shake (0.18) and the toss / the day's SHAKEN (0.6): an order below");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
