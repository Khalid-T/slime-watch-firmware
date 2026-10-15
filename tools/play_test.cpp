/* PLAY: THE BEAD AND THE SPIN on the laptop (src/play.cpp has no Arduino
 * in it): the wrist as a few numbers a frame -- in-plane gravity, the
 * jerk, the gyro's yaw -- and the gates as flags; the arbiter's table
 * pinned: the still-flat 3 s window, a tap too early, a tilt (and a
 * jolt) that breaks the stillness, a tap on him, the bead's roll toward
 * gravity and never past the rim, the bite and its small lump, another
 * bead in the open game, "flat 10 s with no bead" ending it, the spin's
 * yaw threshold and its span and its axis, the speed line, one spin one
 * spell, the 10-min repeat of each game, and each of the six gates --
 * refusing the start, and closing an open game. CATCH THE DROP (F50,
 * 2026-10-08): no drop on the first bite, the second owes one 2 s after,
 * it leaves the air door over his wall, falls toward the floor on a
 * level glass, follows a CHANGING gravity at once (no sideways
 * momentum), caught inside his box = the meal's lump 1.0 sinking over a
 * minute, missed = the wet mark at the rim fading over 60 s, a gate
 * closing drops the owed / falling drop and the mark. THE COURSE (F52):
 * a fall in the open game with no bead opens it (in flight nothing;
 * landed, the start), the three other walls as gates the way the fall
 * went, a wrong wall nothing, each gate lit in order, the third = done,
 * the 10-min gap, the direction both ways and for a half turn, 30 s at
 * one wall = left and a gate passed resets it, no course with a bead on
 * the glass or with no game, a gate closing ends it, the status line.
 * THE TOSS AND THE RHYTHM (2026-10-09): a flick
 * along the plane is the toss (read 250 ms after it, every time), its
 * direction kept, the loft to 8 px and the fall back under FALL_G, the
 * landing once, the small splat half the fall's easing off, a shake is
 * not a toss (nor its last half), a bump is not (too short; along the
 * normal), a soft flick is not, the sense once per 10 min while every
 * toss plays, a fall on refuses it; THE RHYTHM: the gaps echoed a
 * second after the last tap, each pulse 0.10 on a half sine for 120
 * ms, the cap at six, the happy take at the end, the sense's gap, a
 * gate closing stops the echo; and each of the seven gates -- refusing
 * the start of all four, and closing an open game.
 * Build: c++ -std=gnu++17 -I src -o play_test tools/play_test.cpp src/play.cpp -lm
 * Run:   ./play_test
 */
#include "../src/play.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* --- the harness: a clock at 40 fps, a wrist, the gates ------------------- */

static const float DT = 0.025f;
static uint32_t now_ms = 5000;
static play_in_t in;
static int last_start = PLAY_NONE;         /* the last non-NONE play_tick answer of a run */
static int starts = 0;                     /* how many frames of a run said a game started */
static float max_r = 0;                    /* the bead's farthest reach from the centre over a run */
static uint32_t evs = 0;                   /* PLAY_EV_* seen over a run */
static char last_log[128] = "";

static void wrist_flat(void)  { in.gx = 0; in.gy = 0; in.jerk_g = 0.0f; in.yaw_dps = 0; in.rate_dps = 0; }
static void gates_open(void)  { in.gates.lit = true; in.gates.quiet = false; in.gates.shelf = false; in.gates.holding = true; in.gates.page = false; in.gates.word = false; in.gates.asleep = false; in.gates.falling = false; }

static void tick(void)
{
    in.now_ms = now_ms; in.dt = DT;
    int r = play_tick(&in);
    if (r != PLAY_NONE) { last_start = r; starts++; }
    evs |= play_events();
    if (play_log_new()) { strncpy(last_log, play_log(), sizeof last_log - 1); last_log[sizeof last_log - 1] = 0; play_log_taken(); }
    float x, y;
    if (play_bead_on(&x, &y)) { float rr = sqrtf((x - 120) * (x - 120) + (y - 120) * (y - 120)); if (rr > max_r) max_r = rr; }
    now_ms += (uint32_t)(DT * 1000.0f + 0.5f);
}
static void run(float seconds) { starts = 0; last_start = PLAY_NONE; max_r = 0; evs = 0; for (float t = 0; t < seconds - 1e-4f; t += DT) tick(); }
/* F50 / F52: he sits landed on a wall; his painted box on the bottom wall (the base 156 px wide, the lower 80 px of the glass) */
static void land(int w)   { in.wall = w; in.on_wall = true; }
static void box_bottom(void) { in.box_x = 42; in.box_y = 160; in.box_w = 156; in.box_h = 80; }
static void box_none(void)   { in.box_w = 0; }
static void run_on(float seconds) { for (float t = 0; t < seconds - 1e-4f; t += DT) tick(); }   /* ... without zeroing the run's tallies (the toss is read after its flick) */
static int  tap(int x, int y, bool on_gel) { int r = play_tap(now_ms, x, y, on_gel); if (play_log_new()) { strncpy(last_log, play_log(), sizeof last_log - 1); play_log_taken(); } return r; }
static void eat(void) { play_bead_eat(now_ms); if (play_log_new()) { strncpy(last_log, play_log(), sizeof last_log - 1); play_log_taken(); } }
static void jump(uint32_t ms) { now_ms += ms; }
/* THE TOSS: an in-plane jerk of (jx, jy) g for ``seconds``, then quiet (the whole jerk = the in-plane part: a flick along the plane) */
static void flick(float jx, float jy, float seconds)
{
    in.jx_g = jx; in.jy_g = jy; in.jerk_g = sqrtf(jx * jx + jy * jy);
    starts = 0; last_start = PLAY_NONE;
    run_on(seconds);
    in.jx_g = 0; in.jy_g = 0; in.jerk_g = 0;
}
/* THE RHYTHM: the detector's gaps handed over (the log taken) */
static int rhythm(const uint32_t *gaps, int n, uint32_t since) { int r = play_rhythm(now_ms, gaps, n, since); if (play_log_new()) { strncpy(last_log, play_log(), sizeof last_log - 1); play_log_taken(); } return r; }
static void fresh(void) { play_begin(); memset(&in, 0, sizeof in); wrist_flat(); gates_open(); now_ms = 5000; last_log[0] = 0; }
/* the bead's game: fresh, 3.1 s flat and still, a bead beside him */
static void bead_game(void) { fresh(); run(3.1f); tap(60, 120, false); }

int main(void)
{
    float x, y;

    printf("play: the still-flat window\n");
    fresh();
    run(2.9f);
    CHECK(!play_bead_armed(), "flat and still 2.9 s: not yet armed (%.2f s)", play_still_s());
    CHECK(tap(60, 120, false) == 0 && play_refused() == PLAY_REFUSED_NOT_ARMED, "a tap too early is not the bead's (the far tap's, as ever): %s", play_refused_name(play_refused()));
    CHECK(!play_bead_open(), "no game from it");
    run(0.2f);
    CHECK(play_bead_armed(), "flat and still 3.1 s: armed");
    CHECK(tap(60, 120, false) == 2, "a tap beside him: THE GAME STARTS with this bead (2: the sense goes once)");
    CHECK(play_bead_on(&x, &y) && x == 60 && y == 120, "the bead is where the finger landed (%.0f,%.0f)", x, y);
    CHECK(play_bead_open() && play_count(PLAY_BEAD) == 1, "the game is open; one bead game played");
    CHECK(strstr(last_log, "the bead at 60,120") != NULL, "the log: %s", last_log);

    printf("play: a tilt, a jolt, a tap on him\n");
    fresh();
    run(2.0f);
    in.gx = 0.5f; run(0.2f);                                  /* a tilt */
    wrist_flat(); run(2.0f);
    CHECK(!play_bead_armed() && play_still_s() < 2.1f, "a tilt at 2 s breaks the stillness: 2 s after it, not armed (%.2f s)", play_still_s());
    run(1.1f);
    CHECK(play_bead_armed(), "... armed 3 s after the tilt");
    fresh();
    run(2.5f);
    in.jerk_g = 0.2f; run(0.1f);                              /* a jolt */
    in.jerk_g = 0; run(2.5f);
    CHECK(!play_bead_armed(), "a jolt at 2.5 s breaks it too (%.2f s)", play_still_s());
    fresh();
    run(3.1f);
    CHECK(tap(120, 200, true) == 0 && play_refused() == PLAY_REFUSED_ON_GEL, "armed, a tap ON him is the pet's, not the bead's: %s", play_refused_name(play_refused()));
    CHECK(play_bead_armed() && !play_bead_open(), "... and he stays armed for the next tap beside him");

    printf("play: the roll\n");
    bead_game();                                              /* the bead at (60,120): 60 px left of the centre */
    in.gx = 0.5f; run(1.0f);                                  /* the glass tilted: down is toward the right */
    play_bead_on(&x, &y);
    CHECK(x > 60 + 20, "gravity toward the right rolls it right (%.0f px from 60 in 1 s)", x);
    in.gx = 1.0f; in.gy = 0.3f; run(5.0f);
    CHECK(max_r <= BEAD_RIM_R + 0.01f, "a hard tilt for 5 s: never past the rim (farthest %.1f of %.0f)", max_r, (double)BEAD_RIM_R);
    play_bead_on(&x, &y);
    float rr = sqrtf((x - 120) * (x - 120) + (y - 120) * (y - 120));
    CHECK(rr > BEAD_RIM_R - 1.0f && x > 120, "it rests on the low side of the bowl (r %.1f, x %.0f)", rr, x);
    in.gx = 0; in.gy = 0; run(2.0f);                          /* level: the loss brings it to rest */
    float x2; play_bead_on(&x2, NULL);
    run(1.0f);
    float x3; play_bead_on(&x3, NULL);
    CHECK(fabsf(x3 - x2) < 1.0f, "level again: it comes to rest (%.1f -> %.1f over 1 s)", x2, x3);
    CHECK(play_busy(), "a bead on the glass: idle life waits");

    printf("play: the bite, another bead, the end\n");
    eat();
    CHECK(!play_bead_on(NULL, NULL) && play_bead_open(), "eaten: the bead gone, the game still open");
    CHECK(fabsf(play_lump() - BEAD_LUMP) < 1e-4f, "the small lump at the bite: %.2f (the meal's is 1.0)", play_lump());
    CHECK(strstr(last_log, "eaten") != NULL, "the log: %s", last_log);
    in.gx = 0.4f; run(10.0f);                                 /* tilted, no bead: the game waits */
    CHECK(play_bead_open(), "tilted 10 s with no bead: still open (only FLAT ends it)");
    CHECK(play_lump() > 0.10f && play_lump() < BEAD_LUMP, "10 s on, the lump is sinking: %.2f", play_lump());
    CHECK(tap(180, 120, false) == 1, "a tap beside him in the open game: another bead (1: no second sense, no stillness owed)");
    CHECK(play_count(PLAY_BEAD) == 1, "... the same game");
    CHECK(tap(60, 60, false) == 0, "a tap with a bead already on the glass: not the bead's (one in the bowl at a time)");
    eat();
    wrist_flat(); run(9.9f);
    CHECK(play_bead_open(), "flat 9.9 s with no bead: still open");
    run(0.2f);
    CHECK(!play_bead_open(), "flat 10 s with no bead: over");
    CHECK(strstr(last_log, "over") != NULL && strstr(last_log, "flat 10 s") != NULL, "the log: %s", last_log);
    run(25.0f);
    CHECK(play_lump() == 0, "30 s after the bite the lump is gone");
    CHECK(play_bead_armed(), "flat and still since: armed again ...");
    CHECK(tap(60, 120, false) == 0 && play_refused() == PLAY_REFUSED_GAP, "... but a new game inside 10 min is refused, nothing visible: %s", play_refused_name(play_refused()));
    jump(600000);
    run(0.1f);
    CHECK(tap(60, 120, false) == 2 && play_count(PLAY_BEAD) == 2, "10 min on: the bead plays again");

    printf("play: the spin\n");
    fresh();
    in.yaw_dps = 350; in.rate_dps = 350; run(1.0f);
    CHECK(starts == 0 && play_dizzy() == 0, "350 deg/s for a second: under the turn a second, nothing");
    wrist_flat(); run(0.5f);
    in.yaw_dps = 400; in.rate_dps = 400; run(0.1f);
    CHECK(starts == 0, "400 deg/s for 100 ms: a flick, not a spin (under %u ms)", (unsigned)SPIN_SPAN_MS);
    run(0.05f);
    CHECK(starts == 1 && last_start == PLAY_SPIN, "400 deg/s held 150 ms: THE SPIN (once)");
    CHECK(fabsf(play_dizzy() - 1.0f) < 0.05f && play_dizzy_sign() == 1, "dizzy 1.0, clockwise");
    CHECK(strstr(last_log, "400 deg/s") != NULL && strstr(last_log, "dizzy") != NULL, "the speed line: %s", last_log);
    CHECK(play_spin_dps() == 400, "the peak kept: %.0f", play_spin_dps());
    run(1.0f);
    CHECK(starts == 0, "the spin going on for a second more: no second spell (one spin, one spell)");
    CHECK(play_dizzy() > 0.4f && play_dizzy() < 0.6f, "the spell half over (%.2f)", play_dizzy());
    wrist_flat(); run(1.1f);
    CHECK(play_dizzy() == 0 && !play_busy(), "two seconds: the world stops");
    in.yaw_dps = -500; in.rate_dps = 500; run(0.2f);
    CHECK(starts == 0 && play_refused() == PLAY_REFUSED_GAP, "a spin inside 10 min: refused, nothing (%s)", play_refused_name(play_refused()));
    wrist_flat(); run(0.2f);
    jump(600000);
    in.yaw_dps = -500; in.rate_dps = 500; run(0.2f);
    CHECK(starts == 1 && play_dizzy_sign() == -1 && play_count(PLAY_SPIN) == 2, "10 min on: the spin again, widdershins");
    wrist_flat(); run(2.5f);
    jump(600000);
    in.yaw_dps = 400; in.rate_dps = 800; run(0.5f);           /* the turn's axis 60 deg off the normal */
    CHECK(starts == 0, "400 deg/s of yaw inside an 800 deg/s swing of the arm: not a FLAT spin, nothing");
    wrist_flat(); run(0.2f);
    in.yaw_dps = 400; in.rate_dps = 440; run(0.5f);           /* within 30 deg */
    CHECK(starts == 1, "400 of 440: within 30 deg of the normal, the spin");

    printf("play: the gates\n");
    struct { const char *name; void (*close)(void); int why; } G[] = {
        { "the dark house",  [](){ in.gates.lit = false; },    PLAY_REFUSED_DARK },
        { "the quiet hours", [](){ in.gates.quiet = true; },   PLAY_REFUSED_QUIET },
        { "the shelf",       [](){ in.gates.shelf = true; },   PLAY_REFUSED_SHELF },
        { "the empty glass", [](){ in.gates.holding = false; }, PLAY_REFUSED_EMPTY },
        { "the page up",     [](){ in.gates.page = true; },    PLAY_REFUSED_PAGE },
        { "a word up",       [](){ in.gates.word = true; },    PLAY_REFUSED_WORD },
        { "asleep",          [](){ in.gates.asleep = true; },  PLAY_REFUSED_ASLEEP },
    };
    for (size_t i = 0; i < sizeof G / sizeof G[0]; i++) {
        fresh(); run(3.1f); G[i].close(); run(0.1f);
        int r = tap(60, 120, false);
        CHECK(r == 0 && play_refused() == G[i].why && !play_bead_open() && !play_bead_on(NULL, NULL),
              "%s: the bead's tap does nothing visible (%s)", G[i].name, play_refused_name(play_refused()));
        in.yaw_dps = 400; in.rate_dps = 400; run(0.3f);
        CHECK(starts == 0 && play_refused() == G[i].why && play_dizzy() == 0,
              "%s: the spin does nothing visible (%s)", G[i].name, play_refused_name(play_refused()));
        wrist_flat();
        bead_game(); G[i].close(); run(0.1f);
        CHECK(!play_bead_open() && !play_bead_on(NULL, NULL) && strstr(last_log, "over") != NULL,
              "%s closing under an open game ends it, the bead gone: %s", G[i].name, last_log);
    }
    fresh(); run(3.1f);
    CHECK(tap(60, 120, false) == 2, "every gate open: the bead plays (the table's last row)");

    printf("catch the drop: the bite's rhythm, the door, the fall\n");
    bead_game(); land(0); box_none(); run(0.1f);
    eat();
    CHECK(play_drop_due_s() == 0 && !play_drop_on(NULL, NULL) && strstr(last_log, "drop") == NULL, "the first bite: no drop (the bead's own): %s", last_log);
    tap(180, 120, false); eat();
    CHECK(fabsf(play_drop_due_s() - CATCH_DELAY_S) < 1e-4f, "the second bite: a drop owed in %.1f s", play_drop_due_s());
    CHECK(strstr(last_log, "a drop is coming") != NULL, "the log: %s", last_log);
    run(1.9f);
    CHECK(!play_drop_on(NULL, NULL) && play_drop_due_s() > 0.05f, "1.9 s on: still owed (%.2f s), nothing on the glass", play_drop_due_s());
    run(0.2f);
    CHECK(play_drop_on(&x, &y) && (evs & PLAY_EV_DROP), "2.1 s: THE DROP leaves the air door (%.0f,%.0f)", x, y);
    CHECK(fabsf(x - 120) < 1.0f && y < 10.0f, "... the door is the rim opposite his wall (the bottom): the top of the glass");
    CHECK(strstr(last_log, "air door") != NULL && strstr(last_log, "bottom wall") != NULL, "the log: %s", last_log);
    CHECK(play_busy(), "a drop in the air: idle life waits");
    run(0.5f);
    play_drop_on(&x, &y);
    CHECK(y > 10.0f && fabsf(x - 120) < 1.0f, "a level glass still drips: toward the floor he sits on (y %.0f after 0.5 s, x %.0f)", y, x);
    box_bottom(); run(3.0f);
    CHECK((evs & PLAY_EV_CAUGHT) && !play_drop_on(NULL, NULL), "falling into his box: CAUGHT, the drop gone");
    CHECK(play_lump() > 0.9f, "the meal's lump at the catch: %.2f (the bead's is 0.30)", play_lump());
    CHECK(!play_mark_on(NULL, NULL, NULL), "caught: no wet mark");
    CHECK(strstr(last_log, "CAUGHT") != NULL, "the log: %s", last_log);
    { uint32_t d, c, m; play_catch_tally(&d, &c, &m); CHECK(d == 1 && c == 1 && m == 0, "the tally: %lu drops, %lu caught, %lu missed", (unsigned long)d, (unsigned long)c, (unsigned long)m); }
    run(58.0f);
    CHECK(play_lump() > 0 && play_lump() < 0.01f, "58 s on, the meal's lump is nearly gone (%.3f)", play_lump());
    run(2.1f);
    CHECK(play_lump() == 0, "a minute: gone");

    printf("catch the drop: steered by the tilt, missed, the mark's fade\n");
    bead_game(); land(0); box_none(); run(0.1f);
    eat(); tap(180, 120, false); eat();
    run(2.1f);
    CHECK(play_drop_on(&x, &y) && y < 10.0f, "the drop at the door");
    in.gx = 0.8f; in.gy = 0.6f; run(0.5f);                    /* down is toward the lower right (53 deg off his wall: still his wall's) */
    play_drop_on(&x, &y);
    CHECK(x > 150.0f && y > 25.0f && y < 40.0f, "gravity to the lower right: it falls that way in a straight line (%.0f,%.0f after 0.5 s)", x, y);
    float xr = x;
    in.gx = -0.8f; run(0.2f);                                 /* the tilt reversed: the drop follows at once (it has no sideways momentum to lose) */
    play_drop_on(&x, &y);
    CHECK(x < xr - 15.0f, "gravity reversed: it falls the new way AT ONCE (%.0f -> %.0f in 0.2 s): a drop, not a rolling bead", xr, x);
    run(1.5f);
    float mk;
    CHECK((evs & PLAY_EV_MISSED) && !play_drop_on(NULL, NULL), "it met the bowl's side with nobody under it: MISSED");
    CHECK(play_mark_on(&x, &y, &mk) && x < 60.0f && mk > 0.9f, "the wet mark at the landing on the left side (%.0f,%.0f), %.2f of it", x, y, mk);
    CHECK(strstr(last_log, "missed") != NULL && strstr(last_log, "wet mark") != NULL, "the log: %s", last_log);
    CHECK(!play_busy() || play_bead_open(), "the drop gone: idle life is free of it");
    wrist_flat(); run(30.0f);
    play_mark_on(NULL, NULL, &mk);
    CHECK(mk > 0.45f && mk < 0.55f, "30 s on, half the mark (%.2f)", mk);
    run(30.2f);
    CHECK(!play_mark_on(NULL, NULL, NULL), "a minute: the mark gone");
    { uint32_t d, c, m; play_catch_tally(&d, &c, &m); CHECK(d == 1 && c == 0 && m == 1, "the tally: %lu drops, %lu caught, %lu missed", (unsigned long)d, (unsigned long)c, (unsigned long)m); }

    printf("catch the drop: the gates\n");
    bead_game(); land(0); box_none(); run(0.1f);
    eat(); tap(180, 120, false); eat();
    in.gates.page = true; run(0.1f);
    CHECK(play_drop_due_s() == 0 && !play_bead_open(), "the page up with a drop owed: the game over, the drop not owed");
    run(2.5f);
    CHECK(!play_drop_on(NULL, NULL), "... and none falls");
    gates_open();
    bead_game(); land(0); box_none(); run(0.1f);
    eat(); tap(180, 120, false); eat(); run(2.1f);
    CHECK(play_drop_on(NULL, NULL), "a drop falling ...");
    in.gates.lit = false; run(0.1f);
    CHECK(!play_drop_on(NULL, NULL) && !play_mark_on(NULL, NULL, NULL), "... the dark house: gone, no mark");
    gates_open();
    bead_game(); land(0); box_none(); run(0.1f);
    eat(); tap(180, 120, false); eat(); run(2.1f);
    in.gx = -1.0f; run(2.0f);
    CHECK(play_mark_on(NULL, NULL, NULL), "a wet mark on the glass ...");
    in.gates.word = true; run(0.1f);
    CHECK(!play_mark_on(NULL, NULL, NULL), "... a word up: the mark gone with the game");
    gates_open(); wrist_flat();

    printf("the course: the entry, the gates in order, the run\n");
    int gw[COURSE_GATES]; bool gl[COURSE_GATES];
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    in.wall = 1; in.on_wall = false; run(0.1f);               /* the fall: in the air */
    CHECK(!play_course_on() && starts == 0, "a wall change in the air: nothing yet");
    in.on_wall = true; run(0.1f);
    CHECK(last_start == PLAY_COURSE && play_course_on() && play_count(PLAY_COURSE) == 1, "landed on the right wall in the open game with no bead: THE COURSE (the sense once)");
    play_course_gates(gw, gl);
    CHECK(gw[0] == 2 && gw[1] == 3 && gw[2] == 0 && !gl[0] && !gl[1] && !gl[2], "the gates: the top, the left, the bottom -- the way the fall went round, none lit");
    CHECK(play_course_next() == 2, "the top is next");
    CHECK(strstr(last_log, "THE COURSE") != NULL && strstr(last_log, "top, the left, then the bottom") != NULL, "the log: %s", last_log);
    CHECK(play_busy(), "a course open: idle life waits");
    land(3); run(0.1f);
    CHECK(play_course_gates(gw, gl) == 0 && play_course_next() == 2, "landed on the left out of order: nothing lit, the top still next");
    CHECK(strstr(last_log, "not a gate yet") != NULL, "the log: %s", last_log);
    land(2); run(0.1f);
    CHECK(play_course_gates(gw, gl) == 1 && gl[0] && (evs & PLAY_EV_GATE), "the top: the first gate LIT");
    CHECK(strstr(last_log, "through the top gate (1 of 3") != NULL, "the log: %s", last_log);
    land(3); run(0.1f);
    CHECK(play_course_gates(gw, gl) == 2 && gl[1] && play_course_next() == 0, "the left: the second, the bottom next");
    land(0); run(0.1f);
    CHECK((evs & PLAY_EV_COURSE_DONE) && !play_course_on() && play_course_next() == -1, "the bottom: the third -- THE COURSE RUN, the gates gone");
    CHECK(strstr(last_log, "COURSE RUN") != NULL, "the log: %s", last_log);
    land(1); run(0.1f);
    CHECK(!play_course_on() && play_refused() == PLAY_REFUSED_GAP && strstr(last_log, "no course") != NULL, "another fall inside 10 min: no course (%s)", play_refused_name(play_refused()));
    jump(600000); land(2); run(0.1f);
    CHECK(play_course_on() && play_count(PLAY_COURSE) == 2, "10 min on: the course again");
    play_course_gates(gw, gl);
    CHECK(gw[0] == 3 && gw[1] == 0 && gw[2] == 1, "from the right to the top: the left, the bottom, the right");

    printf("the course: the leaving, the directions, no course\n");
    run(10.5f);
    CHECK(!play_bead_open() && play_course_on(), "flat 10 s with no bead ends the bead's game; the course outlives it");
    run(19.2f);
    CHECK(play_course_on() && play_course_idle_s() > 29.0f, "29.8 s at one wall: still on (%.1f s)", play_course_idle_s());
    run(0.4f);
    CHECK(!play_course_on() && (evs & PLAY_EV_COURSE_GONE), "30 s: the course left, the gates gone");
    CHECK(strstr(last_log, "course left") != NULL && strstr(last_log, "30 s") != NULL, "the log: %s", last_log);
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    land(1); run(0.1f);
    CHECK(play_course_on(), "a fresh course ...");
    run(20.0f); land(2); run(0.1f);
    CHECK(play_course_gates(gw, gl) == 1 && play_course_idle_s() < 0.2f, "... a gate passed at 20 s resets the leaving (%.1f s)", play_course_idle_s());
    run(29.8f);
    CHECK(play_course_on(), "... 29.9 s after the gate: still on");
    run(0.2f);
    CHECK(!play_course_on(), "... 30: left");
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    land(3); run(0.1f); play_course_gates(gw, gl);
    CHECK(play_course_on() && gw[0] == 2 && gw[1] == 1 && gw[2] == 0, "a fall the other way (bottom to left): the top, the right, the bottom");
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    land(2); run(0.1f); play_course_gates(gw, gl);
    CHECK(play_course_on() && gw[0] == 3 && gw[1] == 0 && gw[2] == 1, "a half turn (bottom to top): the bowl's own way round -- the left, the bottom, the right");
    fresh(); land(0); run(3.1f); tap(60, 120, false);
    land(1); run(0.1f);
    CHECK(!play_course_on() && play_bead_on(NULL, NULL), "a fall with a bead rolling: no course (the bead has the bowl)");
    fresh(); land(0); run(0.5f); land(1); run(0.1f);
    CHECK(!play_course_on() && starts == 0, "a fall with no game open: nothing");
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    land(1); run(0.1f);
    in.gates.quiet = true; run(0.1f);
    CHECK(!play_course_on() && (evs & PLAY_EV_COURSE_GONE) && strstr(last_log, "quiet hours") != NULL, "the quiet hours closing under a course end it: %s", last_log);
    gates_open();
    fresh(); land(0); run(3.1f); tap(60, 120, false); eat();
    land(1); run(0.1f);
    { char s[400]; play_status(s, sizeof s);
      CHECK(strstr(s, "the course: top left bottom (0 lit, the top next") != NULL && strstr(s, "1 courses") != NULL, "the status line: %s", s); }

    printf("play: the toss\n");
    fresh();
    flick(0.8f, 0.0f, 0.075f);                                /* 0.8 g toward the right for three frames, then quiet */
    run(0.2f);
    CHECK(starts == 0 && play_toss_lift() == 0, "a flick of 0.8 g for 75 ms: nothing yet (the reversal's wait)");
    run(0.1f);
    CHECK(starts == 1 && last_start == PLAY_TOSS, "... 250 ms after it, no reversal came: THE TOSS");
    CHECK(play_count(PLAY_TOSS) == 1 && play_toss_sense_due(), "one toss; its sense due");
    CHECK(play_toss_dir(&x, &y) && x > 0.99f && fabsf(y) < 0.01f, "the direction kept: the flick's way (%+.2f,%+.2f)", x, y);
    CHECK(strstr(last_log, "a toss") != NULL && strstr(last_log, "8 px") != NULL, "the log: %s", last_log);
    CHECK(play_busy(), "in the air: idle life waits");
    float peak = 0; bool landed = false; int land_frames = -1; float squash_at_land = 0;
    for (int i = 0; i < 40 && !landed; i++) {
        tick();
        if (play_toss_lift() > peak) peak = play_toss_lift();
        if (play_toss_landed()) { landed = true; land_frames = i + 1; squash_at_land = play_toss_squash(); }
    }
    tick(); tick();
    CHECK(peak > TOSS_LIFT_PX - 0.5f && peak <= TOSS_LIFT_PX, "the loft reaches %.1f px (TOSS_LIFT_PX %.0f)", peak, (double)TOSS_LIFT_PX);
    CHECK(landed && land_frames >= 9 && land_frames <= 12, "it lands once, %d frames on (0.12 s up, 0.13 s down under FALL_G)", land_frames);
    CHECK(!play_toss_landed(), "the landing is a take: said once");
    CHECK(fabsf(squash_at_land - TOSS_SPLAT) < 0.03f && play_toss_lift() == 0, "the small splat at the landing: %.2f (half the fall's 0.48), the lift gone", squash_at_land);
    CHECK(play_toss_squash() > 0 && play_toss_squash() < squash_at_land, "the splat easing off (%.2f)", play_toss_squash());
    run(0.5f);
    CHECK(play_toss_squash() == 0 && !play_busy(), "0.5 s on: over, the body free");
    flick(0.0f, -0.9f, 0.075f); run(0.3f);
    CHECK(starts == 1 && play_count(PLAY_TOSS) == 2 && !play_toss_sense_due(), "a second toss at once (up the glass): plays -- no gap on the toss -- the sense gapped");
    CHECK(play_toss_dir(&x, &y) && y < -0.99f, "its direction: up (%+.2f,%+.2f)", x, y);
    run(0.7f);
    jump(600000); run(0.1f);
    flick(-0.7f, 0.0f, 0.075f); run(0.3f);
    CHECK(starts == 1 && play_toss_sense_due(), "10 min on: the sense again");
    run(0.7f);

    printf("play: a shake is not a toss, a bump is not\n");
    fresh();
    flick(0.8f, 0.0f, 0.075f); run(0.05f);                    /* out ... */
    flick(-0.8f, 0.0f, 0.075f); run(0.05f);                   /* ... back ... */
    flick(0.8f, 0.0f, 0.075f);                                /* ... out: the shake's shape */
    run(0.5f);
    CHECK(starts == 0 && play_count(PLAY_TOSS) == 0, "out-back-out at 0.8 g: a shake, not a toss (its last half neither)");
    CHECK(strstr(last_log, "a shake, not a toss") != NULL, "the log: %s", last_log);
    fresh();
    flick(0.9f, 0.0f, 0.025f); run(0.5f);                     /* one frame: a knock */
    CHECK(starts == 0, "0.9 g for one frame (25 ms): a bump, under TOSS_SPAN_MS");
    fresh();
    in.jx_g = 0.3f; in.jy_g = 0; in.jerk_g = 1.0f; run(0.075f); in.jx_g = 0; in.jerk_g = 0; run(0.5f);
    CHECK(starts == 0, "1.0 g with 0.3 in the plane: a knock along the normal, not a toss");
    fresh();
    flick(0.5f, 0.0f, 0.1f); run(0.5f);
    CHECK(starts == 0, "0.5 g for 100 ms: under TOSS_G, a jiggle");
    fresh();
    flick(0.8f, 0.0f, 0.075f); run(0.05f);
    flick(-0.8f, 0.0f, 0.075f); run(0.6f);
    CHECK(starts == 0, "a flick and a hard stop against it inside 250 ms: not a toss (the stop under 0.6 g is)");
    fresh();
    in.gates.falling = true;
    flick(0.8f, 0.0f, 0.075f); run(0.3f);
    CHECK(starts == 0 && play_refused() == PLAY_REFUSED_FALLING, "a flick mid-fall: nothing -- the fall is the fall (%s)", play_refused_name(play_refused()));
    in.gates.falling = false;

    printf("play: the rhythm\n");
    fresh(); run(0.1f);                                       /* (the gates are the last tick's) */
    uint32_t gaps[3] = { 300, 300, 500 };
    CHECK(rhythm(gaps, 3, 700) == 4, "four taps (300 300 500), closed 700 ms after the last: four pulses scheduled");
    CHECK(play_count(PLAY_RHYTHM) == 1 && play_rhythm_sense_due() && play_busy(), "one rhythm; its sense due; the body waits");
    CHECK(strstr(last_log, "300 300 500") != NULL && strstr(last_log, "1000 ms after the last") != NULL, "the log: %s", last_log);
    {
        /* the pulses, by the frame: when each starts (squash from 0) */
        int starts_at[8]; int np = 0; float pk = 0; bool was = false; int done_at = -1;
        for (int i = 0; i < 100; i++) {
            tick();
            float q = play_rhythm_squash();
            if (q > pk) pk = q;
            if (q > 0 && !was && np < 8) starts_at[np++] = i;
            was = q > 0;
            if (done_at < 0 && play_rhythm_done()) done_at = i;
        }
        CHECK(np == 4, "four pulses played (%d)", np);
        CHECK(np == 4 && starts_at[0] == 13, "the first pulse shows at frame %d: a second after the last tap (300 ms after the close), its rise the frame after its start", np == 4 ? starts_at[0] + 1 : -1);
        CHECK(np == 4 && starts_at[1] - starts_at[0] == 12 && starts_at[2] - starts_at[1] == 12 && starts_at[3] - starts_at[2] == 20,
              "the gaps echoed: %d %d %d frames (300 300 500 ms at 25 ms)", np == 4 ? starts_at[1] - starts_at[0] : 0, np == 4 ? starts_at[2] - starts_at[1] : 0, np == 4 ? starts_at[3] - starts_at[2] : 0);
        CHECK(fabsf(pk - RHYTHM_PULSE) < 0.02f, "each pulse %.2f of squash (RHYTHM_PULSE 0.10)", pk);
        CHECK(done_at > 0 && done_at >= starts_at[3] + 4, "the end once, after the last pulse (frame %d): the happy eyes", done_at + 1);
        CHECK(!play_rhythm_done() && !play_busy() && play_rhythm_pending() == 0, "... a take: said once; the body free");
    }
    uint32_t gaps6[7] = { 300, 300, 300, 300, 300, 300, 300 };
    CHECK(rhythm(gaps6, 7, 0) == 6, "eight taps' gaps, closed at once: the cap, six pulses");
    CHECK(!play_rhythm_sense_due(), "a second rhythm inside 10 min: echoed, the sense not");
    run(0.9f);
    CHECK(play_rhythm_pending() == 6, "0.9 s on: all six still to come");
    in.gates.page = true; run(0.1f);
    CHECK(play_rhythm_pending() == 0 && !play_busy() && strstr(last_log, "the echo stops") != NULL, "the page up under it: the echo stops (%s)", last_log);
    in.gates.page = false;
    jump(600000); run(0.1f);
    CHECK(rhythm(gaps, 1, 700) == 2 && play_rhythm_sense_due(), "10 min on: the sense again");
    run(2.0f);

    printf("play: the gates for the toss and the rhythm\n");
    for (size_t i = 0; i < sizeof G / sizeof G[0]; i++) {
        fresh(); G[i].close(); run(0.1f);
        flick(0.8f, 0.0f, 0.075f); run(0.3f);
        CHECK(starts == 0 && play_refused() == G[i].why && play_toss_lift() == 0, "%s: the flick does nothing visible (%s)", G[i].name, play_refused_name(play_refused()));
        CHECK(rhythm(gaps, 3, 700) == 0 && play_refused() == G[i].why && !play_busy(), "%s: the rhythm does nothing visible (%s)", G[i].name, play_refused_name(play_refused()));
    }
    fresh(); run(0.1f);
    flick(0.8f, 0.0f, 0.075f); run(0.3f);
    CHECK(starts == 1 && rhythm(gaps, 3, 700) == 4, "every gate open: the toss and the rhythm play (the table's last row)");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
