/* THE BUS UNSTUCK (src/unstick.h) on the laptop: the dead-count rule, the
 * ladder, the counters and doctor's words, fed synthetic reads at the
 * 40 fps sense rate.
 * Build: c++ -std=gnu++17 -I src -o unstick_test tools/unstick_test.cpp
 * Run:   ./unstick_test
 * 49 unanswered reads ask for nothing and the 50th asks for the recovery;
 * "no new sample" is the caller's to keep out (an answered read resets
 * the run); an answer after the recovery judges it good and the ladder
 * resets; 50 more silences judge it failed and the next try waits 1, 2,
 * 4 .. 60 s and no longer; a chip never heard since boot is doctor's "no
 * IMU found", not ours; the touch's silence is named only while the IMU
 * answers; the words read as the README says. */
#include "unstick.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* n reads at 25 ms, all unanswered; returns how many asked for the recovery (and when the last did) */
static uint32_t now = 5000;
static int silence(unstick_t *u, int n, uint32_t *asked_at)
{
    int asks = 0;
    for (int i = 0; i < n; i++) {
        now += 25;
        if (unstick_note(u, false, now)) { asks++; if (asked_at) *asked_at = now; }
    }
    return asks;
}

int main(void)
{
    unstick_t u;
    char buf[200];

    /* the first line: a healthy bus */
    unstick_init(&u);
    int asked = 0;
    for (int i = 0; i < 100; i++) { now += 25; if (unstick_note(&u, true, now)) asked++; }
    check("an answered read never asks", asked == 0);
    check("answered: ever", u.ever && u.dead == 0 && u.silent_ms == 0);
    unstick_bom(&u, now, buf, sizeof buf);
    check("bom: bus0 ok", !strcmp(buf, "bus0 ok"));
    check("verdict: nothing", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);

    /* the dead count: 49 ask nothing, the 50th asks */
    check("49 silences ask nothing", silence(&u, UNSTICK_DEAD_N - 1, NULL) == 0);
    check("the silence is stamped", u.silent_ms != 0 && u.dead == UNSTICK_DEAD_N - 1);
    check("the 50th asks", silence(&u, 1, NULL) == 1);
    /* the caller runs the recovery and says so; the next answer judges it good */
    unstick_tried(&u, now);
    check("tried: pending, count from zero", u.pending && u.dead == 0 && u.tries == 1);
    now += 25;
    check("the answer judges it good", !unstick_note(&u, true, now) && !u.pending && u.failed == 0 && u.good_ms == now && unstick_good(&u) == 1);
    unstick_bom(&u, now + 3000, buf, sizeof buf);
    check("bom: one recovery, s ago", !strcmp(buf, "bus0 ok (1 recovery, last 3 s ago)"));
    unstick_bom(&u, now + 200000, buf, sizeof buf);
    check("bom: one recovery, min ago", !strcmp(buf, "bus0 ok (1 recovery, last 3 min ago)"));
    check("verdict after a good recovery: nothing", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);

    /* a stuck bus that stays stuck: the ladder */
    uint32_t t_ask = 0, t_first = 0;
    check("stuck again: the 50th asks", silence(&u, UNSTICK_DEAD_N, &t_ask) == 1);
    t_first = t_ask;
    unstick_tried(&u, t_ask);
    /* 50 more silences judge it failed; the next try waits 1 s from the try */
    int asks = silence(&u, UNSTICK_DEAD_N, &t_ask);        /* 1.25 s of silence: past the 1 s rung */
    check("failed once, the 1 s rung passed: asks again", asks == 1 && u.failed == 1 && u.wait_ms == 1000);
    unstick_tried(&u, t_ask);
    asks = silence(&u, UNSTICK_DEAD_N, &t_ask);            /* 1.25 s: under the 2 s rung */
    check("failed twice, under the 2 s rung: waits", asks == 0 && u.failed == 2 && u.wait_ms == 2000);
    asks = 0;
    while (!asks) asks = silence(&u, 1, &t_ask);           /* until the 2 s rung passes (and it keeps asking until tried: the firmware tries at once) */
    check("the 2 s rung passed: asks at 2 s", t_ask - u.tried_ms >= 2000 && t_ask - u.tried_ms < 2050);
    unstick_tried(&u, t_ask);
    /* the rungs double to the top and stay there */
    for (int r = 0; r < 8; r++) {
        silence(&u, UNSTICK_DEAD_N, NULL);                 /* judged failed, the wait doubles */
        uint32_t w = u.wait_ms;
        asks = 0;
        while (!asks) asks = silence(&u, 1, &t_ask);       /* until the rung passes */
        uint32_t gap = t_ask - u.tried_ms;
        check("the rung is waited out", gap >= w && gap < w + 50);
        unstick_tried(&u, t_ask);
    }
    check("the ladder tops at a minute", u.wait_ms == UNSTICK_WAIT_MAX_MS && u.tries == 12);
    silence(&u, UNSTICK_DEAD_N, NULL);
    check("and stays there", u.wait_ms == UNSTICK_WAIT_MAX_MS && u.failed == 11);
    /* the words while silent */
    unstick_bom(&u, now, buf, sizeof buf);
    check("bom: SILENT with the counts", !strncmp(buf, "bus0 SILENT ", 12) && strstr(buf, "(1 recovery, 11 failed)"));
    check("verdict: the IMU silent", unstick_verdict(&u, now, true, buf, sizeof buf) > 0
          && !strncmp(buf, "the IMU has been silent ", 24) && strstr(buf, "(bus0 stuck: 11 recoveries failed)"));
    check("silent_s counts from the run's start", unstick_silent_s(&u, now) == (now - (t_first - (UNSTICK_DEAD_N - 1) * 25)) / 1000);
    /* the count saturates */
    for (int i = 0; i < 70000; i++) unstick_note(&u, false, now);
    check("the dead count saturates", u.dead == UNSTICK_DEAD_CAP);
    /* the minute passes, one more try, and the chip answers: the ladder and the run reset */
    asks = 0;
    while (!asks) asks = silence(&u, 1, &t_ask);
    unstick_tried(&u, t_ask);
    now += 25;
    unstick_note(&u, true, now);
    check("an answer resets the ladder", u.wait_ms == UNSTICK_WAIT_MS && u.dead == 0 && u.run == 0 && u.silent_ms == 0 && !u.pending && u.good_ms == now);
    check("verdict clear again", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);
    unstick_bom(&u, now + 1000, buf, sizeof buf);
    check("bom: good count, some failed", !strcmp(buf, "bus0 ok (2 recoveries, last 1 s ago, some failed)"));

    /* the verdict waits its seconds: 2 s of silence is not yet a word */
    unstick_init(&u); unstick_note(&u, true, now);
    silence(&u, 60, NULL);                                 /* 1.5 s */
    check("under 3 s silent: no verdict yet", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);
    silence(&u, 80, NULL);                                 /* 3.5 s */
    check("past 3 s silent: the verdict", unstick_verdict(&u, now, true, buf, sizeof buf) > 0);

    /* a chip never heard: doctor's "no IMU found", not ours */
    unstick_init(&u);
    check("never heard: the 50th still asks (a late chip is tried)", silence(&u, UNSTICK_DEAD_N, NULL) == 1);
    check("never heard: no verdict here", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);
    unstick_bom(&u, now, buf, sizeof buf);
    check("never heard: bom says ok, not SILENT", !strcmp(buf, "bus0 ok"));

    /* the touch: silent while the IMU answers = the chip's own, named after 10 s, lit only */
    unstick_init(&u); unstick_note(&u, true, now);
    unstick_touch(&u, false, now);
    for (int i = 0; i < 100; i++) { now += 25; unstick_note(&u, true, now); unstick_touch(&u, false, now); }
    check("touch 2.5 s silent: nothing", unstick_verdict(&u, now, true, buf, sizeof buf) == 0);
    for (int i = 0; i < 320; i++) { now += 25; unstick_note(&u, true, now); unstick_touch(&u, false, now); }
    check("touch 10 s silent, lit: named", unstick_verdict(&u, now, true, buf, sizeof buf) > 0 && !strncmp(buf, "the touch has been silent 10 s while the IMU answers", 52));
    check("touch 10 s silent, dark: not named", unstick_verdict(&u, now, false, buf, sizeof buf) == 0);
    unstick_touch(&u, true, now);
    check("an answered poll clears it", u.touch_silent_ms == 0 && unstick_verdict(&u, now, true, buf, sizeof buf) == 0);
    /* a touch silent AND an IMU silent is the bus, not the touch */
    unstick_touch(&u, false, now);
    silence(&u, 200, NULL);
    check("both silent: the IMU's word", unstick_verdict(&u, now, true, buf, sizeof buf) > 0 && !strncmp(buf, "the IMU", 7));

    /* a clock at 0 still stamps */
    unstick_init(&u);
    unstick_note(&u, false, 0);
    check("now 0 stamps as 1", u.silent_ms == 1);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
