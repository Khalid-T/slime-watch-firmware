/* THE TELL on the laptop (no board, no Arduino: tell.cpp is numbers in,
 * a squash / an eye / a kick out -- the same pattern as rest_test):
 *   c++ -std=gnu++17 -I src -o /tmp/tell_test tools/tell_test.cpp src/tell.cpp src/grain.cpp -lm && /tmp/tell_test
 * A restless night then a raise -> droopy once, a second raise ->
 * nothing; a 70-min sit -> the stretch once (and not again as it grows);
 * a walk of 20 steps -> bob pulses in the first 10 s only, none after,
 * none asleep, none on an empty glass; the priority = the freshest fact;
 * the morning face after a sound night and not two hours later; the
 * night light's window; nothing while dozing or empty. THE YAWN AT DUSK
 * (A2, 2026-09-23): in the hour before the quiet hours a raise with
 * nothing to tell yawns once an evening (the bar, the tall intake, the
 * slow slump, the ordinary face); a fact goes first and the yawn takes
 * the next raise; never dozing, never empty, not by day. UNDER THE
 * WEATHER (D34, 2026-10-08): a resting heart over the week's wears the
 * restless night's droopy face once a day (the log line carries the
 * numbers), after any fresher fact, again on a new date, never dozing
 * or empty; 0 / 0 = nothing.
 * HE ARRIVES WITH THE OTHER HOUSE'S DAY (C21, 2026-10-09): the arrival's
 * cue with the grain line's sat= / pets= -- a long desk session = heavy
 * lids for 1.5 s at 90 min, scaled to 3 s at 180 and no longer; a petted
 * afternoon = the content face with one soft settle for 1.2 s; the sit
 * outranks the pets, the desk's day outranks the wrist's own untold fact
 * on the arrival (the fact keeps for the next raise); under both lines
 * nothing; never on a raise; the status line names the two numbers. */
#include "../src/tell.h"
#include "../src/gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static tell_in_t in;
static const tell_out_t *o;
static char last_log[120] = "";
static int logs = 0;

/* one frame (25 ms) */
static void frame(void)
{
    in.dt = 0.025f;
    o = tell_tick(&in);
    in.cue = TELL_CUE_NONE;
    if (o->log_new) { snprintf(last_log, sizeof last_log, "%s", o->log); printf("       log: %s\n", last_log); logs++; tell_log_taken(); }
}
/* seconds of frames; returns the peak |squash| seen, the eye kinds seen as a bitmask, the kicks */
static float peak, kick1_sum; static int eyes_seen, night_frames, done_n;
static void run(float s)
{
    peak = 0; eyes_seen = 0; kick1_sum = 0; night_frames = 0; done_n = 0;
    for (float t = 0; t < s - 1e-4f; t += 0.025f) {
        frame();
        if (fabsf(o->squash) > fabsf(peak)) peak = o->squash;
        if (o->eye_kind >= 0) eyes_seen |= 1 << o->eye_kind;
        kick1_sum += o->kick1;
        if (o->night_light) night_frames++;
        if (o->night_done) done_n++;
    }
}
static void reset_in(void)
{
    memset(&in, 0, sizeof in);
    in.holding = true; in.lit = true; in.minute = 8 * 60; in.date = 20260910; in.rows = 1000;
    in.rows_since_walk = -1; in.norm = 6000;
}
/* the last span: a night of ``min`` minutes ended ``ago`` rows back */
static void night(int min, int awakenings, int restless, int ago)
{
    in.span_known = true; in.span_night = min >= 180; in.span_min = min; in.span_awakenings = awakenings;
    in.span_restless_min = restless; in.span_end_row = in.rows - ago; in.span_end_minute = (in.minute - ago + 1440) % 1440;
    in.sit_min = min + ago;            /* rest.cpp's sit run counts the night */
}

int main(void)
{
    char st[400];

    printf("-- a restless night (5.0 h, 4 awakenings), a raise: droopy once\n");
    tell_begin(); reset_in(); night(300, 4, 40, 0);
    in.cue = TELL_CUE_RAISE; run(0.2f);
    CHECK(tell_active() == TELL_NIGHT && o->eye_kind < 0 && o->squash == 0, "the answer waits the wake's ease (0.3 s): nothing yet");
    run(0.9f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) && peak > 0.09f, "droopy lids and the low settle (peak %+.2f)", peak);
    run(1.5f);
    CHECK(tell_active() == TELL_NONE && o->eye_kind < 0, "then the ordinary creature");
    tell_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "now nothing") && strstr(st, "said droopy"), "`tell`: nothing more to say, droopy said");
    in.rows += 5; in.sit_min += 5;
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0 && peak == 0, "a second raise: nothing (told once)");

    printf("-- a short but calm night (5.5 h, 0 awakenings) is still 'restless' (< 6 h)\n");
    tell_begin(); reset_in(); night(330, 0, 5, 0);
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(eyes_seen & (1 << GEL_EYE_DROOPY), "droopy");
    printf("-- a fair night (7 h, 2 awakenings, 12%% restless): nothing to say\n");
    tell_begin(); reset_in(); night(420, 2, 50, 0);
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "nothing");
    printf("-- a nap (40 min) is not a night: nothing\n");
    tell_begin(); reset_in(); night(40, 0, 0, 0);
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "nothing");

    printf("-- a 70-min sit, still open: the stretch once\n");
    tell_begin(); reset_in(); night(420, 2, 50, 200); in.sit_min = 70;
    in.cue = TELL_CUE_RAISE; run(0.3f);
    run(0.5f);
    CHECK((eyes_seen & (1 << GEL_EYE_WIDE)) && peak < -0.29f, "tall (%+.2f), eyes wide", peak);
    run(0.6f);
    CHECK(kick1_sum > 1.0f && (eyes_seen & (1 << GEL_EYE_OPEN)), "the shake-off wobble (kick %.1f) under open eyes", kick1_sum);
    run(1.0f);
    CHECK(tell_active() == TELL_NONE, "over");
    in.rows += 10; in.sit_min = 80;
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "the sit grown to 80 min: the same sit, not told again");
    in.rows += 30; in.sit_min = 65;                    /* he got up in between: a new sit of 65 */
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(eyes_seen & (1 << GEL_EYE_WIDE), "a new sit past the hour: the stretch again");
    printf("-- the morning: the sit run is the night's -- capped at the wake\n");
    tell_begin(); reset_in(); night(420, 2, 50, 5);   /* sit_min = 425, but the span ended 5 rows ago */
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "5 min up after a fair night: no sit to speak of");

    printf("-- a good walk (1600 steps this hour, ended 3 min ago): the content bob\n");
    tell_begin(); reset_in(); in.steps_hour = 1600; in.rows_since_walk = 3; in.steps_today = 2000;
    in.cue = TELL_CUE_RAISE; run(0.3f); run(0.3f);
    CHECK((eyes_seen & (1 << GEL_EYE_CONTENT)) && peak > 0.07f, "content eyes, the first soft pulse (%+.2f)", peak);
    run(0.3f); float p1 = peak; run(0.5f);
    CHECK(peak > 0.07f, "the second pulse (%+.2f after %+.2f)", peak, p1);
    run(0.5f);
    CHECK(tell_active() == TELL_NONE, "over");
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(eyes_seen == 0, "the same hour: not again");
    printf("-- the norm met (6000 of 6000) with a quiet hour: once for the day\n");
    tell_begin(); reset_in(); in.steps_today = 6000; in.steps_hour = 300;
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(eyes_seen & (1 << GEL_EYE_CONTENT), "the content bob");
    in.rows += 120; in.steps_today = 6400;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(eyes_seen == 0, "two hours on, the same day: not again");

    printf("-- the priority: a restless night 30 min ago, a walk 5 min ago -> the walk first, the night next\n");
    tell_begin(); reset_in(); night(300, 4, 40, 30); in.steps_hour = 1600; in.rows_since_walk = 5;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK((eyes_seen & (1 << GEL_EYE_CONTENT)) && !(eyes_seen & (1 << GEL_EYE_DROOPY)), "the walk (fresher)");
    run(1.0f);
    in.cue = TELL_CUE_RAISE; run(2.5f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) && !(eyes_seen & (1 << GEL_EYE_CONTENT)), "then the night");
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(eyes_seen == 0, "then nothing");
    printf("-- the other way: the night 2 min ago, the walk 20 min ago -> the night first\n");
    tell_begin(); reset_in(); night(300, 4, 40, 2); in.steps_hour = 1600; in.rows_since_walk = 20;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) && !(eyes_seen & (1 << GEL_EYE_CONTENT)), "the night (fresher)");

    printf("-- a sound night (7.5 h, 0 awakenings), the first raise: the morning face\n");
    tell_begin(); reset_in(); night(450, 0, 10, 0);
    in.cue = TELL_CUE_RAISE; run(0.3f); run(0.9f);
    CHECK((eyes_seen & (1 << GEL_EYE_HAPPY)) && peak == 0, "happy eyes, no settle yet (rest's stretch has the first second)");
    run(0.6f);
    CHECK(peak > 0.09f, "the one bright settle (%+.2f)", peak);
    run(0.5f);
    CHECK(tell_active() == TELL_NONE, "over");
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(eyes_seen == 0, "the second raise: nothing");
    tell_begin(); reset_in(); night(450, 0, 10, 130); in.sit_min = 20;
    in.cue = TELL_CUE_RAISE; run(1.5f);
    CHECK(eyes_seen == 0, "the same night 130 min later is not the morning: nothing");

    printf("-- the knock's arrival: the same answer, after the rise\n");
    tell_begin(); reset_in(); night(300, 4, 40, 0);
    in.cue = TELL_CUE_ARRIVAL; run(0.6f);
    CHECK(tell_active() == TELL_NIGHT && eyes_seen == 0, "waiting the arrival's rise (0.7 s)");
    run(0.5f);
    CHECK(eyes_seen & (1 << GEL_EYE_DROOPY), "then droopy");
    printf("-- an empty glass, a dozing wrist: no answer\n");
    tell_begin(); reset_in(); night(300, 4, 40, 0); in.holding = false;
    in.cue = TELL_CUE_RAISE; run(1.5f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "empty: nothing (and the fact is kept for later)");
    in.holding = true; in.cue = TELL_CUE_RAISE; run(1.5f);
    CHECK(eyes_seen & (1 << GEL_EYE_DROOPY), "holding again: told now");
    tell_begin(); reset_in(); night(300, 4, 40, 0); in.dozing = true;
    in.cue = TELL_CUE_RAISE; run(1.5f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "a stir: nothing");

    printf("-- THE STEP BOB: a walk of 20 steps at 2 Hz\n");
    tell_begin(); reset_in(); logs = 0; last_log[0] = 0;
    int bobs_early = 0, bobs_late = 0;
    for (int s = 1; s <= 20; s++) {
        in.step_seq++;
        bool before = tell_walking();
        run(0.5f);
        if (s == 5) CHECK(!before && !tell_walking(), "5 steps: not a walk yet");
        if (s == 6) CHECK(tell_walking() && strstr(last_log, "bob: walking"), "the 6th step within 6 s: `bob: walking`");
        if (s <= 20 && s * 0.5f <= 10.0f) { if (peak > 0.05f) bobs_early++; }
        else if (peak > 0.05f) bobs_late++;
    }
    CHECK(bobs_early == 15 && bobs_late == 0, "pulses on steps 6..20 in the first 10 s (%d), none after (%d)", bobs_early, bobs_late);
    run(0.3f);
    for (int s = 21; s <= 30; s++) { in.step_seq++; run(0.5f); if (peak > 0.05f) bobs_late++; }
    CHECK(bobs_late == 0 && tell_walking(), "10 more steps past the 10 s: no bob, still the walk (the breath's job now)");
    run(61.0f);
    CHECK(!tell_walking(), "60 s without a step: the walk is over");
    CHECK(tell_lump() == 0, "30 steps: no lump (a walk under 300 steps is not a meal)");
    printf("-- THE WALK AS A LUMP: a walk of 320 steps at 2 Hz, then the gap\n");
    for (int s = 0; s < 320; s++) { in.step_seq++; run(0.5f); }
    CHECK(tell_walking() && tell_lump() == 0, "walking, 320 steps: no lump yet (the walk is not over)");
    logs = 0; last_log[0] = 0;
    run(61.0f);
    CHECK(!tell_walking() && strstr(last_log, "the walk is over (320 steps): a lump"), "the gap: the walk is over -- `%s`", last_log);
    float l0 = tell_lump();
    CHECK(l0 > 0.32f && l0 <= 0.35f, "he ate the walk: a lump of 0.35 (%.3f, sinking already)", l0);
    run(600.0f);
    float l10 = tell_lump();
    CHECK(l10 > 0.06f && l10 < 0.11f, "10 min on: ~0.09 (the feed's ease-out, at the walk's size: %.3f)", l10);
    run(600.0f);
    CHECK(tell_lump() == 0, "20 min on: gone (%.3f)", tell_lump());
    printf("-- a second walk of 300 steps: the lump replaces, never adds\n");
    for (int s = 0; s < 300; s++) { in.step_seq++; run(0.5f); }
    run(61.0f);
    CHECK(tell_lump() > 0.32f && tell_lump() <= 0.35f, "the second walk's lump (%.3f)", tell_lump());
    run(300.0f);
    float sunk = tell_lump();
    for (int s = 0; s < 300; s++) { in.step_seq++; run(0.5f); }
    run(61.0f);
    CHECK(tell_lump() > sunk && tell_lump() <= 0.35f, "a third, while the second sinks: back to 0.35, not over (%.3f)", tell_lump());
    run(1300.0f);
    CHECK(tell_lump() == 0, "and gone");
    printf("-- a fidget of 3 steps, then 3 more 20 s later: never a walk\n");
    for (int s = 0; s < 3; s++) { in.step_seq++; run(0.5f); }
    run(20.0f);
    int bobs = 0;
    for (int s = 0; s < 3; s++) { in.step_seq++; run(0.5f); if (peak > 0.05f) bobs++; }
    CHECK(!tell_walking() && bobs == 0, "no walk, no bob");
    printf("-- asleep (a sleepwalk?) and on an empty glass: no bob\n");
    run(61.0f); in.asleep = true; in.dozing = true; bobs = 0;
    for (int s = 0; s < 12; s++) { in.step_seq++; run(0.5f); if (peak > 0.05f) bobs++; }
    CHECK(bobs == 0, "asleep: none (%d)", bobs);
    in.asleep = false; in.dozing = false; in.holding = false; run(61.0f); bobs = 0;
    for (int s = 0; s < 12; s++) { in.step_seq++; run(0.5f); if (peak > 0.05f) bobs++; }
    CHECK(bobs == 0, "empty: none (%d)", bobs);
    in.holding = true;
    printf("-- the pulse's shape: +0.06 over 120 ms\n");
    run(61.0f);
    for (int s = 0; s < 6; s++) { in.step_seq++; run(0.5f); }
    in.step_seq++; run(0.06f); float mid = o->squash; run(0.1f);
    CHECK(mid > 0.055f && o->squash == 0, "peak %+.3f at 60 ms, gone by 160 ms", mid);

    printf("-- THE NIGHT LIGHT: dozing at 03:12, a raise\n");
    tell_begin(); reset_in(); in.dozing = true; in.asleep = true; in.night = true; in.minute = 3 * 60 + 12;
    in.cue = TELL_CUE_RAISE; run(2.9f);
    CHECK(night_frames == (int)(2.9f / 0.025f + 0.5f) && done_n == 0, "the light on for the window (%d frames)", night_frames);
    CHECK(strstr(last_log, "night light: a raise at 03:12") != NULL, "logged: %s", last_log);
    run(0.2f);
    CHECK(done_n == 1 && !o->night_light, "3 s: out, once (%s)", last_log);
    run(1.0f);
    CHECK(done_n == 0 && !o->night_light, "and stays out");
    printf("-- by day (a nap), a raise: no light\n");
    tell_begin(); reset_in(); in.dozing = true; in.asleep = true; in.night = false; in.minute = 14 * 60;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(night_frames == 0, "no light");
    printf("-- awake at night, a raise: no light (the ordinary wake)\n");
    tell_begin(); reset_in(); in.night = true; in.minute = 23 * 60 + 30;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(night_frames == 0, "no light");
    printf("-- the light, then he gets up (a walk ends the span): the light yields\n");
    tell_begin(); reset_in(); in.dozing = true; in.asleep = true; in.night = true; in.minute = 3 * 60;
    in.cue = TELL_CUE_RAISE; run(1.0f); in.dozing = false; in.asleep = false; run(0.5f);
    CHECK(done_n == 0 && !o->night_light && strstr(last_log, "he is up"), "no go-dark: %s", last_log);
    printf("-- the light on an empty glass: still the light (the hour is the point)\n");
    tell_begin(); reset_in(); in.holding = false; in.dozing = true; in.asleep = true; in.night = true;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(night_frames > 0, "on");

    printf("-- THE YAWN AT DUSK: 22:20, the quiet hours at 23 (dusk), a raise with nothing to tell\n");
    tell_begin(); reset_in(); in.minute = 22 * 60 + 20; in.dusk = true;
    in.cue = TELL_CUE_RAISE; run(0.3f);
    CHECK(tell_active() == TELL_YAWN && eyes_seen == 0 && peak == 0, "the yawn, after the wake's ease (0.3 s): nothing yet");
    CHECK(strstr(last_log, "the yawn at dusk (22:20") != NULL, "logged: %s", last_log);
    run(0.3f);
    CHECK(eyes_seen == (1 << GEL_EYE_SLEEP) && peak == 0, "the sleep bar for a beat, the body still");
    run(0.55f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) && peak < -0.18f && peak > -0.21f, "the tall intake (%+.2f, under the stretch's -0.30), the lids droopy", peak);
    run(1.2f);
    CHECK(o->squash > 0.05f && (eyes_seen & (1 << GEL_EYE_DROOPY)) && !(eyes_seen & (1 << GEL_EYE_SLEEP)), "the slow slump past rest (%+.3f at its end)", o->squash);
    run(0.7f);
    CHECK(tell_active() == TELL_NONE && o->squash == 0 && o->eye_kind < 0, "the ordinary face after (%.1f s in all)", 0.3f + 0.35f + 0.5f + 1.2f + 0.6f);
    tell_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "said the yawn") && strstr(st, "dusk yes, yawned this evening"), "`tell`: the yawn said this evening");
    in.minute += 10; in.rows += 10;
    in.cue = TELL_CUE_RAISE; run(3.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0 && peak == 0, "a second raise at dusk: nothing (once an evening)");
    printf("-- the next evening: the yawn again; by day (not dusk) never\n");
    in.date = 20260911; in.minute = 22 * 60 + 5;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_YAWN, "a new date: the yawn again");
    run(3.0f);
    tell_begin(); reset_in(); in.minute = 15 * 60; in.dusk = false;
    in.cue = TELL_CUE_RAISE; run(3.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0 && peak == 0, "15:00, not dusk: nothing");
    printf("-- a fact untold at dusk: the fact first, the yawn on the next raise\n");
    tell_begin(); reset_in(); in.minute = 22 * 60 + 30; in.dusk = true; night(300, 4, 40, 0);
    in.cue = TELL_CUE_RAISE; run(0.5f);
    CHECK(tell_active() == TELL_NIGHT, "droopy first (the restless night)");
    run(2.0f);
    in.rows += 5; in.sit_min += 5;
    in.cue = TELL_CUE_RAISE; run(0.5f);
    CHECK(tell_active() == TELL_YAWN, "the next raise: the yawn");
    run(3.0f);
    printf("-- dozing at dusk (a nap that ran late), or the empty glass: no yawn\n");
    tell_begin(); reset_in(); in.minute = 22 * 60 + 30; in.dusk = true; in.dozing = true;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE, "dozing: nothing");
    tell_begin(); reset_in(); in.minute = 22 * 60 + 30; in.dusk = true; in.holding = false;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE, "empty: nothing");
    in.holding = true;

    printf("-- UNDER THE WEATHER: resting 71 vs the week's 63, a raise: droopy once today, the log says the numbers\n");
    tell_begin(); reset_in(); in.weather_hr = 71; in.weather_norm = 63;
    in.cue = TELL_CUE_RAISE; run(0.2f);
    CHECK(tell_active() == TELL_WEATHER && o->eye_kind < 0, "the answer waits the wake's ease: nothing yet");
    run(0.9f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) && peak > 0.09f, "droopy lids and the low settle (peak %+.2f): the restless night's face", peak);
    CHECK(strstr(last_log, "under the weather (resting 71 vs the week's 63)") && strstr(last_log, "droopy first face"), "the log: %s", last_log);
    run(1.5f);
    CHECK(tell_active() == TELL_NONE, "then the ordinary creature");
    in.rows += 30; in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "a second raise the same day: nothing (once a day)");
    tell_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "weather: under it, worn today") != NULL, "`tell` says it was worn");
    in.date = 20260911; in.rows += 1440; in.cue = TELL_CUE_RAISE; run(1.2f);
    CHECK((eyes_seen & (1 << GEL_EYE_DROOPY)) != 0, "the next day, still under it: droopy again");
    printf("-- a restless night AND under the weather: the night first (fresher), the weather on the next raise\n");
    tell_begin(); reset_in(); in.weather_hr = 71; in.weather_norm = 63; night(300, 4, 40, 0);
    in.cue = TELL_CUE_RAISE; run(0.5f);
    CHECK(tell_active() == TELL_NIGHT, "the restless night first");
    run(2.0f); in.rows += 5; in.sit_min += 5;
    in.cue = TELL_CUE_RAISE; run(0.5f);
    CHECK(tell_active() == TELL_WEATHER, "the next raise: under the weather");
    run(2.0f);
    printf("-- fine (0 / 0), dozing, or the empty glass: nothing\n");
    tell_begin(); reset_in(); in.weather_hr = 0; in.weather_norm = 0;
    in.cue = TELL_CUE_RAISE; run(1.5f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "fine: nothing");
    tell_begin(); reset_in(); in.weather_hr = 71; in.weather_norm = 63; in.dozing = true;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE, "dozing: nothing");
    tell_begin(); reset_in(); in.weather_hr = 71; in.weather_norm = 63; in.holding = false;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NONE, "empty: nothing");
    in.holding = true;
    printf("-- HE ARRIVES WITH THE OTHER HOUSE'S DAY: a three-hour desk session on the arrival -> heavy lids, 3 s (1.5 s x 2, the cap)\n");
    tell_begin(); reset_in(); in.desk_sat_min = 180; in.desk_pets = 9;
    in.cue = TELL_CUE_ARRIVAL; run(0.6f);
    CHECK(tell_active() == TELL_ARRIVE && o->eye_kind < 0, "the answer waits the rise (0.7 s): the desk's day picked, nothing shown yet");
    run(0.5f);
    CHECK(eyes_seen == (1 << GEL_EYE_DROOPY) && o->squash > 0 && tell_active() == TELL_ARRIVE, "heavy lids and the slow low settle (the sit outranks the pets)");
    run(2.0f);
    CHECK(tell_active() == TELL_ARRIVE && (eyes_seen & (1 << GEL_EYE_DROOPY)), "still droopy at 2.4 s into it (the session's length, x2)");
    run(0.8f);
    CHECK(tell_active() == TELL_NONE && o->eye_kind < 0, "3 s of lids, then the ordinary creature");
    tell_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "said the other house's day") && strstr(st, "sat 180 min, pets 9"), "`tell`: the other house's day said, the two numbers named");
    in.cue = TELL_CUE_RAISE; run(2.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0 && peak == 0, "a raise after: nothing (the desk's day is the arrival's alone)");
    printf("-- 90 min exactly: 1.5 s of lids; 89: nothing\n");
    tell_begin(); reset_in(); in.desk_sat_min = 90;
    in.cue = TELL_CUE_ARRIVAL; run(0.8f); run(1.3f);
    CHECK(tell_active() == TELL_ARRIVE && (eyes_seen & (1 << GEL_EYE_DROOPY)), "90 min: droopy at 1.4 s in");
    run(0.3f);
    CHECK(tell_active() == TELL_NONE, "...and done by 1.7 s");
    tell_begin(); reset_in(); in.desk_sat_min = 89; in.desk_pets = 4;
    in.cue = TELL_CUE_ARRIVAL; run(3.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0 && peak == 0, "89 min and 4 pets: under both lines, nothing");
    printf("-- a petted afternoon (7 pets, a short sit): the content face with one soft settle, 1.2 s\n");
    tell_begin(); reset_in(); in.desk_sat_min = 20; in.desk_pets = 7;
    in.cue = TELL_CUE_ARRIVAL; run(0.8f); run(0.6f);
    CHECK(tell_active() == TELL_ARRIVE && eyes_seen == (1 << GEL_EYE_CONTENT) && peak > 0.05f && peak <= 0.08f, "content eyes, the bob's one settle (peak %+.2f)", peak);
    run(0.7f);
    CHECK(tell_active() == TELL_NONE, "1.2 s, then the ordinary creature");
    printf("-- the wrist's own untold fact (a restless night) under a long session: the desk's day first, the night on the next raise\n");
    tell_begin(); reset_in(); night(300, 4, 40, 0); in.desk_sat_min = 120;
    in.cue = TELL_CUE_ARRIVAL; run(1.0f);
    CHECK(tell_active() == TELL_ARRIVE, "the arrival: the other house's day (age 0 beats the night)");
    run(2.5f);
    in.rows += 5; in.sit_min += 5; in.desk_sat_min = 0;
    in.cue = TELL_CUE_RAISE; run(1.0f);
    CHECK(tell_active() == TELL_NIGHT, "the next raise: the restless night, still untold");
    run(3.0f);
    printf("-- a raise with the numbers on the line: never (the arrival is the once)\n");
    tell_begin(); reset_in(); in.desk_sat_min = 200; in.desk_pets = 20;
    in.cue = TELL_CUE_RAISE; run(3.0f);
    CHECK(tell_active() == TELL_NONE && eyes_seen == 0, "a raise: nothing of the desk's day");
    printf("-- the empty glass, or dozing: nothing\n");
    tell_begin(); reset_in(); in.desk_sat_min = 200; in.holding = false;
    in.cue = TELL_CUE_ARRIVAL; run(2.0f);
    CHECK(tell_active() == TELL_NONE, "empty: nothing");
    in.holding = true;

    tell_status(st, sizeof st); printf("       %s\n", st);
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
