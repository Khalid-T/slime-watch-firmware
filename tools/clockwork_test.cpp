/* CLOCKWORK on the laptop (clockwork.cpp has no Arduino in it; face.cpp
 * lends the arc, gel.c the palette; the stub is there for the pattern):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/clockwork_test tools/clockwork_test.cpp src/clockwork.cpp src/face.cpp src/theme.cpp -x c src/gel.c src/gentle.c -lm && /tmp/clockwork_test
 * The hold's window (0.8-1.4 s; not a 0.5 s hold, not the whistle's);
 * the stopwatch's start and stop times and the breath held between
 * (amplitude 0, the intake, wide eyes); the arc's fraction once a
 * minute; the exhale and the final time's 6 s; the timer from the
 * final, its countdown, its single fire (three bounces, three kicks,
 * one HOP3, never again); `timer` by hand; the alarm at the minute,
 * once, not the next day's same minute until the date turns; the
 * dismissals; the sleep guardian (the timer waits, the alarm bends
 * the rule and breaks the nap); nothing on the empty glass but the
 * alarm's light; THE LONG STARE (ruling 27: wide for the first minute,
 * one blink, the ordinary face, a lap's and the stop's wide second);
 * THE ZERO HE SLEPT THROUGH (ruling 26: the ring's minute held through
 * his sleep, the bounces paid on his first lit frame up, a tap before
 * that forgives); THE GENTLE WAKE (D35): the alarm early inside its 20
 min when stirring, dozing and the pulse risen over the night's floor,
 its own theatre, its minute consumed, once per alarm, never without
 one, the ordinary ring untouched. Writes /tmp/clockwork_*.ppm to look at. */
#include "../src/clockwork.h"
#include "../src/gel.h"
#include "../src/face.h"
#include "../src/haptic.h"
#include "../src/gentle.h"      /* THE GENTLE WAKE: the test feeds the pulse and the stir itself */
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now_ms = 10000;
static int      minute = 9 * 60;          /* 09:00 */
static uint32_t date = 20260910;
static bool     lit = true, holding = true, asleep = false;
static const clockwork_out_t *o;
/* what a run of frames saw */
static int   kicks = 0, hop3 = 0, bumps = 0;
static float max_squash = 0, min_squash = 0, min_light = 1;
static int   wide_frames = 0, content_frames = 0, held_frames = 0, keep_frames = 0, closed_frames = 0, plain_frames = 0;
static char  last_log[120] = "";           /* the last log line run() took */

static void reset_seen(void)
{
    kicks = hop3 = bumps = 0; max_squash = min_squash = 0; min_light = 1;
    wide_frames = content_frames = held_frames = keep_frames = closed_frames = plain_frames = 0;
}

/* frames of 20 ms; the minute of the day follows the clock */
static void run(float seconds)
{
    int n = (int)(seconds / 0.02f + 0.5f);
    for (int i = 0; i < n; i++) {
        now_ms += 20;
        static uint32_t last_min_ms = 10000;
        while (now_ms - last_min_ms >= 60000) { last_min_ms += 60000; minute = (minute + 1) % 1440; if (minute == 0) date++; }
        clockwork_in_t in = { 0.02f, now_ms, minute, date, lit, holding, asleep };
        o = clockwork_tick(&in);
        if (o->log_new) { printf("       log: %s\n", o->log); snprintf(last_log, sizeof last_log, "%s", o->log); clockwork_log_taken(); }
        if (o->kick2 != 0) kicks++;
        if (o->haptic == HAPTIC_HOP3) hop3++;
        if (o->haptic == HAPTIC_BUMP) bumps++;
        if (o->squash > max_squash) max_squash = o->squash;
        if (o->squash < min_squash) min_squash = o->squash;
        if (o->light < min_light) min_light = o->light;
        if (o->eye_kind == GEL_EYE_WIDE) wide_frames++;
        if (o->eye_kind == GEL_EYE_CONTENT) content_frames++;
        if (o->eye_kind == GEL_EYE_CLOSED) closed_frames++;
        if (o->eye_kind < 0) plain_frames++;
        if (o->breath_k == 0) held_frames++;
        if (o->keep_awake) keep_frames++;
    }
}

static uint16_t fb[FACE_W * FACE_H];
static int lit_px(void) { int n = 0; for (int i = 0; i < FACE_W * FACE_H; i++) if (fb[i]) n++; return n; }
/* the ring band's lit pixels: r in [116, 118) */
static int ring_px(void)
{
    int n = 0;
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        float dx = x + 0.5f - 120, dy = y + 0.5f - 120, r2 = dx * dx + dy * dy;
        if (r2 >= 116 * 116 && r2 < 118 * 118 && fb[y * FACE_W + x]) n++;
    }
    return n;
}
/* the text's box: the glance's spot, 70 px above the centre */
static int text_px(void) { int n = 0; for (int y = 35; y < 65; y++) for (int x = 80; x < 160; x++) if (fb[y * FACE_W + x]) n++; return n; }
static void draw(const char *name)
{
    memset(fb, 0, sizeof fb);
    clockwork_draw(fb, now_ms, 2 /* GEL_WALL_TOP: the hour's spot */);
    char path[64]; snprintf(path, sizeof path, "/tmp/clockwork_%s.ppm", name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", FACE_W, FACE_H);
    for (int i = 0; i < FACE_W * FACE_H; i++) {
        uint16_t v = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        unsigned char px[3] = { (unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63), (unsigned char)((v & 31) * 255 / 31) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

int main(void)
{
    char st[240];
    clockwork_begin(-1);
    run(0.1f);

    printf("the hold's window\n");
    CHECK(!clockwork_hold(450, now_ms), "a 0.45 s hold is not the stopwatch (under the window: 0.5-1.9 s since THE HOLD'S WINDOW, WIDER)");
    CHECK(!clockwork_hold(1950, now_ms), "a 1.95 s hold is not the stopwatch (past the window; the peekaboo's edge)");
    CHECK(clockwork_hold(600, now_ms), "a 0.6 s hold is the stopwatch (the wider window)");
    clockwork_begin(-1); run(0.1f);                                     /* and off again: the checks below start from nothing */
    CHECK(clockwork_sw_state() == CW_SW_OFF, "nothing started");
    memset(fb, 0, sizeof fb); clockwork_draw(fb, now_ms, 2 /* GEL_WALL_TOP: the hour's spot */);
    CHECK(lit_px() == 0, "nothing drawn while nothing runs");

    printf("the stopwatch: a hold starts it, the breath held\n");
    reset_seen();
    CHECK(clockwork_hold(1000, now_ms), "a 1.0 s hold starts it");
    uint32_t t0 = now_ms;
    run(2.0f);
    CHECK(clockwork_sw_state() == CW_SW_RUN, "running");
    CHECK(bumps == 1, "one BUMP on the start (%d)", bumps);
    CHECK(held_frames == 100, "the breath held every frame (%d of 100)", held_frames);
    CHECK(fabsf(min_squash + 0.08f) < 1e-4f && max_squash == 0, "the intake: squash %.2f held", min_squash);
    CHECK(wide_frames == 100, "eyes wide throughout (%d)", wide_frames);
    CHECK(clockwork_elapsed_ms(now_ms) == now_ms - t0, "elapsed %lu ms", (unsigned long)clockwork_elapsed_ms(now_ms));
    CHECK(clockwork_showing() && clockwork_busy(), "showing, busy (the glance yields, idle life waits)");
    draw("run_2s");
    int a2 = ring_px(), tx = text_px();
    CHECK(tx > 0, "the elapsed drawn at the top (%d px)", tx);
    printf("LAPS ON THE ARC (B16): a tap while it counts is a lap, a tick on the arc; twelve at most; the count goes on\n");
    const uint32_t *lp = NULL;
    CHECK(clockwork_laps(&lp) == 0, "no laps yet");
    CHECK(clockwork_tap(now_ms), "a tap while it counts is taken (a lap, not the pet)");
    CHECK(clockwork_laps(&lp) == 1 && lp && lp[0] == now_ms - t0, "one lap at %lu ms (the split)", (unsigned long)lp[0]);
    CHECK(clockwork_sw_state() == CW_SW_RUN && held_frames == 100, "the count goes on, the breath still held");
    run(1.0f);
    CHECK(clockwork_tap(now_ms) && clockwork_laps(&lp) == 2 && lp[1] == now_ms - t0 && lp[1] > lp[0], "a second lap a second later");
    draw("run_laps");
    {   /* the ticks sit on the ring band at their angle, in the body colour
         * (the arc is the shine): lap 1 at 2 s = 12 deg past 12 o'clock, lap
         * 2 at 3 s = 18 deg -- up and a little right, r 112..118 */
        int n = 0, body = 0;
        for (int y = 0; y < 24; y++) for (int x = 120; x < 165; x++) {
            float dx = x + 0.5f - 120, dy = y + 0.5f - 120, r = sqrtf(dx * dx + dy * dy);
            if (fb[y * FACE_W + x] && r >= 112.0f && r < 118.5f) { n++; if (fb[y * FACE_W + x] != fb[1 * FACE_W + 120]) body++; }
        }
        CHECK(n >= 12 && body >= 12, "the two laps' ticks in the ring band past 12 o'clock, darker than the arc (%d px, %d not the arc's ink)", n, body);
    }
    for (int i = 0; i < 11; i++) { run(0.2f); clockwork_tap(now_ms); }
    CHECK(clockwork_laps(&lp) == 12, "twelve laps at most: the thirteenth tap left no tick (%d)", clockwork_laps(&lp));
    CHECK(clockwork_sw_state() == CW_SW_RUN, "still counting");
    run(28.0f - 1.0f - 2.2f);                               /* 30 s in */
    float f30 = clockwork_arc(now_ms);
    CHECK(fabsf(f30 - 0.5f) < 0.01f, "the arc at 30 s: %.2f of the turn", f30);
    draw("run_30s");
    int a30 = ring_px();
    CHECK(a30 > a2 * 10 && a30 > 300, "the arc grown: %d px at 2 s, %d at 30 s", a2, a30);
    reset_seen();
    run(29.9f);                                             /* 59.9 s */
    CHECK(wide_frames == 1495 && plain_frames == 0 && closed_frames == 0, "THE LONG STARE: wide every frame to 59.9 s (%d)", wide_frames);
    draw("run_59s");
    int a59 = ring_px();
    reset_seen();
    run(0.3f);                                              /* past the minute: the stare softens -- one blink, the ordinary face */
    CHECK(closed_frames >= 5 && closed_frames <= 7 && wide_frames <= 5 && plain_frames >= 3 && held_frames == 15,
          "the softening at the minute: %d wide, %d closed (the blink, 0.12 s), %d the ordinary face; the breath held throughout", wide_frames, closed_frames, plain_frames);
    CHECK(strstr(last_log, "the stare softens") != NULL, "the log: %s", last_log);
    clockwork_status(st, sizeof st);
    CHECK(strstr(st, "the stare softened") != NULL, "`clockwork` says the stare softened");
    draw("run_60s");
    int a60 = ring_px();
    CHECK(a59 > a30 && a60 < a30 / 4, "the arc full at 59.9 s (%d px), begun again past the minute (%d px)", a59, a60);
    CHECK(clockwork_arc(now_ms) < 0.01f, "the fraction restarts each minute: %.3f", clockwork_arc(now_ms));

    printf("the stop (a tap then a hold): the exhale, the final time lit 6 s, the laps with it\n");
    run(4.0f);                                              /* ~1:04 */
    reset_seen();
    CHECK(clockwork_hold(1000, now_ms), "a tap then a hold stops it");
    uint32_t fin = clockwork_elapsed_ms(now_ms);
    CHECK(clockwork_laps(&lp) == 12, "the laps stay with the final time");
    CHECK(clockwork_sw_state() == CW_SW_FINAL, "the final time (%lu ms)", (unsigned long)fin);
    CHECK(fin == now_ms - t0, "the final is start to stop exactly");
    run(0.02f);
    CHECK(bumps == 1, "one BUMP on the stop");
    CHECK(fabsf(o->squash - 0.15f) < 0.02f, "the exhale's settle +%.2f at the stop", o->squash);
    CHECK(o->breath_k == 1.0f, "the breath is back");
    run(0.75f);
    CHECK(o->squash > 0.02f && o->squash < 0.13f, "half way through the exhale: %.3f (easing out)", o->squash);
    run(0.8f);
    CHECK(o->squash == 0, "the exhale done by 1.5 s");
    CHECK(wide_frames >= 49 && wide_frames <= 51 && content_frames >= 24 && content_frames <= 26,
          "THE LONG STARE's stop (the count past the minute): wide a second (%d frames), then content for the exhale's rest (%d)", wide_frames, content_frames);
    CHECK(clockwork_elapsed_ms(now_ms) == fin, "the final time holds while lit");
    draw("final");
    CHECK(text_px() > 0 && ring_px() > 0, "the final time and its frozen arc drawn");
    CHECK(!clockwork_tap(now_ms), "a tap on the final time is not taken (the pet)");
    run(4.6f);                                              /* 6.15 s after the stop */
    CHECK(clockwork_sw_state() == CW_SW_OFF, "gone after 6 s");
    CHECK(clockwork_laps(&lp) == 0, "the laps gone with it");
    draw("gone");
    CHECK(lit_px() == 0, "nothing drawn after (the laps' ticks included)");
    clockwork_hold(1000, now_ms); run(0.5f); clockwork_tap(now_ms); run(0.5f);
    CHECK(clockwork_laps(&lp) == 1, "a new count, one lap");
    clockwork_hold(1000, now_ms); run(6.5f);
    clockwork_hold(1000, now_ms); run(0.1f);
    CHECK(clockwork_sw_state() == CW_SW_RUN && clockwork_laps(&lp) == 0, "a new start clears the laps");
    clockwork_hold(1000, now_ms); run(6.5f);

    printf("THE LONG STARE (ruling 27): a short count is wide to the stop and content through the exhale, as built; a long one softens, a lap widens it a second, the stop a second before the content\n");
    reset_seen();
    clockwork_hold(1000, now_ms); run(5.0f);
    CHECK(wide_frames == 250, "a short count: wide every frame (%d of 250)", wide_frames);
    reset_seen();
    clockwork_hold(1000, now_ms); run(1.6f);
    CHECK(wide_frames == 0 && content_frames >= 74 && content_frames <= 76, "its stop: content through the exhale, no wide second (%d wide, %d content)", wide_frames, content_frames);
    run(6.0f);
    CHECK(clockwork_sw_state() == CW_SW_OFF, "gone");
    clockwork_hold(1000, now_ms); run(61.0f); clockwork_log_taken();
    reset_seen();
    run(2.0f);
    CHECK(plain_frames == 100 && wide_frames == 0, "a minute in: the ordinary face (%d of 100 frames), the breath still held (%d)", plain_frames, held_frames);
    reset_seen();
    CHECK(clockwork_tap(now_ms), "a lap");
    run(1.5f);
    CHECK(wide_frames >= 49 && wide_frames <= 51 && closed_frames >= 5 && closed_frames <= 7 && plain_frames >= 18,
          "the lap: wide again for a second (%d frames), the blink (%d), the ordinary face after (%d)", wide_frames, closed_frames, plain_frames);
    CHECK(strstr(last_log, "the eyes wide again for a second") != NULL, "the log: %s", last_log);
    reset_seen();
    clockwork_tap(now_ms); run(0.5f); clockwork_tap(now_ms); run(1.5f);
    CHECK(wide_frames >= 74 && wide_frames <= 76, "two laps a half second apart: wide from the first to a second past the second (%d frames)", wide_frames);
    reset_seen();
    clockwork_hold(1000, now_ms); run(1.6f);
    CHECK(wide_frames >= 49 && wide_frames <= 51 && content_frames >= 24 && content_frames <= 26 && max_squash > 0.14f,
          "the stop: wide a second over the exhale's start, content for its rest, the settle untouched (%d wide, %d content, +%.2f)", wide_frames, content_frames, max_squash);
    run(6.0f);
    CHECK(clockwork_sw_state() == CW_SW_OFF, "gone");
    clockwork_log_taken();

    printf("the timer from the final time\n");
    clockwork_hold(1000, now_ms); run(1.0f); clockwork_hold(1000, now_ms);
    uint32_t dur = clockwork_elapsed_ms(now_ms);
    run(1.0f);
    reset_seen();
    CHECK(clockwork_hold(1200, now_ms), "a hold while the final shows sets the timer");
    CHECK(clockwork_tm_state() == CW_TM_RUN && clockwork_sw_state() == CW_SW_OFF, "the timer runs, the stopwatch is put away");
    CHECK(clockwork_remaining_ms(now_ms) == (dur < 1000 ? 1000 : dur), "%lu ms to go (the final's length)", (unsigned long)clockwork_remaining_ms(now_ms));
    CHECK(clockwork_arc(now_ms) > 0.99f, "the arc full at the start: %.2f", clockwork_arc(now_ms));
    CHECK(!clockwork_tap(now_ms), "a tap while it counts down is the pet, the timer keeps counting");
    CHECK(!clockwork_hold(1000, now_ms), "a hold while it counts down: nothing");
    run(0.5f);
    CHECK(clockwork_arc(now_ms) < 0.6f && clockwork_arc(now_ms) > 0.3f, "shrinking: %.2f", clockwork_arc(now_ms));
    run(0.7f);
    CHECK(clockwork_tm_state() == CW_TM_RING && clockwork_ring() == CW_RING_TIMER, "at zero: the ring");
    run(1.2f);                                              /* the three bounces, 1.05 s */
    CHECK(hop3 == 1, "one HOP3 (%d)", hop3);
    CHECK(kicks == 3, "three landing kicks (%d)", kicks);
    CHECK(max_squash > 0.19f, "the bounces' pulse +%.2f", max_squash);
    CHECK(wide_frames > 40, "eyes wide through the bounces (%d frames)", wide_frames);
    CHECK(min_light == 1.0f, "HOLDING: the glass's light untouched (the gel bounces)");
    CHECK(clockwork_arc(now_ms) == 0, "the arc empty at zero");
    draw("timer_zero");
    CHECK(text_px() > 0, "0:00 drawn until dismissed");
    reset_seen();
    run(5.0f);
    CHECK(hop3 == 0 && kicks == 0 && max_squash == 0, "never a repeat");
    CHECK(keep_frames == 250, "the ring keeps the glass lit (%d frames)", keep_frames);
    CHECK(clockwork_tap(now_ms), "a tap dismisses");
    CHECK(clockwork_ring() == CW_RING_NONE && clockwork_tm_state() == CW_TM_OFF, "the ring gone, the timer off");
    run(0.1f);
    draw("dismissed");
    CHECK(lit_px() == 0, "nothing drawn after the dismissal");

    printf("a timer by hand, undismissed: clears itself\n");
    clockwork_set_timer(90000, now_ms, "the test");
    run(0.1f);
    CHECK(clockwork_remaining_ms(now_ms) > 89000 && clockwork_remaining_ms(now_ms) <= 90000, "1:30 to go");
    run(90.0f);
    CHECK(clockwork_ring() == CW_RING_TIMER, "at zero");
    reset_seen();
    run(20.0f);
    CHECK(keep_frames == 0 || keep_frames < 1000, "the awake hold ends after 15 s (%d frames of 1000)", keep_frames);
    run(41.0f);
    CHECK(clockwork_ring() == CW_RING_NONE, "cleared itself after a minute, no nag");
    clockwork_set_timer(60000, now_ms, "the test"); run(0.1f);
    clockwork_set_timer(0, now_ms, "the test"); run(0.1f);
    CHECK(clockwork_tm_state() == CW_TM_OFF, "`timer off`");

    printf("the sleep guardian: a timer's zero waits for him, the alarm does not\n");
    asleep = true;
    clockwork_set_timer(2000, now_ms, "the test");
    run(2.5f);
    reset_seen();
    CHECK(clockwork_ring() == CW_RING_TIMER, "the timer at zero while asleep");
    CHECK(!clockwork_due(now_ms, minute, date), "does not break the nap");
    run(3.0f);
    CHECK(hop3 == 0 && kicks == 0, "no bounces while he sleeps");
    asleep = false;
    run(1.5f);
    CHECK(hop3 == 1 && kicks == 3, "the bounces once he is up (%d, %d)", hop3, kicks);
    clockwork_tap(now_ms); run(0.1f);
    printf("THE ZERO HE SLEPT THROUGH (ruling 26): the ring's minute does not run while he sleeps; 0:00 shows; the bounces are paid on his first lit frame up, the minute from then; a tap before that forgives\n");
    asleep = true; lit = true;
    clockwork_set_timer(2000, now_ms, "the test"); run(2.5f); clockwork_log_taken();
    reset_seen();
    run(200.0f);                                            /* three minutes and more asleep */
    CHECK(clockwork_ring() == CW_RING_TIMER && clockwork_tm_state() == CW_TM_RING, "still at zero after 200 s asleep (the minute held)");
    CHECK(hop3 == 0 && kicks == 0 && wide_frames == 0, "no bounces, no wide eyes over the sleep look");
    CHECK(keep_frames == 0, "the glass not kept lit for a sleeper (%d frames)", keep_frames);
    CHECK(!clockwork_due(now_ms, minute, date), "the nap not broken");
    draw("timer_owed");
    CHECK(text_px() > 0, "0:00 drawn meanwhile");
    clockwork_status(st, sizeof st);
    CHECK(strstr(st, "the bounces OWED from his sleep") != NULL, "`clockwork` says the bounces are owed");
    lit = false; asleep = false; run(2.0f);                 /* up, but the glass dark: the bounces wait for the light (and the nap breaks for them) */
    CHECK(hop3 == 0 && clockwork_ring() == CW_RING_TIMER, "up on a dark glass: still waiting for the light");
    CHECK(!clockwork_due(now_ms, minute, date), "... and a ring already entered never wakes the nap: the bounces wait for his raise (the dark house's rule; the zero itself breaks the nap only when he is up -- above)");
    lit = true; reset_seen();
    run(1.5f);
    CHECK(hop3 == 1 && kicks == 3 && wide_frames > 40, "the first lit frame up: the three bounces, HOP3 once (%d, %d)", hop3, kicks);
    CHECK(strstr(last_log, "the zero came in his sleep 3 min 2") != NULL, "the log says how late: %s", last_log);
    CHECK(clockwork_elapsed_ms(now_ms) == 0 && clockwork_arc(now_ms) == 0, "the arc empty, as at any zero");
    CHECK(keep_frames == 75, "the glass kept lit from the paying (%d frames)", keep_frames);
    run(50.0f);
    CHECK(clockwork_ring() == CW_RING_TIMER, "the ring's minute runs from the paying: still there at 51 s");
    run(10.0f);
    CHECK(clockwork_ring() == CW_RING_NONE && clockwork_tm_state() == CW_TM_OFF, "cleared itself a minute after the paying");
    clockwork_log_taken();
    /* a tap before he is up forgives the debt */
    asleep = true; clockwork_set_timer(1000, now_ms, "the test"); run(1.5f); clockwork_log_taken();
    CHECK(clockwork_ring() == CW_RING_TIMER, "at zero asleep again");
    CHECK(clockwork_tap(now_ms), "a tap (a stir's touch) dismisses it");
    run(0.1f);
    CHECK(strstr(last_log, "the bounces owed from his sleep are forgiven") != NULL, "the log: %s", last_log);
    asleep = false; reset_seen(); run(2.0f);
    CHECK(hop3 == 0 && kicks == 0 && clockwork_ring() == CW_RING_NONE, "nothing when he is up: forgiven");
    /* the alarm is untouched by the debt: it bounces asleep, its minute runs */
    clockwork_set_alarm(minute + 1, "the test"); run(0.1f); clockwork_log_taken();
    asleep = true; reset_seen();
    while (minute != clockwork_alarm()) run(1.0f);
    run(1.5f);
    CHECK(hop3 == 1 && kicks == 3, "the alarm still bounces though asleep (%d, %d)", hop3, kicks);
    run(60.0f);
    CHECK(clockwork_ring() == CW_RING_NONE, "its minute runs through his sleep as before");
    clockwork_set_alarm(-1, "the test"); run(0.1f); clockwork_log_taken();
    asleep = false; run(0.1f);

    printf("the alarm at the minute, once\n");
    clockwork_set_alarm(minute + 2, "the test");
    run(0.1f);
    CHECK(clockwork_alarm() == minute + 2, "alarm set for %02d:%02d", (minute + 2) / 60, (minute + 2) % 60);
    int alarm_min = minute + 2;
    reset_seen();
    asleep = true;
    while (minute != alarm_min) run(1.0f);
    CHECK(clockwork_ring() == CW_RING_ALARM, "the alarm rings at the minute");
    CHECK(clockwork_due(now_ms - 1, minute, date) == false, "once fired it no longer wakes the nap");
    run(1.5f);
    CHECK(hop3 == 1 && kicks == 3, "the bounces and HOP3 though he is asleep: the rule bends (%d, %d)", hop3, kicks);
    CHECK(wide_frames > 40, "wide eyes over the sleep look");
    draw("alarm");
    CHECK(text_px() > 0, "the hour drawn at the top");
    reset_seen();
    run(58.0f);                                             /* the same minute, then the next */
    CHECK(hop3 == 0 && kicks == 0, "not again in the minute");
    CHECK(clockwork_ring() == CW_RING_NONE || clockwork_ring() == CW_RING_ALARM, "still the alarm's ring or cleared");
    clockwork_tap(now_ms); run(0.1f);
    CHECK(clockwork_ring() == CW_RING_NONE, "a tap dismisses the alarm");
    asleep = false;
    /* the next day's same minute */
    reset_seen();
    int ticks = 0;
    while (!(minute == alarm_min && clockwork_ring() == CW_RING_ALARM) && ticks < 1500) { run(60.0f); ticks++; }
    CHECK(ticks >= 1439 && ticks < 1441, "fires again the next day (%d minutes on)", ticks);
    clockwork_tap(now_ms); run(0.1f);

    printf("the nap: what breaks it\n");
    clockwork_set_alarm(minute + 1, "the test"); run(0.1f);
    CHECK(!clockwork_due(now_ms, minute, date), "not before the minute");
    CHECK(clockwork_due(now_ms, minute + 1, date), "the alarm's minute breaks the nap");
    clockwork_set_alarm(-1, "the test"); run(0.1f);
    CHECK(clockwork_alarm() < 0, "`alarm off`");
    clockwork_set_timer(1000, now_ms, "the test"); run(0.5f);
    CHECK(!clockwork_due(now_ms, minute, date), "a timer still counting: no");
    CHECK(clockwork_due(now_ms + 600, minute, date), "its zero breaks the nap when he is up");
    asleep = true; run(0.02f);
    CHECK(!clockwork_due(now_ms + 600, minute, date), "... not when he is asleep");
    asleep = false; run(1.0f); clockwork_tap(now_ms); run(0.1f);

    printf("the empty glass\n");
    holding = false; run(0.1f);
    CHECK(!clockwork_hold(1000, now_ms), "a hold on the empty glass is nothing");
    CHECK(clockwork_sw_state() == CW_SW_OFF, "no stopwatch without him");
    holding = true; clockwork_hold(1000, now_ms); run(1.0f);
    holding = false; run(0.1f);
    CHECK(clockwork_sw_state() == CW_SW_OFF, "a count in progress is dropped when he leaves");
    clockwork_set_alarm(minute + 1, "the test"); run(0.1f);
    reset_seen();
    while (minute != (alarm_min = clockwork_alarm())) run(1.0f);
    run(1.2f);
    CHECK(clockwork_ring() == CW_RING_ALARM, "the alarm rings on the empty glass");
    CHECK(min_light < 0.5f, "the glass's light pulses (down to %.2f)", min_light);
    CHECK(hop3 == 1, "HOP3 once (the watch works without him)");
    CHECK(!clockwork_showing(), "nothing in the glance's spot (the face's own hour is at the centre)");
    draw("alarm_empty");
    CHECK(text_px() == 0, "no hour drawn at the top on the empty glass");
    reset_seen();
    run(2.0f);
    CHECK(min_light == 1.0f, "the light steady after the bounces");
    clockwork_tap(now_ms); run(0.1f);
    clockwork_set_alarm(-1, "the test"); run(0.1f);

    printf("THE RIM AS THE DIAL: the finger's angle is the hour hand, 15-min steps, the next such time; the lift sets, the same drag within 30 s clears\n");
    holding = true; asleep = false;
    minute = 9 * 60; run(0.1f);                               /* 09:00 */
    CHECK(!clockwork_dial(90, 90, false, minute, now_ms) && clockwork_dial_minute() == 15 * 60 && face_dial_minute() == 15 * 60,
          "a move to 90 deg at 09:00: dialling 15:00 (3 o'clock, the afternoon's -- the morning's has passed), the face's tick follows");
    CHECK(!clockwork_dial(270, 270, false, minute, now_ms) && clockwork_dial_minute() == 21 * 60, "270 deg: 21:00 (9 o'clock: now is 09:00 exactly, so the evening's)");
    CHECK(!clockwork_dial(100, 100, false, minute, now_ms) && clockwork_dial_minute() == 15 * 60 + 15, "100 deg: 15:15 (200 min on the dial, snapped to the quarter hour: 195)");
    CHECK(!clockwork_dial(7, 7, false, minute, now_ms) && clockwork_dial_minute() == 12 * 60 + 15, "7 deg: 12:15 (14 min -> the 15-min step)");
    CHECK(!clockwork_dial(359, 359, false, minute, now_ms) && clockwork_dial_minute() == 12 * 60, "359 deg: 12:00 (the seam at 12 o'clock rounds up to 720 -> 0)");
    clockwork_status(st, sizeof st); printf("       %s\n", st);
    CHECK(strstr(st, "(dialling 12:00)") != NULL, "`clockwork` says dialling");
    CHECK(clockwork_dial(90, 90, true, minute, now_ms), "the lift at 90 deg: the alarm changes");
    CHECK(clockwork_alarm() == 15 * 60 && clockwork_dial_minute() == -1, "alarm 15:00 set, not dialling");
    CHECK(face_dial_minute() == 15 * 60, "the face's tick is blinking at 15:00 (the face's own clock lets it go)");
    run(0.1f);
    CHECK(strstr(last_log, "alarm 15:00 set by the dial (90 deg): the tick blinks twice") != NULL, "the log: %s", last_log);
    run(10.0f);
    clockwork_dial(90, 90, false, minute, now_ms);
    CHECK(clockwork_dial(90, 90, true, minute, now_ms) && clockwork_alarm() < 0, "the same drag 10 s later: cleared");
    run(0.1f);
    CHECK(strstr(last_log, "alarm 15:00 cleared by the dial (dialled to it again within 30 s): the tick fades") != NULL, "the log: %s", last_log);
    clockwork_dial(90, 90, false, minute, now_ms); clockwork_dial(90, 90, true, minute, now_ms); run(0.1f); clockwork_log_taken();
    CHECK(clockwork_alarm() == 15 * 60, "dialled again: set again");
    run(31.0f);
    clockwork_dial(90, 90, false, minute, now_ms);
    CHECK(clockwork_dial(90, 90, true, minute, now_ms) && clockwork_alarm() == 15 * 60, "the same drag 31 s later: not a clear -- set again (the window is 30 s)");
    run(0.1f); clockwork_log_taken();
    clockwork_dial(120, 120, false, minute, now_ms);
    CHECK(clockwork_dial(120, 120, true, minute, now_ms) && clockwork_alarm() == 16 * 60, "a different hour within 30 s: moved to 16:00, not cleared");
    run(0.1f); clockwork_log_taken();
    clockwork_set_alarm(16 * 60 + 30, "the test"); run(0.1f); clockwork_log_taken();
    clockwork_dial(135, 135, false, minute, now_ms);
    CHECK(clockwork_dial(135, 135, true, minute, now_ms) && clockwork_alarm() < 0, "a serial-set alarm dialled to within 30 s: cleared too (the dial reads any alarm just set)");
    run(0.1f); clockwork_log_taken();
    CHECK(!clockwork_dial(90, 90, false, -1, now_ms) && !clockwork_dial(90, 90, true, -1, now_ms) && clockwork_alarm() < 0 && face_dial_minute() == -1,
          "no clock: the dial does nothing");
    holding = false; run(0.1f);
    clockwork_dial(180, 180, false, minute, now_ms);
    CHECK(clockwork_dial(180, 180, true, minute, now_ms) && clockwork_alarm() == 15 * 60 + 0 * 0 + 3 * 60, "on the empty glass too: 18:00 set (the watch works without him)");
    run(0.1f); clockwork_log_taken();
    clockwork_set_alarm(-1, "the test"); run(0.1f); clockwork_log_taken();
    /* the alarm rings; a dial while it rings sets a new one and ends the ring */
    clockwork_set_alarm(minute + 1, "the test"); run(0.1f); clockwork_log_taken();
    while (minute != clockwork_alarm()) run(1.0f);
    run(0.5f); CHECK(clockwork_ring() == CW_RING_ALARM, "the alarm rings");
    clockwork_dial(90, 90, false, minute, now_ms); clockwork_dial(90, 90, true, minute, now_ms); run(0.1f); clockwork_log_taken();
    CHECK(clockwork_ring() == CW_RING_NONE && clockwork_alarm() == 15 * 60, "a dial while it rings: the ring ends, the new alarm set");
    clockwork_set_alarm(-1, "the test"); run(0.1f); clockwork_log_taken();

    printf("THE TIMER FROM THE RIM (B13's other half): the arc while the final time shows is the timer's dial -- a quarter turn a quarter hour, 5-min steps; the lift sets; its tick on the minute dial\n");
    holding = true; asleep = false; run(0.1f);
    clockwork_hold(1000, now_ms); run(3.0f); clockwork_hold(1000, now_ms); run(0.1f);   /* a 3 s count, stopped: the final shows */
    CHECK(clockwork_sw_state() == CW_SW_FINAL, "the final time lit");
    CHECK(clockwork_dial(90, 90, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 15 && clockwork_dial_minute() == -1,
          "a quarter turn under the finger: dialling 15 min (not the alarm's dial)");
    CHECK(fabsf(clockwork_arc(now_ms) - 0.25f) < 1e-4f, "the arc follows: a quarter of the ring (%.2f)", clockwork_arc(now_ms));
    draw("timer_dial");
    CHECK(text_px() > 0, "the duration at the top (15:00)");
    CHECK(clockwork_dial(100, 100, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 15, "100 deg: still 15 (16.7 min snaps to the 5-min step)");
    CHECK(clockwork_dial(200, 200, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 35, "200 deg: 35 min");
    CHECK(clockwork_dial(370, 370, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 60 && clockwork_arc(now_ms) == 1.0f, "a full turn: the hour, the ring full");
    CHECK(clockwork_dial(400, 800, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 120, "two turns and more: capped at 120 min");
    CHECK(clockwork_dial(300, -300, false, minute, now_ms) == 0 && clockwork_dial_timer_min() == 0, "counter-clockwise: nothing dialled");
    run(7.0f);                                                /* past the final's own 6 s */
    CHECK(clockwork_sw_state() == CW_SW_FINAL, "the final time waits while the finger goes round");
    clockwork_dial(90, 90, false, minute, now_ms);
    CHECK(clockwork_dial(90, 90, true, minute, now_ms) == 2, "the lift at a quarter turn: the timer set (2, not the alarm's 1)");
    CHECK(clockwork_tm_state() == CW_TM_RUN && clockwork_sw_state() == CW_SW_OFF && clockwork_dial_timer_min() == -1, "the timer runs, the stopwatch put away, not dialling");
    CHECK(clockwork_remaining_ms(now_ms) == 15u * 60000u, "15:00 to go (%lu ms)", (unsigned long)clockwork_remaining_ms(now_ms));
    CHECK(clockwork_alarm() < 0, "no alarm was touched");
    run(0.1f);
    draw("timer_from_rim");
    {   /* the timer's tick: the minute hand at the ring is (minute % 60 + 15) -- the test's minute is on the hour, so 15 = 3 o'clock, r 108..112, the shine, 1 px */
        int m15 = (minute % 60 + 15) % 60;
        float ang = (float)m15 / 60.0f * 2.0f * (float)M_PI;
        int cx = (int)(120 + sinf(ang) * 110), cy = (int)(120 - cosf(ang) * 110);
        int n = 0;
        for (int y = cy - 4; y <= cy + 4; y++) for (int x = cx - 4; x <= cx + 4; x++) if (fb[y * FACE_W + x]) n++;
        CHECK(n >= 3 && n <= 10, "the timer's tick on the minute dial where it will ring (%d px near minute %d)", n, m15);
    }
    clockwork_set_timer(0, now_ms, "the test"); run(0.1f); clockwork_log_taken();
    /* let go under a step: nothing, the final stays; no clock: the timer's dial still works */
    clockwork_hold(1000, now_ms); run(1.0f); clockwork_hold(1000, now_ms); run(0.1f);
    clockwork_dial(10, 10, false, minute, now_ms);           /* 10 deg = 1.7 min: under the step (20 deg would already round up to 5) */
    CHECK(clockwork_dial(10, 10, true, minute, now_ms) == 0 && clockwork_sw_state() == CW_SW_FINAL && clockwork_tm_state() == CW_TM_OFF,
          "let go under 5 min: no timer, the final stays");
    clockwork_dial(180, 180, false, -1, now_ms);
    CHECK(clockwork_dial_timer_min() == 30 && clockwork_dial(180, 180, true, -1, now_ms) == 2 && clockwork_tm_state() == CW_TM_RUN,
          "no clock: the timer's dial works all the same (30 min)");
    run(0.1f); draw("timer_noclock");
    CHECK(lit_px() > 0, "the countdown drawn; the tick needs a minute hand and is not");
    clockwork_set_timer(0, now_ms, "the test"); run(0.1f); clockwork_log_taken();

    printf("THE GENTLE WAKE: the alarm early, in the light phase\n");
    clockwork_begin(-1); run(0.1f); clockwork_log_taken();
    lit = false; holding = true; asleep = true;
    {
        int am = (minute + 30) % 1440;
        clockwork_set_alarm(am, "the test"); run(0.1f); clockwork_log_taken();
        /* the night's floor and the rise, as the pulse's ring would have fed them */
        gentle_pulse(60, true, now_ms - 3600000u, true); gentle_pulse(58, true, now_ms - 2400000u, true); gentle_pulse(59, true, now_ms - 1200000u, true);
        gentle_pulse(64, true, now_ms - 300000u, true); gentle_pulse(66, true, now_ms - 120000u, true);   /* the newest two minutes old: stale by the time the window opens */
        clockwork_stir(now_ms);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "30 min before, stirring, the pulse up: not yet (the window is the last 20)");
        clockwork_status(st, sizeof st);
        CHECK(strstr(st, "gentle: armed") != NULL, "the status: %s", strstr(st, "gentle:"));
        while (minute != (am - 15 + 1440) % 1440) run(1.0f);                       /* 15 min before, the stir and the readings now 15 min old */
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "15 min before, the stir 15 min old: not stirring");
        clockwork_status(st, sizeof st);
        CHECK(strstr(st, "gentle: in the window, not stirring") != NULL, "the status: %s", strstr(st, "gentle:"));
        clockwork_stir(now_ms);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, false), "stirring but rest says awake: never (the alarm will ring)");
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "stirring, dozing, the readings 15 min old: no pulse known");
        gentle_pulse(64, true, now_ms - 60000u, true); gentle_pulse(63, true, now_ms, true);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "a fresh pair over the floor but falling: no");
        gentle_pulse(66, true, now_ms, true);
        CHECK(clockwork_ring() == CW_RING_NONE, "nothing rings yet");
        CHECK(clockwork_gentle_due(now_ms, minute, date, true), "the pulse 58 -> 63, 66, stirring, dozing, 15 min before: WAKE -- the nap breaks");
        CHECK(clockwork_ring() == CW_RING_ALARM, "the alarm's own ring");
        reset_seen(); lit = true; run(0.5f);
        CHECK(hop3 == 1 && kicks >= 1 && wide_frames > 0, "the alarm's theatre over the sleep look: one HOP3, the bounces (%d kicks so far), wide eyes", kicks);
        CHECK(strstr(last_log, "clockwork: the gentle wake -- ") != NULL && strstr(last_log, "(stirring 0 s ago, pulse 58 -> 66)") != NULL, "the log: %s", last_log);
        run(1.0f);
        CHECK(kicks == 3 && hop3 == 1, "three bounces, one HOP3");
        draw("gentle");
        CHECK(text_px() > 0, "the alarm's hour at the top, as at its minute");
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "once per alarm: not again while it rings");
        clockwork_tap(now_ms); run(0.1f);
        CHECK(clockwork_ring() == CW_RING_NONE, "a tap dismisses it");
        gentle_pulse(70, true, now_ms, true); clockwork_stir(now_ms);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "once per alarm: not again after, however the pulse climbs");
        clockwork_status(st, sizeof st);
        CHECK(strstr(st, "gentle: spent for") != NULL, "the status: %s", strstr(st, "gentle:"));
        lit = false;
        while (minute != (am - 1 + 1440) % 1440) run(1.0f);
        CHECK(!clockwork_due(now_ms, am, date), "its minute does not break the nap: already rung");
        run(0.1f); CHECK(clockwork_ring() == CW_RING_NONE, "... and does not ring");
        reset_seen(); lit = true; run(90.0f);
        CHECK(clockwork_ring() == CW_RING_NONE && hop3 == 0 && kicks == 0, "through its minute, lit: nothing -- the alarm was consumed");
        CHECK(strstr(last_log, "already rung by the gentle wake; nothing") != NULL, "the log: %s", last_log);
        clockwork_status(st, sizeof st);
        CHECK(strstr(st, "gentle: armed") != NULL, "the minute passed: armed for the next day (%s)", strstr(st, "gentle:"));
        /* the ordinary ring untouched: the window never says (no stir), the alarm rings at its minute as ever */
        am = (minute + 25) % 1440;
        clockwork_set_alarm(am, "the test"); run(0.1f); clockwork_log_taken();
        lit = false; asleep = true;
        while (minute != (am - 1 + 1440) % 1440) run(1.0f);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "no stir in the window: the gentle wake says nothing");
        CHECK(clockwork_due(now_ms, am, date), "... the alarm's minute breaks the nap as ever");
        reset_seen(); lit = true; while (minute != am) run(1.0f); run(1.5f);
        CHECK(clockwork_ring() == CW_RING_ALARM && hop3 == 1 && kicks == 3, "... and rings at its minute, the bounces, HOP3");
        clockwork_tap(now_ms); run(0.1f); clockwork_log_taken();
        /* the lit path: asleep but lit (THE NIGHT LIGHT's stir), the tick itself says it */
        am = (minute + 10) % 1440;
        clockwork_set_alarm(am, "the test"); run(0.1f); clockwork_log_taken();
        gentle_pulse(58, true, now_ms - 1200000u, true); gentle_pulse(58, true, now_ms - 900000u, true); gentle_pulse(58, true, now_ms - 600000u, true);
        gentle_pulse(64, true, now_ms - 60000u, true); gentle_pulse(65, true, now_ms, true);
        clockwork_stir(now_ms);
        lit = true; asleep = true; reset_seen(); run(0.5f);
        CHECK(clockwork_ring() == CW_RING_ALARM && hop3 == 1, "lit through a stir, 10 min before: the tick rings it (no nap to break)");
        CHECK(strstr(last_log, "clockwork: the gentle wake -- ") != NULL, "the log: %s", last_log);
        clockwork_tap(now_ms); run(0.1f);
        /* a new alarm rearms it; `alarm off` = never */
        clockwork_set_alarm((minute + 5) % 1440, "the test");
        clockwork_stir(now_ms); gentle_pulse(66, true, now_ms, true);
        lit = false;
        CHECK(clockwork_gentle_due(now_ms, minute, date, true), "a new alarm 5 min off: the window armed again, and it says");
        clockwork_tap(now_ms); run(0.1f);
        clockwork_set_alarm(-1, "the test"); run(0.1f); clockwork_log_taken();
        clockwork_stir(now_ms); gentle_pulse(68, true, now_ms, true);
        CHECK(!clockwork_gentle_due(now_ms, minute, date, true), "`alarm off`: never, whatever the wrist does");
        clockwork_status(st, sizeof st);
        CHECK(strstr(st, "gentle: no alarm") != NULL, "the status: %s", strstr(st, "gentle:"));
    }
    lit = true; asleep = false;
    clockwork_begin(-1); run(0.1f); clockwork_log_taken();

    clockwork_status(st, sizeof st);
    printf("       %s\n", st);
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
