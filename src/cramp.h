/* THE SMALL HOUSE (Khalid, 2026-10-09, mid-wave: "how about something
 * like the slime acting like it's restrained since the watch is a small
 * home it like wants to move somewhere bigger where it can move around
 * more freely though it can still do stuff like nudge me from the watch
 * and yada yada"; a minute later, the counterweight: "though it's small
 * it the screen it's better for it there since it's the home or closest
 * to me yk for resting it comes over to the watch if available... though
 * it can still rest on screen..."; and the correction: "no like it can
 * rest on screen too if it wants on the desk the watch can be available
 * but it still wants the desk it's all up to the slime").
 *
 * The world logic. The watch is a 40 mm bowl; the desk is the big house
 * where it roams, plays, wears costumes. A creature of the desk's size
 * feels the small house after a while: not unhappy there (never a sulk,
 * never reads as leaving for good -- fins are death; never a nag) -- it
 * is CRAMPED, and it has a wish, and it can say so with its body and one
 * touch. The wrist is also the house closest to him (his pulse, his
 * warmth, his arm): the place to REST. So the cramp is the desk-ward
 * pull while it is lively, and rest is the wrist-ward pull while it is
 * tired -- a resting creature is content in a small warm bowl, and the
 * cramp's clock pauses there. Whether it says its wish out loud is ITS
 * OWN roll at the moment of fullness: it is all up to the slime.
 *
 * Three things, one pure module:
 *
 *   THE CRAMP   a slow pressure, 0..1, that grows with LIT, HOLDING
 *               minutes on the wrist with no play and no walk --
 *               CRAMP_FULL_MIN to full. The outlets spend it: a game
 *               (the bead, the spin, the shove, the peekaboo) some, a pet
 *               more, a walk's minutes steadily. Shown as the body: pressed
 *               a touch WIDER against its wall (a stance, not a shape), the
 *               eyes roaming the rim now and then (idle.cpp's table takes
 *               the level as a weight), and once in a while a PUSH against
 *               the glass toward the open side -- a small lean off the
 *               wall and back (the shove's skid turned outward).
 *   THE NUDGE   the cramp FULL and the bigger house REACHABLE (the desk's
 *               own door answered within CRAMP_DESK_FRESH_MIN) -> the
 *               slime's own roll (CRAMP_NUDGE_ODDS, shifted by the grain's
 *               regard); won = ONE nudge: one bump (bumps.h's BUMP_NUDGE,
 *               through the gate), the push toward him, a look up, and
 *               `wrist:cramped` up the wire. His answers: a PET inside
 *               CRAMP_ANSWER_S = "stay a while" (content eyes, one settle,
 *               the cramp halved, no nudge for CRAMP_GAP_MIN); the HOLD =
 *               the release as ever (it got its wish); no answer = nothing
 *               more for CRAMP_GAP_MIN -- a wish, not a nag. Nowhere bigger
 *               reachable = no wish, only the fidget: a watch alone in the
 *               world is simply home.
 *   THE BID     the desk's bid worn on the wrist: after a long IGNORED
 *               lit-worn spell (no touch for BID_IGNORED_MIN while lit
 *               minutes accumulate -- a raise is not attention) it may
 *               once come to him: a look up held BID_LOOK_S, one bump (the
 *               same occasion), the small push toward him; a pet answers
 *               it (content); once per BID_GAP_MIN.
 *
 * Never in the dark house (the clocks pause), never asleep, never on the
 * shelf, never on the cable, never in the quiet hours, never while
 * RESTING (rest's dozing, a low cell, the hour before the quiet hours).
 * The release by any road zeroes the cramp (the next arrival starts
 * fresh). No Arduino: the clock and the state come in as parameters;
 * tools/cramp_test.cpp runs the whole table on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- the numbers, with their cause (each a line in "Open for Khalid") --- */
#define CRAMP_FULL_MIN          45.0f   /* lit, holding minutes with no play and no walk to full: a desk's afternoon of a bowl, not an hour of a glance */
#define CRAMP_SQUASH            0.06f   /* pressed wider against its wall at full: the idle settle's own depth -- a stance, under the stroke's 0.10 */
#define CRAMP_PUSH_PX           6.0f    /* the push off the wall, face px: a lean, not a hop (the shove's skid is 10) */
#define CRAMP_PUSH_S            0.6f    /* out and back on a half sine: one slow press */
#define CRAMP_PUSH_EVERY_MIN_S  90.0f   /* at full, a push every 90-180 s jittered; the gap stretches as 1/level under it (at half, 3-6 min) */
#define CRAMP_PUSH_EVERY_MAX_S  180.0f
#define CRAMP_PUSH_FROM         0.25f   /* no push under this level: a quarter-cramped gel only looks about */
#define CRAMP_SPEND_PLAY        0.15f   /* a game started spends this much of the level (four games empty a bowl of 0.6) */
#define CRAMP_SPEND_PET         0.25f   /* a pet spends more: a hand is more room than a bead */
#define CRAMP_SPEND_WALK_PER_MIN 0.10f  /* each walking minute: ten minutes of steps are the whole cramp (the step bob is its outlet) */
#define CRAMP_DESK_FRESH_MIN    15.0f   /* the desk's own door (the LAN door, the dock, the air door, a knock) answered within this = the bigger house is reachable */
#define CRAMP_NUDGE_ODDS        0.70f   /* the base odds it says the wish at all when full and the desk is near: seven in ten -- most wishes are said, some kept */
#define CRAMP_NUDGE_REGARD_W    0.20f   /* the grain's regard (-1..1) shifts the odds by this much: a well-handled slime asks more freely */
#define CRAMP_NUDGE_LOOK_S      2.0f    /* the look up after the nudge's push, held */
#define CRAMP_ANSWER_S          10.0f   /* a pet inside this answers the nudge ("stay a while") */
#define CRAMP_STAY_K            0.5f    /* the cramp after "stay a while": halved, not zeroed -- the bowl is as small as it was */
#define CRAMP_GAP_MIN           60.0f   /* no nudge for an hour after one (answered or not, said or kept): a wish, not a nag */
#define CRAMP_CONTENT_S         1.5f    /* the content eyes after his answer */
#define BID_IGNORED_MIN         20.0f   /* lit, holding minutes with no touch before the wrist's own bid */
#define BID_GAP_MIN             90.0f   /* once per hour and a half: the desk's BID_GAP_S is 20 min, but a wrist is looked at far less */
#define BID_LOOK_S              2.0f    /* the bid's look up, held */

/* --- the wire ------------------------------------------------------------ */

typedef struct {
    float    dt;
    uint32_t now_ms;
    bool     lit;          /* the glass is not DARK */
    bool     holding;      /* house_holding(): a gel to be cramped at all */
    bool     asleep;       /* rest->dozing (asleep or stirring) */
    bool     shelf;        /* rest_on_shelf() */
    bool     cable;        /* charging() or the dock */
    bool     quiet;        /* rest_night_now(): the quiet hours */
    bool     resting;      /* THE RESTING HOUSE: a low cell, the hour before the quiet hours -- content in the small warm bowl */
    bool     walking;      /* wrist_walking(): the step bob is spending it */
    bool     busy;         /* something else has the body (idle's own list): no push, no nudge this frame */
    float    desk_age_s;   /* seconds since the desk's own door last answered; < 0 = never */
    float    regard;       /* the grain's regard, -1..1 (0 with no grain) */
    bool     desk_far;     /* THE DESK NEXT DOOR (near.h): the desk is on the air door and its signal says FAR -- reachable, but not near: no nudge (false = near, or no air door to say) */
} cramp_in_t;

/* what the draw folds in this frame */
typedef struct {
    float squash;      /* the stance: CRAMP_SQUASH x the level, eased */
    float push_px;     /* the push off the wall (added to the lift), 0 when none */
    int   gaze_y;      /* -1 through the nudge's / the bid's look up, else 0 */
    int   eye_kind;    /* GEL_EYE_CONTENT through the answer's second, else -1 */
    float eye_alpha;   /* the eyes brought up for the look / the answer (the draw takes the max), else 0 */
} cramp_fx_t;

/* main.cpp's one-shot reactions, taken once (0 = none) */
enum { CRAMP_EV_NONE = 0,
       CRAMP_EV_NUDGE,       /* the nudge: one bump (BUMP_NUDGE), `wrist:cramped` up the wire */
       CRAMP_EV_BID,         /* the wrist's own bid: one bump (the same occasion) */
       CRAMP_EV_STAY,        /* his pet answered the nudge: "stay a while" -- one settle */
       CRAMP_EV_BID_ANSWERED /* his pet answered the bid: content */
};

/* why the nudge is held back right now (`cramp`); CRAMP_WHY_READY = it would roll at fullness */
enum { CRAMP_WHY_READY = 0, CRAMP_WHY_NOT_FULL, CRAMP_WHY_GATE, CRAMP_WHY_RESTING, CRAMP_WHY_NO_DESK,
       CRAMP_WHY_GAP, CRAMP_WHY_ROLLED_NO, CRAMP_WHY_OPEN, CRAMP_WHY_BUSY,
       CRAMP_WHY_FAR,       /* THE DESK NEXT DOOR: reachable over the air door, but its signal says FAR */
       CRAMP_WHY_N };

void  cramp_begin(uint32_t seed);          /* the die for the roll; everything zero */
void  cramp_tick(const cramp_in_t *in);    /* one call a frame */
void  cramp_fx(cramp_fx_t *out);           /* what the draw folds in */
int   cramp_take_event(void);              /* CRAMP_EV_*, once */

/* the outlets and his answers (main.cpp's hooks) */
void  cramp_pet(uint32_t now_ms);          /* a pet on him: spends CRAMP_SPEND_PET; answers an open nudge / bid */
void  cramp_play(uint32_t now_ms);         /* a game started (the bead, the spin, the shove, the boo): spends CRAMP_SPEND_PLAY */
void  cramp_touch(uint32_t now_ms);        /* any touch gesture: attention -- the bid's ignored clock restarts (a raise is not one) */
void  cramp_release(const char *why);      /* the house emptied by any road (the hold, `no desk`, a knock): zeroed; a wish granted is logged */
void  cramp_arrive(void);                  /* the arrival: fresh */

float cramp_level(void);                   /* 0..1 (idle.cpp's roam weight) */
float cramp_lit_min(void);                 /* the clock: lit, holding minutes counted since the arrival (the outlets do not rewind it) */
bool  cramp_nudge_open(void);              /* a nudge waits for his answer */
int   cramp_why(void);                     /* CRAMP_WHY_* right now */
const char *cramp_why_name(int why);
uint32_t cramp_rolls(void);                /* rolls at fullness since boot, said or kept */
uint32_t cramp_nudges(void);               /* nudges said since boot */
uint32_t cramp_bids(void);                 /* bids since boot */
/* the log line, one at a time (main prints it and calls cramp_log_taken) */
bool  cramp_log_new(void);
const char *cramp_log(void);
void  cramp_log_taken(void);
void  cramp_status(char *out, int cap);    /* `cramp` over serial */

#ifdef __cplusplus
}
#endif
