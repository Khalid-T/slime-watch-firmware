/* THE KNOCK FROM BEHIND on the laptop (knock.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/knock_test tools/knock_test.cpp -x c src/knock.c -lm && /tmp/knock_test
 * A bowl at rest and a walk (nothing); a rap along the normal (one
 * frame over 0.5 g, back under: FIRST on the frame it ends, the eyes
 * WIDE for 0.4 s, then the draw's own); the ring's bounce inside 150 ms
 * (the same rap); a second rap inside 2 s (AGAIN: the SQUINT 0.6 s), a
 * third inside 2 s of the second (the squint again, never a sulk), a
 * rap past the window (FIRST again); the lines (0.5 exactly is not
 * over; the in-plane part at 0.4 of the normal is not along it); a
 * flick (in the plane: nothing -- the toss's), a shove (a frame along
 * the normal with a plane part: nothing), a push (two frames over:
 * 50 ms, nothing), a frame that goes clean then leans (nothing); the
 * gates (a touch down, the dark, the empty glass, asleep, a fall, the
 * weightless beat: nothing; a gate shutting mid-excursion drops it);
 * the tick gap; dt clamped; the numbers. */
#include "knock.h"
#include "gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static knock_in_t in = { 0.025f, 0, 0, 0, true, true, false, false, false, false };
static int last_ev = KNOCK_EV_NONE, events = 0;

/* n frames at 40 fps with the jerk (jx, jy, jz); the last non-NONE event kept, every event counted */
static void frames(int n, float jx, float jy, float jz)
{
    in.jx = jx; in.jy = jy; in.jz = jz;
    for (int i = 0; i < n; i++) {
        int ev = knock_tick(&in);
        if (ev != KNOCK_EV_NONE) { last_ev = ev; events++; }
    }
}
static void quiet(int n) { frames(n, 0.01f, 0.0f, 0.01f); }
static int take_ev(void) { int e = last_ev; last_ev = KNOCK_EV_NONE; return e; }
static void reset(void) { knock_begin(); in = (knock_in_t){ 0.025f, 0, 0, 0, true, true, false, false, false, false }; last_ev = KNOCK_EV_NONE; events = 0; }
/* one rap: a frame over along the normal, then quiet; the event lands on the quiet frame */
static int rap(float g) { frames(1, 0.02f, 0.0f, g); quiet(1); return take_ev(); }

int main(void)
{
    reset();

    puts("-- a bowl at rest, a walk: nothing");
    quiet(200);
    CHECK(events == 0 && knock_eye_kind() == -1, "5 s still: no event, the draw's eyes");
    for (int i = 0; i < 40; i++) { frames(1, 0.3f, 0.1f, 0.2f); frames(1, -0.3f, -0.1f, -0.2f); }
    CHECK(events == 0, "a walk's swings (0.3 g in the plane, 0.2 along the normal): nothing");
    for (int i = 0; i < 10; i++) { frames(1, 0.0f, 0.0f, 0.45f); quiet(3); }
    CHECK(events == 0, "a wrist's bob along the normal at 0.45 g: under the line, nothing");

    puts("-- a rap from behind");
    frames(1, 0.05f, 0.02f, 0.9f);
    CHECK(take_ev() == KNOCK_EV_NONE && knock_eye_kind() == -1, "the frame over 0.5 g: not read yet (the excursion is judged whole)");
    quiet(1);
    CHECK(take_ev() == KNOCK_EV_FIRST && events == 1, "the frame it ends: FIRST");
    CHECK(knock_eye_kind() == GEL_EYE_WIDE, "the eyes WIDE");
    quiet(14);
    CHECK(knock_eye_kind() == GEL_EYE_WIDE, "0.35 s on: still wide");
    quiet(3);
    CHECK(knock_eye_kind() == -1, "past 0.4 s: the ordinary face");
    CHECK(knock_since_s() > 0.3f && knock_since_s() < 0.5f, "since the knock: %.2f s", knock_since_s());

    puts("-- the ring's bounce is the same rap");
    reset(); quiet(4);
    frames(1, 0.0f, 0.0f, 0.8f); frames(1, 0.0f, 0.0f, 0.1f); frames(1, 0.0f, 0.0f, -0.6f); quiet(1);
    CHECK(events == 1 && take_ev() == KNOCK_EV_FIRST, "over, under, over again (the ring) inside 150 ms: one knock, FIRST");
    frames(1, 0.0f, 0.0f, 0.7f); quiet(1);
    CHECK(events == 1, "a frame over 75 ms after the read: still the dead time, nothing");
    quiet(3); frames(1, 0.0f, 0.0f, 0.7f); quiet(1);
    CHECK(events == 2 && take_ev() == KNOCK_EV_AGAIN, "a frame over 200 ms after: past the dead time, a knock (the second)");

    puts("-- a second knock inside 2 s");
    reset(); quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "the first");
    quiet(30);
    CHECK(rap(0.9f) == KNOCK_EV_AGAIN, "0.8 s later: AGAIN");
    CHECK(knock_eye_kind() == GEL_EYE_SQUINT, "the SQUINT (I heard you)");
    quiet(22);
    CHECK(knock_eye_kind() == GEL_EYE_SQUINT, "0.55 s on: still the squint");
    quiet(3);
    CHECK(knock_eye_kind() == -1, "past 0.6 s: the ordinary face");
    quiet(40);
    CHECK(rap(0.9f) == KNOCK_EV_AGAIN && knock_eye_kind() == GEL_EYE_SQUINT, "a third 1.6 s after the second: the squint again, never a sulk");
    quiet(80);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST && knock_eye_kind() == GEL_EYE_WIDE, "2.05 s after the third: past the window, FIRST again, wide");
    reset(); quiet(4); rap(0.9f); quiet(76);
    CHECK(rap(0.9f) == KNOCK_EV_AGAIN, "the window's edge: 1.95 s after is inside it");
    reset(); quiet(4); rap(0.9f); quiet(79);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "2.025 s after: outside it");

    puts("-- the lines");
    reset(); quiet(4);
    CHECK(rap(0.5f) == KNOCK_EV_NONE, "0.5 g exactly along the normal: not over the line");
    CHECK(rap(0.51f) == KNOCK_EV_FIRST, "0.51 g: a knock");
    reset(); quiet(4);
    CHECK(rap(-0.9f) == KNOCK_EV_FIRST, "a rap the other way (the table under it): a knock too");
    reset(); quiet(4);
    frames(1, 0.4f, 0.0f, 1.0f); quiet(1);
    CHECK(take_ev() == KNOCK_EV_NONE, "the in-plane part at 0.4 of the normal: not along it");
    frames(1, 0.39f, 0.0f, 1.0f); quiet(1);
    CHECK(take_ev() == KNOCK_EV_FIRST, "at 0.39: along it");
    reset(); quiet(4);
    frames(1, 0.3f, 0.3f, 1.0f); quiet(1);
    CHECK(take_ev() == KNOCK_EV_NONE, "both in-plane parts at 0.3 (0.42 together): not along it");

    puts("-- what is not a knock");
    reset(); quiet(4);
    frames(1, 0.9f, 0.1f, 0.1f); quiet(1); frames(2, 0.8f, 0.0f, 0.2f); quiet(1);
    CHECK(events == 0, "a flick in the plane (the toss's): nothing");
    frames(1, 0.5f, 0.0f, 0.9f); quiet(1);
    CHECK(events == 0, "a shove: along the normal with a plane part over 0.4 of it: nothing");
    frames(2, 0.0f, 0.0f, 0.9f); quiet(1);
    CHECK(events == 0, "two frames over (50 ms): a push, nothing");
    frames(4, 0.0f, 0.0f, 0.9f); quiet(1);
    CHECK(events == 0, "four frames over: a press, nothing");
    frames(1, 0.0f, 0.0f, 0.9f); frames(1, 0.6f, 0.0f, 0.9f); quiet(1);
    CHECK(events == 0, "clean then leaning into the plane, still over: nothing (the excursion is judged whole)");
    quiet(10);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "... and a clean rap after reads");

    puts("-- the gates");
    reset(); in.touched = true; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_NONE, "a touch down: nothing (a tap's own jerk is along the normal: the finger's)");
    in.touched = false; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "the finger gone: a knock");
    reset(); in.lit = false; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_NONE, "the dark house: nothing (the wake's own verdict)");
    reset(); in.holding = false; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_NONE, "the empty glass: nothing (nobody home to hear it)");
    reset(); in.asleep = true; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_NONE, "asleep: nothing (the shaken sleeper is rest's)");
    reset(); in.falling = true; quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_NONE, "a fall in progress: nothing");
    reset(); in.weightless = true; quiet(4);
    CHECK(rap(2.5f) == KNOCK_EV_NONE, "the weightless beat's impact (2.5 g along the normal): the drop's, nothing");
    reset(); quiet(4);
    frames(1, 0.0f, 0.0f, 0.9f); in.touched = true; quiet(1); in.touched = false;
    CHECK(take_ev() == KNOCK_EV_NONE && events == 0, "a touch landing on the read's frame: dropped silently");
    quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "... the next clean rap reads");
    reset(); quiet(4); rap(0.9f);
    in.lit = false; quiet(2); in.lit = true;
    CHECK(knock_eye_kind() == GEL_EYE_WIDE, "the eyes' clock runs through a dark frame (the draw draws nothing there)");
    quiet(4);
    CHECK(rap(0.9f) == KNOCK_EV_AGAIN, "lit again inside the window: the second");

    puts("-- the tick gap and dt");
    reset(); quiet(4);
    frames(1, 0.0f, 0.0f, 0.9f); in.dt = 0.8f; quiet(1); in.dt = 0.025f;
    CHECK(take_ev() == KNOCK_EV_NONE && events == 0, "a 0.8 s gap after the frame over (the dark lay between): dropped silently");
    in.dt = 0.4f; frames(1, 0.0f, 0.0f, 0.9f); in.dt = 0.025f; quiet(1);
    CHECK(take_ev() == KNOCK_EV_NONE, "a frame over that arrived 0.4 s late: not a knock (the span is its dt)");
    in.dt = 0.035f; frames(1, 0.0f, 0.0f, 0.9f); in.dt = 0.025f; quiet(1);
    CHECK(take_ev() == KNOCK_EV_FIRST, "a 35 ms frame over (a slow frame): under the 40, a knock");
    reset(); quiet(4); rap(0.9f);
    in.dt = 0.25f; quiet(7); in.dt = 0.025f;
    CHECK(rap(0.9f) == KNOCK_EV_AGAIN, "seven 250 ms ticks (the clamp's own) then the rap: 1.8 s, the second");
    reset(); quiet(4); rap(0.9f);
    in.dt = 0.25f; quiet(8); in.dt = 0.025f;
    CHECK(rap(0.9f) == KNOCK_EV_FIRST, "eight then the rap: 2.05 s, past the window, the first");

    puts("-- the numbers");
    CHECK(KNOCK_G == 0.50f && KNOCK_PLANE_K == 0.40f && KNOCK_SPAN_MS == 40u, "KNOCK_G 0.50, KNOCK_PLANE_K 0.40, KNOCK_SPAN_MS 40 (ruling 133)");
    CHECK(KNOCK_WIDE_S == 0.40f && KNOCK_AGAIN_S == 2.0f && KNOCK_SQUINT_S == 0.60f && KNOCK_DEAD_MS == 150u, "KNOCK_WIDE_S 0.4, KNOCK_AGAIN_S 2, KNOCK_SQUINT_S 0.6, KNOCK_DEAD_MS 150");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
