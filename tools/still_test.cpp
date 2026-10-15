/* THE STILL SPOT on the laptop (still.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/still_test tools/still_test.cpp -x c src/still.c && /tmp/still_test
 * The arbiter's whole table: a still wrist lights the LEDs after
 * STILL_ARM_S and never before; a jerk on the line resets the arming; a
 * wrist never still gives up at the wait with the IMU as the witness; a
 * burst mid-read discards the samples (a restart, the LEDs dark, the
 * wrist waited for again) and the read starts over when it is still
 * again; a tremor under the burst line is nothing; a third burst (or a
 * burst with no whole read left in the budget) is the verdict "moved"
 * with how far in and how hard; the row's words (waiting / reading); the
 * clean read runs to its end with no word at all; idle and done say
 * nothing; the two reads' lengths (the row's 30, the day's 10) give the
 * budgets they should. */
#include "still.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.025f;   /* the lit loop's frame */

/* feed `secs` of a steady jerk; the first word other than NONE, or NONE */
static int run(still_t *s, float secs, float jerk, float *at = NULL)
{
    float t = 0;
    while (t < secs) {
        int w = still_tick(s, DT, jerk);
        t += DT;
        if (w != STILL_NONE) { if (at) *at = t; return w; }
    }
    return STILL_NONE;
}

int main(void)
{
    still_t s;

    puts("-- the shapes");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    CHECK(s.state == STILL_WAIT && still_waiting(&s) && !still_reading(&s) && !strcmp(still_why(&s), ""), "a read asked: WAIT, no verdict");
    CHECK(s.budget_s == PPG_WAIT_STILL_S + 20.0f && s.read_s == 10.0f, "the day's 10 s read: a budget of the wait and two reads (%.0f s)", (double)s.budget_s);
    still_begin(&s, 30, PPG_WAIT_STILL_S);
    CHECK(s.budget_s == PPG_WAIT_STILL_S + 60.0f, "the row's 30 s read: %.0f s", (double)s.budget_s);
    still_t idle; memset(&idle, 0, sizeof idle);
    CHECK(still_tick(&idle, DT, 1.0f) == STILL_NONE && !still_waiting(&idle) && !still_reading(&idle), "idle says nothing, whatever the jerk");

    puts("-- the arming");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    float at = 0;
    int w = run(&s, 5, 0.0f, &at);
    CHECK(w == STILL_LIGHT && at >= STILL_ARM_S - 0.01f && at <= STILL_ARM_S + 2 * DT, "a still wrist: LIGHT at %.2f s (STILL_ARM_S %.0f)", (double)at, (double)STILL_ARM_S);
    CHECK(still_reading(&s) && !still_waiting(&s), "then READ");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    CHECK(run(&s, STILL_ARM_S - 0.2f, 0.0f) == STILL_NONE, "not yet at %.1f s", (double)(STILL_ARM_S - 0.2f));
    CHECK(still_tick(&s, DT, STILL_JERK_G) == STILL_NONE && s.quiet_s == 0, "a jerk ON the line resets the arming");
    CHECK(run(&s, STILL_ARM_S - 0.2f, 0.0f) == STILL_NONE, "...and the wait starts over");
    CHECK(run(&s, 1.0f, 0.0f) == STILL_LIGHT, "...LIGHT once it is still long enough");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    CHECK(run(&s, 1.5f, STILL_JERK_G - 0.001f) == STILL_NONE && s.quiet_s > 1.4f, "a jerk just under the line counts as still");

    puts("-- never still: the wait gives up");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    w = run(&s, 20, 0.2f, &at);
    CHECK(w == STILL_GAVE_UP && at >= PPG_WAIT_STILL_S - 0.01f && at <= PPG_WAIT_STILL_S + 2 * DT, "a moving wrist: GAVE UP at %.1f s", (double)at);
    CHECK(s.state == STILL_DONE && strstr(still_why(&s), "not still in 10 s") && strstr(still_why(&s), "0.20 g"), "the verdict names the wait and the jerk: '%s'", still_why(&s));
    CHECK(still_tick(&s, DT, 0.0f) == STILL_NONE, "done says nothing more");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    for (int i = 0; i < 6; i++) { run(&s, 1.5f, 0.0f); still_tick(&s, DT, 0.3f); }   /* still 1.5 s, a jerk, six times: never armed */
    w = run(&s, 5, 0.0f, &at);
    CHECK(w == STILL_GAVE_UP, "a fidget every 1.5 s: never armed, gave up (%.1f s in all)", (double)s.t);

    puts("-- the clean read: no word");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    CHECK(run(&s, 5, 0.0f) == STILL_LIGHT, "lit");
    CHECK(run(&s, 12, 0.02f) == STILL_NONE && still_reading(&s) && s.restarts == 0, "12 s of a quiet wrist (0.02 g): nothing said, still READ");
    CHECK(run(&s, 3, STILL_BURST_G - 0.001f) == STILL_NONE, "a tremor just under the burst line: nothing");
    still_end(&s);
    CHECK(s.state == STILL_IDLE && !still_reading(&s), "the read finished: IDLE");

    puts("-- the burst: a restart");
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    run(&s, 5, 0.0f);
    run(&s, 3, 0.0f);
    w = still_tick(&s, DT, 0.25f);
    CHECK(w == STILL_RESTART && still_waiting(&s) && s.restarts == 1, "a 0.25 g burst 3 s in: RESTART, WAIT again");
    CHECK(s.spoil_g > 0.249f && s.spoil_at_s > 2.9f && s.spoil_at_s < 3.1f, "the spoiler kept: %.2f g at %.1f s", (double)s.spoil_g, (double)s.spoil_at_s);
    CHECK(run(&s, 1.0f, 0.3f) == STILL_NONE, "still moving: nothing yet");
    w = run(&s, 5, 0.0f, &at);
    CHECK(w == STILL_LIGHT && at >= STILL_ARM_S - 0.01f, "still again %.1f s: LIGHT (the read starts over)", (double)at);
    CHECK(run(&s, 11, 0.0f) == STILL_NONE && still_reading(&s), "...and runs clean to its end");
    still_end(&s);

    puts("-- the budget: the verdict moved");
    still_begin(&s, 10, PPG_WAIT_STILL_S);          /* budget 30 s */
    run(&s, 5, 0.0f);                               /* lit at ~2 s */
    run(&s, 8, 0.0f);                               /* 8 s into the read: t ~ 10 */
    CHECK(still_tick(&s, DT, 0.3f) == STILL_RESTART, "t 10: a burst -- a restart fits (10 + 2 + 10 <= 30)");
    run(&s, 5, 0.0f);                               /* lit again at ~12 */
    run(&s, 7, 0.0f);                               /* t ~ 19.5 */
    w = still_tick(&s, DT, 0.4f);
    CHECK(w == STILL_GAVE_UP && strstr(still_why(&s), "moved 7 s in") && strstr(still_why(&s), "0.40 g") && strstr(still_why(&s), "no time left"),
          "t 20: a burst -- no whole read left in the budget: '%s'", still_why(&s));
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    run(&s, 2.5f, 0.0f); still_tick(&s, DT, 0.3f);  /* lit ~2, burst at once: restart 1 */
    run(&s, 2.5f, 0.0f); still_tick(&s, DT, 0.3f);  /* restart 2 */
    CHECK(s.restarts == 2 && still_waiting(&s), "two quick bursts: two restarts");
    run(&s, 2.5f, 0.0f);
    w = still_tick(&s, DT, 0.3f);
    CHECK(w == STILL_GAVE_UP && strstr(still_why(&s), ", again"), "a third burst: the verdict, 'again': '%s'", still_why(&s));
    still_begin(&s, 10, PPG_WAIT_STILL_S);
    run(&s, 2.5f, 0.0f); still_tick(&s, DT, 0.3f);  /* restart at ~2.5 */
    w = run(&s, 30, 0.3f, &at);
    CHECK(w == STILL_GAVE_UP && strstr(still_why(&s), "then not still again") && s.t <= s.budget_s - s.read_s + 2 * DT,
          "moved, then never still again: gave up at %.1f s (the budget minus a read): '%s'", (double)s.t, still_why(&s));

    puts("-- the row's 30 s under the nap's polls (dt 1 s)");
    still_begin(&s, 30, PPG_WAIT_STILL_S);
    int lit_at = -1;
    for (int i = 0; i < 12 && lit_at < 0; i++) if (still_tick(&s, 1.0f, 0.01f) == STILL_LIGHT) lit_at = i + 1;
    CHECK(lit_at == 2, "the dark house's 1 s polls: LIGHT on the second (%d)", lit_at);
    int none = 1;
    for (int i = 0; i < 30; i++) if (still_tick(&s, 1.0f, 0.01f) != STILL_NONE) none = 0;
    CHECK(none && still_reading(&s), "30 polls of stillness: nothing said");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
