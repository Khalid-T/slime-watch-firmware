/* THE SMALL HOUSE -- cramp.h says what; this is how.
 *
 * Three clocks: the CRAMP (0..1, growing on lit holding seconds, spent
 * by the outlets), the IGNORED spell (lit holding seconds since the
 * last touch -- the wrist's own bid), and the GAPS (since the last
 * nudge, since the last bid). The body's part is a few deltas: the
 * stance's squash (eased, never a snap), the push's lift on a half
 * sine, the look up's gaze, the answer's eyes. The nudge is a ROLL at
 * fullness -- the slime's own choice, logged either way.
 *
 * No Arduino. */
#include "cramp.h"
#include "gel.h"            /* GEL_EYE_CONTENT: the answer's eyes */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

/* the stance eases in and out over this: the width of a cramped gel
 * is a slow fact, never a frame's jump (the outlets drop the level at
 * once; the body follows) */
static const float STANCE_EASE_S = 8.0f;

static struct {
    uint32_t die;
    uint32_t now_ms;
    float    level;            /* the cramp, 0..1 */
    float    shown;            /* the stance's eased level */
    float    lit_s;            /* lit, holding seconds since the arrival (the clock's own tally) */
    float    ignored_s;        /* lit, holding seconds since the last touch */
    float    push_t;           /* the push's clock, -1 = none */
    float    next_push_s;      /* the fidget's next push, counted down while the clock runs */
    float    look_t;           /* the look up's clock, -1 = none */
    float    look_s;           /* how long this look holds */
    float    content_t;        /* the answer's content eyes, -1 = none */
    /* the nudge */
    bool     open;             /* a nudge waits for his answer */
    float    open_s;           /* seconds since it was said */
    uint32_t nudge_ms;         /* the last nudge (said or rolled away), 0 = never */
    bool     kept;             /* the last roll kept the wish to itself (the why's word while the gap holds) */
    bool     said_alone;       /* "a watch alone in the world is simply home" said for this fullness */
    bool     wished;           /* a nudge was said and not answered this stay: the hold after is the wish granted */
    int      why;              /* CRAMP_WHY_* */
    /* the bid */
    bool     bid_open;
    float    bid_open_s;
    uint32_t bid_ms;           /* the last bid, 0 = never */
    /* the tallies */
    uint32_t rolls, nudges, bids, stays, answered_bids, pushes;   /* rolls: every roll at fullness, said or kept */
    /* the log */
    char     log[220];
    bool     log_new;
    int      event;
} C;

static const char *WHY_NAMES[CRAMP_WHY_N] = {
    "ready: it rolls at fullness", "not full", "a gate is shut (dark, asleep, the shelf, the cable, the quiet hours)",
    "resting: the small warm bowl is the place for it", "no bigger house reachable: a watch alone is simply home",
    "inside the hour's gap", "it rolled to keep the wish to itself this time", "a nudge is open: waiting for him", "the body is busy",
    "the desk is reachable but FAR (the air door's signal): not near",
};

static uint32_t roll(void)
{
    C.die = C.die * 1664525u + 1013904223u;
    return C.die >> 8;
}
static float rollf(void) { return (roll() & 0xFFFF) / 65535.0f; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(C.log, sizeof C.log, fmt, ap);
    va_end(ap);
    C.log_new = true;
}

static float next_push(float level)
{
    float base = CRAMP_PUSH_EVERY_MIN_S + rollf() * (CRAMP_PUSH_EVERY_MAX_S - CRAMP_PUSH_EVERY_MIN_S);
    return base / (level > 0.05f ? level : 0.05f);
}

static void start_push(void) { C.push_t = 0; C.pushes++; }

static void start_look(float s)
{
    C.look_t = 0; C.look_s = s;
}

void cramp_begin(uint32_t seed)
{
    memset(&C, 0, sizeof C);
    C.die = seed ? seed : 1;
    C.push_t = -1; C.look_t = -1; C.content_t = -1;
    C.next_push_s = next_push(1.0f);
    C.why = CRAMP_WHY_NOT_FULL;
}

static void fresh(void)
{
    C.level = 0; C.lit_s = 0; C.ignored_s = 0;
    C.push_t = -1; C.look_t = -1; C.content_t = -1;
    C.open = false; C.open_s = 0; C.kept = false; C.said_alone = false; C.wished = false;
    C.bid_open = false; C.bid_open_s = 0;
    C.next_push_s = next_push(1.0f);
}

void cramp_arrive(void)
{
    fresh();
    say("cramp: a fresh arrival -- the small house, no cramp yet");
}

void cramp_release(const char *why)
{
    if (C.wished || C.open) {
        say("cramp: he let it go to the desk (%s) -- the wish granted", why);
    } else if (C.level > 0.05f) {
        say("cramp: the house emptied (%s) with the cramp at %.2f -- zeroed", why, C.level);
    }
    fresh();
    C.shown = 0;             /* the body is gone with it: no stance to ease off */
}

/* --- the outlets and his answers ------------------------------------------- */

static void spend(float k, const char *what)
{
    float was = C.level;
    C.level = clampf(C.level - k, 0, 1);
    if (C.level < 0.5f) C.said_alone = false;   /* the next fullness says it again */
    if (was > 0.05f) say("cramp: %s spends %.2f -> %.2f", what, was, C.level);
}

void cramp_touch(uint32_t now_ms)
{
    (void)now_ms;
    C.ignored_s = 0;
}

void cramp_pet(uint32_t now_ms)
{
    C.ignored_s = 0;
    if (C.open) {
        /* "stay a while": content eyes, one settle, the cramp halved, the hour's gap from the nudge */
        C.open = false; C.wished = false;
        C.level = clampf(C.level * CRAMP_STAY_K, 0, 1);
        C.kept = false; C.said_alone = false;
        C.content_t = 0; C.look_t = -1;
        C.stays++;
        C.event = CRAMP_EV_STAY;
        say("cramp: he says stay a while (a pet %.0f s after the nudge) -- content; the cramp halved to %.2f, no nudge for %.0f min",
            C.open_s, C.level, CRAMP_GAP_MIN);
        (void)now_ms;
        return;
    }
    if (C.bid_open) {
        C.bid_open = false;
        C.content_t = 0; C.look_t = -1;
        C.answered_bids++;
        C.event = CRAMP_EV_BID_ANSWERED;
        say("cramp: the bid answered (a pet %.0f s after) -- content", C.bid_open_s);
    }
    spend(CRAMP_SPEND_PET, "a pet");
}

void cramp_play(uint32_t now_ms)
{
    (void)now_ms;
    C.ignored_s = 0;
    spend(CRAMP_SPEND_PLAY, "a game");
}

/* --- the frame ---------------------------------------------------------------- */

void cramp_tick(const cramp_in_t *in)
{
    float dt = in->dt;
    C.now_ms = in->now_ms;
    bool alive = in->lit && in->holding;
    bool gate = alive && !in->asleep && !in->shelf && !in->cable && !in->quiet;
    bool runs = gate && !in->resting;      /* the clock runs: lit, holding, awake, worn, off the cable, by day, lively */

    /* the clocks */
    if (runs) {
        C.lit_s += dt;
        C.ignored_s += dt;
        if (in->walking) {
            float was = C.level;
            C.level = clampf(C.level - CRAMP_SPEND_WALK_PER_MIN * dt / 60.0f, 0, 1);
            if (was >= 0.5f && C.level < 0.5f) C.said_alone = false;
        } else {
            C.level = clampf(C.level + dt / (CRAMP_FULL_MIN * 60.0f), 0, 1);
        }
    }
    /* the stance eases toward the level while the gel is drawn; gone, it is gone */
    if (in->holding) {
        float step = dt / STANCE_EASE_S;
        if (C.shown < C.level) C.shown = clampf(C.shown + step, 0, C.level);
        else if (C.shown > C.level) C.shown = clampf(C.shown - step, C.level, 1);
    } else C.shown = 0;

    /* the things in progress */
    if (C.push_t >= 0) { C.push_t += dt; if (C.push_t >= CRAMP_PUSH_S) C.push_t = -1; }
    if (C.look_t >= 0) { C.look_t += dt; if (C.look_t >= C.look_s) C.look_t = -1; }
    if (C.content_t >= 0) { C.content_t += dt; if (C.content_t >= CRAMP_CONTENT_S) C.content_t = -1; }

    /* an open nudge waits CRAMP_ANSWER_S for his pet; silence = nothing more this hour */
    if (C.open) {
        C.open_s += dt;
        if (!alive) {
            C.open = false;   /* the glass went dark / the house emptied under it: the wish stands (the release's line says if it was granted) */
        } else if (C.open_s >= CRAMP_ANSWER_S) {
            C.open = false;
            say("cramp: no answer to the nudge in %.0f s -- it keeps the wish to itself for %.0f min (the cramp stays at %.2f)",
                CRAMP_ANSWER_S, CRAMP_GAP_MIN, C.level);
        }
    }
    if (C.bid_open) {
        C.bid_open_s += dt;
        if (!alive || C.bid_open_s >= CRAMP_ANSWER_S) {
            C.bid_open = false;
            if (alive) say("cramp: the bid went unanswered -- back to its life, not again for %.0f min", BID_GAP_MIN);
        }
    }

    bool free_ = runs && !in->busy && C.push_t < 0 && C.look_t < 0;

    /* THE NUDGE: at fullness, the slime's own roll -- once per fullness, once per hour */
    bool gap = C.nudge_ms && (uint32_t)(in->now_ms - C.nudge_ms) < (uint32_t)(CRAMP_GAP_MIN * 60000.0f);
    bool desk_fresh = in->desk_age_s >= 0 && in->desk_age_s <= CRAMP_DESK_FRESH_MIN * 60.0f;
    bool desk_near = desk_fresh && !in->desk_far;   /* THE DESK NEXT DOOR: reachable, and not FAR by the air door's signal (no signal = near, as before) */
    if (C.open) C.why = CRAMP_WHY_OPEN;
    else if (C.level < 1.0f) C.why = CRAMP_WHY_NOT_FULL;
    else if (!gate) C.why = CRAMP_WHY_GATE;
    else if (in->resting) C.why = CRAMP_WHY_RESTING;
    else if (!desk_fresh) C.why = CRAMP_WHY_NO_DESK;
    else if (!desk_near) C.why = CRAMP_WHY_FAR;
    else if (gap && C.kept) C.why = CRAMP_WHY_ROLLED_NO;
    else if (gap) C.why = CRAMP_WHY_GAP;
    else if (in->busy || C.push_t >= 0 || C.look_t >= 0) C.why = CRAMP_WHY_BUSY;
    else C.why = CRAMP_WHY_READY;

    if (C.level >= 1.0f && runs && !C.open) {
        if (!desk_near) {
            if (!C.said_alone) {
                C.said_alone = true;
                if (desk_fresh) say("cramp: full, and the bigger house is reachable but not near (the desk FAR by the air door's signal) -- no wish said across a room: the fidget only");   /* THE DESK NEXT DOOR */
                else say("cramp: full, and nowhere bigger is reachable (the desk's door %s) -- a watch alone in the world is simply home: the fidget only",
                    in->desk_age_s < 0 ? "never answered" : "answered too long ago");
            }
        } else if (!gap && free_) {
            C.nudge_ms = in->now_ms ? in->now_ms : 1;
            float odds = clampf(CRAMP_NUDGE_ODDS + CRAMP_NUDGE_REGARD_W * clampf(in->regard, -1, 1), 0.10f, 0.95f);
            float r = rollf();
            C.rolls++;
            C.kept = r >= odds;
            if (!C.kept) {
                C.open = true; C.open_s = 0; C.wished = true;
                C.nudges++;
                start_push();
                start_look(CRAMP_NUDGE_LOOK_S);
                C.event = CRAMP_EV_NUDGE;
                say("cramp: full after %.0f lit min, the desk near (%.0f s ago) -- it chose to say so (rolled %.2f under %.2f): one bump, the push toward him, a look up; `wrist:cramped` up the wire",
                    C.lit_s / 60.0f, in->desk_age_s, r, odds);
            } else {
                say("cramp: full after %.0f lit min, the desk near -- it chose to keep the wish to itself (rolled %.2f over %.2f): the fidget only, another roll in %.0f min",
                    C.lit_s / 60.0f, r, odds, CRAMP_GAP_MIN);
            }
            C.why = C.open ? CRAMP_WHY_OPEN : CRAMP_WHY_ROLLED_NO;
        }
    }

    /* THE BID: a long ignored lit spell, once per BID_GAP_MIN, the body free */
    if (free_ && !C.open && !C.bid_open && C.ignored_s >= BID_IGNORED_MIN * 60.0f
        && (!C.bid_ms || (uint32_t)(in->now_ms - C.bid_ms) >= (uint32_t)(BID_GAP_MIN * 60000.0f))) {
        C.bid_ms = in->now_ms ? in->now_ms : 1;
        C.bid_open = true; C.bid_open_s = 0;
        C.ignored_s = 0;
        C.bids++;
        start_look(BID_LOOK_S);
        start_push();
        C.event = CRAMP_EV_BID;
        say("cramp: the wrist's own bid -- %.0f lit min without a touch: a look up, one bump, the small push toward him", BID_IGNORED_MIN);
        free_ = false;
    }

    /* THE FIDGET: the push now and then, from a quarter cramped, the gap as 1/level */
    if (free_ && C.level >= CRAMP_PUSH_FROM) {
        C.next_push_s -= dt;
        if (C.next_push_s <= 0) {
            start_push();
            C.next_push_s = next_push(C.level);
        }
    }
}

void cramp_fx(cramp_fx_t *out)
{
    out->squash = CRAMP_SQUASH * C.shown;
    out->push_px = C.push_t >= 0 ? CRAMP_PUSH_PX * sinf((float)M_PI * clampf(C.push_t / CRAMP_PUSH_S, 0, 1)) : 0;
    out->gaze_y = C.look_t >= 0 ? -1 : 0;
    out->eye_kind = C.content_t >= 0 ? GEL_EYE_CONTENT : -1;
    out->eye_alpha = (C.look_t >= 0 || C.content_t >= 0) ? 1.0f : 0;
}

int cramp_take_event(void) { int e = C.event; C.event = CRAMP_EV_NONE; return e; }
float cramp_level(void) { return C.level; }
float cramp_lit_min(void) { return C.lit_s / 60.0f; }
bool cramp_nudge_open(void) { return C.open; }
int cramp_why(void) { return C.why; }
const char *cramp_why_name(int why) { return (why >= 0 && why < CRAMP_WHY_N) ? WHY_NAMES[why] : "?"; }
uint32_t cramp_rolls(void) { return C.rolls; }
uint32_t cramp_nudges(void) { return C.nudges; }
uint32_t cramp_bids(void) { return C.bids; }
bool cramp_log_new(void) { return C.log_new; }
const char *cramp_log(void) { return C.log; }
void cramp_log_taken(void) { C.log_new = false; }

void cramp_status(char *out, int cap)
{
    snprintf(out, cap, "cramp %.2f (stance %.2f) | lit %.1f min since the arrival, ignored %.1f min | rolls %lu, nudges %lu (stays %lu), last %s%s | bids %lu (answered %lu), last %s | pushes %lu, next in %.0f s | now: %s",
             C.level, C.shown, C.lit_s / 60.0f, C.ignored_s / 60.0f,
             (unsigned long)C.rolls, (unsigned long)C.nudges, (unsigned long)C.stays,
             C.nudge_ms ? (C.kept ? "kept" : "said") : "never", C.open ? " (OPEN: waiting for his pet)" : "",
             (unsigned long)C.bids, (unsigned long)C.answered_bids, C.bid_ms ? (C.bid_open ? "open" : "done") : "never",
             (unsigned long)C.pushes, C.next_push_s > 0 ? C.next_push_s : 0,
             cramp_why_name(C.why));
    if (C.nudge_ms && cap > 0) {
        int n = (int)strlen(out);
        if (n < cap) snprintf(out + n, cap - n, " | the last roll %lu s ago", (unsigned long)((C.now_ms - C.nudge_ms) / 1000u));
    }
}
