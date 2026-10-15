/* The gestures (Khalid's stage "4+3", 2026-09-09): what a tap, a double
 * tap, three knocks, a STROKE, a FEED, a shake and a double shake on the
 * wrist MEAN, and the WATER LOCK -- the glass ignoring a wet hand.
 * 2026-09-10 afternoon (Khalid: "the gestures for the lease are way too
 * hard to access"): a plain hold of about a second is THE LEASE (one
 * gesture, always the same: come / release by the house), a tap then a
 * hold is the stopwatch, a drag UP the glass is THE WATCH PAGE. The
 * README's GESTURES table is the one place every gesture is listed.
 *
 * A pure detector: no Arduino, no clock of its own (time comes in as
 * milliseconds), no output but events. main.cpp feeds it one call a
 * frame and maps the events onto the body's own reactions; tools/
 * gesture_test.cpp feeds it synthetic days on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "imu.h"
#include "touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GES_NONE = 0,
    GES_TAP,            /* one tap, after the double-tap window closed: the pet */
    GES_DOUBLE_TAP,     /* wrist:water -- he logged a drink */
    GES_TRIPLE_TAP,     /* three knocks: the whistle (come; armed, the hold after is release) */
    GES_STROKE,         /* wrist:stroked -- a finger drawn across the glass: the long pet (dx, dy = its travel) */
    GES_FEED,           /* wrist:meal -- a drag DOWN the glass, top third to bottom: he logged a meal */
    GES_DRAG_UP,        /* a drag UP the glass, bottom third to top third: THE WATCH PAGE (main.cpp; dx, dy = its travel) */
    GES_SWIPE,          /* THE SWIPE: a FLICK along his wall (under SWIPE_MS, over SWIPE_PX, not the feeding axis) -- with a page up it turns the page, on the bare glass it is THE SHOVE (main.cpp; n = ms the flick took, dx, dy = its travel in screen px: main signs it along the wall) */
    GES_HOLD,           /* a hold outside every window below (0.4-0.5 s: the one gap, over a slow tap): nothing (n = ms held) */
    GES_LEASE_HOLD,     /* THE LEASE, EASY: a plain hold of about a second (LEASE_MIN_MS..LEASE_MAX_MS, 0.5-1.9 s since 09-10 21:08 -- THE HOLD'S WINDOW, WIDER; no taps pending) -- the lease toggles: come on the empty glass, release while he is here (n = ms held) */
    GES_TAP_HOLD,       /* a tap (or two), then a press held 0.5-1.9 s (the lease's own window): the double-tap-and-hold -- CLOCKWORK's stopwatch (n = ms held) */
    GES_LONG_PRESS,     /* a plain hold lifted past the lease's window (LONG_MS 1.9 s), or a drag not lifted inside 1.5 s: nothing of its own here (main.cpp: the knocks' armed release; the lamp on the page; the creature's peekaboo past 2 s while it stays) (n = ms held) */
    GES_SHAKE,          /* wrist:up -- "I'm up" */
    GES_DOUBLE_SHAKE,   /* two shakes: the water lock toggles (screen on) */
    GES_LOCKED,         /* the water lock went on (n: 0 `lock` / the page's row, 1 a double shake, 2 THE WET GLASS LOCKED ITSELF); never NVS: a reboot unlocks */
    GES_UNLOCKED,       /* ... and off (n: 0 `unlock`, 1 a double shake, 2 the glass dried, 3 THE LOCK IS NEVER A TRAP: a plain hold of UNLOCK_HOLD_MS -- a hand that keeps pressing is not water) */
    GES_PHANTOM,        /* a burst of touches: a wet glass, nothing counted (n = touches) */
    GES_TOUCH_LOCKED,   /* a touch under the lock: inert (one line per LOCK_NOTE_MS) */
    GES_TAPS_DROPPED,   /* taps pending when a hold or a burst came: dropped (n = taps) */
    /* PEEKABOO (design/watch-ideas.md F49): a plain finger down PEEKABOO_MS
     * with no travel is a hand over the bowl -- PEEK_DOWN fires WHILE the
     * finger stays (n = ms so far), PEEK_UP on its lift (n = ms held),
     * just before the GES_LONG_PRESS the lift also yields as ever. The
     * detector knows nothing of the house or the knocks: main gates it
     * (HOLDING, no knock armed) and swallows the long press it took. */
    GES_PEEK_DOWN,
    GES_PEEK_UP,
    /* THE RIM AS THE DIAL (design/watch-ideas.md B13): a drag whose
     * points all stay within RIM_BAND_PX of the rim and that has swept
     * more than ARC_MIN_DEG of arc is the ARC. While it goes on, ARC_MOVE
     * for each new degree under the finger (n = the finger's angle in
     * degrees from 12 o'clock clockwise, dx = the degrees swept so far,
     * signed; x, y = the finger); on the lift ARC (n = the end angle, dx
     * = the sweep, x, y = where it began) -- and nothing else for that
     * drag: not the stroke, not the hold, however long it took. The
     * meaning (the alarm's dial) is clockwork's; main routes it. */
    GES_ARC_MOVE,
    GES_ARC,
    /* THE ECHOED RHYTHM (design/watch-ideas.md F51, 2026-10-09): a FOURTH
     * tap inside the knocks' window (DOUBLE_WINDOW_MS after the third's
     * lift) un-arms the knocks and OPENS THE RHYTHM -- RHYTHM_START at
     * once (n = 4; main un-arms the knocks' hold); the taps that follow
     * inside RHYTHM_GAP_MS of the last join it, up to GES_RHYTHM_MAX
     * (the sixth closes it at once; more inside the gap are swallowed),
     * and none of them is a pet or a drink. The gap passing closes it:
     * RHYTHM (n = the taps, 4-6; dx = ms since the last tap's landing)
     * -- gesture_rhythm_gaps hands the intervals over. A hold or a drag
     * while it is open drops it (TAPS_DROPPED, n = its taps). The
     * detector knows nothing of the house: on the empty glass the knocks
     * were `come`, and main lets the rhythm pass (nobody to echo). The
     * echo itself is play.cpp's. */
    GES_RHYTHM_START,
    GES_RHYTHM,
    /* THE TICKLE (THE OPERATOR NIGHT, design/watch-night-2026-10-12.md
     * C4): ONE finger down the whole way, scrubbed back and forth --
     * TICKLE_N reversals (3) inside TICKLE_MS (1 s), each a real turn
     * of TICKLE_PX or more back the other way, not a jitter -- read on
     * the lift, however long the scrub went on (as the arc is), before
     * the drags and the holds: a scrub was never a stroke, a lease's
     * hold or a tap. n = the reversals; x, y, dx, dy = the scrub's
     * BOUNDING BOX (its corner and its size), not a travel -- a scrub
     * goes nowhere, and main asks whether the whole of it lay inside
     * his painted box (ON him: the tickle; beside him: nothing). A
     * phantom burst is many touch-downs and never this; the detector
     * knows nothing of the house: the wriggle is main.cpp's. */
    GES_TICKLE,
} gesture_kind_t;

#define GES_RHYTHM_MAX 6    /* "four to six taps": the cap (play.h's RHYTHM_MAX is the same 6) */

typedef struct {
    gesture_kind_t kind;
    int n;              /* taps in the burst, ms held, touches in the phantom burst */
    int x, y;           /* where on the glass (touch events; a drag's start) */
    int dx, dy;         /* a drag's travel in screen px (the stroke, the feed, the swipe); 0 otherwise */
} gesture_event_t;

/* THE GEL'S FRAME: the wall he sits on (walls.h's order, gel.h's
 * GEL_WALL_*: 0 bottom, 1 right, 2 top, 3 left), told by main each
 * frame. Every drag is turned by it before it is judged: "down" is
 * toward the base, "up" off the crown, "along" the wall. A boot sits
 * on the bottom wall (the screen's own frame) until told otherwise */
void gesture_set_wall(int wall);
int  gesture_wall(void);
/* at boot: UNLOCKED, always -- THE LOCK IS NEVER A TRAP (2026-09-10
 * night): no lock, whoever set it, survives a reboot; there is no NVS
 * key for it */
void gesture_begin(uint32_t now_ms);
/* one call a frame: the clock, the latest IMU sample (repeats are
 * ignored: the loop runs faster than the IMU), the touch state, whether
 * the screen is lit (a double shake in the dark is not the lock).
 * Fills up to ``cap`` events, returns how many. */
int  gesture_tick(uint32_t now_ms, const vec3_t *acc, const touch_t *t, bool screen_on,
                  gesture_event_t *out, int cap);
bool gesture_locked(void);
/* the lock is the wet glass's own (a phantom storm turned it on; it
 * lets go by itself once the glass is dry, or on the double shake) */
bool gesture_auto_locked(void);
/* `lock` / `unlock` over serial, the page's WATER LOCK row: the
 * GES_LOCKED / GES_UNLOCKED event surfaces on the next tick, so every
 * way in is logged the same. RAM only: a reboot forgets it, and the
 * double shake or a 3-s hold on the glass clears it like any lock */
void gesture_set_lock(bool on);
/* a finger on the glass counts as attention -- unless the lock is on */
bool gesture_touch_live(void);
/* what the gel shows: the drink's one contented settle and the stroke's
 * deeper one (a squash to add); and the meal's LUMP, 0..1 -- the
 * engulfed meal as THE BELLY (gel.h: GEL_BELLY_AMP x lump wider at the
 * lower flank, the crown flat -- Khalid 09-10: the desk's crown lump
 * read as "its head enlarges"), 1.0 at the feed, sinking away over
 * LUMP_S as it dissolves (ease-out:
 * fast at first, the last of it lingering); a second feed while one is
 * sinking tops it back up (max, not sum: one belly). main.cpp puts it
 * in gel_cell_params_t.lump */
float gesture_squash(void);
float gesture_lump(void);               /* 0..1 */
/* THE DRINK LINE (design/watch-ideas.md D30, 2026-09-23): each double-tap
 * drink lifts the meniscus a hair -- DRINK_LINE of resting squash, the
 * rain's own fullness term at half -- capped at DRINK_LINE_MAX (three
 * drinks' worth), each hair sinking back over DRINK_SINK_S (two hours);
 * a dry afternoon is a gel at its resting line, nothing said. Rides
 * gesture_squash() with the settles; this is the line alone, 0..MAX */
float gesture_drink_line(void);
/* A DRINK ON A HOT DAY (design/watch-ideas.md A9, 2026-10-08): "water
 * is welcome to a warm gel". main.cpp tells the detector each frame
 * whether the sky's word is `hot` (sky_hot()); a drink taken WARM has
 * its settle HOT_DRINK_SETTLE_K deeper (0.08 -> 0.12 of squash, for
 * that settle alone), its hair HOT_DRINK_LINE_K taller (0.02 -> 0.04:
 * the rain's own term, SKY_RAIN_SQUASH -- a hot day's drink fills him
 * like a wet day), the cap and the sink untouched (one law: a hair a
 * two hours, so the taller hair takes four); and main holds the
 * content eyes HOT_DRINK_EYES_K longer (1.2 -> 1.8 s). The sky
 * changing after the drink changes nothing already drunk. The
 * detector knows nothing of the sky but this one word */
#define HOT_DRINK_SETTLE_K 1.5f     /* the settle's depth on a hot day (a guess at "deeper": half again, under the stroke's 0.10) */
#define HOT_DRINK_EYES_K   1.5f     /* the content eyes' hold on a hot day (a guess at "held longer": half again) */
#define HOT_DRINK_LINE_K   2.0f     /* the hair on a hot day: the rain's own 0.04 (the item's "the rain's own term") */
void  gesture_set_warm(bool warm);
bool  gesture_warm(void);
/* the stroke's lean envelope, 0..1: in fast, easing back over
 * STROKE_LEAN_S. main.cpp signs it by the stroke's travel along the
 * gel's own floor (the detector knows nothing of the bowl's angle) */
float gesture_stroke(void);
/* THE LULL (lull.c, watch-night C11): how long the last stroke's finger
 * was down, ms (the stroke's event carries its samples and its travel,
 * not its time; a stroke over LULL_STROKE_MS is a slow one) */
uint32_t gesture_stroke_ms(void);
/* the glass was empty: whatever the last event set showing (the lump,
 * a settle, the lean) is undone -- there was no gel to show it, and he
 * must not arrive a minute later wearing a meal he never had */
void  gesture_unshow(void);
/* THE WISP EATEN (watch-ideas E41, wisp.cpp): the feed that landed on a
 * wisp was the creature's meal, not his -- the meal's lump alone is
 * undone (the drink line, the settles, the lean stay) */
void  gesture_unlump(void);
const char *gesture_name(gesture_kind_t k);
/* THE RHYTHM's intervals, ms between the taps' landings of the last
 * rhythm that closed (GES_RHYTHM): taps - 1 of them, at most
 * GES_RHYTHM_MAX - 1; returns how many (0 = none yet) */
int   gesture_rhythm_gaps(uint32_t *gaps, int cap);

#ifdef __cplusplus
}
#endif
