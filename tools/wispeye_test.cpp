/* THE WISP WATCHED on the laptop (wispeye.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/wispeye_test tools/wispeye_test.cpp -x c src/wispeye.c -lm && /tmp/wispeye_test
 * The gaze in his frame (the three posts and the air door from the
 * bottom wall, the same posts from the right wall, the dead band, a
 * point on him); the watch: it begins with the flight and follows,
 * the landing starts the dwell, two seconds and the ordinary face, a
 * newer mote is a new watch, an older one newest again is not, the
 * dark glass and the empty house end it, the mote fading ends it, a
 * mote already at its post is watched at once for its two seconds;
 * the blink: one for a swell, none twice, none on the empty glass. */
#include "wispeye.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 1.0f / 30.0f;
/* the gel's centre on the bottom wall as main's look_at finds it: 58 px under the face's centre */
static const float CX = 120.0f, CY = 178.0f;

static wispeye_in_t in;
static wispeye_fx_t fx;

static void frame(void) { in.dt = DT; wispeye_tick(&in, &fx); in.swelled = 0; }
static void run(float s) { for (float t = 0; t < s; t += DT) frame(); }

int main(void)
{
    int gx, gy;
    puts("-- the gaze in his frame (the bottom wall)");
    wispeye_gaze(194, 138, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == WISPEYE_GX && gy == -WISPEYE_GY, "the right post (194,138): %+d,%+d -- his right, up", gx, gy);
    wispeye_gaze(46, 138, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == -WISPEYE_GX && gy == -WISPEYE_GY, "the left post (46,138): %+d,%+d -- his left, up", gx, gy);
    wispeye_gaze(168, 76, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == WISPEYE_GX && gy == -WISPEYE_GY, "the third post (168,76): %+d,%+d", gx, gy);
    wispeye_gaze(120, 4, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == 0 && gy == -WISPEYE_GY, "the air door (120,4): %+d,%+d -- straight up", gx, gy);
    wispeye_gaze(120, 178, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == 0 && gy == 0, "on him: no turn");
    wispeye_gaze(139, 178, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == 0 && gy == 0, "19 px to his right: inside the dead band");
    wispeye_gaze(141, 178, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == WISPEYE_GX && gy == 0, "21 px to his right: the turn");
    wispeye_gaze(120, 200, CX, CY, 0.0f, &gx, &gy);
    CHECK(gx == 0 && gy == WISPEYE_GY, "22 px under him: down");

    puts("-- the right wall (phi = pi/2: gravity toward the right edge, his right is screen up)");
    {
        float phi = (float)M_PI / 2.0f;
        float cx = 120.0f + 58.0f, cy = 120.0f;             /* the centre 58 px toward the right edge */
        wispeye_gaze(138, 46, cx, cy, phi, &gx, &gy);        /* the right post: depth 18 along g (x), along 74 along r = (0, -1) (y up) */
        CHECK(gx == WISPEYE_GX && gy == -WISPEYE_GY, "the right post on the right wall (138,46): %+d,%+d", gx, gy);
        wispeye_gaze(4, 120, cx, cy, phi, &gx, &gy);
        CHECK(gx == 0 && gy == -WISPEYE_GY, "the air door on the right wall (4,120): straight up");
    }

    puts("-- the watch: the flight, the landing, the dwell");
    wispeye_reset();
    in.lit = true; in.holding = true; in.have = true; in.cx = CX; in.cy = CY; in.phi = 0;
    in.wx = 120; in.wy = 4; in.flying = true; in.seq = 1; in.swelled = 0;
    frame();
    CHECK(fx.began && fx.watching && fx.gaze_x == 0 && fx.gaze_y == -WISPEYE_GY, "the first frame of the flight: the watch begins, the gaze up at the door");
    frame();
    CHECK(!fx.began && fx.watching, "began says so once");
    in.wx = 160; in.wy = 60; frame();                     /* on the bow, halfway over */
    CHECK(fx.gaze_x == WISPEYE_GX && fx.gaze_y == -WISPEYE_GY, "halfway along the bow: the gaze follows (%+d,%+d)", fx.gaze_x, fx.gaze_y);
    CHECK(wispeye_dwell() < 0, "not dwelling while it flies");
    in.wx = 194; in.wy = 138; in.flying = false; frame();  /* landed */
    CHECK(fx.watching && wispeye_dwell() >= 0 && wispeye_dwell() < 0.1f, "landed: the dwell begins (%.2f s)", wispeye_dwell());
    run(1.5f);
    CHECK(fx.watching && fx.gaze_x == WISPEYE_GX && fx.gaze_y == -WISPEYE_GY, "1.5 s at the post: still on it");
    run(0.6f);
    CHECK(!fx.watching && fx.gaze_x == 0 && fx.gaze_y == 0, "past %.0f s: the ordinary face", WISPEYE_DWELL_S);
    CHECK(wispeye_dwell() < 0 && wispeye_watched() == 1, "the dwell over, #1 remembered as watched");
    run(1.0f);
    CHECK(!fx.watching && !fx.began, "the same mote hovering on: no second watch");

    puts("-- the newest of three");
    in.wx = 46; in.wy = 138; in.flying = true; in.seq = 2; frame();
    CHECK(fx.began && fx.watching && fx.gaze_x == -WISPEYE_GX, "a second mote (#2) flying to the left post: a new watch, the gaze his left");
    in.wx = 168; in.wy = 76; in.seq = 3; frame();
    CHECK(fx.began && fx.watching && fx.gaze_x == WISPEYE_GX, "a third (#3) before the second landed: the newest is watched");
    in.flying = false; run(0.5f);
    CHECK(fx.watching && wispeye_dwell() > 0.4f, "#3 landed, dwelling");
    in.wx = 46; in.wy = 138; in.seq = 2; frame();          /* #3 cleared at its source: #2 is the newest again */
    CHECK(!fx.watching && !fx.began && wispeye_watched() == 3, "#3 gone, #2 the newest again: not watched twice (it had its look)");
    in.seq = 4; in.wx = 168; in.wy = 76; in.flying = false; frame();
    CHECK(fx.began && fx.watching && wispeye_dwell() >= 0, "#4 already at its post (the watch began late): watched at once, the dwell from now");
    run(2.1f);
    CHECK(!fx.watching, "... for its two seconds");

    puts("-- the gates");
    in.seq = 5; in.flying = true; in.wx = 120; in.wy = 4; frame();
    CHECK(fx.watching, "#5 flies: watched");
    in.lit = false; frame();
    CHECK(!fx.watching && fx.gaze_x == 0 && fx.gaze_y == 0, "the glass dark: no look");
    in.lit = true; frame();
    CHECK(!fx.watching && !fx.began, "lit again: #5 was already the one watched -- no restart");
    in.seq = 6; frame();
    CHECK(fx.watching, "#6: watched");
    in.holding = false; frame();
    CHECK(!fx.watching, "he left: no look (the empty glass)");
    in.holding = true; in.seq = 7; frame();
    CHECK(fx.watching, "#7 after his return: watched");
    in.have = false; in.seq = 0; frame();
    CHECK(!fx.watching, "the mote fading (none to watch): the watch ends with it");
    in.have = true; in.seq = 7; in.flying = false; frame();
    CHECK(!fx.watching, "#7 back as the newest (it was not fading after all): not watched twice");

    puts("-- the blink on a swell");
    in.swelled = 1; frame();
    CHECK(fx.blink, "a halo swelled: one blink");
    frame();
    CHECK(!fx.blink, "the next frame: no blink (one per swell)");
    in.swelled = 3; frame();
    CHECK(fx.blink, "three swelled in one frame: one blink");
    in.holding = false; in.swelled = 1; frame();
    CHECK(!fx.blink, "the empty glass: no blink");
    in.holding = true; in.lit = false; in.swelled = 1; frame();
    CHECK(!fx.blink, "the dark glass: no blink");
    in.lit = true; in.have = false; in.seq = 0; in.swelled = 1; frame();
    CHECK(fx.blink, "a swell on an older mote while none is watched: still the blink (the swell is its own)");

    printf("wispeye_test: %d checks, %d failed\n", checks, fails);
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
