/* THE HOUSE (Khalid, 2026-09-09 23:00: "the watch and my laptop both
 * have the slime running? the slime is only supposed to be in one place,
 * that's the whole lease"; 23:05: "when the slime leaves or is in another
 * machine the watch is EMPTY -- the slime shouldn't be there"; 23:20:
 * no door mark, and the wrist is the default home when offline).
 *
 * One soul, one body at a time (multi-body-roadmap; watch-link.md
 * section 6). The wrist is a HOUSE the slime lives in or has left:
 *
 *   HOLDING  the lease is here: the gel is drawn, the creature as ever
 *   EMPTY    the slime is elsewhere (the desk has him): the black glass
 *            -- nothing but the glance's clock on a raise and the word
 *            bubble if one came
 *   UNKNOWN  never told (a first boot, nothing in NVS): drawn as EMPTY
 *
 * The gel appears on the wrist only by ARRIVING: rising out of the rim
 * at its resting spot after an `ok held`, or on the offline rule below.
 *
 * Learned from the link's lease answers (link.cpp feeds them here):
 * `ok held` -> HOLDING (the ARRIVAL when it was not drawn); `no desk` ->
 * EMPTY (the DEPARTURE when it was); `no lapsed` on a beat while holding
 * -> one more claim, then EMPTY; `no box` / `no gate` / `no soul` ->
 * nothing changes (sticky). The last known state persists in NVS
 * (`house`): a wrist that held when it left the house keeps the slime
 * on the bus; one that was empty stays empty until it finds a door.
 *
 * THE WRIST IS THE DEFAULT HOME WHEN OFFLINE: no link for LINK_LOST_S
 * (awake seconds without one, kept across naps, cleared by a link) AND
 * the watch WORN (motion within WORN_WINDOW_MIN -- the wrist on his arm
 * is the proof of him, as the chair is for the desk) makes the house
 * HOLDING on its own. Offline and still (a shelf) keeps the last state.
 * When a door is found again the honest `claim` decides as before: `ok
 * held` keeps it, `no desk` is the departure -- the desk had him; the
 * silent correction of the guess.
 *
 * The theatre (watch-body.md section 1), calm and wordless: the arrival
 * = the gel RISES out of the rim at its resting spot, the squash from
 * the floor easing up, eyes in (RISE_S); the departure = THE GOODBYE
 * first (design/watch-ideas.md C19: a creature leaves a room facing
 * the person in it -- a glance up and one calm settle over BYE_S, the
 * desk's own goodbye mirrored, so the two houses leave the same way),
 * then it SINKS into the rim, the squash down to nothing, eyes out
 * (SINK_S). A tap on the empty glass does nothing at all; the long
 * press is still the whistle (link.cpp).
 *
 * Nothing here touches the gel, the bowl or the fall: main.cpp asks
 * house_fx() what to draw and adds the few numbers to its own. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum { HOUSE_UNKNOWN = 0, HOUSE_HOLDING = 1, HOUSE_EMPTY = 2 };

/* what main.cpp's draw() adds this frame */
typedef struct {
    bool  gel;          /* draw the gel at all (HOLDING, or the theatre's rise / sink) */
    float squash;       /* added to the authored squash (the floor's flatness rising out, the sink) */
    float lift;         /* added to the fall's lift (negative = pushed under the rim) */
    float eye_k;        /* the face's presence scaled: fading out, coming in */
    int   gaze_y;       /* THE GOODBYE: the glance up (-1) before the sink, else 0 -- added to the draw's gaze */
    float eye_alpha;    /* THE GOODBYE: the eyes brought up for it (the draw takes the max), else 0 */
} house_fx_t;

/* main.cpp's one-shot reactions, taken once (0 = none) */
enum { HOUSE_EV_NONE = 0, HOUSE_EV_ARRIVED, HOUSE_EV_LEFT };

void house_begin(void);                 /* NVS: the last known state, or UNKNOWN */
/* one call a frame: the theatre's clocks and the offline rule --
 * linked = link_linked() || link_resting() (THE RADIO BUDGET: a radio
 * resting by choice is not offline; only a due dial that found no door
 * lets the clock run), still_ms = since the body last moved */
void house_tick(float dt, bool linked, uint32_t still_ms);
void house_fx(house_fx_t *out);         /* what to draw this frame */
int  house_take_event(void);            /* HOUSE_EV_*, once */
/* THE BUMPS (bumps.h): true once when the SINK began this frame -- the
 * goodbye's settle done (or the at-once sink mid-arrival); never for the
 * night's release (no theatre: the glass is dark, nobody to feel it) */
bool house_take_sink(void);

/* the link's lease answers: verb = "claim" | "beat" | "come" | "release",
 * line = the reply as said; true = ask the link to claim once more (a
 * beat's `no lapsed` while holding) */
bool house_lease(const char *verb, const char *line);
/* THE LEASE, EASY (2026-09-10): the release on the wrist's OWN word --
 * the sink starts the instant the hold is read, before the box answers
 * (Khalid: "the slime still doesn't go away" -- never again for want of
 * a reply). `ok released` confirms it; `no desk` / `no lapsed` agree
 * (he is elsewhere either way); any other `no ...` to the release
 * UN-SINKS him (he stands again, logged); no answer at all leaves the
 * wrist's word standing (link.cpp says so). Call it AFTER link_whistle()
 * (the whistle reads the house to pick come / release). */
void house_release_now(void);
/* THE NIGHT RELEASE (ruling 52): the wrist lets go for the night on its
 * own word too -- HOLDING -> EMPTY with NO theatre (the glass is dark:
 * nobody to say goodbye to), the box's `ok released` confirming as for
 * the hold's release; a refusal stands him up again without theatre
 * either (a rise at the 3 am glance would be a lie). false = the house
 * was not holding: nothing to let go of, nothing to send. The rising's
 * `ok held` is the ordinary arrival: the gel rises at the first lit
 * frame -- the morning. */
bool house_night_release(void);
bool house_release_pending(void);       /* released on the wrist's word, the box not yet heard */
void house_release_unanswered(void);    /* the link gave up waiting: the wrist's word stands */
bool house_holding(void);               /* HOLDING: the pet, the senses, the Zs' drawing */
/* WORN ON THE BEAT (SAGE-WATCH-LINK-SLIME-ACK section 8): the offline
 * rule's own worn test -- motion on the arm within WORN_WINDOW_MIN (5)
 * as of the last tick; link.cpp puts the word on the minute's verb */
bool house_worn(void);
/* ... and the number under it: ms since the body last moved, as of the
 * last tick (THE DOCK HANDS HIM OVER reads the undock's minute off it) */
uint32_t house_still_ms(void);
int  house_state(void);
const char *house_state_name(void);
/* which senses still go up while EMPTY: his day regardless -- raised,
 * shaken, slept; nothing the gel would have felt */
bool house_sense_ok(const char *kind);
void house_force(int state, const char *why);   /* `house holding|empty`: a test aid, logged */
void house_status(char *out, int cap);  /* `house`: the state, its cause, its age, the theatre, the offline clock */
