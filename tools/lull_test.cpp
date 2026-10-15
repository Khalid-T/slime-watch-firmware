/* THE LULL on the laptop (lull.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/lull_test tools/lull_test.cpp -x c src/lull.c -lm && /tmp/lull_test
 * Three slow strokes (600 ms or more) inside a minute arm it; it begins
 * only after the third's content face (4 s); quick strokes count for
 * nothing; a stale first stroke drops out (the newest two stand); the
 * lids drift to DROOPY over 3 s and the level reaches 1; the 45 s end
 * eases out over 1.5 s; a slow stroke inside the lull renews the 45 s;
 * a pet, a jolt (over 0.12 g), a wall change end it at once (level 0
 * that frame); the dark takes it silently; never on the empty glass,
 * never dozing; a pet while it waits drops the wait with no event; a
 * fourth slow stroke while it waits moves the wait; no Zs, no sense:
 * the only eye kind it ever says is DROOPY. */
#include "lull.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static lull_in_t in = { 0.04f, true, true, false, 0, 0.0f };
static lull_fx_t fx;
static uint32_t now_ms = 1000;
static int last_ev = LULL_EV_NONE, events = 0, kinds_seen = 0, other_kind = 0;

/* n frames at 25 fps; the last non-NONE event of the run is kept; every eye kind the lull ever says is noted */
static void frames(int n)
{
    for (int i = 0; i < n; i++) {
        int ev = lull_tick(&in);
        now_ms += 40;
        if (ev != LULL_EV_NONE) { last_ev = ev; events++; }
        lull_fx(&fx);
        if (fx.eye_kind >= 0) { kinds_seen++; if (fx.eye_kind != GEL_EYE_DROOPY) other_kind++; }
    }
}
static void seconds(float s) { frames((int)(s / 0.04f + 0.5f)); }
static int take_ev(void) { int e = last_ev; last_ev = LULL_EV_NONE; return e; }
static int stroke(uint32_t held_ms) { return lull_stroke(now_ms, held_ms, 4.0f); }
static void reset(void) { lull_begin(); in = (lull_in_t){ 0.04f, true, true, false, 0, 0.0f }; last_ev = LULL_EV_NONE; }
static float near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

int main(void)
{
    printf("THE LULL: three slow strokes, the wait, the look\n");
    reset();
    CHECK(stroke(400) == LULL_STROKE_QUICK && lull_count() == 0, "a 400 ms stroke: quick, nothing counted");
    CHECK(stroke(700) == LULL_STROKE_COUNTED && lull_count() == 1, "a 700 ms stroke: counted (1 of 3)");
    seconds(5);
    CHECK(stroke(650) == LULL_STROKE_COUNTED && lull_count() == 2, "another at 5 s: counted (2 of 3)");
    seconds(5);
    CHECK(stroke(300) == LULL_STROKE_QUICK && lull_count() == 2, "a quick one between: nothing, the count stands");
    CHECK(stroke(600) == LULL_STROKE_ARMED && lull_armed() && !lull_on(), "a 600 ms third at 10 s: armed, not yet on");
    seconds(3.5f);
    CHECK(!lull_on() && take_ev() == LULL_EV_NONE && fx.eye_kind < 0, "3.5 s into the content face: nothing yet (the face is the stroke's)");
    seconds(1.0f);
    CHECK(lull_on() && take_ev() == LULL_EV_BEGIN, "after the 4 s face: BEGIN");
    CHECK(fx.eye_kind == GEL_EYE_DROOPY && fx.eye_alpha > 0 && fx.eye_alpha < 0.5f, "the lids droopy, drifting in (%.2f)", fx.eye_alpha);
    seconds(3.0f);
    CHECK(near(fx.level, 1.0f, 0.02f) && fx.eye_kind == GEL_EYE_DROOPY, "3 s in: the level at 1 (%.2f), droopy", fx.level);
    CHECK(lull_level() == fx.level, "lull_level is the fx's level (rest's field)");
    seconds(40.0f);
    CHECK(lull_on() && take_ev() == LULL_EV_NONE, "43.5 s of the 45: still on, no event");
    seconds(2.5f);
    CHECK(!lull_on() && take_ev() == LULL_EV_END && lull_end() == LULL_END_TIME, "at 45 s: END, the time (%s)", lull_end_name(lull_end()));
    CHECK(fx.eye_kind == GEL_EYE_DROOPY && fx.level > 0 && fx.level < 1, "the time's end eases out: still droopy, fading (%.2f)", fx.level);
    seconds(1.6f);
    CHECK(fx.eye_kind < 0 && fx.level == 0, "1.5 s later: gone, the draw's own eyes");
    CHECK(other_kind == 0 && kinds_seen > 0, "every eye kind the lull said was DROOPY (%d frames): never Zs", kinds_seen);

    printf("the minute's window\n");
    reset();
    stroke(700); seconds(30); stroke(700); seconds(31);
    CHECK(stroke(700) == LULL_STROKE_COUNTED && lull_count() == 2 && !lull_armed(), "the third at 61 s: the first too old, dropped -- 2 stand, not armed");
    seconds(5);
    CHECK(stroke(700) == LULL_STROKE_ARMED, "a fourth at 66 s: the newest three inside a minute, armed");

    printf("the renew: a slow stroke inside the lull\n");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f);
    CHECK(lull_on() && take_ev() == LULL_EV_BEGIN, "on");
    seconds(40);
    CHECK(stroke(800) == LULL_STROKE_RENEWED, "a slow stroke at 40 s: renewed");
    seconds(10);
    CHECK(lull_on() && take_ev() == LULL_EV_NONE, "50 s after the begin: still on (the 45 run again from the stroke)");
    CHECK(stroke(300) == LULL_STROKE_QUICK, "a quick stroke inside it: nothing");
    seconds(36);
    CHECK(!lull_on() && lull_end() == LULL_END_TIME, "ends 45 s after the renewing stroke");

    printf("the ends: a pet, a jolt, the wall, the dark, he left, a sleeper\n");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3);
    CHECK(lull_on() && near(fx.level, 1.0f, 0.02f), "on, the lids down");
    lull_pet(); frames(1);
    CHECK(!lull_on() && take_ev() == LULL_EV_END && lull_end() == LULL_END_PET && fx.level == 0, "a pet: END at once, the level 0 that frame (%s)", lull_end_name(lull_end()));
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3); take_ev();
    in.jerk = 0.10f; frames(1);
    CHECK(lull_on() && take_ev() == LULL_EV_NONE, "a 0.10 g jerk (the springs' jolt): kept");
    in.jerk = 0.15f; frames(1); in.jerk = 0;
    CHECK(!lull_on() && take_ev() == LULL_EV_END && lull_end() == LULL_END_JOLT && fx.level == 0, "a 0.15 g jerk: END, a jolt, at once");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3); take_ev();
    in.wall = 1; frames(1); in.wall = 0;
    CHECK(!lull_on() && take_ev() == LULL_EV_END && lull_end() == LULL_END_WALL && fx.level == 0, "the wall changed: END, the wall, at once");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3); take_ev();
    lull_dark(); in.lit = false; frames(1); in.lit = true;
    CHECK(!lull_on() && take_ev() == LULL_EV_NONE && fx.level == 0 && lull_end() == LULL_END_DARK, "the dark: silent, nothing left (the cause kept for `lull`)");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3); take_ev();
    in.holding = false; frames(1); in.holding = true;
    CHECK(!lull_on() && take_ev() == LULL_EV_END && lull_end() == LULL_END_GONE, "the pet left mid-lull: END, he left");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(4.1f); seconds(3); take_ev();
    in.dozing = true; frames(1); in.dozing = false;
    CHECK(!lull_on() && lull_end() == LULL_END_GONE, "a sleeper is never lulled (the night is its own): ended");

    printf("the wait's manners\n");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(2);
    CHECK(stroke(900) == LULL_STROKE_WAITING, "a fourth slow stroke while it waits: the wait moves to its face");
    seconds(3.5f);
    CHECK(!lull_on() && lull_armed(), "3.5 s after the fourth: still waiting (its own 4 s)");
    seconds(1);
    CHECK(lull_on() && take_ev() == LULL_EV_BEGIN, "then BEGIN");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(2);
    lull_pet(); frames(1);
    CHECK(!lull_armed() && !lull_on() && take_ev() == LULL_EV_NONE && lull_count() == 0, "a pet while it waits: the wait dropped, the strokes forgotten, no event");
    seconds(5);
    CHECK(!lull_on() && fx.eye_kind < 0, "nothing begins after");
    reset();
    stroke(700); stroke(700); stroke(700); seconds(2);
    in.holding = false; frames(1); in.holding = true;
    CHECK(!lull_armed() && lull_count() == 0, "the pet leaving while it waits: forgotten");
    reset();
    in.lit = false;
    stroke(700); stroke(700); stroke(700); frames(1);
    CHECK(!lull_armed() && !lull_on(), "strokes on a dark glass arm nothing (main never hands them; the tick forgets them)");
    in.lit = true;
    reset();
    CHECK(lull_stroke(now_ms, 700, 0.0f) == LULL_STROKE_COUNTED && lull_stroke(now_ms, 700, 0.0f) == LULL_STROKE_COUNTED
          && lull_stroke(now_ms, 700, 0.0f) == LULL_STROKE_ARMED, "three slow strokes with no face (a costume's): armed");
    frames(1);
    CHECK(lull_on() && take_ev() == LULL_EV_BEGIN, "... and on at the next frame");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
