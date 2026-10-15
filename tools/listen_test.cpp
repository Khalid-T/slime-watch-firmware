/* THE EAR TO THE WRIST on the laptop (listen.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/listen_test tools/listen_test.cpp -x c src/listen.c -lm && /tmp/listen_test
 * Nothing while the read waits for the wrist (the LEDs dark); the look
 * the frame the LEDs glow (the gaze down, CONTENT) and through the
 * read; a clean read (the ring's count moved) ends with one settle, a
 * failed one with nothing; a read given way to his ask is a failed one;
 * his own reads never looked; the dark, the empty glass, a page and a
 * busy body hide the look (the read runs on; a read ended unseen owes
 * no settle); a pet mid-settle ends it; a restart's dark LEDs lift the
 * ear and the relight brings it back under the same read. */
#include "listen.h"
#include <stdio.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.05f;
static listen_in_t in;

static void reset_in(void)
{
    in.dt = DT; in.reading = false; in.glow = false; in.seq = in.seq; in.lit = true; in.holding = true; in.busy = false; in.page = false;
}
/* run `s` seconds; count the events by kind; the last fx */
static void run(float s, int *ear, int *clean, int *failed, listen_fx_t *last)
{
    int n = (int)(s / DT + 0.5f);
    for (int i = 0; i < n; i++) {
        int ev = listen_tick(&in);
        if (ev == LISTEN_EV_EAR && ear) (*ear)++;
        if (ev == LISTEN_EV_CLEAN && clean) (*clean)++;
        if (ev == LISTEN_EV_FAILED && failed) (*failed)++;
    }
    if (last) listen_fx(last);
}
/* the quiet between reads: nothing running for a few seconds */
static void idle_gap(void) { reset_in(); run(3.0f, NULL, NULL, NULL, NULL); }

int main(void)
{
    listen_fx_t fx; int ear, clean, failed;
    in.seq = 7;

    puts("-- a read of the creature's own, clean: the wait, the look, the settle");
    idle_gap();
    in.reading = true; in.glow = false;                                   /* THE STILL SPOT: waiting for the wrist, the LEDs dark */
    ear = clean = failed = 0; run(2.0f, &ear, &clean, &failed, &fx);
    CHECK(ear == 0 && !listen_on() && fx.gaze_y == 0 && fx.eye_kind < 0, "waiting for the wrist (LEDs dark): no look yet");
    in.glow = true;                                                       /* the LEDs light (the agc, then the read) */
    ear = 0; run(DT, &ear, &clean, &failed, &fx);
    CHECK(ear == 1 && listen_on(), "the LEDs glow: the ear pressed (one event)");
    CHECK(fx.gaze_y == LISTEN_GY && fx.eye_kind == GEL_EYE_CONTENT && fx.squash == 0, "the gaze down (%+d), the eyes CONTENT, no squash", fx.gaze_y);
    ear = 0; run(10.0f, &ear, &clean, &failed, &fx);
    CHECK(ear == 0 && listen_on() && fx.gaze_y == LISTEN_GY, "10 s on: the look held, the event not repeated");
    CHECK(fabsf(listen_ear_s() - 10.05f) < 0.06f, "the ear's clock at %.2f s", listen_ear_s());
    in.reading = false; in.glow = false; in.seq++;                        /* the result pushed into the ring, the read over */
    clean = failed = 0; run(DT, &ear, &clean, &failed, &fx);
    CHECK(clean == 1 && failed == 0 && listen_clean() == 1, "the ring's count moved: a clean read (one event)");
    CHECK(!listen_on() && fx.gaze_y == 0 && fx.eye_kind < 0, "the ear lifted: the ordinary gaze and eyes");
    CHECK(listen_settling() && fx.squash == 0, "the settle beginning (its first frame at the foot)");
    run(0.75f, NULL, NULL, NULL, &fx);
    CHECK(fx.squash > LISTEN_SETTLE_SQUASH * 0.95f, "the settle's crest at 0.75 s: +%.3f (of %.2f)", fx.squash, LISTEN_SETTLE_SQUASH);
    run(0.65f, NULL, NULL, NULL, &fx);
    CHECK(listen_settling() && fx.squash > 0 && fx.squash < 0.015f, "at 1.4 s nearly home (+%.3f)", fx.squash);
    run(0.1f, NULL, NULL, NULL, &fx);
    CHECK(!listen_settling() && fx.squash == 0, "at %.1f s the settle done", LISTEN_SETTLE_S);
    CHECK(listen_ear_s() == 0, "the ear's clock at zero outside a read");

    puts("-- a failed read: nothing");
    idle_gap();
    in.reading = true; in.glow = true;
    ear = 0; run(4.0f, &ear, NULL, NULL, &fx);
    CHECK(ear == 1 && listen_on(), "the look through a read that will spoil");
    in.reading = false; in.glow = false;                                  /* moved: no push */
    clean = failed = 0; run(DT, NULL, &clean, &failed, &fx);
    CHECK(clean == 0 && failed == 1 && listen_failed() == 1, "the count unmoved: a failed read (one event)");
    CHECK(!listen_on() && !listen_settling() && fx.squash == 0 && fx.gaze_y == 0 && fx.eye_kind < 0, "... the ear lifts, no settle, nothing");

    puts("-- given way to his ask: a failed one; his read never looked");
    idle_gap();
    in.reading = true; in.glow = true; run(3.0f, NULL, NULL, NULL, &fx);
    in.reading = false; in.glow = false;                                  /* ppg_spot_start_s dropped it (no push) ... */
    clean = failed = 0; run(DT, NULL, &clean, &failed, &fx);
    CHECK(failed == 1 && clean == 0 && !listen_settling(), "the creature's read gives way: failed, nothing");
    ear = 0; run(30.0f, &ear, NULL, NULL, &fx);                           /* ... and HIS 30 s runs: ppg_own_left says false the whole way */
    CHECK(ear == 0 && !listen_on() && fx.gaze_y == 0 && fx.eye_kind < 0, "his 30 s row read: no face (the page's row has it)");

    puts("-- hidden: the dark, the empty glass, a page, a busy body; the read runs on");
    const char *names[] = { "the dark", "the empty glass", "a page", "a busy body" };
    for (int k = 0; k < 4; k++) {
        idle_gap();
        in.reading = true; in.glow = true;
        ear = 0; run(2.0f, &ear, NULL, NULL, &fx);
        switch (k) { case 0: in.lit = false; break; case 1: in.holding = false; break; case 2: in.page = true; break; case 3: in.busy = true; break; }
        run(1.0f, NULL, NULL, NULL, &fx);
        CHECK(ear == 1 && !listen_on() && fx.gaze_y == 0 && fx.eye_kind < 0, "%s mid-read: the look hidden", names[k]);
        reset_in(); in.reading = true; in.glow = true;
        ear = 0; run(1.0f, &ear, NULL, NULL, &fx);
        CHECK(ear == 0 && listen_on() && fx.gaze_y == LISTEN_GY, "%s over: the look back, no second event", names[k]);
    }

    puts("-- a read ended unseen owes no settle");
    idle_gap();
    in.reading = true; in.glow = true; run(5.0f, NULL, NULL, NULL, NULL);
    in.lit = false; run(1.0f, NULL, NULL, NULL, NULL);
    in.reading = false; in.glow = false; in.seq++;
    clean = 0; run(DT, NULL, &clean, NULL, &fx);
    CHECK(clean == 1 && !listen_settling() && fx.squash == 0, "clean in the dark: counted, no settle");
    in.lit = true; run(1.0f, NULL, NULL, NULL, &fx);
    CHECK(!listen_settling() && fx.squash == 0 && fx.gaze_y == 0, "the glass lit after: nothing stale");

    puts("-- a read begun in the dark, seen lit");
    idle_gap(); in.lit = false;
    in.reading = true; in.glow = true; ear = 0; run(3.0f, &ear, NULL, NULL, &fx);
    CHECK(ear == 0 && !listen_on(), "3 s of the read in the dark: nothing");
    in.lit = true; run(DT, &ear, NULL, NULL, &fx);
    CHECK(ear == 1 && listen_on() && fx.eye_kind == GEL_EYE_CONTENT, "the raise mid-read: the ear pressed from here");
    in.reading = false; in.glow = false; in.seq++;
    clean = 0; run(DT, NULL, &clean, NULL, &fx);
    CHECK(clean == 1 && listen_settling(), "... clean, the settle seen");

    puts("-- a pet mid-settle ends it");
    run(0.3f, NULL, NULL, NULL, &fx);
    CHECK(listen_settling() && fx.squash > 0, "the settle under way (+%.3f)", fx.squash);
    in.busy = true; run(DT, NULL, NULL, NULL, &fx);
    CHECK(!listen_settling() && fx.squash == 0, "busy: the settle ended, the pet's face has the stage");
    in.busy = false; run(1.0f, NULL, NULL, NULL, &fx);
    CHECK(!listen_settling() && fx.squash == 0, "... and not resumed");

    puts("-- THE STILL SPOT's restart: the LEDs dark lift the ear, the relight brings it back");
    idle_gap();
    in.reading = true; in.glow = true; ear = 0; run(3.0f, &ear, NULL, NULL, &fx);
    in.glow = false; run(2.0f, &ear, NULL, NULL, &fx);                    /* moved: the LEDs dark, waiting for the wrist again */
    CHECK(ear == 1 && !listen_on() && fx.gaze_y == 0 && fx.eye_kind < 0, "the restart's dark LEDs: the ear lifted, the read still taken");
    in.glow = true; failed = 0; run(1.0f, &ear, NULL, &failed, &fx);
    CHECK(ear == 1 && failed == 0 && listen_on(), "the relight: the look back under the same read, no verdict yet, no second event");
    in.reading = false; in.glow = false; in.seq++;
    clean = 0; run(DT, NULL, &clean, NULL, &fx);
    CHECK(clean == 1 && listen_settling(), "... and clean in the end: the settle");

    puts("-- heard: the end's log line is only for a read he saw");
    CHECK(listen_heard(), "the last read's look was shown (kept past its end)");
    idle_gap(); in.lit = false; in.reading = true; in.glow = true; run(2.0f, NULL, NULL, NULL, NULL);
    CHECK(!listen_heard(), "a read in the dark: never heard");
    idle_gap();

    puts("-- the counts");
    CHECK(listen_clean() == 4 && listen_failed() == 7, "%d clean, %d failed since boot (the hidden four and the dark one ended unpushed: failed)", listen_clean(), listen_failed());

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
