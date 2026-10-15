/* THE STARE RETURNED on the laptop (stare.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/stare_test tools/stare_test.cpp -x c src/stare.c -lm && /tmp/stare_test
 * Nothing before 8 s; the look at 8 s of lit, raised, still, untouched
 * glass (the gaze up, CONTENT, the settle's half sine); its 1.5 s; once
 * per lit spell and owed again on the next; the clock put back to zero
 * by a touch / a move / a page / a word / the body busy / the face
 * away; the empty glass and the dark never; idle's thing holds it at
 * the mark and it fires the frame idle is done; a touch mid-look ends
 * it; the 10 s quiet after. */
#include "stare.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.05f;
static stare_in_t in;

static void reset_in(void)
{
    in.dt = DT; in.lit = true; in.holding = true; in.raised = true; in.still = true;
    in.touched = false; in.busy = false; in.page = false; in.word = false; in.idle_playing = false;
}
/* run `s` seconds; count the look events; the last fx */
static int run(float s, stare_fx_t *last)
{
    int n = (int)(s / DT + 0.5f), evs = 0;
    for (int i = 0; i < n; i++) { if (stare_tick(&in) == STARE_EV_LOOK) evs++; }
    if (last) stare_fx(last);
    return evs;
}
/* a fresh spell: the dark for a second, then lit */
static void new_spell(void)
{
    reset_in(); in.lit = false; run(1.0f, NULL); in.lit = true;
}

int main(void)
{
    stare_fx_t fx;

    puts("-- the watched glass: nothing before 8 s, the look at 8");
    new_spell();
    CHECK(run(7.9f, &fx) == 0 && !stare_playing() && fx.gaze_y == 0 && fx.eye_kind < 0 && fx.squash == 0, "7.9 s held: nothing yet (held %.1f)", stare_held_s());
    CHECK(fabsf(stare_held_s() - 7.9f) < 0.06f, "the clock at %.2f s", stare_held_s());
    CHECK(run(0.15f, &fx) == 1 && stare_playing(), "the look at %.0f s: one event", STARE_S);
    CHECK(fx.gaze_y == STARE_GY && fx.eye_kind == GEL_EYE_CONTENT, "the gaze up (%+d), the eyes CONTENT", fx.gaze_y);
    CHECK(fx.squash > 0 && fx.squash < STARE_SETTLE_SQUASH * 0.5f, "the settle beginning: squash +%.3f", fx.squash);
    run(0.75f - 0.1f, &fx);
    CHECK(fx.squash > STARE_SETTLE_SQUASH * 0.95f, "the settle's crest at 0.75 s: +%.3f (of %.2f)", fx.squash, STARE_SETTLE_SQUASH);
    CHECK(fx.gaze_y == STARE_GY && fx.eye_kind == GEL_EYE_CONTENT, "the look still up at 0.75 s");
    run(0.7f, &fx);
    CHECK(stare_playing() && fx.gaze_y == STARE_GY && fx.squash < 0.015f, "at 1.45 s the look still up, the settle nearly home (+%.3f)", fx.squash);
    run(0.1f, &fx);
    CHECK(!stare_playing() && fx.gaze_y == 0 && fx.eye_kind < 0 && fx.squash == 0, "at %.1f s forward, the ordinary face", STARE_LOOK_S);
    CHECK(stare_spent() && stare_count() == 1, "this spell's look spent (%d so far)", stare_count());
    CHECK(stare_quiet_s() > STARE_QUIET_S - 1.6f && stare_quiet_s() < STARE_QUIET_S - 1.4f, "the quiet after: %.1f s of %.0f left", stare_quiet_s(), STARE_QUIET_S);

    puts("-- once per lit spell");
    CHECK(run(30.0f, &fx) == 0 && stare_held_s() == 0, "30 s more of the same stare: nothing again (the clock does not run)");
    CHECK(stare_quiet_s() == 0, "the quiet over");
    in.lit = false; run(1.0f, NULL); in.lit = true; run(DT, NULL);
    CHECK(!stare_spent(), "the dark then lit: the look owed again");
    CHECK(run(8.05f, &fx) == 1 && stare_count() == 2, "... and played at 8 s of the new spell (%d so far)", stare_count());

    puts("-- the clock put back to zero");
    const char *names[] = { "a touch", "a move", "the body busy", "a page", "the word", "the face away" };
    for (int k = 0; k < 6; k++) {
        new_spell();
        run(6.0f, NULL);
        switch (k) { case 0: in.touched = true; break; case 1: in.still = false; break; case 2: in.busy = true; break;
                     case 3: in.page = true; break; case 4: in.word = true; break; case 5: in.raised = false; break; }
        run(0.5f, NULL);
        float held_broken = stare_held_s();
        reset_in();
        int evs = run(7.9f, &fx);
        CHECK(held_broken == 0 && evs == 0 && stare_held_s() > 7.8f, "%s at 6 s: the clock to zero, nothing at 8 s of the first start (held %.1f since)", names[k], stare_held_s());
        CHECK(run(0.15f, &fx) == 1, "... the look 8 s after it ended");
    }

    puts("-- never on the empty glass, never in the dark");
    new_spell(); in.holding = false;
    CHECK(run(60.0f, &fx) == 0 && !stare_playing(), "60 s of the empty glass: nothing");
    new_spell(); in.lit = false;
    CHECK(run(60.0f, &fx) == 0 && !stare_playing() && stare_held_s() == 0, "60 s of the dark: nothing");

    puts("-- idle's thing: never two glances stacked");
    new_spell();
    run(7.0f, NULL); in.idle_playing = true;
    CHECK(run(3.0f, &fx) == 0 && !stare_playing(), "idle playing from 7 s to 10 s: the stare waits");
    CHECK(fabsf(stare_held_s() - STARE_S) < 0.001f, "... its clock held at the mark (%.1f)", stare_held_s());
    in.idle_playing = false;
    CHECK(run(DT, &fx) == 1 && stare_playing() && fx.gaze_y == STARE_GY, "the frame idle is done: the look");

    puts("-- a touch mid-look ends it");
    new_spell();
    run(8.1f, &fx); CHECK(stare_playing(), "the look running");
    in.touched = true; run(DT, &fx);
    CHECK(!stare_playing() && fx.gaze_y == 0 && fx.eye_kind < 0 && fx.squash == 0, "a touch at 0.1 s: forward, the ordinary face (the pet's has the stage)");
    CHECK(stare_spent(), "... and the spell's look is spent all the same");
    new_spell();
    run(8.1f, &fx); in.busy = true; run(DT, &fx);
    CHECK(!stare_playing(), "the body taken mid-look (busy): ends it");
    new_spell();
    run(8.1f, &fx); in.still = false; run(0.5f, &fx);
    CHECK(stare_playing() && fx.gaze_y == STARE_GY, "a move mid-look does NOT end it (the look rides the wrist; 0.6 s in, still up)");

    printf("stare_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
