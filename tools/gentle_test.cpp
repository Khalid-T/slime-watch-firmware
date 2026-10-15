/* THE GENTLE WAKE on the laptop (gentle.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/gentle_test tools/gentle_test.cpp -x c src/gentle.c && /tmp/gentle_test
 * The rule's every gate: no alarm = never, no clock = never; the
 * window's edges (20 min before = in, 21 = out, the alarm's own minute
 * = out, past midnight wraps); awake = never; no stir / a stir too old
 * = never; no pulse, a floor of fewer than three readings, a stale
 * newest, a reading taken awake (the floor's night over) = never; the
 * pulse flat, or one reading up and the other not, or falling = never;
 * the rise over the floor by five = wake; once only (spent / rearm);
 * the seen-once feed; the status phrases. */
#include "gentle.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now_ms = 8 * 3600 * 1000u;   /* an arbitrary uptime */
static gentle_in_t in;
static gentle_out_t out;

static void feed(float bpm, bool ok, int age_s, bool dozing) { gentle_pulse(bpm, ok, now_ms - (uint32_t)age_s * 1000u, dozing); }
/* the night's floor: three clean dozing readings at the floor, then the pair under test */
static void night(float floor_bpm, float prev, float last)
{
    gentle_begin();
    feed(floor_bpm + 2, true, 3600, true);
    feed(floor_bpm, true, 2400, true);
    feed(floor_bpm + 1, true, 1200, true);
    feed(prev, true, 600, true);
    feed(last, true, 60, true);
}
static void ask(int alarm, int minute, bool dozing, int stir_age)
{
    in.alarm_minute = alarm; in.minute = minute; in.dozing = dozing; in.stir_age_s = stir_age; in.now_ms = now_ms;
    gentle_check(&in, &out);
}
static const int A = 7 * 60 + 30;   /* 07:30 */

int main(void)
{
    char s[128];

    printf("never without an alarm, never without a clock\n");
    night(58, 64, 66);
    ask(-1, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_ALARM, "no alarm: never, whatever the wrist and the pulse say (%s)", gentle_state_name(out.state));
    ask(A, -1, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_CLOCK, "an alarm but no clock: never (%s)", gentle_state_name(out.state));

    printf("the window's edges\n");
    ask(A, A - 10, true, 10);
    CHECK(out.wake && out.state == GENTLE_WAKE, "10 min before 07:30, stirring, the pulse 58 -> 64, 66: WAKE");
    CHECK(out.from_minute == A - GENTLE_WINDOW_MIN && out.floor_bpm == 58 && out.last_bpm == 66 && out.prev_bpm == 64,
          "the window from %02d:%02d, floor %d, last %d <- %d", out.from_minute / 60, out.from_minute % 60, out.floor_bpm, out.last_bpm, out.prev_bpm);
    ask(A, A - GENTLE_WINDOW_MIN, true, 10);
    CHECK(out.wake, "%d min before: the window's first minute, in", GENTLE_WINDOW_MIN);
    ask(A, A - GENTLE_WINDOW_MIN - 1, true, 10);
    CHECK(!out.wake && out.state == GENTLE_ARMED, "%d min before: out (armed, waiting)", GENTLE_WINDOW_MIN + 1);
    ask(A, A - 1, true, 10);
    CHECK(out.wake, "1 min before: in");
    ask(A, A, true, 10);
    CHECK(!out.wake && out.state == GENTLE_ARMED, "the alarm's own minute: not the window's (the alarm's own ring)");
    ask(A, A + 1, true, 10);
    CHECK(!out.wake, "a minute after: out");
    ask(0 * 60 + 10, 23 * 60 + 55, true, 10);          /* 00:10 alarm, 23:55 now */
    CHECK(out.wake && out.from_minute == 23 * 60 + 50, "past midnight: 23:55 is inside 00:10's window (from 23:50)");
    ask(0 * 60 + 10, 23 * 60 + 49, true, 10);
    CHECK(!out.wake, "23:49 is not");

    printf("the wrist\n");
    ask(A, A - 10, false, 10);
    CHECK(!out.wake && out.state == GENTLE_AWAKE, "awake (rest's verdict): never -- he is up, the alarm will ring");
    ask(A, A - 10, true, -1);
    CHECK(!out.wake && out.state == GENTLE_NO_STIR, "never stirred: never");
    ask(A, A - 10, true, GENTLE_STIR_S + 1);
    CHECK(!out.wake && out.state == GENTLE_NO_STIR, "a stir %d s ago: too old", GENTLE_STIR_S + 1);
    ask(A, A - 10, true, GENTLE_STIR_S);
    CHECK(out.wake, "a stir %d s ago: stirring", GENTLE_STIR_S);
    ask(A, A - 10, true, 0);
    CHECK(out.wake, "a stir this second: stirring");

    printf("the pulse\n");
    gentle_begin();
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_PULSE, "no reading ever: no pulse known");
    gentle_begin(); feed(58, true, 1200, true); feed(66, true, 60, true);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_PULSE, "two readings only (no floor under the pair): no pulse known");
    gentle_begin(); feed(58, true, 1200, true); feed(64, true, 600, true); feed(66, true, 60, true);
    ask(A, A - 10, true, 10);
    CHECK(out.wake, "three: the first is the floor, the pair rises over it: wake");
    gentle_begin(); feed(58, true, 1200, true); feed(64, true, 600, true); feed(66, false, 60, true);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_PULSE, "the newest failed (not ok): it is not read, the pair is 64 <- 58 (no floor under it): no pulse known");
    night(58, 64, 66);
    feed(0, false, 10, true);
    ask(A, A - 10, true, 10);
    CHECK(out.wake, "a failed spot after a good pair: the pair still reads (the failure is remembered, not read)");
    night(58, 64, 66);
    now_ms += (uint32_t)(GENTLE_PULSE_AGE_S + 1) * 1000u;
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_PULSE, "the newest %d s old: stale, no pulse known", GENTLE_PULSE_AGE_S + 61);
    now_ms -= (uint32_t)(GENTLE_PULSE_AGE_S + 1) * 1000u;
    night(58, 64, 66);
    feed(70, true, 30, false);                                /* a reading taken awake */
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_NO_PULSE && out.floor_bpm == 0, "a reading taken awake ends the night: the floor gone, no pulse known");
    feed(60, true, 20, true); feed(62, true, 10, true); feed(66, true, 5, true);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_FLAT && out.floor_bpm == 60, "a new night's floor from 60: 62, 66 is not five over it for both: flat");
    night(58, 62, 66);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_FLAT, "58 -> 62, 66: the previous only 4 up: flat (both must be %d over)", GENTLE_RISE_BPM);
    night(58, 63, 63);
    ask(A, A - 10, true, 10);
    CHECK(out.wake, "58 -> 63, 63: both five over, level: wake (a plateau in the light phase is one)");
    night(58, 66, 64);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_FLAT, "58 -> 66, 64: falling back: not a rise");
    night(58, 59, 60);
    ask(A, A - 10, true, 10);
    CHECK(!out.wake && out.state == GENTLE_FLAT, "58 -> 59, 60: the night's own wander: flat");
    gentle_begin();
    feed(70, true, 3600, true); feed(60, true, 2400, true); feed(58, true, 1200, true); feed(64, true, 600, true); feed(66, true, 60, true);
    ask(A, A - 10, true, 10);
    CHECK(out.wake && out.floor_bpm == 58 && gentle_count() == GENTLE_HIST, "the floor is the night's LOWEST (58 under an earlier 70), the ring keeps %d", GENTLE_HIST);

    printf("once per alarm\n");
    night(58, 64, 66);
    ask(A, A - 10, true, 10);
    CHECK(out.wake && !gentle_is_spent(), "wake, not yet spent");
    gentle_spent();
    ask(A, A - 9, true, 10);
    CHECK(!out.wake && out.state == GENTLE_SPENT && gentle_is_spent(), "spent: the same window says nothing more");
    gentle_rearm();
    ask(A, A - 9, true, 10);
    CHECK(out.wake, "rearmed (a new alarm, or its minute passed): it may again");

    printf("the feed, once per result\n");
    gentle_begin();
    gentle_pulse_seen(0, 60, true, now_ms, true);
    CHECK(gentle_count() == 0, "seq 0 = no spot finished yet: nothing taken");
    gentle_pulse_seen(1, 60, true, now_ms, true);
    gentle_pulse_seen(1, 60, true, now_ms, true);
    gentle_pulse_seen(1, 60, true, now_ms, true);
    CHECK(gentle_count() == 1, "the same seq polled thrice (the lit loop and the nap both ask): one reading");
    gentle_pulse_seen(2, 61, true, now_ms, true);
    CHECK(gentle_count() == 2, "a new seq: a second");

    printf("the status\n");
    gentle_begin();
    ask(-1, A - 10, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: no alarm"), "%s", s);
    ask(A, -1, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: no clock"), "%s", s);
    ask(A, 6 * 60, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: armed 07:10-07:30"), "%s", s);
    ask(A, A - 10, true, -1); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: in the window, not stirring"), "%s", s);
    ask(A, A - 10, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: in the window, no pulse known"), "%s", s);
    ask(A, A - 10, false, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: in the window, awake"), "%s", s);
    night(58, 59, 60);
    ask(A, A - 10, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: in the window, pulse flat (floor 58, last 60 <- 59)"), "%s", s);
    night(58, 64, 66);
    ask(A, A - 10, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: in the window, wake (floor 58, last 66 <- 64)"), "%s", s);
    gentle_spent();
    ask(A, A - 10, true, 10); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: spent for 07:30 (floor 58, last 66 <- 64)"), "%s", s);
    ask(A, 6 * 60, true, 10); gentle_rearm(); gentle_status(&in, s, sizeof s);
    CHECK(!strcmp(s, "gentle: armed 07:10-07:30 (floor 58, last 66 <- 64)"), "%s", s);
    CHECK(!strcmp(gentle_state_name(GENTLE_WAKE), "wake") && !strcmp(gentle_state_name(GENTLE_NO_STIR), "not stirring") && !strcmp(gentle_state_name(99), "?"),
          "the names");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
