/* IDLE LIFE (Khalid, 2026-09-09: "make it a true home for the slime ...
 * fill the firmware with RAM-light stuff"): a creature at rest in its
 * house is not a still image. While the glass is lit, the house
 * HOLDING, nothing else on (no touch, no pet, no fall, no theatre, no
 * gesture face, not asleep) and the gel at rest for IDLE_MIN_S, one
 * small thing every IDLE_EVERY_MIN_S..IDLE_EVERY_MAX_S (jittered, a die
 * seeded from the boot's clock):
 *
 *   LOOK       a slow look around: the gaze sweeps -2..2 over LOOK_S,
 *              eyes open (brought up softly if they were down), then back
 *   BLINK      the eyes closed for BLINK_MS -- only when they are up
 *   SETTLE     the mass settling: squash +SETTLE_SQUASH in and out over
 *              SETTLE_S, a small lean one way or the other
 *   BUBBLE     the gel clock advancing faster for BUBBLE_S -- the rim's
 *              ripple quickening, a bubble rising inside
 *   GLANCE UP  the gaze up (gaze_y -1) for GLANCE_S, eyes content
 *   ROAM       THE SMALL HOUSE (cramp.h, 2026-10-09): the eyes roaming
 *              the rim -- the gaze walking the bowl's edge round over
 *              ROAM_S, eyes open; on the table only while the cramp is
 *              on (in->cramp), weighted by it (ROAM_WEIGHT x cramp)
 *
 * Never a hop (the anti-Sonic law: nothing frantic, no hop for no
 * reason); never two of a kind in a row; never within PET_QUIET_S of a
 * pet (the pet's own face has the stage); never asleep; never on the
 * empty glass. The expression vocabulary is the locked one: eye kinds,
 * gaze, squash, lean, the breath -- no new shape.
 *
 * Output = a few DELTAS the draw folds in (gaze, an eye kind or -1, an
 * eye presence it takes the max of, squash, a1, laps added to the gel
 * clock). No Arduino: the clock and the state come in as parameters,
 * tools/idle_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float    dt;
    uint32_t now_ms;
    bool     lit;            /* the screen is on */
    bool     holding;        /* house_holding(): a gel to be alive at all */
    bool     asleep;         /* rest->asleep: never in his sleep */
    bool     busy;           /* a touch down, a pet, a fall, the theatre, the stretch, a gesture's face on: something else has the body */
    bool     moving;         /* the springs still ringing, the bowl still sliding, the wrist not still: not at rest */
    float    eye_alpha;      /* main's own eyes' presence (a blink needs them up) */
    float    cramp;          /* THE SMALL HOUSE (cramp.h, 2026-10-09): the cramp's level 0..1 -- THE ROAM's weight (0 = the roam is not on the table) */
} idle_in_t;

typedef struct {
    int   gaze_x, gaze_y;    /* added to the draw's gaze */
    int   eye_kind;          /* -1 = leave the draw's; else GEL_EYE_* for this frame */
    float eye_alpha;         /* the eyes brought up softly for a look: the draw takes max(its own, this) */
    float squash;            /* the settle, added */
    float a1;                /* the settle's lean, added */
    float gel_extra;         /* laps added to the gel clock (the bubble); never rewinds */
} idle_fx_t;

enum { IDLE_NONE = 0, IDLE_LOOK, IDLE_BLINK, IDLE_SETTLE, IDLE_BUBBLE, IDLE_GLANCE_UP, IDLE_ROAM, IDLE_KINDS };   /* IDLE_ROAM: THE SMALL HOUSE -- the eyes round the rim, weighted by the cramp */

void idle_begin(uint32_t seed);
void idle_pet(void);                     /* a pet (a tap, the HTTP page, a drink): the quiet after it */
void idle_tick(const idle_in_t *in);     /* one call a frame */
void idle_fx(idle_fx_t *out);            /* what the draw folds in this frame */
bool idle_playing(void);                 /* a thing in progress */
/* THE OLD HABIT (design/watch-ideas.md A10, 2026-09-23): the grain's
 * favourite thing, once a lit spell, plays a touch bigger (HABIT_X on
 * its length, or the settle's depth) -- the one tic that makes this
 * slime this slime in both houses. The debt is owed afresh at each lit
 * spell's start (in->lit rising) and spent on the favourite's first
 * roll in it; a nominal grain (no favourite) never owes it */
bool idle_habit_owed(void);              /* this spell's habit not yet played */
bool idle_habit_playing(void);           /* the thing in progress is the habit's (played bigger) */
int  idle_last(void);                    /* the last thing rolled, IDLE_* */
uint32_t idle_last_ms(void);             /* when it began (now_ms's clock), 0 = never */
int  idle_count(void);                   /* things rolled since boot */
const char *idle_name(int kind);
void idle_status(char *out, int cap);    /* `idle` over serial */
