/* The wrist's night and its breath -- rest.h says what; this is how.
 *
 * THE NIGHT (watch-body.md section 1, "night, you asleep: it sleeps too,
 * Zs"): the wrist's own data decides. A still run of the day log's
 * minute rows (energy under WRIST_STILL_MG, no steps) long enough,
 * begun in the quiet hours, is sleep; the gel sleeps with him. Real
 * motion ends it -- a raise or a shake (main's wake), a walk -- but in
 * the quiet hours a raise is first a STIR (a turn-over lifts the wrist
 * too), forgiven when the wrist lies still again within minutes. A span
 * of three hours or more was the night: the first motion after it is
 * the morning stretch and `wrist:slept`; under that it was a nap.
 * "Sleep guardian: it will not wake you; the morning stretch waits for
 * your movement" (section 5).
 *
 * THE SHELF RULE (Khalid, 2026-09-10 05:40: the watch had sat on the
 * desk, on the cable, all night; in the morning it wore the sleep look
 * and the Zs -- "a shelf is not a bed"): a still run is his night only
 * if the wrist was WORN when it began. The day log's WORN flag (motion
 * in the last 10 min) on the run's first still rows is only the
 * set-down's echo, so the rule reads the flag past that echo: a WORN
 * row SHELF_WORN_MIN or more rows into the run is motion after it
 * began -- a wrist -- and a run on the cable (the row's CHARGING flag:
 * the dock) is never worn. A run that fails is THE SHELF: no sleep
 * look, no Zs, no span, nothing told, not his sit; a still row that
 * moves later (WORN, off the cable) makes it his after all, asleep
 * since the run began; anything else ends it quietly.
 *
 * A TOUCH ENDS SLEEP (the same morning: he tapped it and the Zs stayed;
 * only motion ended a span): a hand on the glass is a person awake --
 * rest_touch() ends the span as a rising, the stretch and the sense as
 * for a raise; the replay reads the row's taps the same way.
 *
 * THE SHAKEN SLEEPER (design/watch-ideas.md A1, built 2026-09-10 on
 * the coordinator's reading: like the touch): a shake is the body
 * being roused -- "I'm up" is the shake's own word (gesture.cpp) -- so
 * a shake while asleep or stirring ends the span as a RISING, never a
 * stir, whatever the hour: rest_shake(), the stretch and the sense as
 * for a raise. A raise in the quiet hours stays a stir (a turn-over
 * lifts the wrist too; nobody shakes a wrist in their sleep). The
 * replay reads the row's UP flag (the "I'm up" shake, the same
 * gesture) the same way; the coarser SHAKEN flag (a 0.6 g jerk) stays
 * plain motion, as before.
 *
 * THE BREATH (section 5, "you see yourself calm down" -- without the
 * PPG, from the day's motion): an hour still and the breath goes slow
 * and shallow; a walk and it is quicker and fuller for ten minutes;
 * asleep it is slowest. Every change eases over BREATH_EASE_S and the
 * phase is integrated, so a rate change never jumps the gel.
 *
 * THE PULSE AS THE BREATH (design/watch-ideas.md D31, 2026-10-08; the
 * PPG on his wrist since 10-03): with a clean pulse known and fresh
 * (pulse.h: in the band, under PULSE_STALE_MIN old) the day's guess
 * gives way to the truth -- the resting target is his pulse in the
 * gel's own proportion (pulse_breath_hz: 60 bpm = the grain's rate,
 * 90 = the walk's, clamped to the sleeper's and the walk's), eased
 * over PULSE_BREATH_EASE_S 60 instead of 30, in and out. Asleep and
 * the walk keep their bands (the sleeper's breath is its own; a walk
 * is seen in the steps before any spot could read it); the hour's sit
 * yields (it was the guess the pulse replaces). THE BUMPS' resting
 * rate stays the grain's, so a pulse-led breath above it reads as
 * quick and a pet is rightly kept.
 *
 * No Arduino here: the rows and the time come in as parameters and
 * tools/rest_test.cpp runs a whole synthetic day on the laptop. */
#include "rest.h"
#include "pulse.h"          /* THE PULSE AS THE BREATH: the pulse's band, the staleness, the target's arithmetic */
#include "grain.h"          /* THE GRAIN: the breath's own rate and depth; night=1 with no clock */
#include "vigor.h"          /* VIGOR: a body running down breathes slower and shallower (nominal 1.0 until a cell says) */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* --- the tunables, with their cause ----------------------------------------- */
/* a still minute: config.py's WRIST_STILL_MG (the day log's band 0), no steps */
static const int   STILL_MG          = 15;
/* asleep after this long still: at night a settled 20 min is sleep; by
 * day a couch is a couch, so a nap needs longer (watch-body 1: "long
 * stillness (>50 min, awake) -- it naps") */
static const int   ONSET_NIGHT_MIN   = 20;
static const int   ONSET_DAY_MIN     = 30;
/* the quiet hours when the desk's grain has not said (grain.quiet_hours) */
static const int   NIGHT_FROM_MIN    = 23 * 60;
static const int   NIGHT_TO_MIN      = 7 * 60;
/* a stir: a raise in the quiet hours is forgiven if the wrist is still
 * again within REJOIN_MIN; motion lasting STIR_MAX_MIN is being up */
static const int   REJOIN_MIN        = 3;
static const int   STIR_MAX_MIN      = 10;
/* tossing with no raise at all (a phone in bed): this long not still ends the span */
static const int   TOSS_MAX_MIN      = 15;
/* steps in one minute that mean he is up, whatever the hour */
static const int   STEPS_UP          = 30;
/* a stir this long counts as an awakening (watch-daylog 2: "stirs >= 3 min") */
static const int   AWAKENING_MIN     = 3;
/* a span this long was the night (watch-daylog 2: "a nap under 3 h is not the night") */
static const float SLEPT_H           = 3.0f;
/* THE SHELF RULE: the day log's WORN flag remembers motion this long
 * (daylog.cpp WORN_MS), so a WORN row this many rows into a still run
 * is motion after the run began -- the wrist, not the set-down */
static const int   SHELF_WORN_MIN    = 10;
/* PUT ON COLD (design/watch-ideas.md A6, 2026-09-23): the first raise
 * after HOURS on the shelf -- a shelf run of COLD_SHELF_MIN or more ended
 * by the wrist picked up (a raise or a shake: main's wake; never a row's
 * coarse "moved", never a hand on the glass) -- gets one shiver (the
 * cold sky's own 0.3 s jiggle of the 2-lobe harmonic: COLD_SHIVER_*
 * are SKY_SHIVER_*'s numbers, sky.h) and a look up (COLD_LOOK_S, the
 * idle glance-up's own), and the breath is snapped to the sleeper's
 * band (SLEEP_HZ, SLEEP_AMP_X) to ease back to the day's over
 * BREATH_EASE_S: it slept without him and the wrist is warm -- cold,
 * and slow to warm. Never in a replay. */
static const int   COLD_SHELF_MIN    = 120;
static const float COLD_SHIVER_S     = 0.3f;
static const float COLD_SHIVER_A2    = 0.08f;
static const float COLD_SHIVER_HZ    = 12.0f;
static const float COLD_LOOK_S       = 1.5f;
/* the morning stretch: tall (a negative squash) held, eyes wide, then the ordinary face */
static const float STRETCH_HOLD_S    = 0.6f;
static const float STRETCH_IN_S      = 0.10f;
static const float STRETCH_OUT_S     = 0.30f;
/* the Zs: small, few, calm; one every Z_EVERY_S, fading as it rises */
static const float Z_EVERY_S         = 1.5f;
static const float Z_LIFE_S          = 3.0f;
static const float Z_RISE_PX         = 36.0f;
static const float Z_DRIFT_PX        = 14.0f;
/* the breath from the day: the rates and the amplitude factors */
static const float SLOW_HZ           = 0.40f;   /* an hour still: slow, shallow */
static const float SLOW_AMP_X        = 0.8f;
static const int   STILL_HOUR_MIN    = 60;
/* "still" for the breath is the SIT (watch-daylog 2: < 30 steps/min, band
 * < 2 = under 60 mg) -- a wrist at a desk, not a sleeper's; an hour under
 * the sleep threshold would already be a nap */
static const int   SIT_MG            = 60;
static const int   SIT_STEPS         = 30;
static const float WALK_HZ           = 0.80f;   /* after a walk: quick, full */
static const float WALK_AMP_X        = 1.2f;
static const int   WALK_WINDOW_MIN   = 10;      /* for ten minutes after a walk */
static const int   WALK_STEPS        = 200;     /* steps in that window that were a walk */
static const float SLEEP_HZ          = 0.30f;   /* asleep: the slowest, the deep sleep */
static const float SLEEP_AMP_X       = 0.7f;
static const float BREATH_EASE_S     = 30.0f;   /* a change is done (95 %) in about this */
/* THE WAKING BREATH (design/watch-night-2026-10-12.md C6, the operator
 * night): every raise from the dark found the breath mid-cycle -- a body
 * that had not noticed the glass lit. On the raise's own frame
 * (rest_in_t.woke_raise, main's wake_up) the phase snaps to the
 * inhale's start (0: the purr's "next inhale" -- bumps_purr_due reads
 * the drop as the wrap, so a purr armed under a pet plays NOW, earlier,
 * never dropped) and the depth is WAKE_BREATH_X easing to 1 over
 * WAKE_BREATH_EASE_S: it noticed me. The rate and its ease are
 * untouched. Never asleep or stirring (the sleeper's breath is the
 * night's; THE NIGHT LIGHT keeps that look), never off a cold shelf
 * (PUT ON COLD's shiver and slow breath own that raise), never under
 * the morning stretch (the night's rising is its own gesture). */
static const float WAKE_BREATH_X      = 1.6f;
static const float WAKE_BREATH_EASE_S = 3.0f;
/* THE RUN AND THE PANT (design/watch-night-2026-10-12.md C10, the
 * operator night): a closed minute row of RUN_SPM steps or more is a
 * run (daylog.cpp's STEP_REFRACT_MS caps the cadence at 4 Hz; 150 a
 * minute is a jog's 2.5 Hz, never a brisk walk's ~110) -- the breath at
 * RUN_HZ with the walk's full depth, the band ABOVE the walk's and the
 * pulse's as the walk is (a run is seen in the steps before any spot
 * could read it; THE PULSE AS THE BREATH clamps the pulse-led breath at
 * the walk's 0.80 and this band sits over that clamp on purpose: the
 * clamp is the pulse's law, the run's rate is the steps'); main squints
 * through it (the wind). The first row after it under RUN_SPM ends the
 * run: THE PANT -- the depth x PANT_AMP_X and the crown PANT_SQUASH
 * lower, in over PANT_IN_S (the row's close is not a jump) and easing
 * out over PANT_S on a smoothstep (held near full the first seconds,
 * the tail soft: a breath got back, no number), while the rate goes
 * down the bands' own road (the walk's 0.80 for its ten minutes, then
 * rest). Never a mark, a word or a sense; the step bob keeps to its
 * first 10 s (tell.cpp's anti-Sonic law); the walk's lump, the day log
 * and the rings untouched. Awake only (a row of 150 steps ends any span
 * or shelf run before the band is read); never from a replayed row. */
static const int   RUN_SPM            = 150;
static const float RUN_HZ             = 1.0f;
static const float PANT_S             = 90.0f;
static const float PANT_AMP_X         = 1.3f;
static const float PANT_SQUASH        = 0.04f;
static const float PANT_IN_S          = 1.0f;
/* THE ITCH IN THE BOWL (design/watch-ideas.md D28, 2026-09-10; the
 * desk's week_itch, worn): a long sit shows as the gel resting a few
 * degrees OFF gravity's angle -- it cannot sit still either -- never a
 * word. From ITCH_FROM_MIN of the open sit (the tell's own 60) the
 * offset is ITCH_DEG_MIN, growing to ITCH_DEG_MAX by ITCH_TO_MIN; the
 * side shifts every ITCH_SHIFT_MIN (main's bowl ease makes each shift
 * a slow slide, never a jump). Gone at the first steps (the walk's own
 * seconds, or a row of 30+), never asleep or on the shelf, and the sit
 * is measured from the last wake (a night is not a sit). */
static const int   ITCH_FROM_MIN     = 60;
static const int   ITCH_TO_MIN       = 180;
static const float ITCH_DEG_MIN      = 4.0f;
static const float ITCH_DEG_MAX      = 10.0f;
static const int   ITCH_SHIFT_MIN    = 3;

/* --- the state ------------------------------------------------------------- */
enum RestState { R_AWAKE, R_ASLEEP, R_STIR, R_SHELF };
static const char *const STATE_NAMES[] = { "awake", "asleep", "stirring", "on the shelf" };

#define RING 60
/* the rows' flags kept beside the ring (the shelf rule) */
#define F_WORN     1
#define F_CHARGING 2
static struct {
    RestState st;
    uint32_t  seq;              /* rows seen */
    int       still_run;        /* consecutive still rows */
    int       sit_run;          /* consecutive sit rows (the breath's hour still) */
    int       move_run;         /* consecutive rows not still (asleep / stirring) */
    uint32_t  bed_seq;          /* the row the span began on */
    int       bed_minute;       /* its minute of the day */
    uint32_t  stir_seq;         /* where the stir began */
    int       stir_minute;
    int       awakenings;
    bool      night_span;       /* the span began in the quiet hours */
    bool      worn_start;       /* THE SHELF RULE: the wrist moved inside the run's onset (the `rest` line's "worn at the start") */
    int       moved_minute;     /* a shelf run promoted: the minute it moved, -1 = worn from the start */
    bool      shelf_cable;      /* the shelf: on the cable (else: never moved) */
    int       replay_shelves;
    uint16_t  span_energy_max;
    int       span_move_min;    /* rows not still while asleep / stirring (the tell's restlessness) */
    /* the last span, for `rest`, the test and the tell */
    uint32_t  last_night_id;    /* THE NIGHT AFTER A REBOOT: the stable id (rest_night_id) */
    uint32_t  date_now;         /* the row clock's date (rest_set_date), 0 = none */
    bool      replaying;        /* the files' rows: no sense, no stretch, no span lines */
    bool      rebuilt;          /* a replay ran */
    int       replay_spans, replay_nights;
    int       last_span_min;
    bool      last_night;
    bool      have_last;
    int       last_end_minute, last_awakenings, last_move_min;
    uint32_t  last_end_row;
    char      last_words[48];
    /* the last 60 rows */
    uint16_t  steps_ring[RING], energy_ring[RING];
    uint8_t   flag_ring[RING];  /* F_WORN | F_CHARGING per row */
    int       ring_n, ring_head;
    int       minute_now;       /* from the last tick, -1 unknown */
    /* the breath */
    float     base_hz, base_amp, hz, amp, phase;
    char      breath_why[12];
    /* the stretch */
    float     stretch_t;        /* seconds into it, < 0 = none */
    /* the Zs */
    float     z_timer;
    int       z_count;
    /* the inputs remembered */
    uint32_t  motion_seen;
    bool      have_motion;
    char      quiet_seen[12];
    int       night_from, night_to;
    /* the sense held for the link */
    bool      sense_pending;
    char      sense_kind[16], sense_words[48];
    /* the log queue */
    char      logs[3][128];
    int       log_n;
    int       lease_ev;         /* THE NIGHT RELEASE: REST_EV_* waiting to be taken */
    bool      lease_night;      /* the night was announced (REST_EV_NIGHT): its end is the rising, whatever the length */
    float     cold_shiver_t;    /* PUT ON COLD: the shiver's seconds left, the look's */
    float     cold_look_t;
    uint32_t  colds;
    uint32_t  chills;           /* THE WARM SKIN (warm.c): the skin read cold -- the shiver and the slow breath on PUT ON COLD's road */
    float     pulse_bpm, pulse_age_s;   /* THE PULSE AS THE BREATH: the last tick's pulse, for `rest` */
    bool      pulse_led;        /* the breath followed his pulse on the last tick (the log's edge, the 60 s ease both ways) */
    float     pulse_drift_t;    /* seconds of the pulse's ease left on the way back to the day's rest (a stale pulse); 0 = the bands' own 30 */
    float     wake_x_t;         /* THE WAKING BREATH: seconds left of the first cycle's ease (x1.6 -> 1) */
    uint32_t  wakes;            /* the waking breaths played, for `rest` */
    bool      run_row;          /* THE RUN AND THE PANT: the newest closed row had RUN_SPM+ steps (never a replayed one) */
    uint16_t  run_steps;        /* ... its steps, for the log line */
    bool      run_on;           /* the breath was the run's on the last tick (the pant's edge) */
    float     pant_t;           /* the pant's seconds left, 0 = none */
    uint32_t  runs;             /* runs seen, for `rest` */
} R;

/* THE NIGHT RELEASE's two moments, live only: the night's onset when the
 * verdict lands in the quiet hours (the onset row's minute, or the
 * fidget's that promotes a shelf); the rising when THAT span ends --
 * rest remembers it announced the night, so the two always pair */
static void lease_event(int ev)
{
    if (R.replaying) return;
    R.lease_ev = ev;
    R.lease_night = ev == REST_EV_NIGHT;
}
int rest_take_lease_event(void) { int e = R.lease_ev; R.lease_ev = REST_EV_NONE; return e; }

static rest_out_t out;

static void say(const char *fmt, ...)
{
    if (R.log_n >= 3) { memmove(R.logs[0], R.logs[1], sizeof R.logs[0] * 2); R.log_n = 2; }
    va_list ap; va_start(ap, fmt);
    vsnprintf(R.logs[R.log_n], sizeof R.logs[0], fmt, ap);
    va_end(ap);
    R.log_n++;
}

static void hhmm(int minute, char *o, int cap)
{
    if (minute < 0) { snprintf(o, cap, "--:--"); return; }
    minute = ((minute % 1440) + 1440) % 1440;
    snprintf(o, cap, "%02d:%02d", minute / 60, minute % 60);
}

static bool is_night(int minute)
{
    if (minute < 0) return grain_night();       /* THE GRAIN: no clock, but the desk said night=1 */
    int f = R.night_from, t = R.night_to;
    if (f == t) return false;
    return f > t ? (minute >= f || minute < t) : (minute >= f && minute < t);
}

/* "23-7" / "23:30-7" from the grain; "-" or nothing = the default */
static void set_quiet(const char *q)
{
    R.night_from = NIGHT_FROM_MIN;
    R.night_to = NIGHT_TO_MIN;
    if (!q || !q[0] || !strcmp(q, "-")) return;
    int fh = 0, fm = 0, th = 0, tm = 0;
    const char *dash = strchr(q, '-');
    if (!dash) return;
    if (sscanf(q, "%d:%d", &fh, &fm) < 1) return;
    if (sscanf(dash + 1, "%d:%d", &th, &tm) < 1) return;
    if (fh < 0 || fh > 23 || th < 0 || th > 23) return;
    R.night_from = fh * 60 + fm;
    R.night_to = th * 60 + tm;
}

static void ring_push(uint16_t steps, uint16_t energy, uint8_t flags)
{
    R.steps_ring[R.ring_head] = steps;
    R.energy_ring[R.ring_head] = energy;
    R.flag_ring[R.ring_head] = flags;
    R.ring_head = (R.ring_head + 1) % RING;
    if (R.ring_n < RING) R.ring_n++;
}

/* the flags of the row ``back`` rows ago (1 = the newest); 0 past the ring */
static uint8_t ring_flags(int back)
{
    if (back < 1 || back > R.ring_n) return 0;
    return R.flag_ring[(R.ring_head - back + RING) % RING];
}

static int ring_steps(int back_n)
{
    int n = 0;
    if (back_n > R.ring_n) back_n = R.ring_n;
    for (int i = 1; i <= back_n; i++) n += R.steps_ring[(R.ring_head - i + RING) % RING];
    return n;
}

/* --- the span ---------------------------------------------------------------- */

static const char *shelf_why(void) { return R.shelf_cable ? "on the cable" : "never moved"; }

/* the still run's onset: THE SHELF RULE decides whose it is */
static void span_begin(int row_minute)
{
    R.bed_seq = R.seq - R.still_run;
    R.bed_minute = row_minute < 0 ? -1 : ((row_minute - (R.still_run - 1)) % 1440 + 1440) % 1440;
    R.awakenings = 0;
    R.move_run = 0;
    R.span_move_min = 0;
    R.moved_minute = -1;
    /* the wrist, or the shelf: a WORN row SHELF_WORN_MIN+ rows into the
     * run (past the set-down's echo) is motion after it began; the newest
     * row on the cable is the dock */
    bool moved = false;
    for (int i = 1; i <= R.still_run - SHELF_WORN_MIN; i++) if (ring_flags(i) & F_WORN) moved = true;
    R.shelf_cable = (ring_flags(1) & F_CHARGING) != 0;
    R.worn_start = moved && !R.shelf_cable;
    char b[8]; hhmm(R.bed_minute, b, sizeof b);
    if (!R.worn_start) {
        R.st = R_SHELF;
        R.night_span = false;
        R.sit_run = 0;                                  /* a shelf is not his sit */
        if (R.replaying) { R.replay_shelves++; return; }
        say("rest: on the shelf since %s (still %d min, %s): not his night", b, R.still_run, shelf_why());
        return;
    }
    R.night_span = is_night(row_minute);
    R.st = R_ASLEEP;
    if (R.night_span) lease_event(REST_EV_NIGHT);       /* THE NIGHT RELEASE: the night began (never a nap, never the shelf) */
    if (R.replaying) return;
    say("rest: asleep since %s (still %d min, %s, worn at the start)", b, R.still_run, R.night_span ? "night" : "a nap by day");
}

/* THE SHELF ends: picked up, or a hand on the glass -- no span, nothing
 * told. PUT ON COLD: picked up (the wrist's own motion) after hours of
 * it -- the shiver, the look up and the sleeper's breath to warm from */
static void shelf_end(int end_minute, const char *why, bool picked_up)
{
    int span = (int)(R.seq - R.bed_seq);
    R.st = R_AWAKE;
    R.still_run = 0;
    R.move_run = 0;
    R.sit_run = 0;
    if (R.replaying) return;
    bool cold = picked_up && span >= COLD_SHELF_MIN;
    if (cold) {
        R.cold_shiver_t = COLD_SHIVER_S;
        R.cold_look_t = COLD_LOOK_S;
        R.hz = SLEEP_HZ;                                /* the sleeper's breath, easing back to the day's over BREATH_EASE_S */
        R.amp = R.base_amp * grain_knobs()->breath_amp * vigor_breath_amp_k() * SLEEP_AMP_X;
        R.colds++;
    }
    char u[8]; hhmm(end_minute, u, sizeof u);
    say("rest: off the shelf at %s (%s): %.1f h still, not worn (%s) -- no night%s", u, why, span / 60.0f, shelf_why(),
        cold ? "; put on cold" : "");                    /* (the line is 128 wide: the shiver, the look up and the slow breath are the item's) */
}

/* a shelf run that moves while still, off the cable: his after all */
static void shelf_worn(int row_minute)
{
    R.st = R_ASLEEP;
    R.night_span = is_night(R.bed_minute >= 0 ? R.bed_minute : row_minute);
    R.moved_minute = row_minute;
    R.awakenings = 0;
    R.move_run = 0;
    R.span_move_min = 0;
    if (is_night(row_minute)) lease_event(REST_EV_NIGHT);   /* THE NIGHT RELEASE: his night after all -- late, but the night (the fidget's hour, not the bed's: a 22:00 bed promoted at 02:00 is the night) */
    if (R.replaying) { R.replay_shelves--; return; }
    char b[8], m[8]; hhmm(R.bed_minute, b, sizeof b); hhmm(row_minute, m, sizeof m);
    say("rest: the shelf moved at %s: worn after all -- asleep since %s (%s)", m, b, R.night_span ? "night" : "a nap by day");
}

/* the span ends at row ``end_seq``; ``why`` is the motion that ended it */
static void span_end(uint32_t end_seq, int end_minute, const char *why)
{
    int span = (int)(end_seq - R.bed_seq);
    if (span < 0) span = 0;
    float h = span / 60.0f;
    bool slept = h >= SLEPT_H;
    char b[8], u[8]; hhmm(R.bed_minute, b, sizeof b); hhmm(end_minute, u, sizeof u);
    R.last_span_min = span;
    R.last_night = slept;
    R.have_last = true;
    R.last_end_minute = end_minute;
    R.last_awakenings = R.awakenings;
    R.last_move_min = R.span_move_min;
    R.last_end_row = R.seq;
    R.st = R_AWAKE;
    R.still_run = 0;
    R.move_run = 0;
    if (R.lease_night) lease_event(REST_EV_RISING);     /* THE NIGHT RELEASE: the announced night ended, whatever its length: the wrist claims back */
    if (slept) R.last_night_id = R.date_now ? rest_date_ordinal(R.date_now) * 1440u + (uint32_t)(((end_minute % 1440) + 1440) % 1440) : 0;
    if (R.replaying) {                                  /* THE NIGHT AFTER A REBOOT: the files' spans, quietly */
        R.replay_spans++;
        if (slept) R.replay_nights++;
        if (slept) snprintf(R.last_words, sizeof R.last_words, "slept %.1f h (%s to %s)", h, b, u);
        else snprintf(R.last_words, sizeof R.last_words, "napped %d min (%s to %s)", span, b, u);
        return;
    }
    if (slept) {
        R.stretch_t = 0;                                /* the morning stretch */
        snprintf(R.sense_kind, sizeof R.sense_kind, "wrist:slept");
        snprintf(R.sense_words, sizeof R.sense_words, "slept %.1f h, %s to %s, %d awakening%s",
                 h, b, u, R.awakenings, R.awakenings == 1 ? "" : "s");
        snprintf(R.last_words, sizeof R.last_words, "slept %.1f h (%s to %s)", h, b, u);
        say("rest: up at %s (%s): slept %.1f h since %s, %d awakening%s, worn at the start %s -- the stretch%s", u, why, h, b,
            R.awakenings, R.awakenings == 1 ? "" : "s", R.worn_start ? "yes" : "no",
            R.span_energy_max == 0 ? " (dead still: on the shelf?)" : "");
    } else {
        snprintf(R.sense_kind, sizeof R.sense_kind, "wrist:napped");
        snprintf(R.sense_words, sizeof R.sense_words, "napped %d min, %s to %s", span, b, u);
        snprintf(R.last_words, sizeof R.last_words, "napped %d min (%s to %s)", span, b, u);
        say("rest: up at %s (%s): a nap of %d min since %s", u, why, span, b);
    }
    R.sense_pending = true;
}

static void stir_begin(int minute)
{
    R.st = R_STIR;
    R.stir_seq = R.seq;
    R.stir_minute = minute;
    R.move_run = 0;
    R.still_run = 0;
    if (R.replaying) return;
    char m[8]; hhmm(minute, m, sizeof m);
    say("rest: a stir at %s (the quiet hours: forgiven if still again)", m);
}

void rest_minute(int minute, uint16_t steps, uint16_t energy_mg, uint8_t still_s, bool worn, bool charging)
{
    (void)still_s;
    bool still = energy_mg < STILL_MG && steps == 0;
    R.seq++;
    ring_push(steps, energy_mg, (uint8_t)((worn ? F_WORN : 0) | (charging ? F_CHARGING : 0)));
    if (still) { R.still_run++; R.move_run = 0; }
    else       { R.still_run = 0; R.move_run++; }
    if (energy_mg < SIT_MG && steps < SIT_STEPS && R.st != R_SHELF) R.sit_run++; else R.sit_run = 0;   /* a shelf is not his sit */
    R.run_row = !R.replaying && steps >= RUN_SPM;        /* THE RUN AND THE PANT: this row a run's (the files' rows never are) */
    R.run_steps = steps;
    if (R.st != R_AWAKE && energy_mg > R.span_energy_max) R.span_energy_max = energy_mg;
    if (R.st != R_AWAKE && R.st != R_SHELF && !still) R.span_move_min++;

    switch (R.st) {
    case R_SHELF:
        if (!still) { shelf_end(minute, "moved", false); break; }
        if (worn && !charging) shelf_worn(minute);         /* a still row that moved, off the cable: the wrist */
        break;
    case R_AWAKE:
        if (!still) break;
        if (R.still_run >= (is_night(minute) ? ONSET_NIGHT_MIN : ONSET_DAY_MIN)) {
            R.span_energy_max = 0;
            for (int i = 1; i <= R.still_run && i <= R.ring_n; i++) {
                uint16_t e = R.energy_ring[(R.ring_head - i + RING) % RING];
                if (e > R.span_energy_max) R.span_energy_max = e;
            }
            span_begin(minute);
        }
        break;
    case R_ASLEEP:
        if (still) break;
        if (R.move_run == AWAKENING_MIN) R.awakenings++;      /* a toss this long is an awakening */
        if (steps >= STEPS_UP) { span_end(R.seq - 1, minute, "steps"); break; }
        if (R.move_run >= TOSS_MAX_MIN) {
            span_end(R.seq - R.move_run, ((minute - R.move_run + 1) % 1440 + 1440) % 1440, "restless");
            break;
        }
        break;
    case R_STIR:
        if (still) {
            if (R.still_run >= REJOIN_MIN) {
                int stirred = (int)(R.seq - R.stir_seq) - R.still_run;
                if (stirred >= AWAKENING_MIN) R.awakenings++;
                R.st = R_ASLEEP;
                R.move_run = 0;
                if (!R.replaying) say("rest: the stir forgiven (still again after %d min); %d awakening%s", stirred, R.awakenings,
                    R.awakenings == 1 ? "" : "s");
            }
            break;
        }
        if (steps >= STEPS_UP) { span_end(R.stir_seq, R.stir_minute, "steps"); break; }
        if ((int)(R.seq - R.stir_seq) >= STIR_MAX_MIN) span_end(R.stir_seq, R.stir_minute, "up for a while");
        break;
    }
}

/* real motion now: a raise, a shake (main's wake) or a walk */
static void motion(const char *why, bool walk)
{
    int m = R.minute_now;
    if (R.st == R_SHELF) { shelf_end(m, why, !walk); return; }    /* picked up: no span (PUT ON COLD reads the hours) */
    if (R.st == R_ASLEEP) {
        if (walk || !is_night(m)) span_end(R.seq, m, why);
        else stir_begin(m);
    } else if (R.st == R_STIR) {
        if (walk) span_end(R.stir_seq, R.stir_minute, why);
    }
}

/* A TOUCH ENDS SLEEP: a hand on the glass is a person awake, whatever
 * the hour -- never a stir. The morning stretch rules as for motion. */
static void rising(const char *why, bool picked_up)
{
    int m = R.minute_now;
    if (R.st == R_ASLEEP) span_end(R.seq, m, why);
    else if (R.st == R_STIR) span_end(R.stir_seq, R.stir_minute, why);
    else if (R.st == R_SHELF) shelf_end(m, why, picked_up);
}
void rest_touch(void) { rising("a hand on the glass", false); }   /* a hand on the shelf's glass is not the wrist put on */
/* THE SHAKEN SLEEPER: a shake is "I'm up" -- the same rising */
void rest_shake(void) { rising("a shake", true); }

/* --- the frame ------------------------------------------------------------------ */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

const rest_out_t *rest_tick(const rest_in_t *in)
{
    float dt = in->dt > 0 ? (in->dt < 0.25f ? in->dt : 0.25f) : 0;
    R.minute_now = in->minute;
    if ((in->quiet ? strncmp(in->quiet, R.quiet_seen, sizeof R.quiet_seen - 1) : R.quiet_seen[0]) != 0) {
        strncpy(R.quiet_seen, in->quiet ? in->quiet : "", sizeof R.quiet_seen - 1);
        set_quiet(in->quiet);
    }
    /* motion: the counter's bump, or steps under the wrist */
    if (!R.have_motion) { R.have_motion = true; R.motion_seen = in->motion_seq; }
    if (in->motion_seq != R.motion_seen) { R.motion_seen = in->motion_seq; motion("a raise or a shake", false); }
    if (in->walking) motion("a walk", true);

    /* the breath from the day */
    /* THE GRAIN: the resting rate is the desk's own breath knob (u(0.92,1.08):
     * Config.BREATH_HZ x knobs["breath"] there), the depth the wrist's
     * breath_amp (u(0.90,1.10)); the day's bands keep their own rates */
    /* VIGOR (vigor.cpp): the cell's level on the same two -- the resting
     * rate x (0.7 + 0.3 v), the depth x (0.8 + 0.2 v) on every band's
     * base; a body running down, wordless */
    float base_amp = R.base_amp * grain_knobs()->breath_amp * vigor_breath_amp_k();
    float rest_hz = R.base_hz * grain_knobs()->breath * vigor_breath_hz_k();
    float thz = rest_hz, tamp = base_amp;
    const char *why = "rest";
    /* THE PULSE AS THE BREATH (pulse.h): a clean pulse, fresh, outranks
     * the day's guesses (the rest, the hour's sit) -- never the sleeper's
     * band nor the walk's. The depth rides the ratio inside the bands'
     * own (the sit's 0.8 .. the walk's 1.2): a quick breath is fuller */
    bool pulse_now = pulse_fresh(in->pulse_bpm, in->pulse_age_s), pulse_led = false;
    R.pulse_bpm = in->pulse_bpm; R.pulse_age_s = in->pulse_age_s;
    if (R.st == R_ASLEEP) { thz = SLEEP_HZ; tamp = base_amp * SLEEP_AMP_X; why = "sleep"; }
    else if (R.run_row && R.st == R_AWAKE) { thz = RUN_HZ; tamp = base_amp * WALK_AMP_X; why = "run"; }   /* THE RUN AND THE PANT: over the walk's and the pulse's clamp */
    else if (in->walking || ring_steps(WALK_WINDOW_MIN) >= WALK_STEPS) { thz = WALK_HZ; tamp = base_amp * WALK_AMP_X; why = "walk"; }
    else if (pulse_now) { thz = pulse_breath_hz(in->pulse_bpm, rest_hz); tamp = base_amp * clampf(thz / rest_hz, SLOW_AMP_X, WALK_AMP_X); why = "pulse"; pulse_led = true; }
    else if (R.sit_run >= STILL_HOUR_MIN) { thz = SLOW_HZ; tamp = base_amp * SLOW_AMP_X; why = "still"; }
    if (pulse_led && !R.pulse_led)
        say("breath: his pulse, %.0f bpm -- the resting breath to %.2f Hz over %.0f s", in->pulse_bpm, thz, PULSE_BREATH_EASE_S);
    else if (!pulse_led && R.pulse_led && !pulse_now && pulse_valid(in->pulse_bpm))
        say("breath: the pulse is %.0f min old -- drifting back to the grain's %.2f Hz over %.0f s", in->pulse_age_s / 60.0f, rest_hz, PULSE_BREATH_EASE_S);
    /* the pulse's ease, 60 s: into its band, and out of it on the way
     * back to the day's rest (the stale drift); the walk's and the
     * sleeper's bands keep their 30 */
    if (!pulse_led && R.pulse_led) R.pulse_drift_t = PULSE_BREATH_EASE_S;
    if (!strcmp(why, "walk") || !strcmp(why, "sleep") || !strcmp(why, "run")) R.pulse_drift_t = 0;
    if (R.pulse_drift_t > 0) { R.pulse_drift_t -= dt; if (R.pulse_drift_t < 0) R.pulse_drift_t = 0; }
    R.pulse_led = pulse_led;
    if (in->lulled > 0.0f && thz > SLEEP_HZ) thz += (SLEEP_HZ - thz) * clampf(in->lulled, 0, 1);   /* THE LULL (lull.c, watch-night C11): petted toward rest -- the target toward the sleeper's rate by the lull's level, the band's own ease; the why unchanged (a look, not a band) */
    float ease_s = (pulse_led || R.pulse_drift_t > 0) ? PULSE_BREATH_EASE_S : BREATH_EASE_S;
    float k = clampf(dt / (ease_s / 3.0f), 0, 1);               /* 95 % of the way by the ease's seconds */
    R.hz += (thz - R.hz) * k;
    R.amp += (tamp - R.amp) * k;
    R.phase += 2.0f * (float)M_PI * R.hz * dt;
    if (R.phase > 2.0f * (float)M_PI) R.phase -= 2.0f * (float)M_PI;
    strncpy(R.breath_why, why, sizeof R.breath_why - 1);
    /* THE WAKING BREATH: the ease runs down; the raise's frame snaps the
     * inhale (after the integration above: the phase IS 0 this frame) --
     * refused dozing, off a cold shelf (shelf_end set the shiver this
     * tick), under a stretch (motion() set it this tick) */
    if (in->chilled && R.st != R_ASLEEP && R.st != R_STIR && !R.replaying) {   /* THE WARM SKIN (warm.c, watch-night C20): the skin read COLD -- PUT ON COLD's road, the look up left out */
        R.cold_shiver_t = COLD_SHIVER_S;
        R.hz = SLEEP_HZ;
        R.amp = R.base_amp * grain_knobs()->breath_amp * vigor_breath_amp_k() * SLEEP_AMP_X;
        R.chills++;
        say("breath: the skin reads cold -- the shiver, the breath snapped to %.2f Hz, warming over %.0f s", SLEEP_HZ, BREATH_EASE_S);
    }
    if (R.wake_x_t > 0) { R.wake_x_t -= dt; if (R.wake_x_t < 0) R.wake_x_t = 0; }
    if (in->woke_raise && R.st != R_ASLEEP && R.st != R_STIR && R.cold_shiver_t <= 0 && R.stretch_t < 0 && !R.replaying) {
        R.phase = 0;
        R.wake_x_t = WAKE_BREATH_EASE_S;
        R.wakes++;
        say("breath: the waking breath -- the inhale now, x%.1f easing to 1 over %.0f s", WAKE_BREATH_X, WAKE_BREATH_EASE_S);
    }
    /* THE RUN AND THE PANT: the run's edges (the band above decided), the
     * pant's clock after it -- in over PANT_IN_S, out over PANT_S on a
     * smoothstep; nothing of it asleep or on the shelf (the state the
     * band's own guard refused) */
    {
        bool run = !strcmp(why, "run");
        if (run && !R.run_on) { R.runs++; say("breath: a run (%u steps in the minute) -- %.2f Hz, the walk's depth, the wind in the eyes", (unsigned)R.run_steps, RUN_HZ); }
        if (!run && R.run_on) { R.pant_t = PANT_S; say("breath: the run is over -- the pant, x%.1f and the crown a hair lower, easing out over %.0f s", PANT_AMP_X, PANT_S); }
        R.run_on = run;
        if (R.pant_t > 0) { R.pant_t -= dt; if (R.pant_t < 0) R.pant_t = 0; }
    }
    float pant_k = 0;
    if (R.pant_t > 0 && R.st == R_AWAKE) {
        float u = R.pant_t / PANT_S;                                /* 1 -> 0 */
        pant_k = u * u * (3.0f - 2.0f * u);
        float t_in = PANT_S - R.pant_t;
        if (t_in < PANT_IN_S) pant_k *= t_in / PANT_IN_S;
    }

    /* the stretch */
    float stretch = 0;
    if (R.stretch_t >= 0) {
        R.stretch_t += dt;
        float t = R.stretch_t;
        if (t < STRETCH_IN_S) stretch = t / STRETCH_IN_S;
        else if (t < STRETCH_IN_S + STRETCH_HOLD_S) stretch = 1;
        else if (t < STRETCH_IN_S + STRETCH_HOLD_S + STRETCH_OUT_S) stretch = 1 - (t - STRETCH_IN_S - STRETCH_HOLD_S) / STRETCH_OUT_S;
        else { stretch = 0; R.stretch_t = -1; }
        stretch = stretch * stretch * (3 - 2 * stretch);
    }

    /* the Zs: only asleep (a stir included: the night is not over) with
     * the glass lit; never on the shelf */
    bool dozing = R.st == R_ASLEEP || R.st == R_STIR;
    bool zs = dozing && in->lit;
    if (!zs) {
        for (int i = 0; i < REST_ZS; i++) out.z[i].on = false;
        R.z_timer = Z_EVERY_S * 0.6f;                      /* the first one soon after the glass lights */
    } else {
        R.z_timer += dt;
        if (R.z_timer >= Z_EVERY_S) {
            for (int i = 0; i < REST_ZS; i++) if (!out.z[i].on) {
                out.z[i].on = true; out.z[i].age = 0; out.z[i].rise = 0; out.z[i].drift = 0; out.z[i].alpha = 0;
                out.z[i].px = (R.z_count++ % 3 == 1) ? 4 : 3;
                R.z_timer = 0;
                break;
            }
        }
        for (int i = 0; i < REST_ZS; i++) {
            rest_z_t *z = &out.z[i];
            if (!z->on) continue;
            z->age += dt;
            float u = z->age / Z_LIFE_S;
            if (u >= 1) { z->on = false; continue; }
            z->rise = Z_RISE_PX * u;
            z->drift = Z_DRIFT_PX * u;
            z->alpha = u < 0.15f ? u / 0.15f : 1.0f - (u - 0.15f) / 0.85f;
        }
    }

    /* THE ITCH IN THE BOWL: the open sit since the last wake, shown as
     * degrees off gravity's angle, the side shifting */
    out.itch_deg = 0;
    if (R.st == R_AWAKE && !in->walking) {
        int sit = R.sit_run;
        if (R.have_last && (int)(R.seq - R.last_end_row) < sit) sit = (int)(R.seq - R.last_end_row);
        if (sit >= ITCH_FROM_MIN) {
            float k = clampf((float)(sit - ITCH_FROM_MIN) / (float)(ITCH_TO_MIN - ITCH_FROM_MIN), 0, 1);
            float deg = ITCH_DEG_MIN + (ITCH_DEG_MAX - ITCH_DEG_MIN) * k;
            out.itch_deg = (((sit - ITCH_FROM_MIN) / ITCH_SHIFT_MIN) & 1) ? deg : -deg;
        }
    }

    /* PUT ON COLD: the shiver's jiggle this frame, the look's seconds */
    out.cold_a2 = 0;
    if (R.cold_shiver_t > 0) {
        float t = COLD_SHIVER_S - R.cold_shiver_t;
        out.cold_a2 = COLD_SHIVER_A2 * sinf(2.0f * (float)M_PI * COLD_SHIVER_HZ * t) * (R.cold_shiver_t / COLD_SHIVER_S);
        R.cold_shiver_t -= dt; if (R.cold_shiver_t < 0) R.cold_shiver_t = 0;
    }
    if (R.cold_look_t > 0) { R.cold_look_t -= dt; if (R.cold_look_t < 0) R.cold_look_t = 0; }
    out.cold_look = R.cold_look_t;

    out.asleep = R.st == R_ASLEEP;
    out.dozing = dozing;
    out.breath_hz = R.hz;
    out.breath_rest_hz = rest_hz;                       /* THE BUMPS: the resting rate, as the target above computes it -- the grain's, never the pulse's */
    out.breath_pulse = pulse_led;                       /* THE PULSE AS THE BREATH: the breath is his pulse's now */
    out.breath_amp = R.amp * (1.0f + (WAKE_BREATH_X - 1.0f) * (R.wake_x_t / WAKE_BREATH_EASE_S))   /* THE WAKING BREATH: the first cycle fuller, easing to the day's */
                           * (1.0f + (PANT_AMP_X - 1.0f) * pant_k);                                 /* THE RUN AND THE PANT: fuller after a run, easing out over PANT_S */
    out.run = R.run_on;
    out.pant_squash = PANT_SQUASH * pant_k;
    out.breath_phase = R.phase;
    out.stretch = stretch;
    if (R.sense_pending && in->linked && !out.sense_new) {
        out.sense_new = true;
        strncpy(out.sense_kind, R.sense_kind, sizeof out.sense_kind - 1);
        strncpy(out.sense_words, R.sense_words, sizeof out.sense_words - 1);
        R.sense_pending = false;
    }
    if (!out.log_new && R.log_n) {
        strncpy(out.log, R.logs[0], sizeof out.log - 1);
        out.log_new = true;
        memmove(R.logs[0], R.logs[1], sizeof R.logs[0] * 2);
        R.log_n--;
    }
    return &out;
}

void rest_sense_sent(void) { out.sense_new = false; }
void rest_log_taken(void) { out.log_new = false; }

void rest_begin(float breath_hz, float breath_amp)
{
    memset(&R, 0, sizeof R);
    memset(&out, 0, sizeof out);
    R.st = R_AWAKE;
    R.base_hz = R.hz = breath_hz;
    R.base_amp = R.amp = breath_amp;
    R.stretch_t = -1;
    R.minute_now = -1;
    R.night_from = NIGHT_FROM_MIN;
    R.night_to = NIGHT_TO_MIN;
    R.z_timer = Z_EVERY_S * 0.6f;
    strcpy(R.breath_why, "rest");
    strcpy(R.last_words, "-");
}

int rest_span_min(void)
{
    if (R.st == R_AWAKE) return R.last_span_min;
    return (int)(R.seq - R.bed_seq);                    /* the shelf: its still run so far */
}

int rest_night_min(void)
{
    if (R.st == R_SHELF) return 0;                      /* the shelf is nobody's night */
    if (R.st != R_AWAKE) return R.night_span ? (int)(R.seq - R.bed_seq) : 0;
    return R.last_night ? R.last_span_min : 0;
}

const char *rest_state_name(void) { return STATE_NAMES[R.st]; }
bool rest_dozing_now(void) { return R.st == R_ASLEEP || R.st == R_STIR; }
bool rest_on_shelf(void) { return R.st == R_SHELF; }
bool rest_cold(void) { return (R.colds || R.chills) && !strcmp(R.breath_why, "rest") && R.hz < R.base_hz * grain_knobs()->breath * vigor_breath_hz_k() * 0.9f; }

/* --- the tell's facts (tell.cpp) ------------------------------------------- */
bool rest_last_span(rest_span_t *o)
{
    if (!R.have_last) return false;
    o->minutes = R.last_span_min;
    o->end_minute = R.last_end_minute;
    o->awakenings = R.last_awakenings;
    o->restless_min = R.last_move_min;
    o->end_row = R.last_end_row;
    o->night = R.last_night;
    return true;
}
uint32_t rest_rows(void) { return R.seq; }
int rest_sit_min(void) { return R.sit_run; }
int rest_steps_last(int rows) { return ring_steps(rows); }
int rest_rows_since_walk(void)
{
    for (int i = 1; i <= R.ring_n; i++)
        if (R.steps_ring[(R.ring_head - i + RING) % RING] >= SIT_STEPS) return i - 1;
    return -1;
}
bool rest_night_now(void) { return is_night(R.minute_now); }
bool rest_night_at(int minute) { return is_night(minute); }   /* QUIET TIME (wake.cpp): the nap's own clock, not the last frame's */

/* --- THE NIGHT AFTER A REBOOT (rest.h says why) ------------------------------- */

/* how many live rows may have been lived clockless before the files
 * are older than what the wrist saw: a night's onset */
static const uint32_t REPLAY_LIVE_MAX = 20;

void rest_set_date(uint32_t yyyymmdd) { R.date_now = yyyymmdd; }

/* days from civil (Howard Hinnant's), and back */
uint32_t rest_date_ordinal(uint32_t d)
{
    int y = (int)(d / 10000); unsigned m = (d / 100) % 100, dd = d % 100;
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + dd - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (uint32_t)(era * 146097 + (int)doe - 719468);
}

uint32_t rest_date_prev(uint32_t d)
{
    int32_t z = (int32_t)rest_date_ordinal(d) - 1 + 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = (int)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned dd = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp + (mp < 10 ? 3 : -9);
    y += m <= 2;
    return (uint32_t)y * 10000u + m * 100u + dd;
}

bool rest_replay_begin(void)
{
    if (R.seq >= REPLAY_LIVE_MAX) {
        say("rest: %lu clockless minutes lived already: fresher than the files, not replayed", (unsigned long)R.seq);
        return false;
    }
    /* the detector from nothing (the few clockless rows before the clock
     * came are dropped: the files are the truth); the breath's rate, the
     * quiet hours and what main handed over stay */
    R.st = R_AWAKE;
    R.seq = 0; R.still_run = R.sit_run = R.move_run = 0;
    R.bed_seq = R.stir_seq = 0; R.bed_minute = R.stir_minute = -1;
    R.awakenings = 0; R.night_span = false; R.span_energy_max = 0; R.span_move_min = 0;
    R.worn_start = false; R.shelf_cable = false; R.moved_minute = -1; R.replay_shelves = 0;
    R.lease_ev = REST_EV_NONE; R.lease_night = false;   /* THE NIGHT RELEASE: a rebuilt night was never announced by this boot (the morning's courtesy dial claims) */
    R.last_span_min = 0; R.last_night = false; R.have_last = false;
    R.last_end_minute = -1; R.last_awakenings = R.last_move_min = 0; R.last_end_row = 0;
    R.last_night_id = 0;
    strcpy(R.last_words, "-");
    R.ring_n = R.ring_head = 0;
    R.replaying = true;
    R.replay_spans = R.replay_nights = 0;
    R.stretch_t = -1;
    R.sense_pending = false;
    R.run_row = false; R.run_on = false; R.pant_t = 0;   /* THE RUN AND THE PANT: the files' rows never run, never pant */
    return true;
}

void rest_replay_row(uint32_t date, int minute, uint16_t steps, uint16_t energy_mg, uint8_t still_s,
                     uint8_t raises, bool shaken, uint8_t taps, bool worn, bool charging, bool up)
{
    int keep_minute = R.minute_now;
    R.date_now = date;
    R.minute_now = minute;
    rest_minute(minute, steps, energy_mg, still_s, worn, charging);
    /* the row's raise / shake is the motion the live path saw that
     * minute; its "I'm up" shake the rising (THE SHAKEN SLEEPER); its
     * taps the hand on the glass (A TOUCH ENDS SLEEP) */
    if (up) rest_shake();
    if (raises || shaken) motion("a raise (from the file)", false);
    if (taps) rest_touch();
    R.minute_now = keep_minute;
}

void rest_replay_end(uint32_t date_a, int rows_a, uint32_t date_b, int rows_b)
{
    R.replaying = false;
    R.rebuilt = true;
    if (R.st != R_AWAKE) {
        char b[8]; hhmm(R.bed_minute, b, sizeof b);
        say("rest: rebuilt from the files (%d + %d rows): %s since %s, %d span%s before it",
            rows_a, rows_b, STATE_NAMES[R.st], b, R.replay_spans, R.replay_spans == 1 ? "" : "s");
    } else {
        char sh[24] = "";
        if (R.replay_shelves) snprintf(sh, sizeof sh, ", %d shelf run%s", R.replay_shelves, R.replay_shelves == 1 ? "" : "s");
        say("rest: rebuilt from the files (%d + %d rows): %d span%s, %d night%s%s; last %s",
            rows_a, rows_b, R.replay_spans, R.replay_spans == 1 ? "" : "s",
            R.replay_nights, R.replay_nights == 1 ? "" : "s", sh, R.last_words);
    }
    (void)date_a; (void)date_b;
}

bool rest_rebuilt(void) { return R.rebuilt; }
uint32_t rest_night_id(void) { return R.have_last && R.last_night ? R.last_night_id : 0; }

void rest_status(char *out_s, int cap)
{
    char b[8], f[8], t[8];
    hhmm(R.bed_minute, b, sizeof b);
    hhmm(R.night_from, f, sizeof f);
    hhmm(R.night_to, t, sizeof t);
    if (R.st == R_AWAKE)
        snprintf(out_s, cap, "rest: awake, still %d min sit %d min (itch %+.0f deg) | breath %.2f Hz amp %.3f (%s) | last %s | night %s-%s | rows %lu%s",
                 R.still_run, R.sit_run, out.itch_deg, R.hz, R.amp, R.breath_why, R.last_words, f, t, (unsigned long)R.seq,
                 R.sense_pending ? " | a sense waiting for the link" : "");
    else if (R.st == R_SHELF)                           /* THE SHELF RULE */
        snprintf(out_s, cap, "rest: on the shelf since %s (%.1f h still, not worn: %s) | breath %.2f Hz amp %.3f (%s) | last %s | night %s-%s | rows %lu",
                 b, (R.seq - R.bed_seq) / 60.0f, shelf_why(), R.hz, R.amp, R.breath_why, R.last_words, f, t,
                 (unsigned long)R.seq);
    else {
        char w[24];
        if (R.worn_start) snprintf(w, sizeof w, "yes");
        else { char m[8]; hhmm(R.moved_minute, m, sizeof m); snprintf(w, sizeof w, "no, moved at %s", m); }
        snprintf(out_s, cap, "rest: %s since %s (%.1f h, %d awakening%s%s, worn at the start %s) | breath %.2f Hz amp %.3f (%s) | last %s | night %s-%s | rows %lu",
                 STATE_NAMES[R.st], b, (R.seq - R.bed_seq) / 60.0f, R.awakenings, R.awakenings == 1 ? "" : "s",
                 R.night_span ? ", the night" : ", a nap", w, R.hz, R.amp, R.breath_why, R.last_words, f, t,
                 (unsigned long)R.seq);
    }
    if (R.rebuilt) {                                    /* THE NIGHT AFTER A REBOOT */
        int n = (int)strlen(out_s);
        snprintf(out_s + n, cap > n ? cap - n : 0, " | rebuilt from the files (night id %lu)", (unsigned long)rest_night_id());
    }
    {                                                   /* THE PULSE AS THE BREATH: the pulse the breath was handed, and whether it leads */
        int n = (int)strlen(out_s);
        if (R.pulse_bpm > 0)
            snprintf(out_s + n, cap > n ? cap - n : 0, " | pulse %.0f bpm, %.0f min old: %s", R.pulse_bpm, R.pulse_age_s / 60.0f,
                     R.pulse_led ? "the breath follows it" : !pulse_valid(R.pulse_bpm) ? "past the honest band, ignored"
                     : !pulse_fresh(R.pulse_bpm, R.pulse_age_s) ? "stale, the grain's rate" : "the run's, the walk's or the sleeper's band wins");
        else snprintf(out_s + n, cap > n ? cap - n : 0, " | no pulse known");
    }
    if (R.wakes) {                                      /* THE WAKING BREATH: how many raises found the inhale */
        int n = (int)strlen(out_s);
        snprintf(out_s + n, cap > n ? cap - n : 0, " | waking breaths %lu%s", (unsigned long)R.wakes, R.wake_x_t > 0 ? " (one easing now)" : "");
    }
    if (R.runs) {                                       /* THE RUN AND THE PANT: the runs seen, the pant's seconds left */
        int n = (int)strlen(out_s);
        if (R.run_on) snprintf(out_s + n, cap > n ? cap - n : 0, " | runs %lu (running now)", (unsigned long)R.runs);
        else if (R.pant_t > 0) snprintf(out_s + n, cap > n ? cap - n : 0, " | runs %lu (panting, %.0f s left)", (unsigned long)R.runs, R.pant_t);
        else snprintf(out_s + n, cap > n ? cap - n : 0, " | runs %lu", (unsigned long)R.runs);
    }
}
