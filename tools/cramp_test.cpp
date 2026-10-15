/* THE SMALL HOUSE on the laptop (cramp.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/cramp_test tools/cramp_test.cpp -x c src/cramp.c -lm && /tmp/cramp_test
 * The clock's growth (45 lit minutes to full) and its pauses (the dark
 * house, the empty house, asleep, the shelf, the cable, the quiet
 * hours, RESTING = no cramp); the outlets' spending (a game 0.15, a pet
 * 0.25, a walk's minutes); the stance (0.06 at full, eased, never a
 * snap) and the fidget's push (from a quarter cramped, 6 px on a half
 * sine, never while busy); the nudge's conditions (full + the desk near
 * = the roll; not full = nothing; unreachable = the fidget only, said
 * once); the roll is a roll (over many seeds both happen; regard shifts
 * it); the nudge's shape (one event, the push, the look up 2 s, the
 * open window); his answers (a pet inside 10 s = stay a while: content,
 * halved, the gap; no answer = silence for the hour, the level kept;
 * the release = the wish granted, zeroed); once per hour never twice;
 * the wrist's own bid (20 ignored lit minutes, a touch restarts the
 * spell, the gap of 90 min, the pet's answer); every gate on the bid
 * too; the names and the status line. */
#include "cramp.h"
#include "gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static cramp_in_t in;
static uint32_t now_ms = 100000;
static int events[8]; static int n_events = 0;
static float max_push = 0, max_squash = 0, max_squash_step = 0, prev_squash = 0; static int look_frames = 0, content_frames = 0, pushes_seen = 0; static bool push_was = false;

static void home(void)   /* the ordinary lit afternoon: holding, awake, worn, off the cable, by day, lively, the desk's door answered 2 min ago */
{
    memset(&in, 0, sizeof in);
    in.lit = true; in.holding = true; in.asleep = false; in.shelf = false; in.cable = false; in.quiet = false;
    in.resting = false; in.walking = false; in.busy = false; in.desk_age_s = 120; in.regard = 0;
}
static void reset_seen(void) { n_events = 0; max_push = 0; max_squash = 0; max_squash_step = 0; look_frames = 0; content_frames = 0; pushes_seen = 0; push_was = false; prev_squash = 0; }
static void drain_log(void) { while (cramp_log_new()) { printf("       log: %s\n", cramp_log()); cramp_log_taken(); } }
static void run(float s, bool show_log = true)
{
    for (float t = 0; t < s; t += 0.025f) {
        in.dt = 0.025f; in.now_ms = now_ms;
        cramp_tick(&in);
        int e = cramp_take_event();
        if (e && n_events < 8) events[n_events++] = e;
        cramp_fx_t fx; cramp_fx(&fx);
        if (fx.push_px > max_push) max_push = fx.push_px;
        if (fx.squash > max_squash) max_squash = fx.squash;
        if (fabsf(fx.squash - prev_squash) > max_squash_step) max_squash_step = fabsf(fx.squash - prev_squash);
        prev_squash = fx.squash;
        if (fx.gaze_y == -1) look_frames++;
        if (fx.eye_kind == GEL_EYE_CONTENT) content_frames++;
        bool p = fx.push_px > 0; if (p && !push_was) pushes_seen++; push_was = p;
        now_ms += 25;
        if (show_log) drain_log();
    }
}
static int count(int ev) { int n = 0; for (int i = 0; i < n_events; i++) if (events[i] == ev) n++; return n; }

int main(void)
{
    char st[400];

    puts("-- the clock: 45 lit minutes to full, nothing before");
    cramp_begin(0x5EED1234u); home(); reset_seen();
    run(10 * 60, false);
    CHECK(fabsf(cramp_level() - 10.0f / 45.0f) < 0.01f, "10 lit min: %.3f (10/45)", cramp_level());
    CHECK(fabsf(cramp_lit_min() - 10.0f) < 0.05f, "the lit clock says %.1f min", cramp_lit_min());
    CHECK(count(CRAMP_EV_NUDGE) == 0, "no nudge yet");
    run(36 * 60, false);
    CHECK(cramp_level() >= 1.0f, "46 lit min: full (%.2f)", cramp_level());
    drain_log();

    puts("-- the pauses: each gate holds the clock");
    struct { const char *name; void (*set)(void); } gates[] = {
        { "the dark house", [](){ in.lit = false; } },
        { "the empty house", [](){ in.holding = false; } },
        { "asleep", [](){ in.asleep = true; } },
        { "the shelf", [](){ in.shelf = true; } },
        { "the cable", [](){ in.cable = true; } },
        { "the quiet hours", [](){ in.quiet = true; } },
        { "RESTING (the small warm bowl)", [](){ in.resting = true; } },
    };
    for (auto &g : gates) {
        cramp_begin(1); home(); reset_seen(); g.set();
        run(60 * 60, false);
        CHECK(cramp_level() == 0 && cramp_lit_min() == 0 && n_events == 0 && pushes_seen == 0, "%s, an hour: no cramp, no clock, nothing (%.2f)", g.name, cramp_level());
    }
    cramp_begin(1); home(); reset_seen(); run(20 * 60, false); in.resting = true; run(60 * 60, false);
    CHECK(fabsf(cramp_level() - 20.0f / 45.0f) < 0.01f, "20 lit min then resting an hour: the level kept, not grown (%.2f)", cramp_level());
    drain_log();

    puts("-- the outlets spend it");
    cramp_begin(1); home(); reset_seen(); run(27 * 60, false);
    float before = cramp_level();
    cramp_play(now_ms); run(0.025f);
    CHECK(fabsf(before - cramp_level() - CRAMP_SPEND_PLAY) < 0.01f, "a game: %.2f -> %.2f (-0.15)", before, cramp_level());
    before = cramp_level(); cramp_pet(now_ms); run(0.025f);
    CHECK(fabsf(before - cramp_level() - CRAMP_SPEND_PET) < 0.01f, "a pet: %.2f -> %.2f (-0.25)", before, cramp_level());
    before = cramp_level(); in.walking = true; run(60, false); in.walking = false;
    CHECK(fabsf(before - cramp_level() - CRAMP_SPEND_WALK_PER_MIN) < 0.01f, "a walking minute: %.2f -> %.2f (-0.10, and no growth meanwhile)", before, cramp_level());
    cramp_pet(now_ms); cramp_pet(now_ms); run(0.025f);
    CHECK(cramp_level() < 0.001f, "spent to the floor, never under (%.4f: one frame's growth after)", cramp_level());
    drain_log();

    puts("-- the stance and the fidget");
    cramp_begin(7); home(); reset_seen(); run(46 * 60, false);
    { cramp_fx_t fx; cramp_fx(&fx); CHECK(fabsf(fx.squash - CRAMP_SQUASH) < 0.002f, "full: the stance %.3f (CRAMP_SQUASH)", fx.squash); }
    CHECK(max_squash_step < 0.001f, "the stance eased, never a snap (largest step %.4f)", max_squash_step);
    CHECK(fabsf(max_push - CRAMP_PUSH_PX) < 0.1f, "the push's peak %.1f px (CRAMP_PUSH_PX)", max_push);
    CHECK(pushes_seen >= 8 && pushes_seen <= 40, "pushes over the 46 min: %d (none under a quarter, then every 1.5-3 min / level)", pushes_seen);
    cramp_begin(7); home(); reset_seen(); run(10 * 60, false);
    CHECK(pushes_seen == 0, "under a quarter cramped: no push yet (%d)", pushes_seen);
    cramp_begin(7); home(); in.busy = true; reset_seen(); run(46 * 60, false);
    CHECK(pushes_seen == 0 && n_events == 0, "busy the whole way: no push, no nudge (the body is someone else's)");
    cramp_pet(now_ms); run(0.025f);
    { cramp_fx_t fx; cramp_fx(&fx); CHECK(fx.squash > 0.03f, "a pet drops the level at once, the stance follows slowly (%.3f just after)", fx.squash); }
    drain_log();

    puts("-- the nudge: full + the desk near = the roll (seed 0x5EED1234 says yes)");
    cramp_begin(0x5EED1234u); home(); reset_seen(); run(44 * 60, false);
    CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_why() == CRAMP_WHY_NOT_FULL, "44 min: not full, nothing (%s)", cramp_why_name(cramp_why()));
    run(55, false); reset_seen(); run(10);
    CHECK(count(CRAMP_EV_NUDGE) == 1, "at fullness: ONE nudge event");
    CHECK(max_push > 5.9f, "its push toward him (%.1f px)", max_push);
    CHECK(look_frames >= 76 && look_frames <= 84, "the look up held 2 s (%d frames)", look_frames);
    CHECK(cramp_nudge_open() && cramp_why() == CRAMP_WHY_OPEN, "the nudge is open: waiting for his pet");
    CHECK(cramp_nudges() == 1, "nudges counted: 1");

    puts("-- his answer: a pet inside 10 s = stay a while");
    reset_seen(); run(5, false);
    cramp_pet(now_ms); run(2);
    CHECK(count(CRAMP_EV_STAY) == 1, "the stay event");
    CHECK(!cramp_nudge_open(), "the window closed");
    CHECK(fabsf(cramp_level() - CRAMP_STAY_K) < 0.02f, "halved by the stay (the answering pet spends nothing more): %.2f", cramp_level());
    CHECK(content_frames >= 55 && content_frames <= 62, "content eyes for 1.5 s (%d frames)", content_frames);
    reset_seen(); run(50 * 60, false);
    CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_level() >= 1.0f, "full again inside the hour: no second nudge (%s)", cramp_why_name(cramp_why()));
    CHECK(cramp_why() == CRAMP_WHY_GAP, "`cramp` says: %s", cramp_why_name(cramp_why()));
    drain_log();

    puts("-- no answer: silence for the hour, the level kept; once, never twice");
    cramp_begin(0x5EED1234u); home(); reset_seen(); run(46 * 60, false);
    CHECK(count(CRAMP_EV_NUDGE) == 1, "the nudge said");
    reset_seen(); run(11);
    CHECK(!cramp_nudge_open() && count(CRAMP_EV_STAY) == 0, "10 s with no pet: the window closed, nothing felt");
    CHECK(cramp_level() >= 1.0f, "the cramp stays as it is (%.2f)", cramp_level());
    reset_seen(); run(58 * 60, false);
    CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_rolls() == 1, "58 more minutes: no second nudge, no second roll");
    reset_seen(); run(3 * 60, false);
    CHECK(cramp_rolls() == 2 && count(CRAMP_EV_NUDGE) <= 1, "past the hour, still full: it rolls again (this time: %s)", count(CRAMP_EV_NUDGE) ? "said" : "kept");
    drain_log();

    puts("-- the hold: the release is the wish granted, zeroed; the next arrival starts fresh");
    cramp_begin(0x5EED1234u); home(); reset_seen(); run(46 * 60, false);
    cramp_release("the hold"); drain_log();
    CHECK(cramp_level() == 0 && !cramp_nudge_open() && cramp_lit_min() == 0, "released: zeroed (%.2f)", cramp_level());
    { cramp_fx_t fx; cramp_fx(&fx); CHECK(fx.squash == 0 && fx.push_px == 0, "no stance, no push on the empty glass"); }
    cramp_arrive(); drain_log(); home(); reset_seen(); run(60, false);
    CHECK(cramp_level() < 0.03f && n_events == 0, "a fresh arrival: the small house, no cramp yet (%.3f)", cramp_level());

    puts("-- unreachable: a watch alone in the world is simply home");
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = -1; reset_seen(); run(50 * 60);
    CHECK(count(CRAMP_EV_NUDGE) == 0, "the desk never answered: no nudge, ever");
    CHECK(cramp_why() == CRAMP_WHY_NO_DESK, "`cramp` says: %s", cramp_why_name(cramp_why()));
    CHECK(pushes_seen > 0 && max_squash > 0.05f, "the fidget still shows it (%d pushes, the stance %.3f)", pushes_seen, max_squash);
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = 16 * 60; reset_seen(); run(50 * 60, false);
    CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_why() == CRAMP_WHY_NO_DESK, "the desk's door 16 min stale: not near (15 is the line)");
    in.desk_age_s = 60; reset_seen(); run(60, false);
    CHECK(cramp_why() != CRAMP_WHY_NO_DESK && (count(CRAMP_EV_NUDGE) == 1 || cramp_why() == CRAMP_WHY_ROLLED_NO), "the door answers again while still full: the roll happens then (%s)", count(CRAMP_EV_NUDGE) ? "said" : "kept");
    drain_log();

    puts("-- THE DESK NEXT DOOR: reachable over the air door, but FAR by its signal = not near");
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = 60; in.desk_far = true; reset_seen(); run(50 * 60);
    CHECK(count(CRAMP_EV_NUDGE) == 0, "the desk's door fresh but FAR: no nudge");
    CHECK(cramp_why() == CRAMP_WHY_FAR, "`cramp` says: %s", cramp_why_name(cramp_why()));
    CHECK(pushes_seen > 0 && max_squash > 0.05f, "the fidget still shows it (%d pushes, the stance %.3f)", pushes_seen, max_squash);
    in.desk_far = false; reset_seen(); run(60, false);
    CHECK(cramp_why() != CRAMP_WHY_FAR && (count(CRAMP_EV_NUDGE) == 1 || cramp_why() == CRAMP_WHY_ROLLED_NO), "the desk comes near while still full: the roll happens then (%s)", count(CRAMP_EV_NUDGE) ? "said" : "kept");
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = -1; in.desk_far = true; reset_seen(); run(50 * 60, false);
    CHECK(cramp_why() == CRAMP_WHY_NO_DESK, "never answered AND far: the older word wins (%s)", cramp_why_name(cramp_why()));
    in.desk_far = false;
    drain_log();

    puts("-- the roll is a roll: over many seeds both happen; regard shifts it");
    {
        int said = 0, kept = 0, said_cold = 0, said_warm = 0;
        for (uint32_t s = 1; s <= 200; s++) {
            cramp_begin(s * 2654435761u); home(); reset_seen(); run(46 * 60, false);
            if (count(CRAMP_EV_NUDGE)) said++; else kept++;
            cramp_begin(s * 2654435761u); home(); in.regard = -1; reset_seen(); run(46 * 60, false);
            if (count(CRAMP_EV_NUDGE)) said_cold++;
            cramp_begin(s * 2654435761u); home(); in.regard = 1; reset_seen(); run(46 * 60, false);
            if (count(CRAMP_EV_NUDGE)) said_warm++;
        }
        CHECK(said > 0 && kept > 0, "200 seeds at regard 0: said %d, kept %d", said, kept);
        CHECK(said >= 120 && said <= 160, "about seven in ten say it (%d of 200, CRAMP_NUDGE_ODDS 0.70)", said);
        CHECK(said_cold < said && said_warm > said, "regard shifts the odds: cold %d < plain %d < warm %d", said_cold, said, said_warm);
        CHECK(said_cold >= 80 && said_cold <= 120 && said_warm >= 160 && said_warm <= 195, "0.50 cold, 0.90 warm (CRAMP_NUDGE_REGARD_W 0.20)");
    }
    puts("-- a roll that kept the wish: the fidget only, the hour's gap, then another roll");
    {
        uint32_t seed = 0; int tries = 0;
        for (uint32_t s = 1; s <= 200 && !seed; s++) { cramp_begin(s * 2654435761u); home(); reset_seen(); run(46 * 60, false); if (!count(CRAMP_EV_NUDGE)) seed = s * 2654435761u; tries++; }
        cramp_begin(seed); home(); reset_seen(); run(46 * 60, false); drain_log();
        CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_why() == CRAMP_WHY_ROLLED_NO, "seed %u (try %d) kept it: %s", (unsigned)seed, tries, cramp_why_name(cramp_why()));
        CHECK(!cramp_nudge_open() && cramp_level() >= 1.0f, "nothing open, the cramp full");
        reset_seen(); run(59 * 60, false);
        CHECK(count(CRAMP_EV_NUDGE) == 0 && cramp_why() == CRAMP_WHY_ROLLED_NO, "inside the hour: no roll (%s)", cramp_why_name(cramp_why()));
        run(2 * 60, false);
        CHECK(cramp_rolls() == 2, "past the hour, still full: rolled again (%s)", cramp_why_name(cramp_why()));
    }

    puts("-- the wrist's own bid: 20 ignored lit minutes, once per 90");
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = -1; reset_seen();
    run(19 * 60, false);
    CHECK(count(CRAMP_EV_BID) == 0, "19 min: not yet");
    run(55, false); run(10);
    CHECK(count(CRAMP_EV_BID) == 1, "20 min ignored: ONE bid");
    CHECK(look_frames >= 76 && look_frames <= 84 && max_push > 5.9f, "a look up held 2 s (%d frames), the small push (%.1f px)", look_frames, max_push);
    CHECK(cramp_bids() == 1, "bids counted: 1");
    reset_seen(); run(3); cramp_pet(now_ms); run(2);
    CHECK(count(CRAMP_EV_BID_ANSWERED) == 1 && content_frames > 50, "a pet answers it: content (%d frames)", content_frames);
    reset_seen(); run(85 * 60, false);
    CHECK(count(CRAMP_EV_BID) == 0, "85 more ignored minutes: inside the 90 min gap, no second bid");
    run(5 * 60, false);
    CHECK(count(CRAMP_EV_BID) == 1, "past the gap, still ignored: the second bid");
    drain_log();
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = -1; reset_seen();
    for (int i = 0; i < 6; i++) { run(15 * 60, false); cramp_touch(now_ms); }
    CHECK(count(CRAMP_EV_BID) == 0, "a touch every 15 min for 90 min: never ignored, no bid");
    cramp_begin(0x5EED1234u); home(); in.desk_age_s = -1; reset_seen(); run(20 * 60 + 5, false);
    reset_seen(); run(11, false);
    CHECK(count(CRAMP_EV_BID_ANSWERED) == 0 && content_frames == 0, "unanswered: nothing more (no sigh, no sulk)");
    for (auto &g : gates) {
        cramp_begin(1); home(); in.desk_age_s = -1; reset_seen(); g.set();
        run(100 * 60, false);
        CHECK(count(CRAMP_EV_BID) == 0, "the bid under %s: never", g.name);
    }
    cramp_begin(1); home(); in.desk_age_s = -1; in.busy = true; reset_seen(); run(30 * 60, false);
    CHECK(count(CRAMP_EV_BID) == 0, "the bid while the body is busy: waits");
    in.busy = false; run(5, false);
    CHECK(count(CRAMP_EV_BID) == 1, "the body free: the bid");

    puts("-- the names and the status line");
    CHECK(!strcmp(cramp_why_name(CRAMP_WHY_NOT_FULL), "not full") && !strcmp(cramp_why_name(99), "?"), "the reasons' names");
    cramp_begin(0x5EED1234u); home(); reset_seen(); run(45 * 60 + 5, false);
    cramp_status(st, sizeof st);
    CHECK(strstr(st, "cramp 1.00") && strstr(st, "rolls 1, nudges 1") && strstr(st, "last said") && strstr(st, "OPEN") && strstr(st, "waiting for him"), "`cramp`: %s", st);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
