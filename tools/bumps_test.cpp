/* THE BUMPS' gate on the laptop (bumps.h is header-only, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/bumps_test tools/bumps_test.cpp && /tmp/bumps_test
 * The whole table: on a lit glass by day, worn, off the cable, the wisp
 * / the sink / the rise are one BUMP each and a pet is the PURR; the
 * switch off keeps every one; the dark glass keeps every one; the quiet
 * hours keep every one; the sleep look keeps every one; the cable keeps
 * every one (ruling 65); the dim glass still counts as lit; the purr is
 * kept while the breath is quick (a walk: 0.80 against a resting 0.55,
 * and anything past 1.2 x), plays at the resting rate and just under
 * the line, is kept for a minute after one and plays at 60 s; the gap
 * of half a second between any two touches (three wisps on one frame,
 * a rise under a wisp), and its release after; the rules' order (off
 * before dark before quiet before asleep before cable before the
 * occasion's own); the purr's ride on the inhale (the phase wrapping);
 * the names; an occasion out of range. */
#include "bumps.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the day's ordinary state: lit, by day, awake, worn off the cable, the breath at rest, nothing played yet */
static bumps_in_t day(void)
{
    bumps_in_t in;
    memset(&in, 0, sizeof in);
    in.on = true; in.lit = true; in.quiet = false; in.asleep = false; in.on_cable = false;
    in.breath_hz = 0.55f; in.breath_rest_hz = 0.55f;
    in.now_ms = 100000; in.last_bump_ms = 0; in.last_purr_ms = 0;
    return in;
}

static const char *kind_name(int k) { return k == HAPTIC_BUMP ? "bump" : k == HAPTIC_PURR ? "purr" : k == HAPTIC_HOP3 ? "hop" : "none"; }

/* every occasion under one state: the kinds expected (-1 = kept) and the one reason */
static void table(const char *tag, const bumps_in_t *in, int wisp, int sink, int rise, int pet, int why_wanted)
{
    /* the four of 09-23 and THE SMALL HOUSE's nudge (2026-10-09), which is one BUMP wherever the wisp is */
    static const int occ[5] = { BUMP_WISP, BUMP_SINK, BUMP_RISE, BUMP_PET, BUMP_NUDGE };
    const int want[5] = { wisp, sink, rise, pet, wisp };
    for (int i = 0; i < 5; i++) {
        int why = -1;
        int k = bumps_decide(occ[i], in, &why);
        int why_want = want[i] < 0 ? why_wanted : BUMP_PLAYS;
        CHECK(k == want[i] && why == why_want, "%s: %s -> %s (%s)", tag, bumps_name(occ[i]), kind_name(k), bumps_kept_name(why));
    }
}

int main(void)
{
    puts("-- a lit glass by day, worn, off the cable: the three bumps and the purr");
    { bumps_in_t in = day(); table("day", &in, HAPTIC_BUMP, HAPTIC_BUMP, HAPTIC_BUMP, HAPTIC_PURR, BUMP_PLAYS); }

    puts("-- the switch off: nothing, ever");
    { bumps_in_t in = day(); in.on = false; table("off", &in, -1, -1, -1, -1, BUMP_KEPT_OFF); }

    puts("-- the dark glass: nothing (a wisp waits for the light anyway; the night's release has no theatre)");
    { bumps_in_t in = day(); in.lit = false; table("dark", &in, -1, -1, -1, -1, BUMP_KEPT_DARK); }

    puts("-- the quiet hours: silent");
    { bumps_in_t in = day(); in.quiet = true; table("quiet", &in, -1, -1, -1, -1, BUMP_KEPT_QUIET); }

    puts("-- asleep (the sleep look, a stir included): silent");
    { bumps_in_t in = day(); in.asleep = true; table("asleep", &in, -1, -1, -1, -1, BUMP_KEPT_ASLEEP); }

    puts("-- on the cable (the dock, the charger): nothing -- a bump on a desk rattles the table (ruling 65)");
    { bumps_in_t in = day(); in.on_cable = true; table("cable", &in, -1, -1, -1, -1, BUMP_KEPT_CABLE); }

    puts("-- the dim glass is lit (he can see it): the same as by day");
    { bumps_in_t in = day(); in.lit = true; table("dim", &in, HAPTIC_BUMP, HAPTIC_BUMP, HAPTIC_BUMP, HAPTIC_PURR, BUMP_PLAYS); }

    puts("-- the breath quick (a walk, 0.80 Hz against 0.55): the purr is kept; the three bumps still play");
    { bumps_in_t in = day(); in.breath_hz = 0.80f; table("walk", &in, HAPTIC_BUMP, HAPTIC_BUMP, HAPTIC_BUMP, -1, BUMP_KEPT_BREATH); }
    {
        bumps_in_t in = day(); int why;
        in.breath_hz = 0.55f * 1.19f;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "just under the line (1.19 x): the purr plays");
        in.breath_hz = 0.55f * 1.21f;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == -1 && why == BUMP_KEPT_BREATH, "just over (1.21 x): kept, the breath is quick");
        in.breath_hz = 0.40f;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "slow (an hour still, 0.40): the purr plays -- slow is at rest");
        in.breath_hz = 0.80f; in.breath_rest_hz = 0;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "no resting rate known (0): the ratio is not judged");
        in.breath_rest_hz = 0.66f;   /* the grain's 1.08 x, vigor full: the line moves with the resting rate */
        in.breath_hz = 0.78f;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "a quicker resting slime (0.66) breathing 0.78: still at rest for him");
    }

    puts("-- never twice in a minute: the purr kept for 60 s after one, then plays");
    {
        bumps_in_t in = day(); int why;
        in.last_purr_ms = in.now_ms - 59000;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == -1 && why == BUMP_KEPT_MINUTE, "59 s after a purr: kept (a purr already this minute)");
        in.last_purr_ms = in.now_ms - 60000;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "60 s after: the purr again");
        in.last_purr_ms = in.now_ms - 59000;
        CHECK(bumps_decide(BUMP_WISP, &in, &why) == HAPTIC_BUMP && bumps_decide(BUMP_RISE, &in, &why) == HAPTIC_BUMP,
              "the minute is the purr's own: a wisp and a rise inside it still bump");
        in.last_purr_ms = 0;
        CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR, "never purred (0): plays");
    }

    puts("-- the gap: two touches inside half a second are one (three wisps on a frame, a rise under a wisp)");
    {
        bumps_in_t in = day(); int why;
        in.last_bump_ms = in.now_ms - 400;
        CHECK(bumps_decide(BUMP_WISP, &in, &why) == -1 && why == BUMP_KEPT_GAP, "a wisp 0.4 s after a bump: kept (a bump just played)");
        CHECK(bumps_decide(BUMP_RISE, &in, &why) == -1 && why == BUMP_KEPT_GAP, "a rise 0.4 s after: kept");
        CHECK(bumps_decide(BUMP_PET, &in, &why) == -1 && why == BUMP_KEPT_GAP, "a pet 0.4 s after: kept too");
        in.last_bump_ms = in.now_ms - 500;
        CHECK(bumps_decide(BUMP_WISP, &in, &why) == HAPTIC_BUMP, "0.5 s after: the wisp bumps");
        in.last_bump_ms = in.now_ms;
        CHECK(bumps_decide(BUMP_SINK, &in, &why) == -1 && why == BUMP_KEPT_GAP, "the same instant: kept");
        in.last_bump_ms = 0;
        CHECK(bumps_decide(BUMP_SINK, &in, &why) == HAPTIC_BUMP, "never bumped (0): plays");
    }

    puts("-- the rules' order: his switch, then the glass, the night, the sleep, the desk, then the occasion's own");
    {
        bumps_in_t in = day(); int why;
        in.on = false; in.lit = false; in.quiet = true; in.asleep = true; in.on_cable = true; in.breath_hz = 0.9f; in.last_purr_ms = in.now_ms - 1000; in.last_bump_ms = in.now_ms;
        bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_OFF, "everything against it: the switch is named first");
        in.on = true;  bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_DARK, "switch on: the dark glass");
        in.lit = true; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_QUIET, "lit: the quiet hours");
        in.quiet = false; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_ASLEEP, "by day: asleep");
        in.asleep = false; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_CABLE, "awake: the cable");
        in.on_cable = false; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_GAP, "worn: the gap");
        in.last_bump_ms = 0; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_BREATH, "the gap past: the breath");
        in.breath_hz = 0.55f; bumps_decide(BUMP_PET, &in, &why); CHECK(why == BUMP_KEPT_MINUTE, "at rest: the minute");
        in.last_purr_ms = 0; CHECK(bumps_decide(BUMP_PET, &in, &why) == HAPTIC_PURR && why == BUMP_PLAYS, "and then the purr");
    }

    puts("-- the purr rides the breath: due the instant the phase wraps (the inhale's start)");
    CHECK(!bumps_purr_due(1.0f, 2.0f), "the phase climbing: not yet");
    CHECK(!bumps_purr_due(6.0f, 6.28f), "near the top: not yet");
    CHECK(bumps_purr_due(6.2f, 0.05f), "wrapped: now");
    CHECK(!bumps_purr_due(0.05f, 0.05f), "no motion in the phase (a frame with no dt): not yet");

    puts("-- THE SMALL HOUSE's nudge (2026-10-09): one BUMP by day, kept by every gate as the others are");
    { bumps_in_t in = day(); int why = -1; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == HAPTIC_BUMP && why == BUMP_PLAYS, "the nudge by day: one bump"); }
    { bumps_in_t in = day(); int why = -1; in.lit = false; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == -1 && why == BUMP_KEPT_DARK, "the nudge on the dark glass: kept"); }
    { bumps_in_t in = day(); int why = -1; in.quiet = true; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == -1 && why == BUMP_KEPT_QUIET, "the nudge in the quiet hours: kept"); }
    { bumps_in_t in = day(); int why = -1; in.asleep = true; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == -1 && why == BUMP_KEPT_ASLEEP, "the nudge asleep: kept"); }
    { bumps_in_t in = day(); int why = -1; in.on_cable = true; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == -1 && why == BUMP_KEPT_CABLE, "the nudge on the cable: kept"); }
    { bumps_in_t in = day(); int why = -1; in.on = false; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == -1 && why == BUMP_KEPT_OFF, "the nudge with bumps off: kept"); }
    { bumps_in_t in = day(); int why = -1; in.breath_hz = 0.9f; CHECK(bumps_decide(BUMP_NUDGE, &in, &why) == HAPTIC_BUMP, "the nudge on a quick breath: plays (the breath is the purr's rule only)"); }
    CHECK(!strcmp(bumps_name(BUMP_NUDGE), "the nudge"), "its name");

    puts("-- the names, and an occasion out of range");
    CHECK(!strcmp(bumps_name(BUMP_WISP), "the wisp") && !strcmp(bumps_name(BUMP_SINK), "the sink") && !strcmp(bumps_name(BUMP_RISE), "the rise")
          && !strcmp(bumps_name(BUMP_PET), "the purr under a pet") && !strcmp(bumps_name(99), "?"), "the occasions' names");
    CHECK(!strcmp(bumps_kept_name(BUMP_PLAYS), "plays") && !strcmp(bumps_kept_name(BUMP_KEPT_CABLE), "on the cable")
          && !strcmp(bumps_kept_name(BUMP_KEPT_MINUTE), "a purr already this minute") && !strcmp(bumps_kept_name(99), "?"), "the reasons' names");
    { bumps_in_t in = day(); int why = 0; CHECK(bumps_decide(BUMP_OCCASIONS, &in, &why) == -1 && why == BUMP_KEPT_OFF, "an occasion past the table: kept, never a kind"); }
    { bumps_in_t in = day(); CHECK(bumps_decide(BUMP_WISP, &in, NULL) == HAPTIC_BUMP, "no reason asked for (NULL): still decides"); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
