/* A synthetic day through rest.cpp on the laptop (no board, no Arduino):
 *   c++ -O1 -I src -o /tmp/rest_test tools/rest_test.cpp src/rest.cpp src/grain.cpp src/tell.cpp src/vigor.cpp -lm && /tmp/rest_test
 * The evening awake, still from 23:40 -> asleep by 00:00; a 2-min stir
 * at 03:00 (and a raise) does not end the span; a raise at 07:10 ends
 * it: 7.5 h, the stretch, `wrist:slept`; an hour's sit slows the
 * breath; a 40-min still at 14:00 is a nap (`wrist:napped`, no
 * stretch); a walk at 15:00 quickens the breath, then it eases back.
 * THE NIGHT AFTER A REBOOT: every row the day fed is kept as the day
 * file would hold it (the raise counted in its minute); rest_begin
 * again (the power cycle) and the rows replayed through rest_replay_*
 * rebuild the same night -- bed, wake, hours, awakenings, the ring's
 * minutes, the stable id -- with no sense, no stretch and one log
 * line; the id is what NVS keeps (store.told_night), and tell.cpp,
 * told the rebuilt night by that id, does not answer it again.
 * THE SHELF RULE + A TOUCH ENDS SLEEP (2026-09-10, Khalid's morning):
 * the watch set on the desk, on the cable, at 23:50 and still all night
 * is THE SHELF -- no sleep look, no Zs, no span, the ring empty, `rest`
 * says "on the shelf since 23:51 (5.5 h still, not worn: on the cable)"
 * -- and a touch at 05:40 ends the run with nothing told; a worn night
 * ended by a touch at 06:30 is the rising: the stretch, `wrist:slept`
 * once; a nap ended by a touch is `wrist:napped`; a watch set on the
 * desk without the cable is the shelf too ("never moved") until a still
 * row moves (a fidget): his after all, asleep since the run began. Each
 * replayed from its rows (WORN / CHARGING / taps as the day file holds
 * them) gives the same verdict. The rows' WORN flag is modelled as the
 * day log infers it: worn while wearing, and for 10 rows after a
 * set-down (motion in the last 10 min). PUT ON COLD (A6, 2026-09-23):
 * a shelf run of 2 h or more ended by the wrist picked up (a raise)
 * gets the shiver (a2 for 0.3 s), the look up (1.5 s) and the breath
 * snapped to the sleeper's 0.30 Hz, warming to the day's over the
 * breath's own ease; a shelf of an hour, or one ended by a hand on the
 * glass, gets none of it; a replay none. THE WAKING BREATH (C6, the
 * operator night 2026-10-12): a raise that lit the glass (rest_in_t's
 * last bool, true for one frame) snaps the breath's phase to 0 -- the
 * inhale's start, the purr's wrap -- and the depth to x1.6, easing to
 * x1.0 over 3 s; refused in the night (the 03:00 stir, the morning's
 * rising under its stretch), off the cold shelf (PUT ON COLD's raise)
 * and for a touch-wake (the bool is false: the phase runs on). */
#include "../src/rest.h"
#include "../src/tell.h"
#include "../src/grain.h"      /* grain_night(), for the QUIET TIME check */
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t motion = 0;
static bool walking = false;
static const rest_out_t *o;
static char last_sense[80] = "";
static float max_stretch = 0;
static int zs_seen = 0;
static int logs_seen = 0;
static char last_log[140] = "";

/* THE NIGHT AFTER A REBOOT: the rows as the day file holds them */
static const uint32_t DAY0 = 20260908u, DAY1 = 20260909u;   /* the evening's date, the morning's */
static struct { uint32_t date; int minute, steps, energy, raises, taps, ups; bool worn, charging; } rows[2000];
static int nrows = 0, pending_raises = 0, pending_taps = 0, pending_ups = 0;
static uint32_t date_now = DAY0, date_next = DAY1;
/* THE SHELF RULE's flags as the day log would write them */
static bool wearing = true, cable = false;
static int  since_off = 99;         /* rows since the set-down (the WORN flag's 10-min echo) */
static int  senses_seen = 0;
/* THE NIGHT RELEASE: rest's lease events, folded as the link and the nap would take them */
static int ev_seen = 0;
static void fold_ev(void) { int e = rest_take_lease_event(); if (e) ev_seen = e; }
static int take_ev(void) { int e = ev_seen; ev_seen = 0; return e; }

/* THE PULSE AS THE BREATH: the last clean spot as main hands it over (0 = none); its age runs with the clock */
static float pulse_bpm = 0, pulse_age = 0;
/* THE WAKING BREATH: main's one-frame word that a RAISE lit the glass, holding */
static bool woke_raise = false;
/* THE LULL: the lull's level as main hands it over (lull_level) */
static float lulled = 0;
/* THE WARM SKIN: main's one-frame word that the skin proxy's COLD band is paid (warm_tick's WARM_ACT_SHIVER) */
static bool chilled = false;
static void tick(int minute, float seconds)
{
    for (float t = 0; t < seconds; t += 0.2f) {
        rest_in_t in = { 0.2f, minute, true, motion, walking, true, "23-7", pulse_bpm, pulse_age, woke_raise, lulled, chilled };
        woke_raise = false;
        chilled = false;
        if (pulse_bpm > 0) pulse_age += 0.2f;
        o = rest_tick(&in);
        if (o->log_new) { printf("       log: %s\n", o->log); snprintf(last_log, sizeof last_log, "%s", o->log); logs_seen++; rest_log_taken(); }
        if (o->sense_new) {
            snprintf(last_sense, sizeof last_sense, "%s %s", o->sense_kind, o->sense_words);
            printf("       sense: %s\n", last_sense);
            senses_seen++;
            rest_sense_sent();
        }
        if (o->stretch > max_stretch) max_stretch = o->stretch;
        for (int i = 0; i < REST_ZS; i++) if (o->z[i].on) zs_seen++;
        fold_ev();
    }
}

/* one minute lived: the row closes, then the next minute's frames run */
static int now = 18 * 60;           /* the local minute of the day */
static void minute(int steps, int energy)
{
    bool worn = wearing || since_off < 10;
    if (nrows < 2000) { rows[nrows].date = date_now; rows[nrows].minute = now; rows[nrows].steps = steps;
                        rows[nrows].energy = energy; rows[nrows].raises = pending_raises; rows[nrows].taps = pending_taps;
                        rows[nrows].ups = pending_ups; rows[nrows].worn = worn; rows[nrows].charging = cable; nrows++; }
    pending_raises = pending_taps = pending_ups = 0;
    rest_set_date(date_now);
    rest_minute(now, (uint16_t)steps, (uint16_t)energy, (uint8_t)(energy < 15 ? 58 : 5), worn, cable);
    fold_ev();
    if (!wearing) since_off++;
    now = (now + 1) % 1440;
    if (now == 0) date_now = date_next;
    tick(now, 60);
}
/* a raise now: main's wake bumps the counter; the day log counts it in the minute in progress */
static void raise(void) { motion++; pending_raises++; }
static void raise_lit(void) { raise(); woke_raise = true; }   /* THE WAKING BREATH: the raise that lit the glass (main's wake_up said so) */
/* a hand on the glass now: main's rest_touch(); the day log counts the tap in the minute in progress */
static void touch(void) { rest_touch(); pending_taps++; }
/* THE SHAKEN SLEEPER: a shake now -- main's wake / the lit GES_SHAKE call rest_shake() and count it (the motion counter, the UP flag) */
static void shake(void) { rest_shake(); motion++; pending_ups++; }
/* the watch set down (the WORN flag echoes for 10 rows), put on, or a wrist twitch under the still threshold */
static void take_off(void) { wearing = false; since_off = 0; }
static void put_on(void) { wearing = true; since_off = 99; }
static void fidget(void) { since_off = 0; }
static void minutes(int n, int steps, int energy) { for (int i = 0; i < n; i++) minute(steps, energy); }
/* a fresh detector for a new scenario: the rows kept for its replay */
static void scene(uint32_t day_a, uint32_t day_b, int start_minute)
{
    rest_begin(0.55f, 0.04f);
    motion = 0; walking = false; last_sense[0] = 0; max_stretch = 0; zs_seen = 0; logs_seen = 0; last_log[0] = 0; senses_seen = 0;
    nrows = 0; pending_raises = pending_taps = pending_ups = 0;
    pulse_bpm = 0; pulse_age = 0;
    date_now = day_a; date_next = day_b; now = start_minute;
    put_on(); cable = false;
    tick(now, 0.4f);
    ev_seen = 0;
}
/* THE NIGHT AFTER A REBOOT for the scenario: the rows through the same detector */
static void replay(uint32_t day_a, uint32_t day_b, int minute_now)
{
    rest_begin(0.55f, 0.04f);
    motion = 0; walking = false; last_sense[0] = 0; max_stretch = 0; zs_seen = 0; logs_seen = 0; last_log[0] = 0; senses_seen = 0;
    ev_seen = 0;                                     /* the power cycle: whatever the live night said was taken then */
    tick(minute_now, 0.4f);
    CHECK(rest_replay_begin(), "the replay begins");
    int ra = 0, rb = 0;
    for (int i = 0; i < nrows; i++) {
        rest_replay_row(rows[i].date, rows[i].minute, (uint16_t)rows[i].steps, (uint16_t)rows[i].energy,
                        (uint8_t)(rows[i].energy < 15 ? 58 : 5), (uint8_t)rows[i].raises, false,
                        (uint8_t)rows[i].taps, rows[i].worn, rows[i].charging, rows[i].ups != 0);
        if (rows[i].date == day_a) ra++; else rb++;
    }
    tick(minute_now, 0.4f);
    CHECK(logs_seen == 0 && last_sense[0] == 0 && max_stretch == 0 && zs_seen == 0, "%d rows replayed: no log line, no sense, no stretch, no Zs", nrows);
    rest_replay_end(day_a, ra, day_b, rb);
    tick(minute_now, 0.4f);
    fold_ev();
    CHECK(take_ev() == REST_EV_NONE, "replayed: no lease event (nothing to release or claim for a night already lived)");
}
static const char *hm(int m) { static char b[16]; snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60); return b; }

int main(void)
{
    rest_begin(0.55f, 0.04f);
    char st[240];

    printf("18:00 the evening, awake (a fidget, a stroll, then the couch)\n");
    minutes(30, 10, 40);
    CHECK(!o->asleep, "awake at %s", hm(now));
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.02f, "the resting breath %.2f Hz (100 steps in 10 min is no walk)", o->breath_hz);
    lulled = 1.0f; tick(now, 45);          /* THE LULL: petted toward rest for its 45 s -- the target the sleeper's 0.30, the band's own ease */
    CHECK(o->breath_hz < 0.36f && !o->asleep && !strcmp(rest_state_name(), "awake"), "lulled 45 s: the breath toward the sleeper's %.2f Hz, still awake (%s)", o->breath_hz, rest_state_name());
    lulled = 0; tick(now, 60);
    CHECK(o->breath_hz > 0.50f, "a minute after the lull: back toward rest, %.2f Hz", o->breath_hz);
    minutes(10, 60, 90);
    CHECK(o->breath_hz > 0.75f, "a stroll (600 steps in 10 min): %.2f Hz", o->breath_hz);

    printf("18:40 an hour's sit (30 mg, no steps): the breath slows after 60 min; THE ITCH IN THE BOWL from then\n");
    minutes(59, 0, 30);
    CHECK(!o->asleep, "a sit under 60 mg is not sleep (%s)", rest_state_name());
    CHECK(o->breath_hz > 0.5f, "before the hour: %.2f Hz", o->breath_hz);
    CHECK(o->itch_deg == 0, "no itch before the hour");
    minutes(1, 0, 30);
    CHECK(fabsf(o->itch_deg) == 4.0f, "at 60 min the itch: %+.0f deg off gravity's angle", o->itch_deg);
    float side0 = o->itch_deg;
    minutes(2, 0, 30);
    CHECK(o->breath_hz < 0.45f && o->breath_amp < 0.035f, "after the hour: %.2f Hz amp %.3f (slow, shallow)", o->breath_hz, o->breath_amp);
    CHECK(o->itch_deg * side0 > 0 && fabsf(o->itch_deg) > 4.0f && fabsf(o->itch_deg) < 4.2f, "62 min: the same side, growing (%+.1f)", o->itch_deg);
    minutes(1, 0, 30);
    CHECK(o->itch_deg * side0 < 0 && fabsf(o->itch_deg) > 4.0f && fabsf(o->itch_deg) < 4.3f, "63 min: the other side, a hair more (%+.1f)", o->itch_deg);
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "(itch ") != NULL, "`rest` shows it");
    walking = true; tick(now, 0.4f);
    CHECK(o->itch_deg == 0, "the first steps: gone at once");
    walking = false; tick(now, 0.4f);
    CHECK(fabsf(o->itch_deg) > 4.0f, "...and back while he still sits (%+.1f)", o->itch_deg);
    minutes(2, 50, 45);
    minutes(1, 0, 40);
    CHECK(o->breath_hz > 0.5f, "a stroll ends the sit: %.2f Hz", o->breath_hz);
    CHECK(o->itch_deg == 0, "a row of 50 steps ends the sit: no itch");
    printf("%s a three-hour sit: the itch grows to 10 deg and stays there\n", hm(now));
    minutes(120, 0, 30);
    CHECK(fabsf(o->itch_deg) > 6.9f && fabsf(o->itch_deg) < 7.1f, "at 120 min: %+.1f deg (half way)", o->itch_deg);
    minutes(60, 0, 30);
    CHECK(fabsf(o->itch_deg) == 10.0f, "at 180 min: %+.0f deg", o->itch_deg);
    minutes(30, 0, 30);
    CHECK(fabsf(o->itch_deg) == 10.0f, "at 210 min: still %+.0f deg (capped)", o->itch_deg);
    minutes(1, 60, 90);
    CHECK(o->itch_deg == 0, "a walk: gone");

    printf("%s awake until 23:40\n", hm(now));
    while (now != 23 * 60 + 40) minute(0, 40);
    CHECK(!o->asleep, "awake at %s", hm(now));

    printf("23:40 still (5 mg)\n");
    minutes(19, 0, 5);
    CHECK(!o->asleep, "19 still minutes: not yet (%s)", hm(now));
    minutes(1, 0, 5);
    CHECK(o->asleep, "asleep at %s (20 still minutes in the quiet hours)", hm(now));
    CHECK(take_ev() == REST_EV_NIGHT, "THE NIGHT RELEASE: the night's onset is the lease event (`release asleep` goes)");
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "since 23:40") != NULL, "the bed at 23:40");
    tick(now, 10);
    CHECK(zs_seen > 0, "Zs while asleep and lit");
    CHECK(o->breath_hz < 0.55f, "the breath slowing toward sleep: %.2f Hz", o->breath_hz);

    printf("00:00 -> 03:00 still\n");
    while (now != 3 * 60) minute(0, 4);
    CHECK(o->asleep && o->breath_hz < 0.32f, "still asleep at 03:00, the sleep breath %.2f Hz", o->breath_hz);

    printf("03:00 a stir: a raise in the quiet hours, two restless minutes, then still\n");
    {   /* THE WAKING BREATH, refused: the raise lit the glass, but the night is not over */
        float ph0 = o->breath_phase, amp0 = o->breath_amp;
        raise_lit();
        tick(now, 0.2f);
        CHECK(o->breath_phase != 0 && fabsf(o->breath_phase - ph0 - 2.0f * (float)M_PI * o->breath_hz * 0.2f) < 0.05f
              && fabsf(o->breath_amp - amp0) < 0.002f, "the waking breath refused in the night: the phase runs on (%.2f -> %.2f), the depth the sleeper's (%.3f)",
              ph0, o->breath_phase, o->breath_amp);
        tick(now, 4.8f);
    }
    CHECK(!o->asleep, "a raise at 03:00 is a stir (%s), not the morning", rest_state_name());
    CHECK(max_stretch == 0, "no stretch for a stir");
    minutes(2, 0, 45);
    minutes(3, 0, 5);
    CHECK(o->asleep, "still again: the stir forgiven, asleep (%s)", rest_state_name());
    CHECK(rest_span_min() > 190, "the span continues: %d min so far", rest_span_min());
    CHECK(take_ev() == REST_EV_NONE, "a stir is not the rising: no lease event (the house stays empty through it)");

    printf("03:05 -> 07:10 still\n");
    while (now != 7 * 60 + 10) minute(0, 4);
    CHECK(o->asleep, "asleep at 07:10 before he moves");
    CHECK(max_stretch == 0 && last_sense[0] == 0, "nothing said, no stretch, until he moves");

    printf("07:10 a raise: the morning\n");
    raise_lit();
    tick(now, 0.2f);
    CHECK(o->breath_phase != 0 && o->breath_amp < 0.05f, "the waking breath refused under the morning stretch (the rising is its own gesture): phase %.2f, amp %.3f", o->breath_phase, o->breath_amp);
    tick(now, 0.8f);
    CHECK(!o->asleep, "up (%s)", rest_state_name());
    CHECK(take_ev() == REST_EV_RISING, "THE NIGHT RELEASE: the rising is the lease event (the claim goes on the wake's dial)");
    CHECK(max_stretch > 0.99f, "the morning stretch (peak %.2f)", max_stretch);
    CHECK(rest_span_min() >= 445 && rest_span_min() <= 455, "the span %d min (~7.5 h)", rest_span_min());
    CHECK(!strncmp(last_sense, "wrist:slept slept 7.5 h", 23), "the sense: %s", last_sense);
    tick(now, 1.5f);
    CHECK(o->stretch == 0, "the stretch over after ~1 s");
    CHECK(strstr(last_sense, "1 awakening") == NULL, "a 2-min stir is not an awakening: %s", last_sense);
    rest_span_t live; bool have_live = rest_last_span(&live);
    uint32_t live_id = rest_night_id();
    CHECK(have_live && live.night, "the live night: %d min, %d awakening(s), up at %02d:%02d, id %lu",
          live.minutes, live.awakenings, live.end_minute / 60, live.end_minute % 60, (unsigned long)live_id);
    CHECK(live_id == rest_date_ordinal(DAY1) * 1440u + 430u, "the id is the wake's absolute minute (day %lu x 1440 + 07:10)",
          (unsigned long)rest_date_ordinal(DAY1));

    CHECK(o->itch_deg == 0, "no itch on the morning's raise: the sit is measured from the wake (sit %d min, capped)", rest_sit_min());

    printf("07:11 the day\n");
    max_stretch = 0; last_sense[0] = 0;
    while (now != 14 * 60) minute((now % 7) == 0 ? 40 : 0, 45);
    CHECK(!o->asleep, "awake at 14:00");

    printf("14:00 40 minutes still on the couch, then a raise\n");
    minutes(29, 0, 6);
    CHECK(!o->asleep, "29 still minutes by day: not yet");
    minutes(11, 0, 6);
    CHECK(o->asleep, "asleep at %s (30 still minutes by day is a nap)", hm(now));
    CHECK(take_ev() == REST_EV_NONE, "a nap by day is never the night release");
    raise();
    tick(now, 1.0f);
    CHECK(!o->asleep, "up at %s", hm(now));
    CHECK(max_stretch == 0, "no stretch after a nap");
    CHECK(!strncmp(last_sense, "wrist:napped napped 40 min", 26), "the sense: %s", last_sense);
    CHECK(take_ev() == REST_EV_NONE, "...and its end claims nothing");

    printf("14:41 -> 15:00 awake, then a 5-minute walk\n");
    while (now != 15 * 60) minute(0, 40);
    float before = o->breath_hz;
    walking = true;
    minutes(5, 105, 320);
    walking = false;
    CHECK(o->breath_hz > 0.75f && o->breath_amp > 0.045f, "quicker after the walk: %.2f Hz amp %.3f (was %.2f)", o->breath_hz, o->breath_amp, before);
    minutes(6, 0, 40);
    CHECK(o->breath_hz > 0.75f, "still quick %d min after the walk: %.2f Hz", 6, o->breath_hz);
    minutes(12, 0, 40);
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.03f, "eased back to rest by %s: %.2f Hz", hm(now), o->breath_hz);

    rest_status(st, sizeof st); printf("       %s\n", st);

    /* --- THE NIGHT AFTER A REBOOT ------------------------------------------- */
    printf("THE CALENDAR (no libc TZ): the ordinal and the day before\n");
    CHECK(rest_date_ordinal(19700101u) == 0, "1970-01-01 is day 0");
    CHECK(rest_date_ordinal(20000301u) - rest_date_ordinal(20000228u) == 2, "2000-02-29 exists");
    CHECK(rest_date_prev(20260301u) == 20260228u && rest_date_prev(20260101u) == 20251231u && rest_date_prev(20240301u) == 20240229u,
          "yesterday across a month, a year, a leap day");
    CHECK(rest_date_prev(DAY1) == DAY0, "the morning's yesterday is the evening's date");

    printf("A REBOOT at 07:40 the next morning: nothing known, then the files replayed through the same detector\n");
    int upto = 0;
    while (upto < nrows && !(rows[upto].date == DAY1 && rows[upto].minute == 7 * 60 + 40)) upto++;
    int rows0 = 0, rows1 = 0;
    for (int i = 0; i < upto; i++) { if (rows[i].date == DAY0) rows0++; else rows1++; }
    rest_begin(0.55f, 0.04f);
    motion = 0; walking = false; last_sense[0] = 0; max_stretch = 0; logs_seen = 0; last_log[0] = 0;
    tick(7 * 60 + 40, 0.4f);                     /* the quiet hours told, the motion counter seen */
    /* QUIET TIME (wake.cpp): the half of its rule that lives here -- the
     * quiet hours at the nap's own minute, not the last frame's */
    CHECK(rest_night_at(3 * 60) && rest_night_at(23 * 60) && rest_night_at(0), "rest_night_at: 03:00, 23:00 and midnight are the quiet hours (23-7)");
    CHECK(!rest_night_at(12 * 60) && !rest_night_at(7 * 60) && !rest_night_at(22 * 60 + 59), "...12:00, 07:00 and 22:59 are not");
    CHECK(rest_night_at(-1) == grain_night(), "...no clock: the grain's night=1 (%s)", grain_night() ? "yes" : "no, never told");
    rest_span_t sp;
    CHECK(!rest_last_span(&sp) && rest_night_min() == 0 && !rest_rebuilt(), "after the power cycle no night is known (the ring empty)");
    CHECK(rest_replay_begin(), "the replay begins (nothing lived clockless yet)");
    for (int i = 0; i < upto; i++)
        rest_replay_row(rows[i].date, rows[i].minute, (uint16_t)rows[i].steps, (uint16_t)rows[i].energy,
                        (uint8_t)(rows[i].energy < 15 ? 58 : 5), (uint8_t)rows[i].raises, false,
                        (uint8_t)rows[i].taps, rows[i].worn, rows[i].charging, rows[i].ups != 0);
    tick(7 * 60 + 40, 0.4f);
    CHECK(logs_seen == 0 && last_sense[0] == 0 && max_stretch == 0, "%d rows replayed: no log line, no sense, no stretch", upto);
    rest_replay_end(DAY0, rows0, DAY1, rows1);
    tick(7 * 60 + 40, 0.4f);
    CHECK(logs_seen == 1 && strstr(last_log, "rebuilt from the files (360 + 460 rows): 1 span, 1 night; last slept 7.5 h (23:40 to 07:10)") != NULL,
          "one summary line: %s", last_log);
    CHECK(rest_rebuilt(), "rest_rebuilt()");
    CHECK(rest_last_span(&sp) && sp.night, "the night is known again: %d min, %d awakening(s), up at %02d:%02d",
          sp.minutes, sp.awakenings, sp.end_minute / 60, sp.end_minute % 60);
    CHECK(sp.minutes >= live.minutes - 1 && sp.minutes <= live.minutes + 1, "its length within a minute of the live one (%d vs %d)", sp.minutes, live.minutes);
    CHECK(sp.awakenings == live.awakenings && sp.end_minute == live.end_minute, "the same awakenings and wake");
    CHECK(rest_night_min() == sp.minutes && rest_night_min() > 440, "the night ring has its hours back: %d min", rest_night_min());
    CHECK(rest_night_id() == live_id, "the stable id is the live night's: %lu", (unsigned long)rest_night_id());
    CHECK(!o->asleep && rest_steps_last(60) == 160 && rest_rows_since_walk() >= 0, "awake since; the ring holds the morning's rows (%d steps in the hour, a stroll %d min ago)",
          rest_steps_last(60), rest_rows_since_walk());
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "rebuilt from the files") != NULL, "`rest` says so");
    CHECK(!rest_replay_begin(), "a second replay is refused: the wrist has lived %lu rows since", (unsigned long)rest_rows());
    tick(7 * 60 + 40, 0.4f);
    CHECK(strstr(last_log, "not replayed") != NULL, "and says why: %s", last_log);

    printf("THE TOLD ID: tell.cpp asked by a raise, before and after the id from NVS marks the night told\n");
    tell_in_t tin; memset(&tin, 0, sizeof tin);
    tin.dt = 0.025f; tin.holding = true; tin.lit = true; tin.minute = 7 * 60 + 40; tin.date = DAY1; tin.rows = rest_rows();
    tin.span_known = true; tin.span_min = sp.minutes; tin.span_end_minute = sp.end_minute; tin.span_awakenings = sp.awakenings;
    tin.span_restless_min = sp.restless_min; tin.span_end_row = sp.end_row; tin.span_night = sp.night;
    tin.sit_min = rest_sit_min(); tin.steps_hour = rest_steps_last(60); tin.rows_since_walk = rest_rows_since_walk();
    tell_begin();
    tin.cue = TELL_CUE_RAISE; tell_tick(&tin); tin.cue = TELL_CUE_NONE;
    CHECK(tell_active() != TELL_NONE && tell_night_told_row() == sp.end_row, "untold: the raise answers the night (fact %d, row %lu)",
          tell_active(), (unsigned long)tell_night_told_row());
    uint32_t nvs_told_night = rest_night_id();          /* what daylog.cpp stamps into store.told_night */
    tell_begin();                                        /* the reboot again: tell's RAM gone */
    CHECK(tell_night_told_row() == 0, "after the reboot tell has told nothing");
    if (nvs_told_night == rest_night_id()) tell_told_night(sp.end_row);   /* daylog.cpp's restore after the replay */
    tin.cue = TELL_CUE_RAISE; tell_tick(&tin); tin.cue = TELL_CUE_NONE;
    CHECK(tell_active() == TELL_NONE, "told by the id: the raise says nothing about that night");

    /* --- THE SHELF RULE + A TOUCH ENDS SLEEP (2026-09-10) ---------------------- */
    printf("THE SHELF (Khalid's morning): worn all evening, then set on the desk, on the cable, at 23:50\n");
    scene(20260910u, 20260911u, 22 * 60);
    minutes(110, 0, 40);
    CHECK(!o->asleep && !rest_on_shelf(), "awake, worn, at %s", hm(now));
    take_off(); cable = true;
    minute(0, 40);                                   /* the set-down: one minute not still */
    minutes(19, 0, 2);
    CHECK(!o->asleep && !rest_on_shelf(), "19 still minutes on the cable: not yet (%s)", rest_state_name());
    minute(0, 2);
    CHECK(rest_on_shelf() && !o->asleep && !o->dozing, "20 still minutes on the cable: THE SHELF at %s (%s), not sleep", hm(now), rest_state_name());
    CHECK(take_ev() == REST_EV_NONE, "the shelf is never the night release (the cable)");
    CHECK(strstr(last_log, "on the shelf since 23:51 (still 20 min, on the cable): not his night") != NULL, "the log: %s", last_log);
    zs_seen = 0; tick(now, 5);
    CHECK(zs_seen == 0, "no Zs on the shelf, lit");
    CHECK(rest_night_min() == 0, "the night ring empty");
    while (now != 5 * 60 + 40) minute(0, 2);
    CHECK(rest_on_shelf() && !o->asleep, "still the shelf at 05:40 (%d min)", rest_span_min());
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.03f, "the ordinary rest breath on the shelf: %.2f Hz (not sleep's 0.30, not the sit's 0.40)", o->breath_hz);
    CHECK(rest_sit_min() == 0, "a shelf is not his sit: sit %d min", rest_sit_min());
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "on the shelf since 23:51 (5.8 h still, not worn: on the cable)") != NULL, "`rest` says so");
    CHECK(last_sense[0] == 0 && max_stretch == 0, "nothing told, no stretch");
    printf("05:40 a tap on the glass\n");
    touch();
    tick(now, 1.0f);
    CHECK(!rest_on_shelf() && !o->asleep && !o->dozing, "off the shelf: awake (%s)", rest_state_name());
    CHECK(take_ev() == REST_EV_NONE, "...and off it claims nothing");
    CHECK(strstr(last_log, "off the shelf at 05:40 (a hand on the glass): 5.8 h still, not worn (on the cable) -- no night") != NULL, "the log: %s", last_log);
    CHECK(last_sense[0] == 0 && max_stretch == 0 && senses_seen == 0, "no wrist:slept, no stretch: the shelf was nobody's night");
    CHECK(!rest_last_span(&sp) && rest_night_min() == 0, "no span, the ring empty");
    minute(0, 2);                                    /* the tap's row */
    put_on(); cable = false;
    minutes(5, 0, 40);
    CHECK(!o->asleep && !rest_on_shelf(), "the day, worn again");
    printf("THE SHELF REPLAYED from its rows\n");
    replay(20260910u, 20260911u, 5 * 60 + 46);
    CHECK(logs_seen == 1 && strstr(last_log, "rebuilt from the files (") != NULL && strstr(last_log, "0 spans, 0 nights, 1 shelf run; last -") != NULL,
          "one summary line: %s", last_log);
    CHECK(!rest_on_shelf() && !o->asleep && !rest_last_span(&sp) && rest_night_min() == 0 && rest_night_id() == 0,
          "the same verdict: awake, no span, the ring empty, no night id");

    printf("A TOUCH ENDS SLEEP: worn, still from 23:40, a tap at 06:30\n");
    scene(20260912u, 20260913u, 22 * 60);
    minutes(100, 0, 40);
    minutes(20, 0, 5);
    CHECK(o->asleep && !rest_on_shelf(), "worn: asleep at %s", hm(now));
    CHECK(strstr(last_log, "asleep since 23:40 (still 20 min, night, worn at the start)") != NULL, "the log: %s", last_log);
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "the night, worn at the start yes)") != NULL, "`rest`: worn at the start yes");
    while (now != 6 * 60 + 30) minute(0, 4);
    zs_seen = 0; tick(now, 5);
    CHECK(o->asleep && zs_seen > 0, "asleep at 06:30, the Zs rising");
    touch();
    tick(now, 0.2f);
    CHECK(!o->asleep && !o->dozing, "a hand on the glass: up at once (%s), never a stir", rest_state_name());
    zs_seen = 0; tick(now, 0.8f);
    CHECK(zs_seen == 0, "the Zs gone");
    CHECK(max_stretch > 0.99f, "the morning stretch (peak %.2f)", max_stretch);
    CHECK(!strncmp(last_sense, "wrist:slept slept 6.8 h", 23), "the sense: %s", last_sense);
    CHECK(strstr(last_log, "up at 06:30 (a hand on the glass): slept 6.8 h since 23:40, 0 awakenings, worn at the start yes") != NULL, "the log: %s", last_log);
    tick(now, 5);
    CHECK(senses_seen == 1, "told once (%d sense%s)", senses_seen, senses_seen == 1 ? "" : "s");
    CHECK(rest_last_span(&sp) && sp.night && sp.end_minute == 6 * 60 + 30 && rest_night_min() == sp.minutes && sp.minutes >= 409 && sp.minutes <= 411,
          "the night known: %d min, up at %02d:%02d, the ring %d min", sp.minutes, sp.end_minute / 60, sp.end_minute % 60, rest_night_min());
    uint32_t touch_id = rest_night_id();
    minute(0, 4);                                    /* the tap's row */
    minutes(10, 0, 40);
    touch();
    tick(now, 1.0f);
    CHECK(!o->asleep && senses_seen == 1, "a touch awake: nothing");
    printf("THE TOUCHED NIGHT REPLAYED from its rows\n");
    replay(20260912u, 20260913u, 6 * 60 + 41);
    CHECK(logs_seen == 1 && strstr(last_log, "1 span, 1 night; last slept 6.8 h (23:40 to 06:30)") != NULL, "one summary line: %s", last_log);
    CHECK(rest_last_span(&sp) && sp.night && sp.end_minute == 6 * 60 + 30 && sp.minutes >= 409 && sp.minutes <= 412,
          "the same verdict: the night ended by the tap at 06:30, %d min (the tap lands after its row closes: a minute's slack)", sp.minutes);
    CHECK(rest_night_id() == touch_id && rest_night_min() == sp.minutes, "the id the same (%lu), the ring %d min", (unsigned long)rest_night_id(), rest_night_min());
    printf("14:00 a nap, ended by a tap\n");
    while (now != 14 * 60) minute((now % 7) == 0 ? 40 : 0, 45);
    minutes(40, 0, 6);
    CHECK(o->asleep, "asleep at %s (a nap, worn)", hm(now));
    max_stretch = 0;
    touch();
    tick(now, 1.0f);
    CHECK(!o->asleep && max_stretch == 0 && !strncmp(last_sense, "wrist:napped napped 40 min", 26), "up, no stretch, the sense: %s", last_sense);
    minute(0, 6);
    minutes(3, 0, 40);
    printf("...AND THE NAP REPLAYED\n");
    replay(20260912u, 20260913u, 14 * 60 + 44);
    CHECK(logs_seen == 1 && strstr(last_log, "2 spans, 1 night; last napped 4") != NULL && strstr(last_log, "(14:00 to 14:40)") != NULL,
          "one summary line: %s", last_log);
    CHECK(rest_last_span(&sp) && !sp.night && sp.end_minute == 14 * 60 + 40, "the nap ended by the tap at 14:40, %d min", sp.minutes);

    printf("THE SHELF WITHOUT THE CABLE: set on the desk at 22:00, a fidget at 02:00\n");
    scene(20260914u, 20260915u, 21 * 60);
    minutes(60, 0, 40);
    take_off();
    minute(0, 40);                                   /* the set-down */
    minutes(30, 0, 2);
    CHECK(rest_on_shelf(), "30 still minutes by day, the WORN echo over: the shelf at %s", hm(now));
    CHECK(strstr(last_log, "on the shelf since 22:01 (still 30 min, never moved): not his night") != NULL, "the log: %s", last_log);
    while (now != 2 * 60) minute(0, 2);
    CHECK(rest_on_shelf() && rest_night_min() == 0, "still the shelf at 02:00, no night growing");
    fidget();
    minute(0, 2);
    CHECK(o->asleep && !rest_on_shelf(), "a still row that moved, off the cable: his after all (%s)", rest_state_name());
    CHECK(take_ev() == REST_EV_NIGHT, "THE NIGHT RELEASE: the promoted shelf is the night, late -- `release asleep` at 02:00");
    CHECK(strstr(last_log, "the shelf moved at 02:00: worn after all -- asleep since 22:01") != NULL, "the log: %s", last_log);
    rest_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "worn at the start no, moved at 02:00)") != NULL, "`rest`: worn at the start no, moved at 02:00");
    while (now != 7 * 60) minute(0, 2);
    raise();
    tick(now, 1.0f);
    CHECK(!o->asleep && max_stretch > 0.99f && !strncmp(last_sense, "wrist:slept slept 9.0 h", 23), "07:00 a raise: up, the stretch, %s", last_sense);
    CHECK(take_ev() == REST_EV_RISING, "...and the raise at 07:00 is the rising");
    CHECK(strstr(last_log, "worn at the start no") != NULL, "the log: %s", last_log);
    minutes(5, 0, 40);
    printf("...REPLAYED\n");
    replay(20260914u, 20260915u, 7 * 60 + 5);
    CHECK(logs_seen == 1 && strstr(last_log, "1 span, 1 night; last slept 9.0 h (22:01 to 07:00)") != NULL, "one summary line: %s", last_log);

    /* --- THE SHAKEN SLEEPER (design/watch-ideas.md A1) ------------------------------ */
    printf("THE SHAKEN SLEEPER: worn, still from 23:40; a shake at 03:00 is the rising, never a stir\n");
    scene(20260916u, 20260917u, 22 * 60);
    minutes(100, 0, 40);
    minutes(20, 0, 5);
    CHECK(o->asleep, "asleep at %s", hm(now));
    while (now != 3 * 60) minute(0, 4);
    zs_seen = 0; tick(now, 5);
    CHECK(o->asleep && zs_seen > 0, "asleep at 03:00, the Zs rising");
    shake();
    tick(now, 0.2f);
    CHECK(!o->asleep && !o->dozing, "a shake at 03:00: up at once (%s), never a stir", rest_state_name());
    CHECK(max_stretch > 0.99f, "the morning stretch (peak %.2f)", max_stretch);
    CHECK(!strncmp(last_sense, "wrist:slept slept 3.3 h", 23), "the sense: %s", last_sense);
    CHECK(strstr(last_log, "up at 03:00 (a shake): slept 3.3 h since 23:40") != NULL, "the log: %s", last_log);
    minute(0, 4);                                    /* the shake's row (UP) */
    minutes(10, 0, 40);
    printf("...and THE ITCH counts the sit from the wake, not from the night's still rows\n");
    minutes(20, 0, 30);
    CHECK(o->itch_deg == 0 && rest_sit_min() > 200, "31 sit minutes after the wake: no itch yet (rest_sit_min %d is the tell's, uncapped)", rest_sit_min());
    minutes(31, 0, 30);
    CHECK(fabsf(o->itch_deg) > 4.0f && fabsf(o->itch_deg) < 4.2f, "62 min after the wake: the itch (%+.1f)", o->itch_deg);
    printf("...REPLAYED from the row's UP flag\n");
    replay(20260916u, 20260917u, 3 * 60 + 11);
    CHECK(logs_seen == 1 && strstr(last_log, "1 span, 1 night; last slept 3.3 h (23:40 to 03:00)") != NULL, "one summary line: %s", last_log);
    CHECK(rest_last_span(&sp) && sp.night && sp.end_minute == 3 * 60, "the same verdict: the night ended by the shake at 03:00, %d min", sp.minutes);

    printf("A SHAKE MID-STIR: a raise at 03:00 is a stir; a shake a minute later is the rising\n");
    scene(20260918u, 20260919u, 22 * 60);
    minutes(100, 0, 40);
    minutes(20, 0, 5);
    while (now != 3 * 60) minute(0, 4);
    raise();
    tick(now, 1.0f);
    CHECK(!o->asleep && o->dozing, "a raise at 03:00 is a stir (%s)", rest_state_name());
    minute(0, 45);
    max_stretch = 0;
    shake();
    tick(now, 0.2f);
    CHECK(!o->asleep && !o->dozing && max_stretch > 0.99f, "a shake in the stir: up (%s), the stretch", rest_state_name());
    CHECK(!strncmp(last_sense, "wrist:slept slept 3.3 h", 23), "the sense, the span ended where the stir began: %s", last_sense);
    CHECK(strstr(last_log, "up at 03:00 (a shake)") != NULL, "the log: %s", last_log);
    minute(0, 45);
    minutes(5, 0, 40);
    shake();
    tick(now, 0.5f);
    CHECK(!o->asleep && senses_seen == 1, "a shake awake: nothing");

    printf("A SHAKE ON THE SHELF: set down at 22:00, shaken at 01:00 -- the run ends, nothing told\n");
    scene(20260920u, 20260921u, 21 * 60);
    minutes(60, 0, 40);
    take_off();
    minute(0, 40);
    minutes(30, 0, 2);
    CHECK(rest_on_shelf(), "the shelf at %s", hm(now));
    while (now != 1 * 60) minute(0, 2);
    shake();
    tick(now, 1.0f);
    CHECK(!rest_on_shelf() && !o->asleep && max_stretch == 0 && senses_seen == 0, "off the shelf (%s): no stretch, nothing told", rest_state_name());
    CHECK(strstr(last_log, "off the shelf at 01:00 (a shake)") != NULL, "the log: %s", last_log);

    printf("PUT ON COLD: set on the dock at 22:00, still 3 h, picked up (a raise) at 01:00 -- the shiver, the look up, the breath slow to warm\n");
    scene(20260922u, 20260923u, 21 * 60);
    minutes(60, 0, 40);
    take_off(); cable = true;
    minute(0, 40);
    minutes(30, 0, 2);
    CHECK(rest_on_shelf() && !rest_cold(), "the shelf at %s (on the cable), not cold", hm(now));
    while (now != 1 * 60) minute(0, 2);
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.01f && o->cold_look == 0 && o->cold_a2 == 0, "on the shelf the breath is the ordinary rest (%.2f Hz), no cold", o->breath_hz);
    raise_lit();
    tick(now, 0.2f);
    CHECK(o->breath_phase != 0 && o->breath_amp < 0.04f, "the waking breath refused off the cold shelf (PUT ON COLD's raise): phase %.2f, the sleeper's depth %.3f", o->breath_phase, o->breath_amp);
    tick(now, 0.2f);
    CHECK(!rest_on_shelf() && !o->asleep && max_stretch == 0 && senses_seen == 0, "off the shelf (%s): no stretch, nothing told", rest_state_name());
    CHECK(strstr(last_log, "off the shelf at 01:00 (a raise or a shake)") && strstr(last_log, "-- no night; put on cold"), "the log: %s", last_log);
    CHECK(fabsf(o->cold_a2) > 0.005f, "the shiver's jiggle on the second frame (a2 %+.3f)", o->cold_a2);
    CHECK(o->cold_look > 0.9f && o->cold_look <= 1.5f, "the look up, %.1f s left of 1.5", o->cold_look);
    CHECK(o->breath_hz < 0.35f && rest_cold(), "the breath snapped to the sleeper's (%.2f Hz), cold", o->breath_hz);
    tick(now, 0.2f);
    CHECK(o->cold_a2 == 0, "the shiver over by 0.6 s");
    tick(now, 5.0f);
    CHECK(o->cold_look == 0 && o->breath_hz > 0.35f && o->breath_hz < 0.45f && rest_cold(), "5 s on: the look done, the breath warming (%.2f Hz), still cold", o->breath_hz);
    tick(now, 40.0f);
    CHECK(o->breath_hz > 0.52f && !rest_cold(), "45 s on: warm (%.2f Hz)", o->breath_hz);
    printf("-- an hour on the shelf, picked up: off the shelf, not cold\n");
    scene(20260922u, 20260923u, 21 * 60);
    minutes(60, 0, 40);
    take_off(); cable = true;
    minute(0, 40);
    minutes(60, 0, 2);
    CHECK(rest_on_shelf(), "the shelf at %s", hm(now));
    raise();
    tick(now, 0.4f);
    CHECK(!rest_on_shelf() && o->cold_a2 == 0 && o->cold_look == 0 && fabsf(o->breath_hz - 0.55f) < 0.01f && !rest_cold(), "an hour is not hours: no shiver, no look, the breath as it was (%.2f Hz)", o->breath_hz);
    CHECK(strstr(last_log, "off the shelf at") && !strstr(last_log, "put on cold"), "the log: %s", last_log);
    printf("-- 3 h on the shelf, then a hand on the glass: off the shelf, not cold (a hand is not the wrist put on)\n");
    scene(20260922u, 20260923u, 21 * 60);
    minutes(60, 0, 40);
    take_off(); cable = true;
    minute(0, 40);
    minutes(180, 0, 2);
    CHECK(rest_on_shelf(), "the shelf at %s", hm(now));
    touch();
    tick(now, 0.4f);
    CHECK(!rest_on_shelf() && o->cold_look == 0 && !rest_cold(), "a hand on the glass: no cold (%s)", last_log);
    printf("-- the cold night replayed from its rows: the shelf, then off it, nothing cold\n");
    scene(20260922u, 20260923u, 21 * 60);
    minutes(60, 0, 40);
    take_off(); cable = true;
    minute(0, 40);
    minutes(180, 0, 2);
    raise();
    tick(now, 0.4f);
    CHECK(rest_cold(), "live: cold");
    minute(0, 40);                       /* the raise's row closes (the file holds it); the minute warms the breath */
    replay(20260922u, 20260923u, now);
    CHECK(!rest_on_shelf() && !rest_cold() && o->cold_look == 0, "replayed: off the shelf, not cold");

    printf("THE PULSE AS THE BREATH (D31): a clean pulse leads the resting breath -- 60 bpm = the grain's rate, in proportion, eased 60 s, clamped; stale at 30 min; the walk still wins\n");
    scene(20260924u, 20260925u, 10 * 60);
    minutes(5, 10, 40);
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.02f && !o->breath_pulse, "no pulse known: the grain's rest, %.2f Hz", o->breath_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "| no pulse known"), "`rest`: ... %s", strstr(st, "| no pulse"));
    pulse_bpm = 60; pulse_age = 0;
    tick(now, 5);
    CHECK(o->breath_pulse && fabsf(o->breath_hz - 0.55f) < 0.02f, "60 bpm = the gel's resting rate: %.2f Hz, the breath his pulse's now", o->breath_hz);
    CHECK(strstr(last_log, "breath: his pulse, 60 bpm"), "the log: %s", last_log);
    pulse_bpm = 90; pulse_age = 0;
    tick(now, 30);
    CHECK(o->breath_hz > 0.72f && o->breath_hz < 0.77f, "90 bpm, 30 s in: %.2f Hz -- the 60 s ease (the bands' 30 would already be at 0.79)", o->breath_hz);
    tick(now, 60);
    CHECK(fabsf(o->breath_hz - 0.80f) < 0.02f && o->breath_amp > 0.045f, "90 s in: %.2f Hz amp %.3f -- the walk's rate and depth, from the pulse alone", o->breath_hz, o->breath_amp);
    CHECK(fabsf(o->breath_rest_hz - 0.55f) < 0.01f, "THE BUMPS' resting rate stays the grain's %.2f: this breath reads as quick, a pet is rightly kept", o->breath_rest_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "(pulse)") && strstr(st, "pulse 90 bpm, ") && strstr(st, "min old: the breath follows it"), "`rest`: ... %s", strstr(st, "| pulse"));
    pulse_bpm = 140; pulse_age = 0;
    tick(now, 90);
    CHECK(o->breath_pulse && fabsf(o->breath_hz - 0.80f) < 0.02f, "140 bpm: %.2f Hz -- clamped at the walk's, a gel does not pant", o->breath_hz);
    pulse_bpm = 40; pulse_age = 0;
    tick(now, 120);
    CHECK(o->breath_hz > 0.34f && o->breath_hz < 0.40f && o->breath_amp < 0.035f, "40 bpm: %.2f Hz amp %.3f -- slower than the hour's sit, shallower", o->breath_hz, o->breath_amp);
    pulse_bpm = 20; pulse_age = 0;
    tick(now, 120);
    CHECK(!o->breath_pulse && fabsf(o->breath_hz - 0.55f) < 0.02f, "20 bpm (a table, a half-count): ignored, the grain's %.2f Hz", o->breath_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "pulse 20 bpm") && strstr(st, "past the honest band, ignored"), "`rest`: ... %s", strstr(st, "| pulse"));
    pulse_bpm = 300; pulse_age = 0;
    tick(now, 5);
    CHECK(!o->breath_pulse && fabsf(o->breath_hz - 0.55f) < 0.02f, "300 bpm (a double-count): ignored, %.2f Hz", o->breath_hz);
    printf("-- staleness: a 90 bpm read holds for 29 min and is nobody's at 30\n");
    pulse_bpm = 90; pulse_age = 0;
    tick(now, 90);
    pulse_age = 29 * 60;
    tick(now, 2);
    CHECK(o->breath_pulse && fabsf(o->breath_hz - 0.80f) < 0.02f, "29 min old: still his, %.2f Hz", o->breath_hz);
    pulse_age = 30 * 60;
    tick(now, 30);
    CHECK(!o->breath_pulse && o->breath_hz > 0.58f && o->breath_hz < 0.64f, "30 min old, 30 s on: drifting back (%.2f Hz) over the slow 60 s", o->breath_hz);
    CHECK(strstr(last_log, "breath: the pulse is 30 min old"), "the log: %s", last_log);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "stale, the grain's rate"), "`rest`: ... %s", strstr(st, "| pulse"));
    tick(now, 60);
    CHECK(fabsf(o->breath_hz - 0.55f) < 0.02f, "90 s on: the grain's %.2f Hz again", o->breath_hz);
    printf("-- the walk wins while walking (its own 30 s), his pulse after\n");
    pulse_bpm = 50; pulse_age = 0;
    tick(now, 90);
    CHECK(o->breath_pulse && o->breath_hz > 0.43f && o->breath_hz < 0.49f, "50 bpm: %.2f Hz", o->breath_hz);
    walking = true;
    tick(now, 60);
    CHECK(!o->breath_pulse && fabsf(o->breath_hz - 0.80f) < 0.03f, "walking: the walk's %.2f Hz -- the pulse yields", o->breath_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "the walk's or the sleeper's band wins"), "`rest`: ... %s", strstr(st, "| pulse"));
    walking = false;
    tick(now, 90);
    CHECK(o->breath_pulse && o->breath_hz > 0.43f && o->breath_hz < 0.49f, "the walk done: his pulse again, %.2f Hz", o->breath_hz);

    printf("-- THE WAKING BREATH (C6): a raise from the dark finds the inhale, x1.6 easing to 1 over 3 s\n");
    pulse_bpm = 0; pulse_age = 0;
    tick(now, 90);                                       /* the grain's rest again, settled */
    while (o->breath_phase < 1.0f || o->breath_phase > 5.0f) tick(now, 0.2f);   /* mid-cycle, as every raise used to find it */
    {
        float ph0 = o->breath_phase, amp0 = o->breath_amp;
        raise_lit();
        tick(now, 0.2f);
        CHECK(o->breath_phase == 0.0f, "the raise's frame: the phase snapped from %.2f to the inhale's start (0 -- the purr's wrap, read as 'now')", ph0);
        CHECK(fabsf(o->breath_amp / amp0 - 1.6f) < 0.03f, "the depth x%.2f on that frame (WAKE_BREATH_X 1.6)", o->breath_amp / amp0);
        CHECK(strstr(last_log, "breath: the waking breath -- the inhale now, x1.6 easing to 1 over 3 s"), "the log: %s", last_log);
        tick(now, 1.4f);
        CHECK(o->breath_phase > 3.5f && o->breath_phase < 5.0f, "1.6 s on: the phase ran on from 0 (%.2f) -- no second jump", o->breath_phase);
        CHECK(fabsf(o->breath_amp / amp0 - 1.32f) < 0.04f, "1.6 s on: x%.2f (easing)", o->breath_amp / amp0);
        tick(now, 2.0f);
        CHECK(fabsf(o->breath_amp / amp0 - 1.0f) < 0.03f, "3.6 s on: x%.2f -- the day's depth again", o->breath_amp / amp0);
        rest_status(st, sizeof st);
        CHECK(strstr(st, "| waking breaths 1") && !strstr(st, "easing now"), "`rest`: ... %s", strstr(st, "| waking"));
    }
    printf("-- a touch-wake is not a raise: the bool is false, the phase runs on\n");
    {
        while (o->breath_phase < 1.0f || o->breath_phase > 5.0f) tick(now, 0.2f);
        float ph0 = o->breath_phase, amp0 = o->breath_amp;
        touch();
        tick(now, 0.2f);
        CHECK(o->breath_phase > ph0 && fabsf(o->breath_amp - amp0) < 0.001f, "a touch: the phase %.2f -> %.2f, the depth unchanged", ph0, o->breath_phase);
    }
    printf("-- a second raise mid-ease: the inhale again, the ease restarted (never past x1.6)\n");
    {
        raise_lit(); tick(now, 0.2f);
        float amp1 = o->breath_amp;
        tick(now, 1.0f);
        raise_lit(); tick(now, 0.2f);
        CHECK(o->breath_phase == 0.0f && fabsf(o->breath_amp - amp1) < 0.002f, "the second raise: the inhale again at x1.6 (%.3f = %.3f), not x2.2", o->breath_amp, amp1);
        rest_status(st, sizeof st);
        CHECK(strstr(st, "| waking breaths 3 (one easing now)"), "`rest`: ... %s", strstr(st, "| waking"));
        tick(now, 4.0f);
    }

    printf("-- THE WARM SKIN (C20): the skin read COLD -- PUT ON COLD's road: the shiver, the breath snapped slow and warming over 30 s, no look up\n");
    {
        tick(now, 60);                                       /* the day's rest, settled */
        float hz0 = o->breath_hz;
        chilled = true; tick(now, 0.2f);
        CHECK(o->breath_hz < hz0 * 0.7f, "the chill's frame: the breath snapped from %.2f to %.2f Hz (the sleeper's 0.30)", hz0, o->breath_hz);
        CHECK(strstr(last_log, "breath: the skin reads cold -- the shiver, the breath snapped to 0.30 Hz, warming over 30 s"), "the log: %s", last_log);
        tick(now, 0.2f);
        CHECK(o->cold_a2 != 0.0f && o->cold_look == 0.0f, "the shiver runs (a2 %.3f), no look up (that is the shelf's)", o->cold_a2);
        CHECK(rest_cold(), "rest_cold(): the breath is warming");
        tick(now, 1.0f);
        CHECK(o->cold_a2 == 0.0f, "1.4 s on: the shiver is over (0.3 s)");
        tick(now, 60);
        CHECK(fabsf(o->breath_hz - hz0) < 0.03f && !rest_cold(), "a minute on: the day's %.2f Hz again, warm", o->breath_hz);
        chilled = false;
    }

    printf("THE RUN AND THE PANT (C10): a minute row of 150+ steps is the run (1.0 Hz, the walk's depth, over the pulse); the row after it the pant (x1.3, the crown lower, 90 s)\n");
    scene(20261013u, 20261014u, 17 * 60);
    minutes(5, 10, 40);
    CHECK(!o->run && o->pant_squash == 0 && fabsf(o->breath_hz - 0.55f) < 0.02f, "17:05 a fidget: no run, no pant, the grain's %.2f Hz", o->breath_hz);
    pulse_bpm = 90; pulse_age = 0;
    tick(now, 90);
    CHECK(o->breath_pulse && fabsf(o->breath_hz - 0.80f) < 0.02f, "a 90 bpm pulse leads: %.2f Hz (the pulse's clamp)", o->breath_hz);
    minutes(1, 149, 400);
    CHECK(!o->run && o->breath_pulse && fabsf(o->breath_hz - 0.80f) < 0.02f, "a row of 149 steps is a brisk walk, not a run: the pulse still leads at %.2f Hz", o->breath_hz);
    minutes(1, 150, 500);
    CHECK(o->run && !o->breath_pulse && o->breath_hz > 0.97f && o->breath_hz <= 1.0f, "a row of 150: the run -- %.2f Hz a minute on, the pulse-led breath yields (its clamp is the pulse's law)", o->breath_hz);
    CHECK(fabsf(o->breath_amp - 0.048f) < 0.003f && o->pant_squash == 0, "the walk's full depth (%.3f = 1.2 x 0.040), no pant yet", o->breath_amp);
    CHECK(strstr(last_log, "breath: a run (150 steps in the minute) -- 1.00 Hz"), "the log: %s", last_log);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "(run)") && strstr(st, "the run's, the walk's or the sleeper's band wins") && strstr(st, "| runs 1 (running now)"), "`rest`: ... %s", strstr(st, "| pulse"));
    minutes(3, 160, 500);
    CHECK(o->run && o->pant_squash == 0 && fabsf(o->breath_hz - 1.0f) < 0.01f, "three more minutes of it: still the run (%.2f Hz), one run counted, no pant", o->breath_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "| runs 1 (running now)"), "`rest`: ... %s", strstr(st, "| runs"));
    printf("-- the row after it under 150: the run is over, the pant\n");
    rest_set_date(date_now);
    rest_minute(now, 40, 60, 5, true, false);            /* the row closes (the helper's minute() would live 60 s before we could look) */
    now = (now + 1) % 1440;
    tick(now, 1.2f);
    CHECK(!o->run && strstr(last_log, "breath: the run is over -- the pant, x1.3 and the crown a hair lower, easing out over 90 s"), "the log: %s", last_log);
    CHECK(o->pant_squash > 0.039f && o->pant_squash <= 0.04f, "1.2 s on: the crown %.3f lower (PANT_SQUASH 0.04, in over its second)", o->pant_squash);
    CHECK(fabsf(o->breath_amp / 0.048f - 1.3f) < 0.03f, "the depth x%.2f of the walk's (PANT_AMP_X 1.3)", o->breath_amp / 0.048f);
    CHECK(o->breath_hz > 0.95f, "the rate still the run's (%.2f Hz): it eases down the walk's road, not a jump", o->breath_hz);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "| runs 1 (panting, 8") && strstr(st, "s left)"), "`rest`: ... %s", strstr(st, "| runs"));
    tick(now, 43.8f);
    CHECK(fabsf(o->pant_squash - 0.02f) < 0.002f && fabsf(o->breath_amp / 0.048f - 1.15f) < 0.03f, "45 s on: half way back (crown %.3f, depth x%.2f)", o->pant_squash, o->breath_amp / 0.048f);
    CHECK(o->breath_hz > 0.78f && o->breath_hz < 0.84f, "45 s on: the rate is the walk's %.2f Hz (ten minutes of it, as after any walk)", o->breath_hz);
    tick(now, 45.4f);
    CHECK(o->pant_squash == 0 && fabsf(o->breath_amp - 0.048f) < 0.002f, "90 s on: the breath got back -- no pant, the walk's depth %.3f", o->breath_amp);
    rest_status(st, sizeof st);
    CHECK(strstr(st, "| runs 1") && !strstr(st, "panting") && !strstr(st, "running"), "`rest`: ... %s", strstr(st, "| runs"));
    CHECK(senses_seen == 0, "never a sense");
    printf("-- the run's rows replayed from the files: no run, no pant, no log (the files' rows never run)\n");
    pulse_bpm = 0; pulse_age = 0;
    minute(0, 40);
    replay(20261013u, 20261014u, now);
    tick(now, 60);
    CHECK(!o->run && o->pant_squash == 0 && fabsf(o->breath_hz - 0.80f) < 0.02f, "replayed, a minute on: no run, no pant -- the walk's %.2f Hz from the rows' steps (the ring's ten minutes), never the run's 1.0", o->breath_hz);
    printf("-- the night: a run row ends the span as steps; the run is the awake creature's, never under the Zs\n");
    scene(20261013u, 20261014u, 23 * 60);
    minutes(25, 0, 2);
    CHECK(o->asleep && fabsf(o->breath_hz - 0.30f) < 0.05f, "asleep by %s, the sleeper's %.2f Hz", hm(now), o->breath_hz);
    minutes(1, 200, 600);
    CHECK(!o->asleep && !rest_dozing_now() && o->run, "a row of 200 steps ends the night (%s); the run plays awake, after it", rest_state_name());
    CHECK(strstr(last_log, "breath: a run (200 steps"), "the log: %s", last_log);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
