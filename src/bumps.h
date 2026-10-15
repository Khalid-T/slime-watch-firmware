/* THE BUMPS (2026-09-23; design/watch-inspiration.md C.5 "the bump: the
 * creature's one touch of its own", watch-ideas.md A8 the purr under a
 * pet, E40 the wisp's bump): the creature noticed something and has no
 * words -- so it bumps the glass, through the bare motor on GPIO 33
 * (motor.cpp; haptic_play falls through to it) or the DRV2605 when one
 * is soldered. FOUR OCCASIONS, each ONCE, and nothing else, ever:
 *
 *   the wisp     a wisp LANDS on a lit glass (the box's doorbell, the
 *                phone's notice): one bump -- it landed beside him
 *   the sink     he leaves the wrist: one bump at the goodbye's settle,
 *                the instant the gel starts to sink (he feels it go)
 *   the rise     he arrives: one bump as the gel rises out of the rim
 *   the purr     a pet while the breath is at rest: the PURR, ridden on
 *                the breath's next inhale (main waits for the phase to
 *                wrap), never twice in a minute, never while the breath
 *                is quick (a walk), never asleep
 *
 * NEVER: the sit (the fidget shows it), the night and the quiet hours
 * (silent -- the night's release has no theatre and no bump either), a
 * reminder of any kind, the shelf, and THE DOCK: on the cable the wrist
 * is on a desk, not on him -- a bump there rattles the table and tells
 * nobody anything (ruling 65: this file's call). The dark glass never
 * bumps: a touch from a creature nobody is looking at is a phone's
 * buzz, not a creature's.
 *
 * ONE PURE GATE, no Arduino: occasion + the state -> the haptic kind to
 * play, or -1 with the reason it was kept back. main.cpp reads the
 * state each time (lit, the quiet hours, the sleep look, the cable, the
 * breath's rate against its resting rate, the clocks) and plays what
 * comes back; tools/bumps_test.cpp pins the whole table. The switch
 * (`bumps on|off`, SETTINGS 4's BUMPS row; NVS `bumpsoff`, default on)
 * is one input like the others. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "haptic.h"     /* HAPTIC_BUMP / HAPTIC_PURR: the kinds, not the driver */

enum { BUMP_WISP = 0, BUMP_SINK, BUMP_RISE, BUMP_PET, BUMP_NUDGE, BUMP_OCCASIONS };   /* BUMP_NUDGE: THE SMALL HOUSE's one touch (the nudge, the wrist's bid) */

/* why a bump was kept back (0 = it plays) */
enum { BUMP_PLAYS = 0, BUMP_KEPT_OFF, BUMP_KEPT_DARK, BUMP_KEPT_QUIET, BUMP_KEPT_ASLEEP,
       BUMP_KEPT_CABLE, BUMP_KEPT_BREATH, BUMP_KEPT_MINUTE, BUMP_KEPT_GAP, BUMP_KEPT_N };

/* the tunables, with their cause */
#define BUMP_PURR_GAP_MS   60000u   /* A8: never twice in a minute -- a hand that keeps petting gets one purr */
#define BUMP_GAP_MS        500u     /* two occasions on one frame (three wisps landing, a rise under a wisp) are one touch, never a rattle */
#define BUMP_BREATH_FAST   1.20f    /* the breath is "quick" past this x the resting rate: on its way to the walk's 0.80 Hz from 0.55 (the ease takes 30 s, so a walk just ended still counts as quick) */

typedef struct {
    bool     on;               /* the switch (store.bumps_off inverted) */
    bool     lit;              /* the glass is not DARK (dim counts as lit: he can see it) */
    bool     quiet;            /* the quiet hours now (rest_night_now) */
    bool     asleep;           /* the sleep look: rest->dozing (asleep or stirring) */
    bool     on_cable;         /* charging() or the dock: the wrist is on a desk */
    float    breath_hz;        /* the breath's rate now (rest->breath_hz) */
    float    breath_rest_hz;   /* its resting target (rest->breath_rest_hz: the grain's and vigor's resting rate) */
    uint32_t now_ms;
    uint32_t last_bump_ms;     /* any occasion's last play, 0 = never */
    uint32_t last_purr_ms;     /* the purr's last play (or arm), 0 = never */
} bumps_in_t;

static inline const char *bumps_name(int occasion)
{
    switch (occasion) {
    case BUMP_WISP: return "the wisp";
    case BUMP_SINK: return "the sink";
    case BUMP_RISE: return "the rise";
    case BUMP_PET:  return "the purr under a pet";
    case BUMP_NUDGE: return "the nudge";
    default:        return "?";
    }
}

static inline const char *bumps_kept_name(int why)
{
    switch (why) {
    case BUMP_PLAYS:       return "plays";
    case BUMP_KEPT_OFF:    return "bumps off";
    case BUMP_KEPT_DARK:   return "the glass is dark";
    case BUMP_KEPT_QUIET:  return "the quiet hours";
    case BUMP_KEPT_ASLEEP: return "asleep";
    case BUMP_KEPT_CABLE:  return "on the cable";
    case BUMP_KEPT_BREATH: return "the breath is quick";
    case BUMP_KEPT_MINUTE: return "a purr already this minute";
    case BUMP_KEPT_GAP:    return "a bump just played";
    default:               return "?";
    }
}

/* the gate: HAPTIC_BUMP / HAPTIC_PURR to play now, or -1 (why says
 * which rule kept it). The order of the rules is the order of their
 * weight: his switch, then the glass, the night, the sleep, the desk,
 * then the occasion's own. */
static inline int bumps_decide(int occasion, const bumps_in_t *in, int *why)
{
    int w = BUMP_PLAYS, kind = -1;
    if (occasion < 0 || occasion >= BUMP_OCCASIONS) { if (why) *why = BUMP_KEPT_OFF; return -1; }
    if (!in->on) w = BUMP_KEPT_OFF;
    else if (!in->lit) w = BUMP_KEPT_DARK;
    else if (in->quiet) w = BUMP_KEPT_QUIET;
    else if (in->asleep) w = BUMP_KEPT_ASLEEP;
    else if (in->on_cable) w = BUMP_KEPT_CABLE;
    else if (in->last_bump_ms && (uint32_t)(in->now_ms - in->last_bump_ms) < BUMP_GAP_MS) w = BUMP_KEPT_GAP;
    else if (occasion == BUMP_PET) {
        if (in->breath_rest_hz > 0 && in->breath_hz > in->breath_rest_hz * BUMP_BREATH_FAST) w = BUMP_KEPT_BREATH;
        else if (in->last_purr_ms && (uint32_t)(in->now_ms - in->last_purr_ms) < BUMP_PURR_GAP_MS) w = BUMP_KEPT_MINUTE;
        else kind = HAPTIC_PURR;
    } else kind = HAPTIC_BUMP;
    if (why) *why = w;
    return kind;
}

/* the purr rides the breath: armed at the pet, played on the inhale's
 * start -- the breath's phase (rest.h: radians, integrated, wrapped at
 * 2 pi) wrapping is that instant. true = play it now. */
static inline bool bumps_purr_due(float phase_before, float phase_now)
{
    return phase_now < phase_before;
}
