/* THE RAISE'S RULE on the laptop (raise.cpp has no Arduino and no IMU in
 * it; the wrist is synthetic, the poll and the burst are modelled):
 *   c++ -std=gnu++17 -I src -o /tmp/wake_test tools/wake_test.cpp src/raise.cpp -lm && /tmp/wake_test
 * The wrist is a gravity vector in the watch's frame: theta = the
 * angle from face-up (0 = flat, face to the eyes; 90 = the glass
 * vertical, the arm hanging; > 90 = the glass toward the thigh), phi =
 * the turn about the glass's normal (the forearm swung, the face
 * unchanged). The nap polls it every POLL_S (250 ms, or the ladder's
 * 1 s); a trigger runs a burst as wake.cpp's does -- 300 ms, longer
 * while the pose keeps turning across the tail (HOLD_TURN_DEG 3 over
 * 180 ms), 700 ms at most -- whose end pose is the wrist's then; the
 * peak jerk and the over-span are the scene's (a smooth turn jerks
 * nothing at 224 Hz). The verdict is raise_judge's, QUIET TIME
 * raise_quiet's.
 *
 * THE BUG (2026-09-10 evening) reproduced: the same slow stepped raise
 * through THE OLD RULE (the sleep pose re-based at every turn verdict)
 * stays dark; through the episode rule it lights. Then: a brisk raise;
 * a slow continuous raise from the thigh; the lesser raise (a 17 deg
 * twist); a turn that never ends face-up (dark; re-based after 2 s
 * still, not before; a further 15 deg then judges nothing); a bump; a
 * shake; a pick-up; the ladder's 1 s poll with the raise between two
 * polls; QUIET TIME's turn in the sheets (dark at night, lit by day, the
 * night light lit). THE SMALL MANNERS (2026-09-23): THE HANG SLUMP's
 * run (the polls in the hanging posture before the raise: 90 s of it
 * reads as the slump, 20 s does not, a run left for 8 s is forgotten)
 * and SHOWING SOMEONE's pose (raise_show: turned from the look, the
 * face off, gravity across the strap -- the forearm up; a hanging arm,
 * a face still toward him, a face-down glass are not company). */
#include "../src/raise.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DEG = (float)M_PI / 180.0f;
static const float BURST_S = 0.3f, BURST_MAX_S = 0.7f, TAIL_S = 0.18f, HOLD_TURN_DEG = 3.0f;

/* --- the synthetic wrist ----------------------------------------------------- */

static vec3_t at_angles(float theta, float phi)
{
    /* raw: face-up reads z = -1 (raise.cpp's FACE_UP_Z), so face = cos theta */
    vec3_t a = { sinf(theta * DEG) * cosf(phi * DEG), sinf(theta * DEG) * sinf(phi * DEG), -cosf(theta * DEG) };
    return a;
}

/* the wrist's motion as segments: from t0, over `s` seconds, the angles
 * ease linearly from the previous end to (theta, phi) */
static struct { float t0, s, th0, ph0, th1, ph1; } segs[32];
static int nseg = 0;
static float th_now = 90, ph_now = 0, t_end = 0;
static void wrist_reset(float theta, float phi) { nseg = 0; th_now = theta; ph_now = phi; t_end = 0; }
static void move(float seconds, float theta, float phi)
{
    segs[nseg].t0 = t_end; segs[nseg].s = seconds; segs[nseg].th0 = th_now; segs[nseg].ph0 = ph_now;
    segs[nseg].th1 = theta; segs[nseg].ph1 = phi; nseg++;
    th_now = theta; ph_now = phi; t_end += seconds;
}
static void pause(float seconds) { move(seconds, th_now, ph_now); }
static vec3_t pose_at(float t)
{
    float th = th_now, ph = ph_now;
    if (nseg && t < segs[0].t0) { th = segs[0].th0; ph = segs[0].ph0; }
    for (int i = 0; i < nseg; i++) {
        if (t < segs[i].t0) break;
        float k = segs[i].s > 0 ? (t - segs[i].t0) / segs[i].s : 1.0f;
        if (k > 1) k = 1;
        th = segs[i].th0 + (segs[i].th1 - segs[i].th0) * k;
        ph = segs[i].ph0 + (segs[i].ph1 - segs[i].ph0) * k;
    }
    return at_angles(th, ph);
}

/* a transient the next poll sees (a knock, a shake): the sample is the
 * pose plus a kick of `g`; the burst then reports the peak and the span */
static float kick_g = 0, kick_peak = 0; static uint32_t kick_span = 0; static float kick_at = -1;
static void kick(float when, float g, float peak, uint32_t span_ms) { kick_at = when; kick_g = g; kick_peak = peak; kick_span = span_ms; }

/* --- the nap, modelled ------------------------------------------------------- */

static raise_state_t rs;
static raise_tune_t tune;
static struct {
    int   verdicts, raises, turns, bumps, shakes, quiet, rebases;
    float lit_at, last_turn, last_face;
    float hung_s;                        /* THE HANG SLUMP: raise_hung_s at the raise that lit it */
    const char *lit_by, *last_trigger;
    char  log[2000];
} run;
static bool quiet_hours = false;
static bool legacy = false;              /* THE OLD RULE: re-base at every turn verdict, no face rise, no step gate, the burst 300 ms whatever the wrist does */
static void old_rule(bool on)
{
    legacy = on;
    tune = *raise_defaults();
    if (on) { tune.face_rise = 1e9f; tune.step_deg = 0; }
}
static float poll_s = 0.25f;

static void logf_(const char *fmt, ...)
{
    char line[200]; va_list ap; va_start(ap, fmt); vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
    printf("       %s\n", line);
    size_t n = strlen(run.log); if (n + strlen(line) + 2 < sizeof run.log) { strcat(run.log, line); strcat(run.log, "\n"); }
}

/* the time of the first log line that carries `what`, or -1 */
static float log_time_of(const char *what)
{
    const char *line = run.log;
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        const char *hit = strstr(line, what);
        if (hit && (!nl || hit < nl)) { float t = -1; sscanf(line, "t %f", &t); return t; }
        line = nl ? nl + 1 : NULL;
    }
    return -1;
}

/* the burst from t: as wake.cpp's -- 300 ms, longer while the pose
 * keeps turning across the tail, 700 ms at most; the end pose is the
 * tail's mean, here the pose at the end (a still tail) */
static float burst_end(float t)
{
    float e = t + BURST_S;
    while (!legacy && e < t + BURST_MAX_S) {
        vec3_t a = pose_at(e), b = pose_at(e - TAIL_S);
        if (raise_angle_deg(&a, &b) < HOLD_TURN_DEG) break;
        e += 0.02f;
    }
    return e;
}

/* the nap from the pose at t = 0 until `seconds`, or the glass lights */
static void nap(float seconds)
{
    memset(&run, 0, sizeof run); run.lit_at = -1;
    vec3_t p0 = pose_at(0);
    raise_begin(&rs, &p0);
    float t = 0;
    while (t < seconds) {
        t += poll_s;
        vec3_t a = pose_at(t);
        if (kick_at >= 0 && t >= kick_at) { a.x += kick_g; kick_at = -1; }   /* the transient, one poll */
        bool rebased = false;
        const char *trig = raise_poll(&rs, &tune, &a, poll_s, &rebased);
        if (rebased) { run.rebases++; logf_("t %.2f: the episode pose re-based (%.0f deg from the old, face %.2f -> %.2f, still %.1f s)", t, rs.rebase_deg, rs.rebase_face_from, rs.episode_face, rs.still_s); }
        if (!trig) continue;
        float peak = kick_peak; uint32_t span = kick_span; kick_peak = 0; kick_span = 0;
        float te = burst_end(t);
        vec3_t end = pose_at(te);
        float turn, face;
        int v = raise_judge(&rs, &tune, &end, peak, span, &turn, &face);
        run.verdicts++; run.last_trigger = trig; run.last_turn = turn; run.last_face = face;
        logf_("t %.2f: motion (the poll: %s) -> %s (turned %.0f deg from the episode pose, face %.2f from the episode's %.2f, peak jerk %.2f g over %lu ms, %.0f deg on from the last end, still %.1f s before, %s) [burst %.2f s]",
              t, trig, RAISE_VERDICT_NAMES[v], turn, face, rs.episode_face, peak, (unsigned long)span,
              raise_angle_deg(&end, &rs.last_end), rs.still_s, quiet_hours ? "the quiet hours" : "by day", te - t);
        t = te;
        if (raise_quiet(&tune, v, face, quiet_hours)) {
            run.quiet++; logf_("t %.2f: quiet time: a turn in the sheets, not a raise (face %.2f under %.2f)", t, face, tune.quiet_face);
            raise_judged(&rs, &end);
            if (legacy) raise_begin(&rs, &end);
            continue;
        }
        if (v == RAISE_RAISE) { run.raises++; run.lit_at = t; run.lit_by = "a raise"; run.hung_s = raise_hung_s(&rs); logf_("t %.2f: LIT (a raise; the arm hung %.0f s before)", t, run.hung_s); return; }
        if (v == RAISE_SHAKE) { run.shakes++; run.lit_at = t; run.lit_by = "a shake"; logf_("t %.2f: LIT (a shake)", t); return; }
        if (v == RAISE_TURN) run.turns++; else run.bumps++;
        raise_judged(&rs, &end);
        if (legacy && v == RAISE_TURN) raise_begin(&rs, &end);   /* THE OLD RULE: sleep_pose = end at every turn */
    }
    logf_("t %.2f: the nap goes on, dark (%d verdicts)", t, run.verdicts);
}

/* the stepped raise of the evening: the glass toward the thigh (110
 * deg), three steps of 12 deg, 0.4 s each with 0.8 s between */
static void stepped_raise(void)
{
    wrist_reset(110, 0);
    for (int i = 0; i < 3; i++) { move(0.4f, 110 - 12 * (i + 1), 0); pause(0.8f); }
    pause(3.0f);
}

int main(void)
{
    tune = *raise_defaults();
    printf("the rule: raise >= %.0f deg from the episode pose (or the face up %.2f) face >= %.2f; pick-up %.2f g; jerk %.2f g, shake over %lu ms; "
           "step %.0f deg; still < %.2f g for %.1f s at >= %.0f deg re-bases; quiet time face >= %.2f\n",
           tune.raise_deg, tune.face_rise, tune.raise_face, tune.pickup_g, tune.wake_jerk_g, (unsigned long)tune.shake_span_ms,
           tune.step_deg, tune.still_jerk_g, tune.rebase_still_s, tune.rebase_min_deg, tune.quiet_face);

    puts("-- the frame: face-up flat reads face +1, the glass vertical 0, toward the thigh < 0");
    { vec3_t f = at_angles(0, 0), v = at_angles(90, 0), t = at_angles(120, 0);
      CHECK(fabsf(raise_face_of(&f) - 1.0f) < 1e-4f && f.z < 0, "flat face-up: raw z %.2f (the chip on the back), face %.2f", f.z, raise_face_of(&f));
      CHECK(fabsf(raise_face_of(&v)) < 1e-4f, "the glass vertical: face %.2f", raise_face_of(&v));
      CHECK(raise_face_of(&t) < -0.4f, "toward the thigh (120 deg): face %.2f", raise_face_of(&t));
      CHECK(fabsf(raise_angle_deg(&f, &v) - 90) < 0.01f, "flat to vertical: %.1f deg", raise_angle_deg(&f, &v)); }

    puts("-- THE BUG: the stepped raise (110 -> 74 deg in three slow steps of 12) through THE OLD RULE: dark");
    old_rule(true); stepped_raise(); nap(t_end);
    CHECK(run.lit_at < 0, "the old rule never lit it (%d verdicts: %d turns, the sleep pose re-based mid-raise)", run.verdicts, run.turns);
    CHECK(run.turns == 1 && run.last_face < tune.raise_face, "the second step was judged a turn (face %.2f under %.2f) and re-based; the third, 12 deg from there, fired nothing", run.last_face, tune.raise_face);
    { vec3_t e = pose_at(t_end); CHECK(raise_face_of(&e) >= tune.raise_face, "...though the face ended at %.2f, toward the eyes", raise_face_of(&e)); }
    old_rule(false);

    puts("-- the same stepped raise through THE EPISODE RULE: lit at the third step");
    stepped_raise(); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f by %s", run.lit_at, run.lit_by ? run.lit_by : "-");
    CHECK(run.lit_at < 3.4f, "...within the raise (the third step ends at 3.6 s; lit at %.2f)", run.lit_at);
    CHECK(run.last_turn >= tune.raise_deg && run.last_face >= tune.raise_face, "judged from the episode pose: turned %.0f deg, face %.2f", run.last_turn, run.last_face);
    CHECK(run.turns == 1 && run.rebases == 0, "the second step's turn verdict re-based nothing (%d turns, %d re-bases)", run.turns, run.rebases);

    puts("-- a brisk raise: the arm hanging (100 deg) to the look (45 deg) in 0.4 s");
    wrist_reset(100, 0); move(0.4f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f (%s)", run.lit_at, run.lit_by ? run.lit_by : "-");
    CHECK(run.lit_at <= 1.0f, "...within the second (%.2f s)", run.lit_at);
    CHECK(run.verdicts == 1, "one burst, one verdict (%d)", run.verdicts);
    CHECK(run.last_face > 0.65f, "the end pose is the held look: face %.2f", run.last_face);

    puts("-- a slow continuous raise from the thigh: 120 -> 75 deg over 1.4 s (32 deg/s: under the jerk trigger per poll)");
    wrist_reset(120, 0); move(1.4f, 75, 0); pause(3.0f);
    old_rule(true); nap(t_end);
    CHECK(run.lit_at < 0 && run.turns == 1, "the old rule: dark (%d verdicts, %d turn: the burst ended at 300 ms mid-raise, face %.2f, and the rest of the raise was under %.0f deg from the re-based pose)", run.verdicts, run.turns, run.last_face, tune.raise_deg);
    old_rule(false); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "the episode rule: lit at t %.2f", run.lit_at);
    CHECK(run.lit_at < 2.6f, "...soon after the raise ends at 1.4 s (%.2f s)", run.lit_at);

    puts("-- THE LESSER RAISE: a 17 deg twist toward the eyes from 95 deg (face -0.09 -> 0.21), under the 20 deg");
    wrist_reset(95, 0); move(0.5f, 78, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f", run.lit_at);
    CHECK(run.last_trigger && !strcmp(run.last_trigger, "the face came up"), "the trigger was the face come up (%s)", run.last_trigger ? run.last_trigger : "-");
    CHECK(run.last_turn < tune.raise_deg && run.last_face >= tune.raise_face, "turned %.0f deg (under %.0f), face %.2f", run.last_turn, tune.raise_deg, run.last_face);

    puts("-- a small twist that stays under the face (95 -> 85 deg, face 0.09): nothing");
    wrist_reset(95, 0); move(0.5f, 85, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at < 0 && run.verdicts == 0, "dark, no burst (%d verdicts)", run.verdicts);

    puts("-- a turn that never ends face-up: the forearm swung 40 deg (phi) with the glass vertical");
    wrist_reset(90, 0); move(0.5f, 90, 40); pause(1.7f); pause(0.6f); move(0.5f, 90, 55); pause(3.0f); nap(t_end);
    CHECK(run.lit_at < 0, "dark");
    CHECK(run.turns == 1 && run.raises == 0, "judged a turn once (%d turns, %d bumps, %d verdicts)", run.turns, run.bumps, run.verdicts);
    CHECK(run.rebases == 2, "the episode pose re-based after each stillness: at the 40 deg, then at the further 15 (%d)", run.rebases);
    { float t_turn = log_time_of("a turn, not a raise"), t_reb = log_time_of("re-based");
      CHECK(t_reb >= t_turn + BURST_S + tune.rebase_still_s - 0.01f && t_reb <= t_turn + BURST_MAX_S + tune.rebase_still_s + poll_s,
            "...the first at t %.2f: %.1f s still after the burst (from t %.2f), not before", t_reb, tune.rebase_still_s, t_turn); }
    CHECK(run.verdicts == 1, "the further 15 deg from the re-based pose judged nothing (%d verdicts in all)", run.verdicts);

    puts("-- a bump: a 0.30 g knock on a still wrist, the pose unchanged");
    wrist_reset(90, 0); pause(4.0f); kick(1.0f, 0.30f, 0.30f, 0); nap(t_end);
    CHECK(run.lit_at < 0 && run.bumps == 1 && run.verdicts == 1, "dark: a bump, not a raise (%d bumps, %d verdicts)", run.bumps, run.verdicts);
    CHECK(run.rebases == 0, "nothing re-based (%d)", run.rebases);
    CHECK(run.last_trigger && !strcmp(run.last_trigger, "a jerk"), "the trigger was the jerk (%s)", run.last_trigger ? run.last_trigger : "-");

    puts("-- a shake: 1.5 g jerks spanning 250 ms, the pose unchanged");
    wrist_reset(90, 0); pause(4.0f); kick(1.0f, 1.5f, 1.5f, 250); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a shake"), "lit at t %.2f by a shake", run.lit_at);

    puts("-- a pick-up: the flat puck (face 1.00) lifted 3 deg with a 0.5 g jerk");
    wrist_reset(0, 0); pause(1.0f); move(0.2f, 3, 0); pause(3.0f); kick(1.25f, 0.5f, 0.5f, 60); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f: picked up, face toward the eyes (turned %.0f deg)", run.lit_at, run.last_turn);

    puts("-- the ladder's 1 s poll (30 min still): a 0.5 s raise lands whole between two polls");
    poll_s = 1.0f; wrist_reset(100, 0); pause(2.0f); move(0.5f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f (the poll at 3 s saw the turn)", run.lit_at);
    CHECK(run.lit_at <= 3.4f, "...within the second after the raise (%.2f)", run.lit_at);
    poll_s = 0.25f;

    puts("-- a drift under the re-base's 5 deg: the episode pose stays");
    wrist_reset(90, 0); move(1.0f, 90, 3); pause(4.0f); nap(t_end);
    CHECK(run.rebases == 0 && run.verdicts == 0, "3 deg of drift: nothing re-based, nothing judged");

    puts("-- QUIET TIME: a turn in the sheets (40 deg, ending face 0.31) at night sleeps on; by day it lights; the night light (face 0.64) lights at night");
    quiet_hours = true; wrist_reset(90, 0); move(0.6f, 72, 40); pause(3.0f); nap(t_end);
    CHECK(run.lit_at < 0 && run.quiet == 1, "night: dark, quiet-time turn %d (face %.2f under %.2f)", run.quiet, run.last_face, tune.quiet_face);
    CHECK(run.rebases == 1, "...and the episode pose re-based by the stillness after (%d)", run.rebases);
    quiet_hours = false; nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "day: lit at t %.2f", run.lit_at);
    quiet_hours = true; wrist_reset(90, 0); move(0.6f, 50, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && run.quiet == 0, "night, face %.2f >= %.2f: lit at t %.2f (the night light)", run.last_face, tune.quiet_face, run.lit_at);
    quiet_hours = false;

    puts("-- a shake at night lights (deliberate)");
    quiet_hours = true; wrist_reset(90, 0); pause(4.0f); kick(1.0f, 1.5f, 1.5f, 250); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a shake"), "lit at t %.2f", run.lit_at);
    quiet_hours = false;

    puts("-- the raise after a long pause: the forearm up (30 deg, face 0.0), 3 s still (re-based), then the wrist turned 15 deg to the eyes");
    wrist_reset(90, 0); move(0.4f, 90, 30); pause(3.0f); move(0.4f, 75, 30); pause(3.0f); nap(t_end);
    CHECK(run.rebases == 1, "re-based after the pause (%d)", run.rebases);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise") && run.last_trigger && !strcmp(run.last_trigger, "the face came up"),
          "lit at t %.2f by THE LESSER RAISE (face 0.00 -> %.2f, turned %.0f deg from the new episode)", run.lit_at, run.last_face, run.last_turn);

    puts("-- THE HANG SLUMP (A3): the arm hanging (100 deg, in-plane 0.98 of g) for 90 s, then the brisk raise");
    wrist_reset(100, 0); pause(90.0f); move(0.4f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f", run.lit_at);
    CHECK(run.hung_s >= 60.0f && run.hung_s <= 91.0f, "the arm hung %.0f s before it (a minute or more: the slump)", run.hung_s);
    puts("-- ...the wrist raised (45 deg: posture raised) for 30 s, dropped to the side for 20 s, then raised: hung 20 s, no slump");
    wrist_reset(45, 0); pause(30.0f); move(0.5f, 100, 0); pause(20.0f); move(0.4f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f (the drop to the side was a turn, re-based by the stillness)", run.lit_at);
    CHECK(run.hung_s > 15.0f && run.hung_s < 60.0f, "the arm hung %.0f s: under the minute, no slump", run.hung_s);
    puts("-- ...hung 90 s, then laid flat face-down (posture lying) for 8 s, then raised: the hang forgotten (5 s out of it)");
    wrist_reset(100, 0); pause(90.0f); move(0.5f, 180, 0); pause(8.0f); move(0.5f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.lit_at > 0 && !strcmp(run.lit_by, "a raise"), "lit at t %.2f", run.lit_at);
    CHECK(run.hung_s == 0.0f, "the hang forgotten: hung %.0f s", run.hung_s);
    puts("-- ...a knock on the hanging arm at 40 s does not restart the run: 90 s still reads as the slump");
    wrist_reset(100, 0); pause(90.0f); kick(40.0f, 0.30f, 0.30f, 0); move(0.4f, 45, 0); pause(3.0f); nap(t_end);
    CHECK(run.bumps == 1 && run.lit_at > 0 && run.hung_s >= 60.0f, "a bump at 40 s (%d bumps), lit at t %.2f, hung %.0f s", run.bumps, run.lit_at, run.hung_s);

    puts("-- SHOWING SOMEONE (B18): the look pose is 30 deg from flat (face 0.87), the forearm level");
    { vec3_t look = at_angles(30, 0);
      vec3_t across = at_angles(85, 90), hang = at_angles(100, 0), toward = at_angles(60, 90), small = at_angles(35, 0),
             down = at_angles(180, 0), diag_ok = at_angles(85, 60), diag_no = at_angles(85, 30);
      CHECK(raise_show(&look, &across), "the glass rolled to face across the table (85 deg, gravity on y = across the strap, face %.2f): company", raise_face_of(&across));
      CHECK(!raise_show(&look, &hang), "the arm dropped to the side (100 deg, gravity on x = along the strap, face %.2f): not company -- the lowering's road", raise_face_of(&hang));
      CHECK(!raise_show(&look, &toward), "rolled only to 60 deg (face %.2f, still toward him): not company", raise_face_of(&toward));
      CHECK(!raise_show(&look, &small), "a 5 deg fidget from the look: not company");
      CHECK(!raise_show(&look, &down), "the glass laid face-down (neither axis): not company");
      CHECK(raise_show(&look, &diag_ok) && !raise_show(&look, &diag_no), "gravity more across the strap than along it (phi 60 yes, phi 30 no)");
      vec3_t look2 = at_angles(85, 90);
      CHECK(!raise_show(&look2, &across), "he looked FROM the across pose (a watch read sideways): the same pose is not company"); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
