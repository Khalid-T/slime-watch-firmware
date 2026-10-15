/* THE TELL -- tell.h says what; this is how. Everything here is a curve
 * of squash over a second or two, an eye kind to wear meanwhile, and a
 * kick for a spring: the same vocabulary the pet, the stretch and the
 * splat use, nothing new on the body. No Arduino: tools/tell_test.cpp
 * runs it on the laptop. */
#include "tell.h"
#include "gel.h"
#include "grain.h"          /* THE GRAIN: the mood's first face, regard's cooler notch */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* --- the tunables, with their cause ----------------------------------------- */
/* the answer waits for the body's own wake to finish: the crumple eases
 * out over WAKE_EASE_S 0.26 (a raise), the arrival rises over RISE_S 0.6
 * (a knock) -- the first second is the FIRST SECOND HE IS UP */
static const float DELAY_RAISE_S    = 0.30f;
static const float DELAY_ARRIVE_S   = 0.70f;
/* THE NIGHT: watch-daylog 2's sleep.kind -- restless: restlessness >= 0.20,
 * or >= 3 awakenings, or < 6 h; sound: < 0.10, <= 1 awakening, >= 6 h;
 * fair otherwise (nothing to say) */
static const int   SHORT_NIGHT_MIN  = 6 * 60;
static const float RESTLESS_FRAC    = 0.20f;
static const float SOUND_FRAC       = 0.10f;
static const int   RESTLESS_AWAKE   = 3;
static const int   SOUND_AWAKE      = 1;
/* the morning face is for the FIRST raise of the morning: within 2 h of
 * the span's end (later it is just the day) */
static const int   MORNING_WITHIN_MIN = 120;
/* the droopy tell: heavy lids, a slow low settle (the sluggish morning
 * the desk shows as `droopy` -- watch-daylog 4) */
static const float DROOPY_S         = 1.5f;
static const float DROOPY_SQUASH    = 0.10f;
/* THE SIT: watch-daylog 4's itch at 60 min, still open -- the stretch
 * urge is rest.cpp's stretch (tall -0.30, held 0.6 s, eyes wide) and a
 * shake-off wobble on the release */
static const int   SIT_MIN          = 60;
static const float SIT_IN_S         = 0.10f;
static const float SIT_HOLD_S       = 0.60f;
static const float SIT_OUT_S        = 0.20f;
static const float SIT_SQUASH       = -0.30f;
static const float SHAKE_OFF_KICK   = 1.6f;     /* spring1: a small lateral wobble, under the desk's landing kick 3.2 */
/* THE WALK: steps in the last hour, or the day's norm met -- the content
 * bob is two soft squash pulses, the drink's settle twice (gesture.cpp: 0.08) */
static const int   WALK_STEPS_HOUR  = 1500;
static const float BOB_PULSE_S      = 0.5f;
static const float BOB_GAP_S        = 0.1f;
static const float BOB_SQUASH       = 0.08f;
/* THE MORNING: happy eyes for the stretch's length and a little beyond;
 * the bright settle comes once rest.cpp's stretch (1.0 s) has let go */
static const float MORNING_S        = 1.8f;
static const float MORNING_SETTLE_AT_S = 1.0f;
static const float MORNING_SETTLE_S = 0.5f;
static const float MORNING_SQUASH   = 0.10f;
/* THE STEP BOB: a walk = WALK_MIN_STEPS within WALK_WINDOW_S after
 * WALK_GAP_S without a step (so a fidget of two steps is not a walk);
 * the bob lasts STEP_BOB_S from the walk's first step -- the desk's run
 * band -- then the anti-Sonic law: only the breath quickens */
static const int   WALK_MIN_STEPS   = 6;
static const float WALK_WINDOW_S    = 6.0f;
static const float WALK_GAP_S       = 60.0f;
static const float STEP_BOB_S       = 10.0f;
static const float STEP_PULSE_S     = 0.12f;
static const float STEP_PULSE_SQUASH = 0.06f;
/* THE WALK AS A LUMP: a walk of WALK_LUMP_STEPS or more, once it ends
 * (the gap), is a meal -- a small lump (gel.h's term; the feed's is
 * 1.0) that sinks away over WALK_LUMP_S the way the feed's does over
 * its minute (the same ease-out). A new walk's lump replaces, never
 * adds: one belly. */
static const int   WALK_LUMP_STEPS  = 300;
static const float WALK_LUMP        = 0.35f;
static const float WALK_LUMP_S      = 20.0f * 60.0f;
/* THE NIGHT LIGHT: long enough to read the hour, not to be a lamp */
static const float NIGHT_LIGHT_S    = 3.0f;
/* THE MOOD'S FACE (THE GRAIN): a raise with no fact to tell wears the
 * desk's carried mood for its first second, then the ordinary creature;
 * never a squash, never a kick -- a colour, not a cause. Under
 * REGARD_SHY_AT (grain.cpp) the first face is one notch cooler:
 * content -> open, happy -> content */
static const float MOOD_S           = 1.0f;
/* THE YAWN AT DUSK (design/watch-ideas.md A2): in the hour before the
 * quiet hours (tell_in_t.dusk), once an evening (the date is its id; the
 * row clock's day with no date), on a raise while holding -- after any
 * untold fact (the fact is the first second's; the yawn takes the next
 * raise in the hour), before the mood's face. The shape: the sleep bar
 * for a beat (YAWN_BAR_S), a tall intake (to YAWN_TALL over YAWN_IN_S,
 * the lids droopy), a slow slump (from tall through rest to YAWN_SLUMP
 * over YAWN_SLUMP_S, still droopy), the settle back (YAWN_OUT_S), then
 * the ordinary face. Tall is under the stretch's -0.30: a yawn, not a
 * morning. */
static const float YAWN_BAR_S       = 0.35f;
static const float YAWN_IN_S        = 0.50f;
static const float YAWN_TALL        = -0.20f;
static const float YAWN_SLUMP_S     = 1.20f;
static const float YAWN_SLUMP       = 0.08f;
static const float YAWN_OUT_S       = 0.60f;

/* UNDER THE WEATHER (watch-ideas D34; nightsum.c says when): the day's
 * fact, never fresher than another -- a day-long state, the next raise
 * is soon enough; WEATHER_AGE is its standing in the freshest-first race */
static const int   WEATHER_AGE      = 1 << 20;

static const char *const FACT_NAMES[] = { "nothing", "a restless night", "a long sit", "a good walk", "a sound night", "the mood carried", "dusk", "under the weather", "the desk's day" };
static const char *const TELL_NAMES[] = { "-", "droopy", "the stretch", "the content bob", "the morning face", "the mood's face", "the yawn", "droopy", "the other house's day" };

/* regard's notch: the warm faces one step cooler when it has been handled badly */
static int cooler(int eye)
{
    if (!grain_knobs()->cool) return eye;
    if (eye == GEL_EYE_CONTENT) return GEL_EYE_OPEN;
    if (eye == GEL_EYE_HAPPY) return GEL_EYE_CONTENT;
    return eye;
}

/* --- the state ------------------------------------------------------------- */
static struct {
    /* the glance that answers */
    int    fact;                /* TELL_* playing (or waiting its delay) */
    float  t;                   /* seconds into it, from the delay's end */
    float  delay;               /* seconds still to wait */
    bool   shook;               /* the sit's shake-off given */
    uint32_t told_id[TELL_ARRIVE + 1];        /* per fact: the id told (0 = none), and */
    bool   told_v[TELL_ARRIVE + 1];         /*   whether one was */
    bool   arrive_sat; float arrive_len;   /* THE OTHER HOUSE'S DAY: heavy lids (true) or content, and how long */
    uint32_t told_norm_date; bool told_norm_v;   /* the walk fact's second id: the norm's day */
    uint32_t yawn_day; bool yawn_v;              /* THE YAWN AT DUSK: the evening it yawned (the date, or the row clock's day) */
    uint32_t weather_day; bool weather_v;        /* UNDER THE WEATHER: the day it wore it (the same id) */
    char   last_said[64];
    /* the step bob */
    double T;                   /* the tell's own clock, seconds */
    double last_step_T;
    uint32_t step_seen;         /* wrist_step_seq() as last seen (0 at boot, as the counter is) */
    double cand_T0; int cand_n; /* a walk candidate: the first step after the gap and the steps since */
    bool   walk_on; double walk_T0; int walk_steps, walk_bobs;
    float  pulse_t;             /* the step pulse's seconds left */
    float  lump_t;              /* the walk's lump: its clock, counting down from WALK_LUMP_S (0 = none) */
    /* the night light */
    bool   light_on; float light_t;
    /* the log queue */
    char   logs[2][112]; int log_n;
    tell_in_t last_in;          /* the frame's numbers, for `tell` over serial */
} S;

static tell_out_t out;

static void say(const char *fmt, ...)
{
    if (S.log_n >= 2) { memmove(S.logs[0], S.logs[1], sizeof S.logs[0]); S.log_n = 1; }
    va_list ap; va_start(ap, fmt);
    vsnprintf(S.logs[S.log_n], sizeof S.logs[0], fmt, ap);
    va_end(ap);
    S.log_n++;
}

static void hhmm(int minute, char *o, int cap)
{
    if (minute < 0) { snprintf(o, cap, "--:--"); return; }
    minute = ((minute % 1440) + 1440) % 1440;
    snprintf(o, cap, "%02d:%02d", minute / 60, minute % 60);
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
/* a half-sine pulse: 0 -> amp -> 0 over ``len`` from t0 */
static float pulse(float t, float t0, float len, float amp)
{
    float u = (t - t0) / len;
    return u > 0 && u < 1 ? amp * sinf((float)M_PI * u) : 0.0f;
}

/* --- the facts ------------------------------------------------------------- */

/* the night's kind (watch-daylog 2): 0 fair, 1 restless, 2 sound; only a NIGHT (>= 3 h) */
static int night_kind(const tell_in_t *in)
{
    if (!in->span_known || !in->span_night || in->span_min <= 0) return 0;
    float frac = (float)in->span_restless_min / (float)in->span_min;
    if (frac >= RESTLESS_FRAC || in->span_awakenings >= RESTLESS_AWAKE || in->span_min < SHORT_NIGHT_MIN) return 1;
    if (frac < SOUND_FRAC && in->span_awakenings <= SOUND_AWAKE) return 2;
    return 0;
}

/* the sit he is in NOW: rest.cpp's sit run counts the night too (a
 * sleeper is under 60 mg), so it begins at the wake at the earliest */
static int sit_now(const tell_in_t *in)
{
    int sit = in->sit_min;
    if (in->span_known) {
        int since = (int)(in->rows - in->span_end_row);
        if (since >= 0 && since < sit) sit = since;
    }
    return sit;
}

/* the freshest untold fact: its kind, its id and how many rows old it
 * is (the moment it became true); ``why`` says it in the log's words */
static int freshest(const tell_in_t *in, uint32_t *id, int *age, char *why, int cap)
{
    int best = TELL_NONE, best_age = 1 << 30; uint32_t best_id = 0;
    char w[96] = "";
    /* HE ARRIVES WITH THE OTHER HOUSE'S DAY (C21): on the ARRIVAL's cue the
     * desk's day is the fact of the moment itself (age 0: nothing the wrist
     * knows is fresher than where he just came from) -- the long session's
     * heavy lids first, else the petted afternoon's content face; the
     * wrist's own untold fact keeps for the next raise. Never on a raise. */
    if (in->cue == TELL_CUE_ARRIVAL) {
        if (in->desk_sat_min >= ARRIVE_SAT_MIN) { best = TELL_ARRIVE; best_age = 0; snprintf(w, sizeof w, "sat %d min at the desk", in->desk_sat_min); }
        else if (in->desk_pets >= ARRIVE_PETS) { best = TELL_ARRIVE; best_age = 0; snprintf(w, sizeof w, "petted %d times today at the desk", in->desk_pets); }
    }
    /* the night, either kind */
    int nk = night_kind(in);
    if (nk) {
        int a = (int)(in->rows - in->span_end_row);
        int f = nk == 1 ? TELL_NIGHT : TELL_MORNING;
        bool ok = true;
        if (f == TELL_MORNING && a > MORNING_WITHIN_MIN) ok = false;     /* not the morning any more: just the day */
        if (ok && !(S.told_v[f] && S.told_id[f] == in->span_end_row) && a < best_age) {
            best = f; best_age = a; best_id = in->span_end_row;
            char e[8]; hhmm(in->span_end_minute, e, sizeof e);
            snprintf(w, sizeof w, "%s: %.1f h, %d awakening%s, %d restless min, up at %s", nk == 1 ? "restless" : "sound",
                     in->span_min / 60.0f, in->span_awakenings, in->span_awakenings == 1 ? "" : "s", in->span_restless_min, e);
        }
    }
    /* the sit, still open */
    int sit = sit_now(in);
    if (sit >= SIT_MIN) {
        uint32_t sid = in->rows - (uint32_t)sit;             /* the sit's start row */
        int a = sit - SIT_MIN;                               /* true since it crossed the hour */
        if (!(S.told_v[TELL_SIT] && S.told_id[TELL_SIT] == sid) && a < best_age) {
            best = TELL_SIT; best_age = a; best_id = sid;
            snprintf(w, sizeof w, "sitting %d min", sit);
        }
    }
    /* the walk: the hour's steps, or the day's norm met */
    {
        bool hour_ok = in->steps_hour >= WALK_STEPS_HOUR;
        uint32_t hid = in->rows / 60;
        bool norm_ok = in->norm && in->steps_today >= in->norm;
        uint32_t nid = in->date ? in->date : in->rows / 1440;
        bool untold = (hour_ok && !(S.told_v[TELL_WALK] && S.told_id[TELL_WALK] == hid))
                   || (norm_ok && !(S.told_norm_v && S.told_norm_date == nid));
        int a = in->rows_since_walk < 0 ? 60 : in->rows_since_walk;
        if (untold && a < best_age) {
            best = TELL_WALK; best_age = a; best_id = hour_ok ? hid : nid;
            snprintf(w, sizeof w, "%d steps this hour, %lu today%s", in->steps_hour, (unsigned long)in->steps_today,
                     norm_ok ? " (the norm met)" : "");
        }
    }
    /* UNDER THE WEATHER: the day's resting heart over the week's -- the stalest fact, once a day */
    if (in->weather_hr && in->weather_norm) {
        uint32_t wid = in->date ? in->date : in->rows / 1440;
        if (!(S.weather_v && S.weather_day == wid) && WEATHER_AGE < best_age) {
            best = TELL_WEATHER; best_age = WEATHER_AGE; best_id = wid;
            snprintf(w, sizeof w, "resting %d vs the week's %d", in->weather_hr, in->weather_norm);
        }
    }
    if (id) *id = best_id;
    if (age) *age = best_age;
    if (why) snprintf(why, cap, "%s", w);
    return best;
}

uint32_t tell_night_told_row(void)
{
    if (S.told_v[TELL_NIGHT]) return S.told_id[TELL_NIGHT];
    if (S.told_v[TELL_MORNING]) return S.told_id[TELL_MORNING];
    return 0;
}

void tell_told_night(uint32_t span_end_row)
{
    S.told_v[TELL_NIGHT] = S.told_v[TELL_MORNING] = true;       /* one night, either face of it */
    S.told_id[TELL_NIGHT] = S.told_id[TELL_MORNING] = span_end_row;
}

static void mark_told(int fact, uint32_t id, const tell_in_t *in)
{
    if (fact == TELL_WEATHER) { S.weather_v = true; S.weather_day = id; return; }   /* its own id: the day */
    if (fact == TELL_ARRIVE) return;        /* the other house's day has no id: the arrival's cue is its once */
    S.told_v[fact] = true; S.told_id[fact] = id;
    if (fact == TELL_WALK && in->norm && in->steps_today >= in->norm) {
        S.told_norm_v = true; S.told_norm_date = in->date ? in->date : in->rows / 1440;
    }
}

/* --- the frame ------------------------------------------------------------- */

const tell_out_t *tell_tick(const tell_in_t *in)
{
    float dt = in->dt > 0 ? (in->dt < 0.25f ? in->dt : 0.25f) : 0;
    S.T += dt;
    S.last_in = *in;
    out.squash = 0; out.eye_kind = -1; out.kick1 = out.kick2 = 0; out.night_done = false;

    /* THE NIGHT LIGHT: a raise while dozing in the quiet hours */
    if (in->cue == TELL_CUE_RAISE && in->dozing && in->night && !S.light_on) {
        S.light_on = true; S.light_t = 0;
        char m[8]; hhmm(in->minute, m, sizeof m);
        say("night light: a raise at %s while asleep -- the dimmest glass for %.0f s, the hour in the shade", m, NIGHT_LIGHT_S);
    }
    if (S.light_on) {
        S.light_t += dt;
        if (!in->dozing || !in->lit) {                       /* he is up (a walk), or the glass went dark on its own */
            S.light_on = false;
            say("night light: %s", in->lit ? "he is up -- the ordinary wake" : "the glass went dark");
        } else if (S.light_t >= NIGHT_LIGHT_S) {
            S.light_on = false;
            out.night_done = true;
            say("night light: out");
        }
    }
    out.night_light = S.light_on;

    /* THE GLANCE THAT ANSWERS: the cue picks the freshest untold fact */
    if (in->cue != TELL_CUE_NONE && in->holding && !in->dozing && S.fact == TELL_NONE) {
        uint32_t id; int age; char why[96];
        int f = freshest(in, &id, &age, why, sizeof why);
        if (f != TELL_NONE) {
            S.fact = f; S.t = 0; S.shook = false;
            S.delay = in->cue == TELL_CUE_ARRIVAL ? DELAY_ARRIVE_S : DELAY_RAISE_S;
            if (f == TELL_ARRIVE) {               /* the other house's day: the shape and its length, once per arrival (the cue is the once) */
                S.arrive_sat = in->desk_sat_min >= ARRIVE_SAT_MIN;
                float x = S.arrive_sat ? (float)in->desk_sat_min / (float)ARRIVE_SAT_MIN : 1.0f;
                S.arrive_len = S.arrive_sat ? ARRIVE_SAT_S * (x > ARRIVE_SAT_X ? ARRIVE_SAT_X : x) : ARRIVE_PETS_S;
            }
            mark_told(f, id, in);
            snprintf(S.last_said, sizeof S.last_said, "%s (%s)", TELL_NAMES[f], FACT_NAMES[f]);
            if (f == TELL_WEATHER) say("tell: under the weather (%s) -- the droopy first face, once today", why);
            else say("tell: %s -- %s (%s; %d min fresh, once)", TELL_NAMES[f], FACT_NAMES[f], why, age);
        } else if (in->cue == TELL_CUE_RAISE && in->dusk && !(S.yawn_v && S.yawn_day == (in->date ? in->date : in->rows / 1440))) {
            /* THE YAWN AT DUSK: nothing to tell, the night an hour off -- once an evening */
            S.fact = TELL_YAWN; S.t = 0; S.delay = DELAY_RAISE_S;
            S.yawn_v = true; S.yawn_day = in->date ? in->date : in->rows / 1440;
            snprintf(S.last_said, sizeof S.last_said, "%s (%s)", TELL_NAMES[TELL_YAWN], FACT_NAMES[TELL_YAWN]);
            char m[8]; hhmm(in->minute, m, sizeof m);
            say("tell: the yawn at dusk (%s, the quiet hours within the hour; once an evening)", m);
        } else if (in->cue == TELL_CUE_RAISE && grain_knobs()->mood_eye >= 0) {
            /* THE GRAIN: nothing to tell -- the carried mood's face, a second */
            S.fact = TELL_MOOD; S.t = 0; S.delay = DELAY_RAISE_S;
            say("tell: the mood's face (%s from the desk) for %.0f s%s", grain_mood(), MOOD_S, grain_knobs()->cool ? ", a notch cooler" : "");
        }
    }
    if (S.fact != TELL_NONE) {
        if (in->asleep || !in->holding) S.fact = TELL_NONE;   /* the glass emptied or he dozed off mid-word: nothing */
        else if (S.delay > 0) S.delay -= dt;
        else {
            float t = S.t; S.t += dt;
            switch (S.fact) {
            case TELL_NIGHT:
            case TELL_WEATHER:                                /* UNDER THE WEATHER wears the restless night's face, to the frame */
                out.eye_kind = GEL_EYE_DROOPY;
                out.squash = pulse(t, 0, DROOPY_S, DROOPY_SQUASH);
                if (t >= DROOPY_S) S.fact = TELL_NONE;
                break;
            case TELL_SIT: {
                float s = 0;
                if (t < SIT_IN_S) s = t / SIT_IN_S;
                else if (t < SIT_IN_S + SIT_HOLD_S) s = 1;
                else if (t < SIT_IN_S + SIT_HOLD_S + SIT_OUT_S) s = 1 - (t - SIT_IN_S - SIT_HOLD_S) / SIT_OUT_S;
                out.squash = SIT_SQUASH * smooth(clampf(s, 0, 1));
                out.eye_kind = t < SIT_IN_S + SIT_HOLD_S ? GEL_EYE_WIDE : GEL_EYE_OPEN;
                if (t >= SIT_IN_S + SIT_HOLD_S && !S.shook) { S.shook = true; out.kick1 = SHAKE_OFF_KICK; }   /* the shake-off, once */
                if (t >= SIT_IN_S + SIT_HOLD_S + SIT_OUT_S + 0.4f) S.fact = TELL_NONE;   /* the wobble settles under open eyes */
                break;
            }
            case TELL_WALK:
                out.eye_kind = cooler(GEL_EYE_CONTENT);
                out.squash = pulse(t, 0, BOB_PULSE_S, BOB_SQUASH) + pulse(t, BOB_PULSE_S + BOB_GAP_S, BOB_PULSE_S, BOB_SQUASH);
                if (t >= 2 * BOB_PULSE_S + BOB_GAP_S) S.fact = TELL_NONE;
                break;
            case TELL_MORNING:
                out.eye_kind = cooler(GEL_EYE_HAPPY);
                out.squash = pulse(t, MORNING_SETTLE_AT_S, MORNING_SETTLE_S, MORNING_SQUASH);
                if (t >= MORNING_S) S.fact = TELL_NONE;
                break;
            case TELL_MOOD:
                out.eye_kind = cooler(grain_knobs()->mood_eye);
                if (t >= MOOD_S) S.fact = TELL_NONE;
                break;
            case TELL_ARRIVE:
                /* THE OTHER HOUSE'S DAY: heavy lids and the droopy tell's slow low
                 * settle stretched to the session's length, or the content face
                 * with one soft settle (the bob's pulse) -- never a word */
                if (S.arrive_sat) { out.eye_kind = GEL_EYE_DROOPY; out.squash = pulse(t, 0, S.arrive_len, DROOPY_SQUASH); }
                else { out.eye_kind = cooler(GEL_EYE_CONTENT); out.squash = pulse(t, 0, BOB_PULSE_S, BOB_SQUASH); }
                if (t >= S.arrive_len) S.fact = TELL_NONE;
                break;
            case TELL_YAWN: {
                /* the sleep bar for a beat; the tall intake; the slow slump
                 * past rest; the settle back; the ordinary face */
                const float T_IN = YAWN_BAR_S, T_SLUMP = T_IN + YAWN_IN_S, T_OUT = T_SLUMP + YAWN_SLUMP_S, T_END = T_OUT + YAWN_OUT_S;
                if (t < YAWN_BAR_S) { out.eye_kind = GEL_EYE_SLEEP; out.squash = 0; }
                else if (t < T_SLUMP) { out.eye_kind = GEL_EYE_DROOPY; out.squash = YAWN_TALL * smooth((t - T_IN) / YAWN_IN_S); }
                else if (t < T_OUT) { out.eye_kind = GEL_EYE_DROOPY; out.squash = YAWN_TALL + (YAWN_SLUMP - YAWN_TALL) * smooth((t - T_SLUMP) / YAWN_SLUMP_S); }
                else if (t < T_END) { out.eye_kind = GEL_EYE_DROOPY; out.squash = YAWN_SLUMP * (1.0f - smooth((t - T_OUT) / YAWN_OUT_S)); }
                else S.fact = TELL_NONE;
                break;
            }
            default: S.fact = TELL_NONE; break;
            }
        }
    }

    /* THE STEP BOB: each counted step, for the first seconds of a walk */
    uint32_t fresh = in->step_seq - S.step_seen;
    S.step_seen = in->step_seq;
    if (fresh > 4) fresh = 4;                                 /* a burst after a nap is not four steps in a frame */
    for (uint32_t i = 0; i < fresh; i++) {
        if (S.last_step_T < 0 || S.T - S.last_step_T >= WALK_GAP_S) {   /* the gap: a new candidate */
            S.cand_T0 = S.T; S.cand_n = 0;
            S.walk_on = false;
        }
        S.last_step_T = S.T;
        S.cand_n++;
        if (!S.walk_on && S.cand_n >= WALK_MIN_STEPS && S.T - S.cand_T0 <= WALK_WINDOW_S) {
            S.walk_on = true; S.walk_T0 = S.cand_T0; S.walk_steps = S.cand_n - 1; S.walk_bobs = 0;
            say("bob: walking");
        }
        if (S.walk_on) {
            S.walk_steps++;
            if (S.T - S.walk_T0 < STEP_BOB_S && !in->asleep && in->holding) { S.pulse_t = STEP_PULSE_S; S.walk_bobs++; }
        }
    }
    if (S.walk_on && S.T - S.last_step_T >= WALK_GAP_S) {
        S.walk_on = false;
        if (S.walk_steps >= WALK_LUMP_STEPS) {               /* THE WALK AS A LUMP: he ate the walk */
            S.lump_t = WALK_LUMP_S;
            say("bob: the walk is over (%d steps): a lump, sinking over %.0f min", S.walk_steps, WALK_LUMP_S / 60.0f);
        }
    }
    if (S.lump_t > 0) { S.lump_t -= dt; if (S.lump_t < 0) S.lump_t = 0; }
    if (S.pulse_t > 0) {
        out.squash += STEP_PULSE_SQUASH * sinf((float)M_PI * (1.0f - S.pulse_t / STEP_PULSE_S));
        S.pulse_t -= dt;
        if (S.pulse_t < 0) S.pulse_t = 0;
    }

    if (!out.log_new && S.log_n) {
        strncpy(out.log, S.logs[0], sizeof out.log - 1);
        out.log_new = true;
        memmove(S.logs[0], S.logs[1], sizeof S.logs[0]);
        S.log_n--;
    }
    return &out;
}

void tell_log_taken(void) { out.log_new = false; }

void tell_begin(void)
{
    memset(&S, 0, sizeof S);
    memset(&out, 0, sizeof out);
    out.eye_kind = -1;
    S.last_step_T = -1;
    strcpy(S.last_said, "-");
}

int  tell_active(void) { return S.fact; }
bool tell_walking(void) { return S.walk_on; }
float tell_lump(void)
{
    if (S.lump_t <= 0) return 0;
    float left = S.lump_t / WALK_LUMP_S;
    return WALK_LUMP * left * left;     /* the feed's ease-out, at the walk's size */
}

void tell_status(char *o, int cap)
{
    const tell_in_t *in = &S.last_in;
    uint32_t id; int age; char why[96];
    int f = freshest(in, &id, &age, why, sizeof why);
    int n = snprintf(o, cap, "tell: now %s%s%s%s | said %s | told: night %s sit %s walk %s norm %s",
                     f == TELL_NONE ? "nothing" : TELL_NAMES[f], f == TELL_NONE ? "" : " (", f == TELL_NONE ? "" : why,
                     f == TELL_NONE ? "" : ")", S.last_said,
                     S.told_v[TELL_NIGHT] || S.told_v[TELL_MORNING] ? "yes" : "no", S.told_v[TELL_SIT] ? "yes" : "no",
                     S.told_v[TELL_WALK] ? "yes" : "no", S.told_norm_v ? "yes" : "no");
    if (n < cap)
        snprintf(o + n, cap - n, " | bob: %s (%d steps, %d bobs) | night light %s | dusk %s, yawned %s | weather: %s%s | the desk's day: sat %d min, pets %d",
                 S.walk_on ? "walking" : "resting", S.walk_steps, S.walk_bobs, S.light_on ? "on" : "off",
                 in->dusk ? "yes" : "no", S.yawn_v ? "this evening" : "not yet",
                 in->weather_hr ? "under it" : "fine", S.weather_v ? ", worn today" : "", in->desk_sat_min, in->desk_pets);
}
