/* THE WEIGHTLESS BEAT on the laptop (fall0.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/fall0_test tools/fall0_test.cpp -x c src/fall0.c -lm && /tmp/fall0_test
 * A bowl at rest (1 g: nothing); a hard arm swing (two frames under
 * 0.3 g: nothing -- the 120 ms); a drop (0.1 g for 200 ms: the LOFT on
 * the fifth frame, the lift up to 8 px over 0.12 s, the eyes WIDE, the
 * breath held) that lands (2.5 g inside the second: the IMPACT, the
 * lift gone at once, the fall's own splat 0.48 easing off over 0.38 s,
 * the eyes the draw's own, the breath back); a drop that is caught (no
 * impact for a second: the CLEAR at 1000 ms, the lift and the breath
 * back over 0.3 s); the impact's line (1.8 exactly is not over it); a
 * long weightlessness (one beat: no second loft until |a| came back);
 * the gates (the dark, the empty glass, asleep, a fall in progress:
 * nothing, even at 0 g for a second; a gate shutting mid-air clears
 * silently); the tick gap (the dark between two ticks); a second drop
 * right after the first; dt clamped. */
#include "fall0.h"
#include "gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static fall0_in_t in = { 0.025f, 1.0f, true, true, false, false };
static int last_ev = FALL0_EV_NONE, events = 0;

/* n frames at 40 fps at |a| = a; the last non-NONE event is kept, every event counted */
static void frames(int n, float a)
{
    in.a_g = a;
    for (int i = 0; i < n; i++) {
        int ev = fall0_tick(&in);
        if (ev != FALL0_EV_NONE) { last_ev = ev; events++; }
    }
}
static int take_ev(void) { int e = last_ev; last_ev = FALL0_EV_NONE; return e; }
static bool near(float a, float b, float tol) { return fabsf(a - b) <= tol; }
static void reset(void) { fall0_begin(); in = (fall0_in_t){ 0.025f, 1.0f, true, true, false, false }; last_ev = FALL0_EV_NONE; events = 0; }
static void reset_on_floor(void) { reset(); frames(1, 1.0f); }   /* ... with one sample at 1 g: a floor to leave */

int main(void)
{
    reset();

    puts("-- a bowl at rest: nothing");
    frames(200, 1.0f);
    CHECK(take_ev() == FALL0_EV_NONE && events == 0 && fall0_lift() == 0 && fall0_breath_k() == 1.0f && fall0_eye_kind() == -1 && !fall0_on(),
          "1 g for 5 s: no event, no lift, the breath its own, the draw's eyes");
    frames(40, 1.3f); frames(40, 0.7f);
    CHECK(events == 0 && fall0_lift() == 0, "a walk's swings (0.7-1.3 g): nothing");

    puts("-- a hard arm swing: two frames under 0.3 g, then 1 g");
    frames(2, 0.2f); frames(20, 1.0f);
    CHECK(events == 0 && fall0_lift() == 0 && !fall0_on(), "50 ms under 0.3 g: not a loft (the 120 ms)");
    frames(4, 0.1f); frames(20, 1.0f);
    CHECK(events == 0 && !fall0_on(), "100 ms under 0.3 g: not a loft either");
    frames(3, 0.2f); frames(2, 0.9f); frames(3, 0.2f); frames(20, 1.0f);
    CHECK(events == 0 && !fall0_on(), "two dips with a frame of 0.9 g between: the clock restarts, no loft");

    puts("-- a drop that lands");
    frames(4, 0.1f);
    CHECK(take_ev() == FALL0_EV_NONE && !fall0_on(), "100 ms in: still on the floor");
    frames(1, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT && fall0_on() && fall0_eye_kind() == GEL_EYE_WIDE, "the fifth frame (125 ms): LOFT, in the air, the eyes WIDE");
    float l1 = fall0_lift(), b1 = fall0_breath_k();
    CHECK(l1 > 0 && l1 < FALL0_LIFT_PX && b1 < 1.0f && b1 > 0.0f, "one frame up: the lift rising (%.2f px), the breath on its way to held (%.2f)", l1, b1);
    frames(5, 0.1f);
    CHECK(near(fall0_lift(), FALL0_LIFT_PX, 1e-3f) && near(fall0_breath_k(), 0.0f, 1e-3f) && fall0_eye_kind() == GEL_EYE_WIDE,
          "0.15 s up: the lift at the toss's %.0f px, the breath held (k %.2f), WIDE", (double)FALL0_LIFT_PX, fall0_breath_k());
    CHECK(near(fall0_lift(), TOSS_LIFT_PX, 1e-3f), "the loft IS the toss's lift road (TOSS_LIFT_PX %.0f)", (double)TOSS_LIFT_PX);
    frames(6, 0.15f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_on() && fall0_squash() == 0, "400 ms in the air, no splat yet");
    frames(1, 2.5f);
    CHECK(take_ev() == FALL0_EV_IMPACT && !fall0_on() && fall0_easing(), "2.5 g: IMPACT");
    CHECK(fall0_lift() == 0 && fall0_breath_k() == 1.0f && fall0_eye_kind() == -1, "the lift gone at once, the breath back, the eyes the draw's own (the squint is main's)");
    CHECK(near(fall0_squash(), LAND_SQUASH, 1e-3f), "the fall's own splat: LAND_SQUASH %.2f on the impact's frame", (double)fall0_squash());
    frames(7, 1.0f);
    float sq = fall0_squash();
    CHECK(sq > 0 && sq < LAND_SQUASH, "0.18 s on: the splat easing off (%.2f)", sq);
    frames(10, 1.0f);
    CHECK(fall0_squash() == 0 && !fall0_easing() && !fall0_on() && events == 2, "past LAND_S: the splat spent, on the floor; two events in all");

    puts("-- a second drop right after: read again");
    frames(5, 0.05f);
    CHECK(take_ev() == FALL0_EV_LOFT && fall0_on(), "a second loft, no gap owed");
    frames(1, 3.0f);
    CHECK(take_ev() == FALL0_EV_IMPACT, "... and its impact");
    frames(20, 1.0f);

    puts("-- a drop that is caught (no impact inside the second)");
    frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "LOFT");
    frames(10, 1.0f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_on() && near(fall0_lift(), FALL0_LIFT_PX, 1e-3f), "caught (1 g again): still in the air, waiting on the impact");
    frames(29, 1.0f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_on(), "975 ms after the loft: still waiting");
    frames(1, 1.0f);
    CHECK(take_ev() == FALL0_EV_CLEAR && !fall0_on() && fall0_easing(), "1000 ms: CLEAR, easing down");
    CHECK(fall0_eye_kind() == -1 && fall0_squash() == 0, "the ordinary face, no splat");
    frames(6, 1.0f);
    float l2 = fall0_lift(), b2 = fall0_breath_k();
    CHECK(l2 > 0 && l2 < FALL0_LIFT_PX && b2 > 0 && b2 < 1, "0.15 s into the ease: the lift half down (%.2f px), the breath coming back (%.2f)", l2, b2);
    frames(7, 1.0f);
    CHECK(fall0_lift() == 0 && fall0_breath_k() == 1.0f && !fall0_easing(), "0.3 s: down, the breath its own, on the floor");
    frames(1, 2.5f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_squash() == 0, "a 2.5 g bump after the ease is no impact (the second passed)");

    puts("-- the impact's line");
    frames(20, 1.0f); frames(5, 0.1f); take_ev();
    frames(1, 1.8f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_on(), "1.8 g exactly: not over it, still in the air");
    frames(1, 1.81f);
    CHECK(take_ev() == FALL0_EV_IMPACT, "1.81 g: the impact");
    frames(20, 1.0f);

    puts("-- a long weightlessness is one beat");
    frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "LOFT");
    frames(40, 0.1f);
    CHECK(take_ev() == FALL0_EV_CLEAR, "a second of it: CLEAR");
    frames(60, 0.1f);
    CHECK(take_ev() == FALL0_EV_NONE && !fall0_on() && fall0_lift() == 0, "1.5 s more under 0.3 g: no second loft until |a| came back");
    frames(1, 1.0f); frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "|a| back at 1 g for a frame, then 125 ms of free fall: read again");
    frames(1, 2.0f); frames(20, 1.0f);

    puts("-- the gates");
    reset(); in.lit = false;
    frames(40, 0.0f);
    CHECK(events == 0 && fall0_lift() == 0, "the dark house: 0 g for a second, nothing (the 250 ms poll could not read it anyway)");
    reset(); in.holding = false;
    frames(40, 0.0f);
    CHECK(events == 0 && fall0_lift() == 0, "the empty glass: nothing (no gel to loft)");
    reset(); in.asleep = true;
    frames(40, 0.0f);
    CHECK(events == 0 && fall0_lift() == 0, "asleep: nothing");
    reset(); in.falling = true;
    frames(40, 0.0f);
    CHECK(events == 0 && fall0_lift() == 0, "a fall in progress: nothing (the fall is the fall)");
    reset(); in.lit = false; frames(40, 0.0f); in.lit = true; frames(5, 0.0f);
    CHECK(events == 0 && !fall0_on(), "the gate opening mid-weightlessness: not read (no sample at or over 0.3 g since the gate: one beat needs a floor first)");
    frames(1, 1.0f); frames(5, 0.0f);
    CHECK(take_ev() == FALL0_EV_LOFT, "... a frame of 1 g, then 125 ms under: read");
    reset_on_floor(); frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "(in the air)");
    in.lit = false; frames(1, 0.1f);
    CHECK(take_ev() == FALL0_EV_NONE && !fall0_on() && fall0_lift() == 0 && fall0_breath_k() == 1.0f, "the dark mid-air: cleared silently, nothing left");
    in.lit = true; frames(1, 2.5f);
    CHECK(take_ev() == FALL0_EV_NONE && fall0_squash() == 0, "a 2.5 g on the first lit frame after: no impact (nothing was in the air)");
    reset_on_floor(); frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "(in the air)");
    in.asleep = true; frames(1, 0.1f); in.asleep = false;
    CHECK(take_ev() == FALL0_EV_NONE && !fall0_on(), "sleep mid-air: cleared silently");
    reset_on_floor(); frames(5, 0.1f); take_ev(); frames(1, 2.5f);
    CHECK(take_ev() == FALL0_EV_IMPACT, "(landed)");
    in.holding = false; frames(3, 1.0f);
    CHECK(fall0_squash() > 0 && fall0_easing(), "the pet leaving mid-splat: the splat runs out on its own (nothing worse than a squash on an empty glass)");
    in.holding = true; frames(20, 1.0f);
    CHECK(fall0_squash() == 0 && !fall0_easing(), "... and is spent");

    puts("-- the tick gap and dt");
    reset_on_floor(); frames(5, 0.1f);
    CHECK(take_ev() == FALL0_EV_LOFT, "(in the air)");
    in.dt = 0.8f; frames(1, 0.1f); in.dt = 0.025f;
    CHECK(take_ev() == FALL0_EV_NONE && !fall0_on() && fall0_lift() == 0, "a 0.8 s gap between ticks (the dark lay between): the loft dropped silently");
    reset();
    in.dt = 0.4f; frames(1, 0.1f);           /* under the gap, over the clamp */
    CHECK(events == 0 && !fall0_on(), "from boot, one 0.4 s tick under 0.3 g: nothing (never a floor to leave)");
    in.dt = 0.025f; frames(1, 0.1f);
    CHECK(take_ev() == FALL0_EV_NONE, "... nor on the next (still no sample at or over 0.3 g)");
    reset(); frames(1, 1.0f); in.dt = 0.4f; frames(1, 0.1f); in.dt = 0.025f;
    CHECK(take_ev() == FALL0_EV_LOFT, "armed by a 1 g sample, then one 0.4 s tick under 0.3 g: dt clamped to 0.25 s, still over the 120 ms -- the loft (the clamp bounds a stall, it does not hide a drop)");
    frames(1, 2.0f); frames(20, 1.0f);

    puts("-- the numbers");
    CHECK(FREEFALL_G == 0.30f && FREEFALL_MS == 120u && IMPACT_G == 1.8f && IMPACT_WAIT_MS == 1000u, "FREEFALL_G 0.30, FREEFALL_MS 120, IMPACT_G 1.8, IMPACT_WAIT_MS 1000 (ruling 127)");
    CHECK(FALL0_LIFT_PX == TOSS_LIFT_PX && FALL0_UP_S == TOSS_UP_S, "the loft on the toss's road: TOSS_LIFT_PX, TOSS_UP_S");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
