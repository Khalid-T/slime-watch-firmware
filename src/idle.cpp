/* IDLE LIFE -- idle.h says what; this is how.
 *
 * Two clocks: REST (how long the body has been at rest, lit, held and
 * unbothered -- reset by anything else) and the GAP (since the last
 * thing ended; the next one waits a jittered while). When both are ripe
 * and the pet's quiet has passed, the die picks a kind from the
 * weights, never the last kind, never a blink with the eyes down. The
 * thing plays by its own clock and writes a few deltas; the eyes and
 * the squash EASE (a presence eased up over EYE_UP_S and down over
 * EYE_DOWN_S, the settle on a half sine), the gaze is integer and steps.
 * Anything else taking the body aborts the thing; the eases carry it
 * off, nothing snaps but the gaze.
 *
 * No Arduino. */
#include "idle.h"
#include "gel.h"
#include "grain.h"          /* THE GRAIN: the die's tilt, the gap, the settle's depth, the blink's length */
#include "vigor.h"          /* VIGOR: a body running down does fewer small things; spent, none at all */
#include <string.h>
#include <stdio.h>
#include <math.h>

/* --- the tunables, with their cause ----------------------------------------- */
/* at rest this long before the first thing: a creature settles before
 * it looks about; anything sooner reads as restlessness */
static const float IDLE_MIN_S      = 25.0f;
/* one small thing every so often, jittered so it never reads as a loop */
static const float IDLE_EVERY_MIN_S = 60.0f;
static const float IDLE_EVERY_MAX_S = 120.0f;
/* nothing for this long after a pet: the pet's face has the stage */
static const float PET_QUIET_S     = 10.0f;
/* the look: a slow sweep of the gaze, out and back */
static const float LOOK_S          = 2.0f;
static const float LOOK_BACK_S     = 0.6f;
/* the blink: the desk's own BLINK_CLOSE_MS feel */
static const float BLINK_S         = 0.12f;
/* the settle: a small sink and a small lean, in and out on a half sine */
static const float SETTLE_S        = 1.5f;
static const float SETTLE_SQUASH   = 0.06f;
static const float SETTLE_A1       = 0.04f;
/* the bubble: the rim's ripple quickening -- the gel clock advances
 * BUBBLE_X laps per resting lap for BUBBLE_S (GEL_GEL_HZ 0.12: a
 * resting lap is 8 s; x3 for 3 s is not quite one extra lap) */
static const float BUBBLE_S        = 3.0f;
static const float BUBBLE_X        = 3.0f;
/* the glance up: the gaze lifted, eyes content, held */
static const float GLANCE_S        = 1.5f;
/* THE ROAM (THE SMALL HOUSE, 2026-10-09): the gaze walking the rim once
 * round over ROAM_S -- a creature measuring its bowl; its weight at a
 * full cramp ROAM_WEIGHT (the look's 30: as common as a look when it is
 * truly cramped, nothing at all when it is not) */
static const float ROAM_S          = 3.0f;
static const int   ROAM_WEIGHT     = 30;
/* the eyes brought up for a look: eased up (not snapped -- the desk's
 * eyes snap for a pet, but no one touched him), held EYE_LINGER_S after,
 * then down at main's own EYE_FADE_S */
static const float EYE_UP_S        = 0.35f;
static const float EYE_LINGER_S    = 1.0f;
static const float EYE_DOWN_S      = 0.55f;
/* THE GRAIN's tilt: the favourite kind's weight up by a quarter (30 -> 37
 * for the look, 10 -> 12 for the glance up): a little more its own */
static const int   FAVOUR_NUM      = 5, FAVOUR_DEN = 4;
/* THE OLD HABIT (design/watch-ideas.md A10, 2026-09-23): the favourite,
 * ONCE A LIT SPELL, plays a touch bigger -- HABIT_X on the look's sweep
 * (2 -> 3 s, the eyes held with it), the glance up's hold, the bubble's
 * spell (and so its laps), the blink's close (a slow blink), and on the
 * SETTLE'S DEPTH (0.06 -> 0.09, still under the stroke's 0.10; its
 * length and lean as ever). The same vocabulary, a little more of it;
 * never a hop. The debt is owed afresh when the glass lights and spent
 * on the favourite's first roll in that spell */
static const float HABIT_X         = 1.5f;
/* the weights, with their cause (blink redistributed when the eyes are down) */
static const struct { int kind; int weight; const char *cause; } WEIGHTS[] = {
    { IDLE_LOOK,      30, "a creature at a window looks about most" },
    { IDLE_BLINK,     25, "eyes that are up blink" },
    { IDLE_SETTLE,    20, "gel settles under its own weight" },
    { IDLE_BUBBLE,    15, "a bubble rises now and then" },
    { IDLE_GLANCE_UP, 10, "the rarest: up, toward him" },
    { IDLE_ROAM,       0, "THE SMALL HOUSE: round the rim, by the cramp (weighed at the roll)" },
};
static float cramp_now = 0;        /* this frame's cramp level (in->cramp) */

/* --- the state ------------------------------------------------------------- */
static struct {
    uint32_t die;
    uint32_t now_ms;
    float    rest_s, gap_s, next_gap_s;
    float    pet_quiet_s;       /* seconds of quiet still owed after a pet */
    int      playing;           /* IDLE_* in progress, or IDLE_NONE */
    float    t;                 /* the thing's own clock */
    int      last;
    uint32_t last_ms;
    int      count;
    /* the eased outputs */
    float    eye, eye_target, eye_hold_s;
    float    squash, a1;        /* the settle's, from its phase */
    float    gel_extra;
    int      gaze_x, gaze_y;
    int      eye_kind;
    bool     eyes_up_seen;      /* main's or ours >= 0.5 this frame */
    /* THE OLD HABIT */
    bool     lit_prev;          /* the glass last frame: a rising edge is a new lit spell */
    bool     habit_owed;        /* this spell's habit not yet played */
    float    big;               /* the thing in progress: HABIT_X for the habit's, else 1 */
} I;

static uint32_t roll(void)
{
    I.die = I.die * 1664525u + 1013904223u;
    return I.die >> 8;
}
static float rollf(void) { return (roll() & 0xFFFF) / 65535.0f; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the next gap: 60-120 s jittered, then THE GRAIN's own pace (bored x
 * regard's liveliness -- the desk's dwell rule), then VIGOR's
 * (x (1 + (1 - v)): a low cell, fewer small things) */
static float next_gap(void)
{
    return (IDLE_EVERY_MIN_S + rollf() * (IDLE_EVERY_MAX_S - IDLE_EVERY_MIN_S)) * grain_knobs()->idle_gap * vigor_idle_k();
}
static int weight_of(unsigned i)
{
    int w = WEIGHTS[i].weight;
    if (WEIGHTS[i].kind == IDLE_ROAM) return (int)lroundf(ROAM_WEIGHT * clampf(cramp_now, 0, 1));   /* THE SMALL HOUSE: 0 off the table */
    if (WEIGHTS[i].kind == grain_knobs()->favourite) w = (w * FAVOUR_NUM + FAVOUR_DEN / 2) / FAVOUR_DEN;
    return w;
}

void idle_begin(uint32_t seed)
{
    memset(&I, 0, sizeof I);
    I.die = seed ? seed : 1;
    I.eye_kind = -1;
    I.last = IDLE_NONE;
    I.next_gap_s = next_gap();
    I.habit_owed = true;        /* the boot's spell owes it too */
    I.big = 1.0f;
}

static void end(void);
/* a pet, from the glass or the HTTP page: the thing in progress is over
 * (the pet's face has the stage) and the quiet after it begins */
void idle_pet(void)
{
    I.pet_quiet_s = PET_QUIET_S;
    if (I.playing != IDLE_NONE) end();
}

static int pick(bool eyes_up)
{
    int total = 0;
    for (unsigned i = 0; i < sizeof WEIGHTS / sizeof WEIGHTS[0]; i++) {
        if (WEIGHTS[i].kind == I.last) continue;
        if (WEIGHTS[i].kind == IDLE_BLINK && !eyes_up) continue;
        if (weight_of(i) <= 0) continue;
        total += weight_of(i);
    }
    if (total <= 0) return IDLE_NONE;
    int r = (int)(roll() % (uint32_t)total);
    for (unsigned i = 0; i < sizeof WEIGHTS / sizeof WEIGHTS[0]; i++) {
        if (WEIGHTS[i].kind == I.last) continue;
        if (WEIGHTS[i].kind == IDLE_BLINK && !eyes_up) continue;
        if (weight_of(i) <= 0) continue;
        r -= weight_of(i);
        if (r < 0) return WEIGHTS[i].kind;
    }
    return IDLE_NONE;
}

static void start(int kind)
{
    I.playing = kind;
    I.t = 0;
    I.last = kind;
    I.last_ms = I.now_ms ? I.now_ms : 1;
    I.count++;
    /* THE OLD HABIT: the favourite's first roll this spell plays bigger */
    I.big = 1.0f;
    if (I.habit_owed && kind != IDLE_NONE && kind == grain_knobs()->favourite) { I.habit_owed = false; I.big = HABIT_X; }
    switch (kind) {
    case IDLE_LOOK:      I.eye_target = 1; I.eye_hold_s = LOOK_S * I.big + LOOK_BACK_S + EYE_LINGER_S; I.eye_kind = GEL_EYE_OPEN; break;
    case IDLE_GLANCE_UP: I.eye_target = 1; I.eye_hold_s = GLANCE_S * I.big + EYE_LINGER_S; I.eye_kind = GEL_EYE_CONTENT; I.gaze_y = -1; break;
    case IDLE_BLINK:     I.eye_kind = GEL_EYE_CLOSED; break;
    case IDLE_ROAM:      I.eye_target = 1; I.eye_hold_s = ROAM_S + EYE_LINGER_S; I.eye_kind = GEL_EYE_OPEN; break;
    case IDLE_SETTLE:    I.a1 = (roll() & 1) ? SETTLE_A1 : -SETTLE_A1; break;
    default: break;
    }
}

static void end(void)
{
    I.playing = IDLE_NONE;
    I.gaze_x = I.gaze_y = 0;
    I.eye_kind = -1;
    I.squash = 0;
    I.a1 = 0;
    I.gap_s = 0;
    I.next_gap_s = next_gap();
    I.big = 1.0f;
}

void idle_tick(const idle_in_t *in)
{
    float dt = in->dt;
    I.now_ms = in->now_ms;
    cramp_now = in->cramp;      /* THE SMALL HOUSE: the roam's weight at this frame's roll */
    /* THE OLD HABIT: a new lit spell owes it again */
    if (in->lit && !I.lit_prev) I.habit_owed = true;
    I.lit_prev = in->lit;
    if (I.pet_quiet_s > 0) I.pet_quiet_s -= dt;
    bool alive = in->lit && in->holding && !in->asleep;
    bool free_ = alive && !in->busy;
    bool at_rest = free_ && !in->moving;

    /* anything else taking the body ends the thing (the eases carry it off) */
    if (I.playing != IDLE_NONE && !free_) end();

    /* the clocks */
    if (at_rest) I.rest_s += dt; else I.rest_s = 0;
    if (alive) I.gap_s += dt;
    if (!alive) I.eye_target = 0;

    /* the thing in progress */
    if (I.playing != IDLE_NONE) {
        I.t += dt;
        float t = I.t;
        switch (I.playing) {
        case IDLE_LOOK: {
            float look_s = LOOK_S * I.big;                      /* THE OLD HABIT: a longer look */
            if (t < look_s) I.gaze_x = (int)lroundf(-2.0f * cosf((float)M_PI * t / look_s));   /* -2 .. 2, slow at the ends */
            else if (t < look_s + LOOK_BACK_S) I.gaze_x = (int)lroundf(2.0f * cosf((float)M_PI * 0.5f * (t - look_s) / LOOK_BACK_S));
            else end();
            break;
        }
        case IDLE_BLINK:
            if (t >= BLINK_S * grain_knobs()->blink * I.big) end();     /* THE GRAIN: the desk's own blink knob; THE OLD HABIT: a slow blink */
            break;
        case IDLE_SETTLE: {
            float ph = clampf(t / SETTLE_S, 0, 1);
            float s = sinf((float)M_PI * ph);
            I.squash = SETTLE_SQUASH * grain_knobs()->settle * I.big * s;   /* THE GRAIN: the settle's depth; THE OLD HABIT: deeper */
            if (ph >= 1.0f) end();
            break;
        }
        case IDLE_BUBBLE:
            I.gel_extra += (BUBBLE_X - 1.0f) * GEL_GEL_HZ * dt;
            if (I.gel_extra >= 1.0f) I.gel_extra -= 1.0f;
            if (t >= BUBBLE_S * I.big) end();                   /* THE OLD HABIT: a longer bubble */
            break;
        case IDLE_GLANCE_UP:
            if (t >= GLANCE_S * I.big) end();                   /* THE OLD HABIT: a longer glance up */
            break;
        case IDLE_ROAM: {   /* THE SMALL HOUSE: once round the rim -- x -2..2 on a cosine, y up at the top of the turn */
            float ph = 2.0f * (float)M_PI * t / ROAM_S;
            I.gaze_x = (int)lroundf(-2.0f * cosf(ph));
            I.gaze_y = (int)lroundf(-1.0f * sinf(ph)) < 0 ? -1 : 0;
            if (t >= ROAM_S) end();
            break;
        }
        default: end(); break;
        }
    }

    /* the eyes we brought up: held a while after the thing, then down */
    if (I.eye_hold_s > 0) { I.eye_hold_s -= dt; if (I.eye_hold_s <= 0) I.eye_target = 0; }
    if (I.eye < I.eye_target) I.eye = clampf(I.eye + dt / EYE_UP_S, 0, I.eye_target);
    else if (I.eye > I.eye_target) I.eye = clampf(I.eye - dt / EYE_DOWN_S, I.eye_target, 1);
    I.eyes_up_seen = (in->eye_alpha >= 0.5f) || (I.eye >= 0.5f);

    /* the roll (never spent: VIGOR under VIGOR_EMPTY wears the sleep look and does nothing of its own) */
    if (I.playing == IDLE_NONE && at_rest && I.pet_quiet_s <= 0 && !vigor_spent()
        && I.rest_s >= IDLE_MIN_S && I.gap_s >= I.next_gap_s) {
        int k = pick(I.eyes_up_seen);
        if (k != IDLE_NONE) start(k);
        else I.gap_s = 0;
    }
}

void idle_fx(idle_fx_t *out)
{
    out->gaze_x = I.gaze_x;
    out->gaze_y = I.gaze_y;
    out->eye_kind = I.playing != IDLE_NONE ? I.eye_kind : -1;
    /* the settle's lean rides its own phase (a1 written at the start, scaled here) */
    float lean_k = I.playing == IDLE_SETTLE ? sinf((float)M_PI * clampf(I.t / SETTLE_S, 0, 1)) : 0;
    out->a1 = I.a1 * lean_k;
    out->squash = I.squash;
    out->eye_alpha = I.eye;
    out->gel_extra = I.gel_extra;
}

bool idle_playing(void) { return I.playing != IDLE_NONE; }
bool idle_habit_owed(void) { return I.habit_owed && grain_knobs()->favourite != 0; }
bool idle_habit_playing(void) { return I.playing != IDLE_NONE && I.big > 1.0f; }
int idle_last(void) { return I.last; }
uint32_t idle_last_ms(void) { return I.last_ms; }
int idle_count(void) { return I.count; }

static const char *const NAMES[IDLE_KINDS] = { "-", "a look around", "a blink", "a settle", "a bubble", "a glance up", "the roam round the rim" };
const char *idle_name(int kind) { return (kind >= 0 && kind < IDLE_KINDS) ? NAMES[kind] : "?"; }

void idle_status(char *out, int cap)
{
    if (I.last == IDLE_NONE)
        snprintf(out, cap, "idle: nothing yet | at rest %.0f s (needs %.0f) | next in %.0f s | pet quiet %.0f s | %d things",
                 I.rest_s, IDLE_MIN_S, I.next_gap_s - I.gap_s > 0 ? I.next_gap_s - I.gap_s : 0, I.pet_quiet_s > 0 ? I.pet_quiet_s : 0, I.count);
    else
        snprintf(out, cap, "idle: last %s, %lu s ago%s | at rest %.0f s | next in %.0f s (gap %.0f) | eyes %.2f | %d things | the habit %s",
                 idle_name(I.last), (unsigned long)((I.now_ms - I.last_ms) / 1000u), I.playing ? (I.big > 1.0f ? " (playing, the habit)" : " (playing)") : "",
                 I.rest_s, I.next_gap_s - I.gap_s > 0 ? I.next_gap_s - I.gap_s : 0, I.next_gap_s, I.eye, I.count,
                 grain_knobs()->favourite == 0 ? "none (no grain)" : I.habit_owed ? "owed this spell" : "spent this spell");
}
