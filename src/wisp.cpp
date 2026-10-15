/* THE WISP: see wisp.h. Factored out of ancs.cpp (2026-09-10) so the
 * desk's doorbell can send one over the link in the default build; the
 * mote, the flight and the rules are the ANCS ones, the geometry moved
 * from the four walls to the bowl's angle (the walls are gone: the air
 * door is the rim opposite the floor wherever the floor is).
 *
 * The frame of every number here: g = (sin phi, cos phi) is gravity --
 * from the face's centre toward the floor; r = (cos phi, -sin phi) is
 * the body's right along the floor. A post is (depth along g, along
 * along r); the door is -g * DOOR_R; the bob and the fade's rise are
 * along -g. */
#include "wisp.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */
#include "mark.h"
#include "board.h"
#include <string.h>
#include <strings.h>    /* strcasecmp: the box's id, case folded */
#include <stdio.h>
#include <math.h>

/* --- the tunables ------------------------------------------------------------ */

static const float FLY_S   = 1.4f;            /* the flight in from the air door */
static const float ARC_PX  = 22.0f;           /* the bow of that flight */
static const float EASE    = 3.0f;            /* 1/s: the hover follows its post (the gel slides) */
static const float BOB_PX  = 3.0f;
static const float BOB_HZ  = 0.45f;
static const float SEEN_S  = 1.2f;            /* glanced: it holds this long before going */
static const float FADE_S  = 1.0f;
static const float RISE_PX = 9.0f;            /* how far it lifts as it fades */
static const float PULSE_S = 0.7f;            /* the halo's swell on a second notice */
static const float HALO_K  = 0.42f;           /* the halo's weight over what is under it */
/* THE NOTICE'S OWN REACTION (notice.h): the halo's two other styles */
static const float RING_HZ = 1.1f;            /* the PULSING halo (a call ringing): swells and ebbs at this rate, a ring's pace, until the call ends or the glance */
static const float RING_K  = 1.2f;            /* ...this much over the plain halo at the swell's top (the second notice's swell, kept going) */
static const float WIDE_K  = 0.55f;           /* the WIDE halo (important): its outer ring's weight against the inner one's */
static const int   DOOR_R  = 116;             /* the air door: on the rim, opposite the floor */
/* THE WISP AT THE DOOR (E43): the dim mote while the house is EMPTY -- one px of its ink, this much of it, no halo
 * (a creature nobody is home for: the smallest mark the palette allows, so a raise on the empty glass says "something
 * waits" and no more; the plain mote's core is 1.0) */
static const float DOOR_DIM_K = 0.40f;
static const int   GLASS_R = 117;             /* nothing drawn past the round glass */
static const int   POST_R_MAX = 106;          /* a post never past this from the centre */
/* the posts beside the gel, in gravity's frame: (depth toward the floor
 * from the centre, along the floor toward the body's right) */
static const float POST_DEPTH[WISP_MAX] = { 18.0f, 18.0f, -44.0f };
static const float POST_ALONG[WISP_MAX] = { 74.0f, -74.0f, 48.0f };

/* --- state ------------------------------------------------------------------- */

typedef struct {
    bool     used;
    char     app[WISP_APP_MAX];
    char     title[WISP_TITLE_MAX];
    int      post;
    float    x, y;          /* screen px */
    float    fly_t;         /* 0..1: the flight in */
    float    age, phase;
    float    seen_t;        /* > 0: glanced, holding */
    bool     armed;         /* landed, and a glance has ENDED since: the next one is its own */
    bool     fading;
    float    alpha;
    float    pulse;
    notice_react_t react;   /* THE NOTICE'S OWN REACTION: the landing's haptic and the halo's style */
    int      source;        /* THE WISP'S INK (E42): WISP_SRC_* -- what it wears */
    bool     eating;        /* THE WISP EATEN (E41): on its way into him */
    float    eat_t;         /* 0..WISP_EAT_S */
    float    eat_x0, eat_y0, eat_x1, eat_y1;   /* the swallow's path: from where it hovered to the finger's landing */
} wisp_t;
/* three posts, and one spare struct so the oldest can still be fading
 * while a newcomer flies in to its post */
#define WISP_SLOTS (WISP_MAX + 1)
static wisp_t wisps[WISP_SLOTS];
static uint32_t wisp_born = 0;
static uint32_t wisp_seq[WISP_SLOTS];

/* THE QUEUE: what arrived on a dark glass, waiting for the light */
typedef struct {
    char app[WISP_APP_MAX]; char title[WISP_TITLE_MAX]; notice_react_t react;   /* the reaction waits with it (the message is the ledger's already) */
    int  source;                           /* THE WISP'S INK */
    bool at_door;                          /* THE WISP AT THE DOOR (E43): the house was EMPTY -- it waits at the air door for him, not for the light */
    uint32_t since_ms;                     /* ...since (the wall clock): WISP_DOOR_S later, unclaimed, it fades */
} waiting_t;
static waiting_t waiting[WISP_MAX];
static int n_waiting = 0;
static uint32_t dropped = 0;              /* queued wisps that never flew: a fourth pushed the oldest out */
static uint32_t door_faded = 0;           /* E43: waited the hour at the door, nobody came */
static int      door_faded_take = 0;      /* ...since main last asked */
static uint32_t eaten = 0;                /* E41: fed to him */
static int      swelled = 0;              /* THE WISP WATCHED (C3): halos swelled since main last asked -- the blink */
static float    lump_t = -1;              /* E41: the small lump's clock, -1 = none */
static bool  last_holding = false;        /* the ARRIVAL is holding's rising edge: the door's wisps fly in with him */

static float last_phi = 0;
static bool  last_lit = false;
static uint32_t last_ms = 0;               /* the wall clock as of the last tick (the ledger's stamps) */
static int      landed = 0;                /* THE BUMPS: flights in that ended since main last asked */
static notice_react_t landed_best = { -1, 0, 0 };   /* ...and the strongest reaction among them (notice_react_rank); none until one lands */
static mark_t mk;                          /* THE MARK (mark.h): what this frame drew */

/* THE LEDGER: the last WISP_LEDGER notices, newest first (a fourth
 * pushes the oldest out; the same app again moves to the top with the
 * newer title -- one line per notice, as the wisps are one per app) */
static wisp_note_t ledger[WISP_LEDGER];
static int n_ledger = 0;

static void ledger_note(const char *app, const char *title, const char *msg)
{
    int at = n_ledger < WISP_LEDGER ? n_ledger : WISP_LEDGER - 1;
    char name[WISP_NAME_MAX] = "";
    for (int i = 0; i < n_ledger; i++) if (!strcmp(ledger[i].app, app)) { at = i; strncpy(name, ledger[i].name, sizeof name - 1); break; }   /* the same app again keeps the name it was given */
    if (at > 0) memmove(&ledger[1], &ledger[0], sizeof ledger[0] * (size_t)at);
    memset(&ledger[0], 0, sizeof ledger[0]);
    strncpy(ledger[0].app, app, sizeof ledger[0].app - 1);
    strncpy(ledger[0].name, name, sizeof ledger[0].name - 1);
    if (title) strncpy(ledger[0].title, title, sizeof ledger[0].title - 1);
    if (msg) strncpy(ledger[0].msg, msg, sizeof ledger[0].msg - 1);
    ledger[0].at_ms = last_ms;
    if (n_ledger < WISP_LEDGER && at == n_ledger) n_ledger++;
}

int wisp_ledger(const wisp_note_t **out)
{
    if (out) *out = ledger;
    return n_ledger;
}

void wisp_ledger_keep(const char *app, const char *title, const char *msg) { if (app && app[0]) ledger_note(app, title, msg); }

void wisp_ledger_name(const char *app, const char *name)
{
    if (!app || !app[0]) return;
    for (int i = 0; i < n_ledger; i++)
        if (!strcmp(ledger[i].app, app)) {
            memset(ledger[i].name, 0, sizeof ledger[i].name);
            if (name) strncpy(ledger[i].name, name, sizeof ledger[i].name - 1);
        }
}

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

/* where a post is on the glass now: beside the gel, on the bowl's angle */
static void post_xy(int post, float phi, float *x, float *y)
{
    float gx = sinf(phi), gy = cosf(phi);
    float rx = cosf(phi), ry = -sinf(phi);           /* the body's right along the floor */
    float depth = POST_DEPTH[post], along = POST_ALONG[post];
    float px = gx * depth + rx * along, py = gy * depth + ry * along;
    float r = sqrtf(px * px + py * py);
    if (r > POST_R_MAX) { px *= POST_R_MAX / r; py *= POST_R_MAX / r; }
    *x = SCREEN_W * 0.5f + px;
    *y = SCREEN_H * 0.5f + py;
}

/* the air door: the rim opposite the floor */
static void door_xy(float phi, float *x, float *y)
{
    *x = SCREEN_W * 0.5f - sinf(phi) * DOOR_R;
    *y = SCREEN_H * 0.5f - cosf(phi) * DOOR_R;
}

static void wisp_go(wisp_t *w) { if (!w->fading && !w->eating && w->seen_t <= 0) w->seen_t = SEEN_S; }

static void spawn(const char *app, const char *title, const notice_react_t *react, int source)
{
    /* the same app again: the wisp already here swells once, and takes
     * the NEWER reaction (a ringing that ends in a missed call stops
     * pulsing); no landing, so nothing is felt for it */
    for (int i = 0; i < WISP_SLOTS; i++)
        if (wisps[i].used && !wisps[i].fading && !wisps[i].eating && !strcmp(wisps[i].app, app)) {
            wisps[i].pulse = PULSE_S;
            swelled++;                                /* THE WISP WATCHED (C3): one blink for the swell */
            wisps[i].react = *react;
            wisps[i].source = source;
            if (title && title[0]) { strncpy(wisps[i].title, title, sizeof wisps[i].title - 1); wisps[i].title[sizeof wisps[i].title - 1] = 0; }
            return;
        }
    /* a post: one no hovering wisp holds; with all three held the
     * OLDEST is sent off and the newcomer flies in to its post while it
     * fades (a wisp already fading gives its struct up at once) */
    bool held[WISP_MAX] = { false, false, false };
    int oldest = -1;
    for (int i = 0; i < WISP_SLOTS; i++)
        if (wisps[i].used && !wisps[i].fading && !wisps[i].eating) {   /* an eaten one has left its post */
            held[wisps[i].post] = true;
            if (oldest < 0 || wisp_seq[i] < wisp_seq[oldest]) oldest = i;
        }
    int post = -1;
    for (int p = 0; p < WISP_MAX; p++) if (!held[p]) { post = p; break; }
    if (post < 0 && oldest >= 0) { post = wisps[oldest].post; wisps[oldest].seen_t = 0; wisps[oldest].fading = true; }
    if (post < 0) post = 0;
    int slot = -1;
    for (int i = 0; i < WISP_SLOTS; i++) if (!wisps[i].used) { slot = i; break; }
    if (slot < 0)
        for (int i = 0; i < WISP_SLOTS; i++) if ((wisps[i].fading || wisps[i].eating) && i != oldest) { slot = i; break; }
    if (slot < 0) slot = oldest >= 0 ? oldest : 0;
    wisp_t *w = &wisps[slot];
    memset(w, 0, sizeof *w);
    w->used = true; w->post = post;
    strncpy(w->app, app, sizeof w->app - 1);
    if (title) strncpy(w->title, title, sizeof w->title - 1);
    w->react = *react;
    w->source = source;
    w->alpha = 1.0f;
    w->phase = (float)(wisp_born % 7) * 0.9f;
    door_xy(last_phi, &w->x, &w->y);
    wisp_seq[slot] = ++wisp_born;
}

/* --- the public face ---------------------------------------------------------- */

int wisp_source_of(const char *app) { return app && !strcasecmp(app, WISP_BOX_APP) ? WISP_SRC_BOX : WISP_SRC_DESK; }
const char *wisp_source_name(int source) { return source == WISP_SRC_PHONE ? "phone" : source == WISP_SRC_BOX ? "box" : "desk"; }

/* THE WISP'S INK (E42): the theme's named inks only -- the shine (the desk, and the default), the shade (the phone), the
 * body (the box: the mind speaks in his own flesh's colour; the item's "presence gold" is no theme ink) */
const gel_rgba_t *wisp_ink(int source)
{
    const gel_pal_t *pal = theme_pal();
    return source == WISP_SRC_PHONE ? &pal->shade : source == WISP_SRC_BOX ? &pal->body : &pal->shine;
}

int wisp_arrive(const char *app, const char *title, bool holding)
{
    return wisp_arrive_from(wisp_source_of(app), app, title, NULL, NULL, holding);   /* the default: one bump, the plain halo -- what every wisp was */
}

int wisp_arrive_react(const char *app, const char *title, const char *msg, const notice_react_t *react, bool holding)
{
    return wisp_arrive_from(wisp_source_of(app), app, title, msg, react, holding);
}

int wisp_arrive_from(int source, const char *app, const char *title, const char *msg, const notice_react_t *react, bool holding)
{
    if (!app || !app[0]) return WISP_NO_HOUSE;
    notice_react_t dflt;
    if (!react) { notice_react_default(&dflt); react = &dflt; }
    ledger_note(app, title, msg);                       /* THE LEDGER: his, whether it flies now, waits for the light, or waits at the door */
    if (holding && last_lit) { spawn(app, title, react, source); return WISP_OK; }
    /* THE DARK GLASS: it waits for the light, never makes it; THE WISP AT
     * THE DOOR (E43): nobody home, it waits at the air door for HIM -- the
     * one queue of three holds both kinds */
    for (int i = 0; i < n_waiting; i++)
        if (!strcmp(waiting[i].app, app)) {             /* the same app twice: one wisp, the newer title, reaction and ink; the door's clock from the first */
            if (title && title[0]) { strncpy(waiting[i].title, title, sizeof waiting[i].title - 1); waiting[i].title[sizeof waiting[i].title - 1] = 0; }
            waiting[i].react = *react;
            waiting[i].source = source;
            return WISP_OK;
        }
    if (n_waiting >= WISP_MAX) {                        /* a fourth: the oldest waits no more */
        memmove(&waiting[0], &waiting[1], sizeof waiting[0] * (WISP_MAX - 1));
        n_waiting = WISP_MAX - 1;
        dropped++;
    }
    memset(&waiting[n_waiting], 0, sizeof waiting[0]);
    strncpy(waiting[n_waiting].app, app, sizeof waiting[0].app - 1);
    if (title) strncpy(waiting[n_waiting].title, title, sizeof waiting[0].title - 1);
    waiting[n_waiting].react = *react;
    waiting[n_waiting].source = source;
    waiting[n_waiting].at_door = !holding;
    waiting[n_waiting].since_ms = last_ms;
    n_waiting++;
    return WISP_OK;
}

/* THE WISP EATEN (E41): the feed's end on a hovering wisp -- the nearest within WISP_EAT_PX is his meal */
bool wisp_feed_at(int x, int y)
{
    int best = -1; float best_d2 = WISP_EAT_PX * WISP_EAT_PX;
    for (int i = 0; i < WISP_SLOTS; i++) {
        const wisp_t *w = &wisps[i];
        if (!w->used || w->fading || w->eating || w->fly_t < 1.0f) continue;   /* hovering only: not still flying in, not already going */
        float dx = w->x - (float)x, dy = w->y - (float)y, d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) { best_d2 = d2; best = i; }
    }
    if (best < 0) return false;
    wisp_t *w = &wisps[best];
    w->eating = true; w->eat_t = 0; w->seen_t = 0;
    w->eat_x0 = w->x; w->eat_y0 = w->y; w->eat_x1 = (float)x; w->eat_y1 = (float)y;
    lump_t = 0;                                          /* the small lump: topped back up, never summed (one belly) */
    eaten++;
    return true;
}

/* THE NOTICE CLEARED FROM THE WRIST (T17): the hold on the open card eats that app's hovering wisp the feed's way (read, not lost) */
bool wisp_eat_app(const char *app, int x, int y)
{
    if (!app || !app[0]) return false;
    for (int i = 0; i < WISP_SLOTS; i++) {
        wisp_t *w = &wisps[i];
        if (!w->used || w->fading || w->eating || w->fly_t < 1.0f || strcmp(w->app, app)) continue;   /* hovering, that app */
        w->eating = true; w->eat_t = 0; w->seen_t = 0;
        w->eat_x0 = w->x; w->eat_y0 = w->y; w->eat_x1 = (float)x; w->eat_y1 = (float)y;
        lump_t = 0;
        eaten++;
        return true;
    }
    return false;
}

float wisp_lump(void)
{
    if (lump_t < 0) return 0;
    float t = lump_t / WISP_LUMP_S;
    return WISP_LUMP * (1.0f - t) * (1.0f - t);          /* the meal's own ease-out, the bead's too: fast at first, the last of it lingering */
}

uint32_t wisp_eaten(void) { return eaten; }

void wisp_leave(const char *app)
{
    if (!app) return;
    for (int i = 0; i < WISP_SLOTS; i++)
        if (wisps[i].used && !strcmp(wisps[i].app, app)) wisp_go(&wisps[i]);
    for (int i = 0; i < n_waiting; i++)
        if (!strcmp(waiting[i].app, app)) {
            memmove(&waiting[i], &waiting[i + 1], sizeof waiting[0] * (n_waiting - i - 1));
            n_waiting--;
            return;
        }
}

void wisp_tick(uint32_t now_ms, float dt, float phi, bool holding, bool lit, bool glanced)
{
    last_ms = now_ms;
    last_phi = phi;
    last_lit = lit;
    if (lump_t >= 0) { lump_t += dt; if (lump_t >= WISP_LUMP_S) lump_t = -1; }   /* E41: the eaten notice's lump sinks */
    if (holding && !last_holding) {
        /* THE ARRIVAL (E43): he is home -- what waited at the door is his
         * to receive now; it flies in with him on the light below (a dark
         * arrival keeps it for the first lit frame, as the dark glass does) */
        for (int i = 0; i < n_waiting; i++) waiting[i].at_door = false;
    }
    last_holding = holding;
    if (!holding) {
        /* he left: the hovering wisps were for him -- every one goes; what
         * still waited (unseen) goes to the air door with the rest (E43) */
        for (int i = 0; i < WISP_SLOTS; i++) if (wisps[i].used) wisp_go(&wisps[i]);
        for (int i = 0; i < n_waiting; i++) if (!waiting[i].at_door) { waiting[i].at_door = true; waiting[i].since_ms = now_ms; }
    } else if (lit && n_waiting) {
        /* THE LIGHT: what waited flies in now (the raise that lit the
         * glass is not its glance -- it has not landed yet) */
        for (int i = 0; i < n_waiting; i++) spawn(waiting[i].app, waiting[i].title, &waiting[i].react, waiting[i].source);
        n_waiting = 0;
    }
    /* THE DOOR'S HOUR (E43): an hour unclaimed at the door, it fades (counted; main logs it) */
    for (int i = 0; i < n_waiting; )
        if (waiting[i].at_door && now_ms - waiting[i].since_ms >= (uint32_t)(WISP_DOOR_S * 1000.0f)) {
            memmove(&waiting[i], &waiting[i + 1], sizeof waiting[0] * (n_waiting - i - 1));
            n_waiting--; door_faded++; door_faded_take++;
        } else i++;
    float gx = sinf(phi), gy = cosf(phi);
    float rx = cosf(phi), ry = -sinf(phi);
    for (int i = 0; i < WISP_SLOTS; i++) {
        wisp_t *w = &wisps[i];
        if (!w->used) continue;
        w->age += dt;
        if (w->pulse > 0) w->pulse = fmaxf(0, w->pulse - dt);
        if (w->eating) {
            /* THE WISP EATEN (E41): the short way into him -- to the finger's landing, shrinking; then gone */
            w->eat_t += dt;
            float s = smooth(clampf(w->eat_t / WISP_EAT_S, 0, 1));
            w->x = w->eat_x0 + (w->eat_x1 - w->eat_x0) * s;
            w->y = w->eat_y0 + (w->eat_y1 - w->eat_y0) * s;
            w->alpha = 1.0f - s;
            if (w->eat_t >= WISP_EAT_S) { w->used = false; w->alpha = 0; }
            continue;
        }
        float tx, ty; post_xy(w->post, phi, &tx, &ty);
        if (w->fly_t < 1.0f) {
            /* the flight in: a bow from the air door to the post */
            w->fly_t = fminf(1.0f, w->fly_t + dt / FLY_S);
            if (w->fly_t >= 1.0f) {                        /* THE BUMPS: it landed beside him (one per flight in) */
                landed++;
                if (notice_react_rank(&w->react) > notice_react_rank(&landed_best)) landed_best = w->react;   /* the strongest of the take is the touch */
            }
            float s = smooth(w->fly_t);
            float dxx, dyy; door_xy(phi, &dxx, &dyy);
            float side = POST_ALONG[w->post] >= 0 ? 1.0f : -1.0f;
            float bow = ARC_PX * sinf((float)M_PI * s) * side;
            w->x = dxx + (tx - dxx) * s + rx * bow;
            w->y = dyy + (ty - dyy) * s + ry * bow;
        } else {
            float k = clampf(EASE * dt, 0, 1);
            w->x += (tx - w->x) * k;
            w->y += (ty - w->y) * k;
            /* seen: it may go -- by a glance that BEGAN after it landed
             * (the glance it flew in under is not its own) */
            if (!glanced) w->armed = true;
            else if (w->armed) wisp_go(w);
        }
        if (w->seen_t > 0) {
            w->seen_t -= dt;
            if (w->seen_t <= 0) { w->seen_t = 0; w->fading = true; }
        }
        if (w->fading) {
            w->alpha -= dt / FADE_S;
            w->x -= gx * RISE_PX * dt / FADE_S;   /* it lifts as it goes */
            w->y -= gy * RISE_PX * dt / FADE_S;
            if (w->alpha <= 0) { w->used = false; w->alpha = 0; }
        }
    }
}

int wisp_count(void)
{
    int n = 0;
    for (int i = 0; i < WISP_SLOTS; i++) if (wisps[i].used) n++;
    return n;
}

int wisp_waiting(void) { return n_waiting; }
int wisp_at_door(void) { int n = 0; for (int i = 0; i < n_waiting; i++) if (waiting[i].at_door) n++; return n; }
int wisp_take_door_faded(void) { int n = door_faded_take; door_faded_take = 0; return n; }
int wisp_take_landed(void) { int n = landed; landed = 0; landed_best.haptic = -1; landed_best.repeats = 0; landed_best.halo = NOTICE_HALO_NORMAL; return n; }
int wisp_take_landed_react(int *haptic_kind, int *repeats)
{
    int rank = notice_react_rank(&landed_best);
    if (haptic_kind) *haptic_kind = rank > 0 ? landed_best.haptic : -1;
    if (repeats) *repeats = rank > 0 ? landed_best.repeats : 0;
    return wisp_take_landed();
}
uint32_t wisp_sent(void) { return wisp_born; }

/* THE WISP WATCHED (C3): the newest mote on its way in or hovering -- a fading one is nobody's to watch, an eaten one is the feed's */
static int newest_watched(void)
{
    int best = -1;
    for (int i = 0; i < WISP_SLOTS; i++)
        if (wisps[i].used && !wisps[i].fading && !wisps[i].eating && (best < 0 || wisp_seq[i] > wisp_seq[best])) best = i;
    return best;
}
bool wisp_eye_point(int *x, int *y)
{
    int i = newest_watched();
    if (i < 0) return false;
    if (x) *x = (int)lroundf(wisps[i].x);
    if (y) *y = (int)lroundf(wisps[i].y);
    return true;
}
uint32_t wisp_eye_seq(bool *flying)
{
    int i = newest_watched();
    if (flying) *flying = i >= 0 && wisps[i].fly_t < 1.0f;
    return i < 0 ? 0 : wisp_seq[i];
}
int wisp_take_swelled(void) { int n = swelled; swelled = 0; return n; }
void wisp_door_point(float phi, float *x, float *y) { door_xy(phi, x, y); }   /* THE DESK NEXT DOOR: the door as the wisps know it */

bool wisp_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

/* a pixel of the frame, mixed toward the shine by k (0..1); the frame
 * holds RGB565 in the panel's byte order; nothing past the round glass */
static inline void mix_px(uint16_t *fb, int x, int y, int sr, int sg, int sb, float k)
{
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    float cx = x - (SCREEN_W - 1) * 0.5f, cy = y - (SCREEN_H - 1) * 0.5f;
    if (cx * cx + cy * cy > (float)GLASS_R * GLASS_R) return;
    mark_f(&mk, k); mark_rect(&mk, x, y, 1, 1, (uint32_t)(sr << 16 | sg << 8 | sb));
    uint16_t v = fb[y * SCREEN_W + x];
    uint16_t c = (uint16_t)((v >> 8) | (v << 8));
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    r += (int)lroundf((sr - r) * k); g += (int)lroundf((sg - g) * k); b += (int)lroundf((sb - b) * k);
    c = (uint16_t)((r << 11) | (g << 5) | b);
    fb[y * SCREEN_W + x] = (uint16_t)((c >> 8) | (c << 8));
}

void wisp_draw(uint16_t *fb, float phi)
{
    float gx = sinf(phi), gy = cosf(phi);
    /* THE WISP AT THE DOOR (E43): one dim px of the newest waiter's ink at the air door while the house is EMPTY --
     * never more for more of them (the glass says "something waits", not how many), no halo, no bob */
    if (!last_holding && wisp_at_door()) {
        int src = WISP_SRC_DESK;
        for (int i = n_waiting - 1; i >= 0; i--) if (waiting[i].at_door) { src = waiting[i].source; break; }
        const gel_rgba_t *d = wisp_ink(src);
        float dx, dy; door_xy(phi, &dx, &dy);
        mix_px(fb, (int)lroundf(dx), (int)lroundf(dy), (int)(d->r * 31 + 0.5f), (int)(d->g * 63 + 0.5f), (int)(d->b * 31 + 0.5f), DOOR_DIM_K);
    }
    for (int i = 0; i < WISP_SLOTS; i++) {
        const wisp_t *w = &wisps[i];
        if (!w->used || w->alpha <= 0) continue;
        const gel_rgba_t *s = wisp_ink(w->source);     /* THE WISP'S INK (E42): the mote and its halo in the ink of what it carries */
        int sr = (int)(s->r * 31 + 0.5f), sg = (int)(s->g * 63 + 0.5f), sb = (int)(s->b * 31 + 0.5f);
        float bob = w->eating ? 0 : BOB_PX * sinf(2.0f * (float)M_PI * BOB_HZ * w->age + w->phase);   /* E41: a swallow does not bob */
        int x = (int)lroundf(w->x - gx * bob);
        int y = (int)lroundf(w->y - gy * bob);
        float a = clampf(w->alpha, 0, 1);
        float swell = w->pulse > 0 ? RING_K * (w->pulse / PULSE_S) : 0;
        /* THE NOTICE'S OWN REACTION: the PULSING halo (a call ringing) swells and ebbs on its own clock while it
         * hovers unglanced -- the second notice's swell, kept going; the glance (seen_t) or the call's end stops it */
        if (w->react.halo == NOTICE_HALO_PULSING && w->seen_t <= 0 && !w->fading)
            swell = fmaxf(swell, RING_K * (0.5f + 0.5f * sinf(2.0f * (float)M_PI * RING_HZ * w->age)));
        float halo = w->eating ? 0 : clampf(HALO_K * a * (1.0f + swell), 0, 1);   /* E41: the halo is the first thing swallowed */
        /* the halo: the ring one px out from the 3x3 core (16 px, corners in) */
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                if (dx > -2 && dx < 2 && dy > -2 && dy < 2) continue;
                float kk = (dx == -2 || dx == 2) && (dy == -2 || dy == 2) ? halo * 0.45f : halo;
                mix_px(fb, x + dx, y + dy, sr, sg, sb, kk);
            }
        /* the WIDE halo (important): one ring more, a px further out, lighter (24 px, its corners in) */
        if (w->react.halo == NOTICE_HALO_WIDE)
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3; dx <= 3; dx++) {
                    if (dx > -3 && dx < 3 && dy > -3 && dy < 3) continue;
                    if ((dx == -3 || dx == 3) && (dy == -3 || dy == 3)) continue;
                    mix_px(fb, x + dx, y + dy, sr, sg, sb, halo * WIDE_K);
                }
        /* the core: 3x3 of the shine */
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) mix_px(fb, x + dx, y + dy, sr, sg, sb, a);
    }
}

void wisp_status(char *out, int cap)
{
    int n = snprintf(out, cap, "wisp %d here, %d waiting (%d at the door), %lu flown, %lu dropped, %lu faded at the door, %lu eaten", wisp_count(), n_waiting, wisp_at_door(),
                     (unsigned long)wisp_born, (unsigned long)dropped, (unsigned long)door_faded, (unsigned long)eaten);
    for (int i = 0; i < WISP_SLOTS && n < cap; i++)
        if (wisps[i].used)
            n += snprintf(out + n, cap - n, "; %s%s%s%s [%s]%s%s", wisps[i].app, wisps[i].title[0] ? " \"" : "", wisps[i].title,
                          wisps[i].title[0] ? "\"" : "", wisp_source_name(wisps[i].source),
                          wisps[i].react.halo == NOTICE_HALO_PULSING ? " (ringing)" : wisps[i].react.halo == NOTICE_HALO_WIDE ? " (important)" : "",
                          wisps[i].eating ? " (eaten)" : "");
    for (int i = 0; i < n_waiting && n < cap; i++)
        n += snprintf(out + n, cap - n, "; (%s [%s]%s)", waiting[i].app, wisp_source_name(waiting[i].source),
                      waiting[i].at_door ? " at the door" : "");
    if (n_ledger && n < cap) n += snprintf(out + n, cap - n, " | the ledger: %d (newest %s, %lu s ago)", n_ledger, ledger[0].app,
                                           (unsigned long)((last_ms - ledger[0].at_ms) / 1000u));
}
