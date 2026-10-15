/* THE HOUSE -- house.h says what; this is how. */
#include "house.h"
#include "app.h"
#include "store.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* --- the tunables, with their cause ------------------------------------------ */
/* the arrival: the gel rises out of the rim at its resting spot --
 * pushed RISE_PX under the rim and flattened to the floor (the splat's
 * own flatness, main.cpp's LAND_SQUASH 0.48: the widest the gel is ever
 * authored), standing up and coming in over RISE_S, eyes in */
static const float RISE_S       = 0.60f;
static const float FLOOR_SQUASH = 0.48f;
/* how far under the rim the flattened body must be to be gone: its
 * height at that squash (16 art px x (1 - 0.45 x 0.48) x 6 = 75 face
 * px) and a margin for the bent base's corners */
static const float RISE_PX      = 95.0f;
/* the departure: the same road down, eyes out */
static const float SINK_S       = 0.50f;
/* THE GOODBYE ON THE WRIST (design/watch-ideas.md C19, 2026-09-10): a
 * creature leaves a room facing the person in it. On `no desk` /
 * `release` while HOLDING the gel's small farewell comes BEFORE the
 * sink: a glance up (gaze_y -1, the eyes brought up) and one calm
 * settle -- BYE_SQUASH, the drink's own 0.08, in and out on a half
 * sine over BYE_S -- then the sink as before; the arrival keeps its
 * rise. The trace is read on the goodbye's first frame (the settle at
 * 0 there: the resting silhouette still). */
static const float BYE_S        = 0.60f;
static const float BYE_SQUASH   = 0.08f;
/* THE WRIST IS THE DEFAULT HOME WHEN OFFLINE (Khalid, 23:20): awake
 * seconds without a link (kept across naps, cleared by a link) ... --
 * THE RADIO BUDGET (2026-09-10): "without a link" means a dial was DUE
 * and found none; main.cpp passes link_linked() || link_resting(), so
 * a radio resting by choice never counts */
static const float LINK_LOST_S  = 90.0f;
/* ... while WORN: motion within this many minutes */
static const int   WORN_WINDOW_MIN = 5;

/* --- the state --------------------------------------------------------------- */
static int      state = HOUSE_UNKNOWN;
static char     cause[48] = "never told";
static uint32_t since_ms = 0, since_epoch = 0;
static bool     lapsed_once = false;      /* a beat's `no lapsed` answered with one more claim */
static int      event = HOUSE_EV_NONE;
static bool     sink_began = false;       /* THE BUMPS: the sink's first frame, taken once by main */
static float    offline_s = 0;            /* awake seconds with no link, since the last one */
static bool     release_pending = false;  /* THE LEASE, EASY: released on the wrist's word; the box's answer still to come */
static bool     night_release = false;    /* THE NIGHT RELEASE: the pending release is the night's (no theatre either way) */
static uint32_t still_last = 0xffffffffu;  /* the last tick's still_ms: the worn test's number (never ticked = still) */

enum Th { TH_NONE, TH_RISE, TH_BYE, TH_SINK };
static const char *TH_NAMES[] = { "-", "the gel rises", "the goodbye", "the gel sinks" };
static Th    th = TH_NONE;
static float th_t = 0;

static const char *NAMES[3] = { "unknown", "holding", "empty" };

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static void when(char *out, int cap)
{
    if (since_epoch) {
        struct tm lt; time_t loc = (time_t)since_epoch + (time_t)store.tz_min * 60;
        gmtime_r(&loc, &lt);
        snprintf(out, cap, "since %02d:%02d", lt.tm_hour, lt.tm_min);
    } else snprintf(out, cap, "%lu s ago", (unsigned long)((millis() - since_ms) / 1000));
}

/* the state itself: set, remembered, logged with its reason */
static void become(int s, const char *why)
{
    bool changed = s != state;
    state = s;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    if (s != HOUSE_HOLDING) lapsed_once = false;
    if (store.house != (uint8_t)s) { store.house = (uint8_t)s; store_save_house(); }
    if (!changed) return;                 /* a refresh (a beat's `ok held`): the cause noted, no line, the since kept */
    since_ms = millis();
    since_epoch = clock_is_set() ? (uint32_t)time(NULL) : 0;
    char w[24]; when(w, sizeof w);
    LOG("house: %s (%s, %s)", NAMES[state], why, w);
}

/* the gel is on the glass right now (drawn, or rising / saying goodbye / sinking) */
static bool drawn(void)
{
    return state == HOUSE_HOLDING || th == TH_BYE || th == TH_SINK;
}

static void set_holding(const char *why)
{
    bool was = drawn();
    become(HOUSE_HOLDING, why);
    if (!was) {
        th = TH_RISE; th_t = 0;
        event = HOUSE_EV_ARRIVED;
        LOG("house: the arrival -- the gel rises out of the rim");
    } else if (th == TH_SINK || th == TH_BYE) { th = TH_NONE; }   /* called back mid-goodbye or mid-sink: it stands again */
}

static void set_empty(const char *why)
{
    bool was = drawn();
    become(HOUSE_EMPTY, why);
    if (was && th != TH_SINK && th != TH_BYE) {
        event = HOUSE_EV_LEFT;
        if (th == TH_RISE) {                  /* told to leave mid-arrival: no one to face yet, the sink at once */
            th = TH_SINK; th_t = 0; sink_began = true;
            LOG("house: the departure -- the gel sinks into the rim");
        } else {
            th = TH_BYE; th_t = 0;            /* THE GOODBYE: a look up and one settle, then the sink */
            LOG("house: the departure -- the goodbye (a look up, one settle), then the gel sinks into the rim");
        }
    }
}

/* --- the public face ------------------------------------------------------------ */

void house_begin(void)
{
    since_ms = millis();
    since_epoch = 0;
    th = TH_NONE; th_t = 0; event = HOUSE_EV_NONE; sink_began = false;   /* a boot has no theatre in progress */
    if (store.house == HOUSE_HOLDING || store.house == HOUSE_EMPTY) {
        state = store.house;
        strcpy(cause, state == HOUSE_HOLDING ? "held when it left the house" : "empty when it left the house");
        LOG("house: %s (%s; NVS) -- %s", NAMES[state], cause,
            state == HOUSE_HOLDING ? "the slime is here" : "the black glass");
    } else {
        state = HOUSE_UNKNOWN;
        LOG("house: empty (never told) -- the black glass; the gel arrives after an `ok held`, or worn offline %d s", (int)LINK_LOST_S);
    }
}

bool house_lease(const char *verb, const char *line)
{
    char why[40];
    snprintf(why, sizeof why, "%s: %s", verb, line);
    /* THE LEASE, EASY: the release went out on the wrist's word; this is the box's answer to it */
    bool answering_release = release_pending && !strcmp(verb, "release");
    bool answering_night = answering_release && night_release;
    if (answering_release) { release_pending = false; night_release = false; }
    if (!strcmp(line, "ok held")) { lapsed_once = false; set_holding(why); return false; }
    if (!strcmp(line, "ok released")) {
        if (answering_release) LOG("house: the %srelease confirmed (`ok released`): the wrist's word stood", answering_night ? "night's " : "");
        set_empty(why);
        return false;
    }
    if (answering_night && strncmp(line, "no desk", 7) && strncmp(line, "no lapsed", 9)) {
        /* THE NIGHT RELEASE refused: he stands again, quietly (no rise on
         * a dark glass); `no desk` / `no lapsed` fall through below -- he
         * is elsewhere either way */
        LOG("house: the night's release refused ('%s'): holding again, no theatre", line);
        snprintf(why, sizeof why, "night release refused: %s", line);
        become(HOUSE_HOLDING, why);
        return false;
    }
    if (!strcmp(line, "no desk")) { set_empty("the desk has him"); return false; }
    if (!strcmp(line, "no lapsed")) {
        if (state == HOUSE_HOLDING && !strcmp(verb, "beat") && !lapsed_once) {
            lapsed_once = true;
            LOG("house: the lease lapsed on the beat; claiming again once");
            return true;
        }
        set_empty(why);
        return false;
    }
    if (answering_release) {
        /* the box refused the release (no box / no gate / no soul / no
         * verb ...): the lease was never let go of -- he stands again
         * (mid-sink it snaps back; after the sink it is a second arrival) */
        LOG("house: the release refused ('%s'): un-sinking -- he stands again", line);
        snprintf(why, sizeof why, "release refused: %s", line);
        set_holding(why);
        return false;
    }
    /* no box / no gate / no soul / anything else: sticky */
    LOG("house: %s says '%s': %s kept", verb, line, NAMES[state]);
    return false;
}

void house_release_now(void)
{
    if (state != HOUSE_HOLDING) { LOG("house: release on the wrist's word: the house is %s -- nothing to sink", NAMES[state]); return; }
    release_pending = true;
    LOG("house: release on the wrist's word -- the sink starts now (the box's `ok released` confirms; a refusal un-sinks)");
    set_empty("release: the wrist's own word");
}

bool house_night_release(void)
{
    if (state != HOUSE_HOLDING) { LOG("house: the night: the house is %s -- nothing to let go of", NAMES[state]); return false; }
    release_pending = true;
    night_release = true;
    th = TH_NONE;                          /* no goodbye, no sink: the glass is dark */
    become(HOUSE_EMPTY, "the night: released on the wrist's word");
    LOG("house: the night's release -- empty without theatre (the glass is dark); `ok released` confirms, a refusal stands him up again");
    return true;
}

bool house_release_pending(void) { return release_pending; }

void house_release_unanswered(void)
{
    if (!release_pending) return;
    release_pending = false;
    night_release = false;
    LOG("house: no answer to the release -- the wrist's word stands (%s); the next claim decides", NAMES[state]);
}

bool house_holding(void) { return state == HOUSE_HOLDING; }
bool house_worn(void) { return still_last < (uint32_t)WORN_WINDOW_MIN * 60000u; }
uint32_t house_still_ms(void) { return still_last; }
int  house_state(void) { return state; }
const char *house_state_name(void) { return NAMES[state]; }

bool house_sense_ok(const char *kind)
{
    if (state == HOUSE_HOLDING) return true;
    return !strcmp(kind, "wrist:raised") || !strcmp(kind, "wrist:shaken") || !strcmp(kind, "wrist:slept");
}

void house_force(int s, const char *why)
{
    char w[40];
    snprintf(w, sizeof w, "forced: %s", why);
    LOG("house: forced %s (a test aid)", NAMES[s]);
    if (s == HOUSE_HOLDING) set_holding(w);
    else set_empty(w);
}

void house_status(char *out, int cap)
{
    char w[24]; when(w, sizeof w);
    snprintf(out, cap, "house %s (%s, %s) theatre %s offline %.0f/%.0f s nvs %s%s%s",
             NAMES[state], cause, w, TH_NAMES[th], offline_s, LINK_LOST_S,
             NAMES[store.house < 3 ? store.house : 0], lapsed_once ? " lapsed-once" : "",
             release_pending ? (night_release ? " release-pending (the night's, the box not yet heard)" : " release-pending (the wrist's word, the box not yet heard)") : "");
}

void house_tick(float dt, bool linked, uint32_t still_ms)
{
    still_last = still_ms;
    switch (th) {
    case TH_RISE:
        th_t += dt;
        if (th_t >= RISE_S) { th = TH_NONE; LOG("house: arrived -- the slime is on the wrist"); }
        break;
    case TH_BYE:
        th_t += dt;
        if (th_t >= BYE_S) { th = TH_SINK; th_t = 0; sink_began = true; }     /* the farewell said: the sink (THE BUMPS: he feels it go) */
        break;
    case TH_SINK:
        th_t += dt;
        if (th_t >= SINK_S) { th = TH_NONE; LOG("house: gone -- the glass is empty"); }
        break;
    default: break;
    }
    /* THE DEFAULT HOME: no link for LINK_LOST_S of awake time, and worn */
    if (linked) { offline_s = 0; return; }
    offline_s += dt;
    if (state != HOUSE_HOLDING && offline_s >= LINK_LOST_S && house_worn())
        set_holding("offline, on the wrist -- the default home");
}

int house_take_event(void)
{
    int e = event; event = HOUSE_EV_NONE; return e;
}

bool house_take_sink(void)
{
    bool b = sink_began; sink_began = false; return b;
}

void house_fx(house_fx_t *o)
{
    o->gel = drawn();
    o->squash = 0; o->lift = 0; o->eye_k = 1.0f; o->gaze_y = 0; o->eye_alpha = 0;
    if (th == TH_BYE) {
        /* THE GOODBYE: the look up, the eyes up for it, one settle in and out */
        float t = clampf(th_t / BYE_S, 0, 1);
        o->gaze_y = -1;
        o->eye_alpha = 1.0f;
        o->squash = BYE_SQUASH * sinf((float)M_PI * t);
    } else if (th == TH_RISE) {
        float t = smooth(clampf(th_t / RISE_S, 0, 1));
        o->squash = FLOOR_SQUASH * (1.0f - t);
        o->lift = -RISE_PX * (1.0f - t);
        o->eye_k = t;
    } else if (th == TH_SINK) {
        float t = smooth(clampf(th_t / SINK_S, 0, 1));
        o->squash = FLOOR_SQUASH * t;
        o->lift = -RISE_PX * t;
        o->eye_k = 1.0f - t;
    }
}
