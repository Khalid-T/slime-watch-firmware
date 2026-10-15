/* The gestures and the water lock: the detector. See gesture.h.
 *
 * Reasoned from the material (design-zoom-out): the glass is the gel's
 * window; a finger on it is attention, a tap a pet, a finger DRAWN
 * across it a longer one (the stroke), and a rhythm of taps a WORD --
 * the only words the wrist has, so they stay few: two taps "I drank",
 * three knocks "come here" (and, held after, "go"); four to six are
 * not a word but a BEAT, THE ECHOED RHYTHM (F51, 2026-10-09: a fourth
 * tap in the knocks' window un-arms them and opens it; the gel repeats
 * it a second later -- play.cpp's). The meal is not a
 * word but a MOTION: a finger drawn down the glass from the top, as if
 * feeding him from above. A shake is the body being roused ("I'm up");
 * two shakes are the hand asking the glass to stop listening, because
 * water on it is not a hand. Under the lock the gel goes on living and
 * ignores the glass; nothing on the wrist is ever a buzz or a word.
 *
 * THE LEASE, EASY (Khalid, 2026-09-10 afternoon: "I have to triple tap
 * and then some, and the slime still doesn't go away"): a hand RESTING
 * on the glass for about a second is the one gesture for the lease --
 * the same hold whether he is here or not (main.cpp reads the house:
 * come / release). The knocks stay as a second way to call him. The
 * stopwatch, which had the plain hold, moved to a tap-then-hold (the
 * double-tap-and-hold every phone knows). And a finger drawn UP the
 * glass -- the one axis that meant nothing -- turns the glass over to
 * its other face, THE WATCH PAGE (main.cpp / page.cpp).
 *
 * THE LOCK IS NEVER A TRAP (2026-09-10 night, from Khalid's 21:16-21:29
 * trap: the settings page's WATER LOCK, tapped on while exploring, was
 * written to NVS and two reboots kept the glass inert; his knocks and
 * hold would have claimed the slime back and the lock ate them). The
 * lock is a MOMENT, whoever set it -- the page, the console, the wet
 * glass -- never a setting: nothing of it reaches NVS (gesture_begin
 * starts unlocked, always), and ANY lock lets go on the double shake or
 * on a plain hold of UNLOCK_HOLD_MS with no travel: water flickers, a
 * hand that keeps pressing is not water. The unlocking hold's lift is
 * inert (no long press, no peekaboo out of it).
 *
 * Plain C++ with no Arduino: time is a parameter, output is events. */
#include "gesture.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --- the tunables ------------------------------------------------------- */

/* TAPS. The CST816S reports a finger ~40 times a second from the loop;
 * a real tap is 60-200 ms of contact. Under TAP_MIN_MS is a blip (a
 * drop of water landing, a knuckle brushing), over TAP_MAX_MS a hold. */
static const uint32_t TAP_MIN_MS      = 30;
static const uint32_t TAP_MAX_MS      = 400;    /* main's old tap bound, kept */
/* the double-tap window: a second tap this soon after the first's
 * release makes them a word. 350 ms is what phones settle on; the pet
 * is delayed by exactly this, which reads as the gel noticing. */
static const uint32_t DOUBLE_WINDOW_MS = 350;
/* THE ECHOED RHYTHM (design/watch-ideas.md F51, 2026-10-09): the fourth
 * tap must land inside the knocks' own window (DOUBLE_WINDOW_MS after
 * the third's lift -- the item's "a fourth tap in the knocks' window");
 * from there each next tap must LAND within RHYTHM_GAP_MS of the last
 * one's landing (a beat slower than ~85 a minute is two things, not a
 * rhythm; the knocks' 350 would make every rhythm a drum roll), up to
 * RHYTHM_TAPS_MAX taps (the item's six; the sixth closes it at once).
 * The floor is the phantom's rule, not a number here: five landings
 * inside a second is water (PHANTOM_N), so a rhythm's beats sit 250 ms
 * apart at the fastest. The gap passing closes it; the intervals are
 * then play.cpp's to echo. */
static const uint32_t RHYTHM_GAP_MS   = 700;
static const int      RHYTHM_TAPS_MAX = GES_RHYTHM_MAX;
/* the long press: a PLAIN hold lifted past the lease's window (over
 * LEASE_MAX_MS: 1.9 s -- the peekaboo's 2 s is past it, so a plain
 * hold that stays is the creature's), or a DRAG that never lifted
 * inside STROKE_MS (1.5 s). Nothing of its own: main's knocks' armed
 * release; the lamp on the page. (Until 2026-09-10 21:08 the long press
 * began at 1.5 s for every hold; THE HOLD'S WINDOW, WIDER moved the
 * plain hold's edge out to the lease's ceiling.) */
static const uint32_t LONG_MS         = 1901;   /* = LEASE_MAX_MS + 1: the first ms past the lease's ceiling */
/* THE LEASE: a plain hold (no taps pending, under STROKE_PX of travel)
 * that lifts inside LEASE_MIN_MS..LEASE_MAX_MS is the lease's hold --
 * about a second (LEASE_HOLD_MS is the number to say; the window is
 * how a hand keeps it). THE HOLD'S WINDOW, WIDER (Khalid, 2026-09-10
 * 21:08, on his wrist: "make it easy access for the watch for it to
 * leave" -- 0.8-1.4 s was too tight for a thumb): 0.5-1.9 s now. The
 * floor is a hair over a slow tap's 0.4 s (0.4-0.5 s is GES_HOLD,
 * nothing: the one gap left, so a dragged-out tap never toggles the
 * lease); the ceiling is under PEEKABOO's 2 s, so a hand that STAYS
 * past 2 s is still the peekaboo and never the lease. */
static const uint32_t LEASE_HOLD_MS   = 1000;
static const uint32_t LEASE_MIN_MS    = 500;
static const uint32_t LEASE_MAX_MS    = 1900;
/* THE STOPWATCH's hold: a tap (or two) pending, then a press held
 * TAP_HOLD_MS..TAP_HOLD_MAX_MS -- the lease's own window, the length a
 * hand calls a second (widened with it, 09-10 21:08: the same thumb):
 * the double-tap-and-hold. Outside it the taps are dropped and the
 * hold is nothing (or the long press), as a tap then a long press
 * always was. clockwork.cpp's HOLD_MIN_MS / HOLD_MAX_MS are these. */
static const uint32_t TAP_HOLD_MS     = 500;
static const uint32_t TAP_HOLD_MAX_MS = 1900;
/* PEEKABOO (design/watch-ideas.md F49, 2026-09-10): a finger that
 * stays on the glass this long without travelling (under STROKE_PX) is
 * a hand over the bowl -- past the lease's and the stopwatch's 1.9 s
 * ceiling, so a plain long hold is the only way in; GES_PEEK_DOWN
 * while it stays, GES_PEEK_UP on the lift (the boo), the long press
 * still yielded after it for whoever else holds the glass that long */
static const uint32_t PEEKABOO_MS     = 2000;
/* THE LOCK IS NEVER A TRAP: under any lock a plain finger that stays
 * this long without travelling (under STROKE_PX) is a hand, and the
 * lock lets go while it still presses -- past peekaboo's 2 s (which is
 * only ever read unlocked) so the two never meet; the lift after is
 * inert. Water never holds a steady 3 s on a capacitive glass */
static const uint32_t UNLOCK_HOLD_MS  = 3000;
/* THE ARC (design/watch-ideas.md B13, 2026-09-10): a finger drawn
 * round the rim. Every sample within RIM_BAND_PX of the rim (r >= 120
 * - 30 = 90 from the centre: the band the rings and the hour tick live
 * in, wide enough for a fingertip) and more than ARC_MIN_DEG swept --
 * a quarter turn is 90, a stroke along the rim is under a sixth -- is
 * the arc, whatever the time it took (the dial is turned slowly). One
 * sample inside the band disqualifies the drag for good: it is then
 * judged as any drag. The angle is 0 at 12 o'clock, + clockwise. */
static const float    RIM_BAND_PX     = 30.0f;
static const float    ARC_MIN_DEG     = 60.0f;
/* DRAGS. A finger that travels STROKE_PX across the glass before it
 * lifts is a drag (under that it is a tap, a wobbly one: the old 24 px
 * slop is folded in here, a drag now having a meaning of its own); the
 * drag needs STROKE_MIN_N samples (a finger, not a jump between two
 * drops of water), must lift inside STROKE_MS (a finger still on the
 * glass past that is the LONG PRESS, wherever it went -- never the
 * lease's hold: a finger drawn is not a hand resting), and must go MOSTLY ONE
 * WAY: its net travel at least STROKE_STRAIGHT of the path it drew (a
 * scrub back and forth has no direction and is nothing). A clearly
 * vertical drag (|dy| > FEED_VERTICAL x |dx|) is the feeding axis:
 * DOWN from the top third of the glass to the bottom third is the FEED;
 * UP from the bottom third to the top third is THE WATCH PAGE's drag
 * (GES_DRAG_UP); any other vertical drag (down but not from the top, up
 * but not from the bottom) is nothing. Every other drag is the STROKE
 * -- or, flicked (under SWIPE_MS, below), THE SWIPE. */
static const int      STROKE_PX       = 60;
static const uint32_t STROKE_MS       = 1500;
static const int      STROKE_MIN_N    = 3;
static const float    STROKE_STRAIGHT = 0.6f;
static const int      FEED_VERTICAL   = 2;
static const int      GLASS_PX        = 240;    /* SCREEN_H (board.h): the thirds are 80 px */
/* THE GEL'S FRAME (design/watch-swipes.md section 2, built 2026-09-23):
 * the axes above are HIS, not the screen's. main tells the detector the
 * wall he sits on (gesture_set_wall: 0 bottom, 1 right, 2 top, 3 left
 * -- walls.h's order) and a drag's travel and its start and end are
 * turned by the wall's quarter before the tests above are made, so
 * "down" is toward his base whichever wall that is (on the right wall
 * the feed is a drag from the left of the glass RIGHT, into his mouth;
 * the page's drag is a drag LEFT, off his crown) and "along" is along
 * his wall. A quarter turn is a swap of the axes, so it is integer
 * arithmetic, no trig. The rim's arc is not turned: the dial is the
 * watch's, not the slime's (the far tap in main is in his frame too).
 * THE SWIPE (section 3): a drag along his wall that is FLICKED -- lifted
 * under SWIPE_MS with SWIPE_PX or more of travel -- is the swipe; drawn
 * slower it is the stroke as ever. The line between them is SPEED, both
 * sides pinned in gesture_test: 250 ms is a swipe, 350 a stroke. The
 * flick's floor (40 px) is under the stroke's 60: a flick is short
 * and quick, a stroke long and slow; a 40-60 px drag that is not quick
 * is still the wobbly tap it always was. A flick on the feeding axis
 * is not a swipe (that axis is the feed's and the page's; a fast feed
 * across the thirds is a feed); a scrub is nothing, as ever. (Rulings
 * 54-57 in design/watch-map.md: built with the proposal's numbers.) */
static const uint32_t SWIPE_MS        = 300;
static const int      SWIPE_PX        = 40;
/* THE TICKLE (watch-night-2026-10-12 C4): a scrub back and forth under
 * ONE finger -- TICKLE_N reversals inside TICKLE_MS. A reversal is the
 * finger turning back on an axis by TICKLE_PX or more past its last
 * extreme (a peak detector with that much hysteresis: the CST816S
 * wobbles a px or two under a still finger, and that is not a turn);
 * both axes reversing on one sample (a diagonal scrub) count once.
 * The last TICKLE_N reversals' stamps are kept: the newest inside
 * TICKLE_MS of the oldest makes the touch a tickle, read on the lift.
 * Three reversals is the fewest that is a scrub and not a wobbly
 * drag (out, back, out again); a second is a tickle's own tempo (a
 * slow out-and-back is a stroke there and a stroke back). */
static const int      TICKLE_N        = 3;
static const uint32_t TICKLE_MS       = 1000;
static const int      TICKLE_PX       = 6;
/* PHANTOMS. Water on a capacitive glass reads as touches that come and
 * go faster than any finger: more than PHANTOM_N touch-downs inside
 * PHANTOM_MS is a wet glass, and the burst -- and anything pending --
 * is dropped; the glass is deaf until PHANTOM_QUIET_MS of nothing. */
static const int      PHANTOM_N       = 4;
static const uint32_t PHANTOM_MS      = 1000;
static const uint32_t PHANTOM_QUIET_MS = 1500;
/* THE WET GLASS LOCKS ITSELF. One burst is a splash; a STORM -- more
 * than PHANTOM_STORM_N dropped bursts inside PHANTOM_STORM_S (a burst
 * can be read at most every 1.5 s, so this is the flicker going on for
 * half the window: rain, a shower, a wet sleeve) -- is water that stays,
 * and the water lock turns itself ON for AUTO_LOCK_S. It comes off by
 * itself once that window has passed AND the glass has been quiet of
 * phantoms for AUTO_QUIET_S (still flickering: still wet, still locked),
 * or sooner on the double shake or the 3-s hold (UNLOCK_HOLD_MS). A lock
 * the hand or the console set is never let go of by itself; and NO lock
 * reaches NVS -- a wet glass is a moment, not a setting, and since
 * 09-10 night neither is the hand's (THE LOCK IS NEVER A TRAP). */
static const int      PHANTOM_STORM_N = 3;
static const uint32_t PHANTOM_STORM_MS = 10000;
static const uint32_t AUTO_LOCK_MS    = 120000;
static const uint32_t AUTO_QUIET_MS   = 30000;
/* under the lock a touch is inert; one log line per LOCK_NOTE_MS so a
 * shower is not a thousand lines */
static const uint32_t LOCK_NOTE_MS    = 5000;

/* SHAKES. The IMU's sample, high-passed (a one-pole low-pass of
 * SHAKE_TAU_MS subtracted: gravity and a slow turn drop out, a flick
 * stays). An EXCURSION is |hp| beyond SHAKE_G on one axis; a REVERSAL is
 * that axis crossing from beyond +G to beyond -G or back. Two reversals
 * inside SHAKE_WINDOW_MS on one axis is the shape of a deliberate
 * shake (out-back-out). SHAKE_G sits above what the gel reads as a
 * jolt: main's slosh saturates at JOLT_CAP (a jerk of ~1.4 g against its
 * own k 0.18 filter) and the day log's SHAKEN is 0.6 g -- a shake here
 * is meant, never a walk (a walk's high-passed peaks are 0.2-0.4 g). */
static const float    SHAKE_G         = 1.0f;
static const uint32_t SHAKE_TAU_MS    = 300;
static const uint32_t SHAKE_WINDOW_MS = 500;
/* a shake is a BURST: it is read only once the excursions have stopped
 * for SHAKE_GAP_MS, and only if the whole burst was shorter than
 * SHAKE_MAX_MS -- a run reverses the vertical axis at stride rate for
 * minutes and never goes quiet, so it is never a shake (nor a lock). */
static const uint32_t SHAKE_GAP_MS    = 250;
static const uint32_t SHAKE_MAX_MS    = 900;
/* the double shake: a second shake read within DOUBLE_SHAKE_MS of the
 * first is the lock, and the first is then NOT an "up" -- one gesture,
 * one meaning. A lone shake waits the window out before it is "up". */
static const uint32_t DOUBLE_SHAKE_MS = 1200;

/* WHAT THE GEL SHOWS. The meal: THE LUMP IS A BELLY -- the engulfed
 * meal widening the lower flank (gel.c's dome term, GEL_BELLY_AMP 0.25;
 * the desk's crown lump, LUMP_AMP 0.60 on sin^3, read as "its head
 * enlarges" on the wrist, Khalid 09-10 18:55), 1.0 at the feed and
 * sinking away over LUMP_S as the meal dissolves: the silhouette is
 * the progress bar. The sinking is an EASE-OUT -- the meal's clock runs linear and
 * the lump is (1 - t)^2 of it, fast at first and the last of it
 * lingering, the way a lump goes in the desk's dissolve; a second feed
 * while one is sinking tops it back up (the clock reset: max, not sum --
 * one belly, not two meals). The drink: one contented settle,
 * WATER_SQUASH for WATER_S, in and out on a half sine (calm: less than
 * the pet). */
static const float    LUMP_S          = 60.0f;
static const float    WATER_SQUASH    = 0.08f;
static const float    WATER_S         = 0.40f;
/* THE DRINK LINE (design/watch-ideas.md D30, 2026-09-23: "what a gel
 * drank shows in the gel"): past the settle, each drink LIFTS THE
 * MENISCUS a hair -- DRINK_LINE of resting squash, half the sky's rain
 * term (SKY_RAIN_SQUASH 0.04: "the gel fuller, the meniscus a little
 * higher"; the water line bends up with the body, the same term) --
 * capped at DRINK_LINE_MAX (three drinks' worth: a wet day and a half,
 * never a puddle), and each hair sinks back over DRINK_SINK_S (the
 * item's two hours; a capped line takes three of them). The raster's
 * squash quantum is 1/32 = 0.031: one hair moves the line through the
 * breath's crossings, two move it outright. Nothing said, nothing
 * counted on the glass: a dry afternoon is a gel at its resting line */
static const float    DRINK_LINE      = 0.02f;
static const float    DRINK_LINE_MAX  = 0.06f;
static const float    DRINK_SINK_S    = 7200.0f;
static const float    DRINK_LINE_GONE = 1e-4f;    /* under this the line is at rest (a float's last crumb, a thousandth of a quantum) */
/* the stroke: a deeper settle than the drink's (a longer pet presses
 * more), in and out on a half sine over STROKE_SETTLE_S; and a lean
 * into the finger's travel that comes in over STROKE_LEAN_IN_S and
 * eases back by STROKE_LEAN_S (the amplitude is main's, signed by the
 * floor) */
static const float    STROKE_SQUASH   = 0.10f;
static const float    STROKE_SETTLE_S = 1.2f;
static const float    STROKE_LEAN_IN_S = 0.3f;
static const float    STROKE_LEAN_S   = 2.0f;

/* --- the state ------------------------------------------------------------- */

static bool     locked = false;
static bool     lock_pending = false, lock_pending_on = false;   /* `lock` / `unlock` typed */
static uint32_t last_ms = 0;

/* touch */
static int      wall = 0;               /* THE GEL'S FRAME: the wall he sits on (gesture_set_wall); 0 = the bottom, the screen's own frame */
static bool     down_prev = false;
static uint32_t down_ms = 0;
static int      tx0 = 0, ty0 = 0;       /* where the finger landed */
static int      tx1 = 0, ty1 = 0;       /* where it is now */
static int      drag_n = 0;             /* samples with the finger down */
static float    path = 0;               /* the length of the path it drew */
static int      taps = 0;               /* taps pending in the burst */
static uint32_t tap_down[3];            /* the pending taps' landings (the knocks' three, kept for the rhythm's stamps) */
static uint32_t knock_up_ms = 0;        /* THE RHYTHM: the third knock's lift -- a fourth tap landing inside DOUBLE_WINDOW_MS of it opens the rhythm (0 = no knocks just now) */
static bool     rhythm_open = false;    /* the rhythm is being tapped */
static uint32_t rh_stamp[GES_RHYTHM_MAX];   /* its taps' landings */
static int      rh_n = 0;
static uint32_t rh_full_until = 0;      /* the sixth tap closed it: taps landing before this are swallowed, not pets */
static uint32_t last_gaps[GES_RHYTHM_MAX - 1];   /* the last closed rhythm's intervals */
static int      last_ngaps = 0;
static bool     peek_sent = false;      /* PEEKABOO: this touch's PEEK_DOWN went out; the lift owes a PEEK_UP */
/* THE TICKLE: this touch's reversals */
static int      tk_ext[2] = { 0, 0 };   /* each axis's last extreme since its last reversal */
static int8_t   tk_dir[2] = { 0, 0 };   /* each axis's way, +1 / -1, 0 not yet moved TICKLE_PX */
static int      tk_rev = 0;             /* reversals so far */
static uint32_t tk_stamp[TICKLE_N];     /* the last TICKLE_N reversals' stamps (a ring) */
static int      tk_head = 0;
static bool     tk_live = false;        /* TICKLE_N reversals inside TICKLE_MS: the touch IS a tickle */
static int      tk_min[2] = { 0, 0 }, tk_max[2] = { 0, 0 };   /* the scrub's bounding box (main reads it against his painted box) */
static bool     unlock_took = false;    /* THE LOCK IS NEVER A TRAP: this touch's hold unlocked the glass; its lift is inert */
/* THE ARC */
static bool     arc_ok = false;         /* every sample so far in the rim's band */
static bool     arc_live = false;       /* ARC_MIN_DEG swept: the drag IS the arc */
static float    arc_ang = 0;            /* the last sample's angle, deg */
static float    arc_swept = 0;          /* the degrees swept, signed */
static int      arc_sent = -1;          /* the last ARC_MOVE's degree */
static uint32_t last_up_ms = 0;
static uint32_t press_ring[8];          /* the last touch-downs, for the phantom count */
static int      press_head = 0;
static uint32_t phantom_until = 0;      /* deaf until then */
static uint32_t last_lock_note_ms = 0;
/* the storm: the last bursts read (locked or not), for the auto-lock */
static uint32_t storm_ring[8];
static int      storm_head = 0;
static uint32_t last_phantom_ms = 0;
static bool     auto_lock = false;      /* the lock is the wet glass's own, not a setting */
static uint32_t auto_until = 0;         /* the auto-lock's window end */

/* shake */
static vec3_t   lp;
static bool     lp_set = false;
static vec3_t   acc_prev;
static bool     acc_prev_set = false;
static uint32_t sample_ms = 0;
static int      sgn[3] = { 0, 0, 0 };   /* the axis's last excursion side */
static uint32_t rev_ms[3] = { 0, 0, 0 };/* the axis's last reversal */
static bool     burst = false;          /* excursions in progress */
static uint32_t burst_start_ms = 0, burst_last_ms = 0;
static bool     burst_shape = false;    /* two reversals seen inside the window */
static bool     shake_pending = false;  /* one shake read, the double window open */
static uint32_t shake_ms = 0;

/* what the gel shows */
static float    lump_t = 0;             /* the meal's clock, counting down from LUMP_S (0 = no lump) */
static float    water_t = 0;
static float    water_amp = WATER_SQUASH;   /* A DRINK ON A HOT DAY: this settle's depth (WATER_SQUASH, or x HOT_DRINK_SETTLE_K when drunk warm) */
static bool     warm = false;           /* A DRINK ON A HOT DAY: the sky's word is `hot` (main tells it each frame) */
static float    drink_line = 0;         /* THE DRINK LINE: the meniscus's lift, 0..DRINK_LINE_MAX, sinking */
static float    stroke_t = 0;           /* the stroke's clock, counting down from STROKE_LEAN_S */
static uint32_t stroke_ms_last = 0;     /* THE LULL (lull.c): how long the last stroke's finger was down (gesture_stroke_ms) */

/* --- helpers --------------------------------------------------------------- */

static int push(gesture_event_t *out, int cap, int n, gesture_kind_t k, int v, int x, int y)
{
    if (n < cap) { out[n].kind = k; out[n].n = v; out[n].x = x; out[n].y = y; out[n].dx = 0; out[n].dy = 0; }
    return n + 1;
}

/* a drag's event: n = its samples, x,y where it began, dx,dy its travel */
static int push_drag(gesture_event_t *out, int cap, int n, gesture_kind_t k, int dx, int dy)
{
    n = push(out, cap, n, k, drag_n, tx0, ty0);
    if (n <= cap) { out[n - 1].dx = dx; out[n - 1].dy = dy; }
    return n;
}

static int stamps_within(const uint32_t *ring, uint32_t now, uint32_t ms)
{
    int n = 0;
    for (int i = 0; i < 8; i++)
        if (ring[i] && now - ring[i] <= ms) n++;
    return n;
}
static int presses_within(uint32_t now, uint32_t ms) { return stamps_within(press_ring, now, ms); }

/* THE RHYTHM closes: the intervals kept, the event out (n = the taps,
 * dx = ms since the last tap landed) */
static int rhythm_close(gesture_event_t *out, int cap, int n, uint32_t now)
{
    rhythm_open = false;
    last_ngaps = 0;
    for (int i = 1; i < rh_n && last_ngaps < GES_RHYTHM_MAX - 1; i++) last_gaps[last_ngaps++] = rh_stamp[i] - rh_stamp[i - 1];
    n = push(out, cap, n, GES_RHYTHM, rh_n, tx0, ty0);
    if (n <= cap) out[n - 1].dx = (int)(now - rh_stamp[rh_n - 1]);
    rh_n = 0;
    return n;
}
/* ... or is dropped (a hold, a drag, a wet glass): nothing echoed */
static int rhythm_drop(gesture_event_t *out, int cap, int n)
{
    if (rhythm_open) n = push(out, cap, n, GES_TAPS_DROPPED, rh_n, tx0, ty0);
    rhythm_open = false; rh_n = 0;
    return n;
}

/* THE GEL'S FRAME: a screen travel (dx, dy) as his (along the wall,
 * + = the body's right; down, + = toward his base). The body's right
 * on the face is (cos phi, -sin phi), gravity (sin phi, cos phi) --
 * walls.h's phi 0 / pi/2 / pi / -pi/2: each wall a swap of the axes */
static void gel_frame(int dx, int dy, int *along, int *down)
{
    switch (wall & 3) {
    default: *along =  dx; *down =  dy; break;      /* the bottom wall: the screen's frame */
    case 1:  *along = -dy; *down =  dx; break;      /* the right wall: down is screen right, his right is screen up */
    case 2:  *along = -dx; *down = -dy; break;      /* the top wall: down is screen up */
    case 3:  *along =  dy; *down = -dx; break;      /* the left wall: down is screen left, his right is screen down */
    }
}
/* a point's depth on the glass in his frame: 0 at the crown's far edge,
 * GLASS_PX at his base's rim (the thirds are read on this) */
static int depth_of(int x, int y)
{
    int along, down;
    gel_frame(x - GLASS_PX / 2, y - GLASS_PX / 2, &along, &down);
    return down + GLASS_PX / 2;
}

/* THE ARC: a point's angle from 12 o'clock, clockwise, in [0, 360); and whether it lies in the rim's band */
static float rim_angle(int x, int y)
{
    float a = atan2f((float)x - GLASS_PX / 2.0f, -((float)y - GLASS_PX / 2.0f)) * 180.0f / (float)M_PI;
    return a < 0 ? a + 360.0f : a;
}
static bool in_rim_band(int x, int y)
{
    float dx = (float)x - GLASS_PX / 2.0f, dy = (float)y - GLASS_PX / 2.0f;
    float rmin = GLASS_PX / 2.0f - RIM_BAND_PX;
    return dx * dx + dy * dy >= rmin * rmin;
}

/* --- the touch half -------------------------------------------------------- */

static int touch_step(uint32_t now, const touch_t *t, gesture_event_t *out, int cap, int n)
{
    bool down = t->down;
    bool deaf = now < phantom_until;

    if (down && !down_prev) {
        /* a touch-down */
        if (locked && (!last_lock_note_ms || now - last_lock_note_ms >= LOCK_NOTE_MS)) {
            last_lock_note_ms = now;
            n = push(out, cap, n, GES_TOUCH_LOCKED, 0, t->x, t->y);
        }
        /* the phantom count runs under the lock too (silently): the
         * auto-lock needs to know whether the glass is still wet */
        press_ring[press_head] = now ? now : 1;
        press_head = (press_head + 1) % 8;
        int burst_n = presses_within(now, PHANTOM_MS);
        if (burst_n > PHANTOM_N) {
            /* a wet glass: the burst and everything pending, dropped */
            if (!deaf) {
                if (!locked) {
                    if (taps) n = push(out, cap, n, GES_TAPS_DROPPED, taps, tx0, ty0);
                    n = rhythm_drop(out, cap, n);       /* THE RHYTHM: a wet glass drums nothing */
                    n = push(out, cap, n, GES_PHANTOM, burst_n, t->x, t->y);
                }
                last_phantom_ms = now ? now : 1;
                storm_ring[storm_head] = last_phantom_ms;
                storm_head = (storm_head + 1) % 8;
                /* THE WET GLASS LOCKS ITSELF: a storm of bursts */
                if (!locked && stamps_within(storm_ring, now, PHANTOM_STORM_MS) > PHANTOM_STORM_N) {
                    locked = true;
                    auto_lock = true;
                    auto_until = now + AUTO_LOCK_MS;
                    last_lock_note_ms = now;            /* this burst is not also "a touch under the lock" */
                    n = push(out, cap, n, GES_LOCKED, 2, t->x, t->y);
                }
            }
            taps = 0; rhythm_open = false; rh_n = 0; knock_up_ms = 0;
            phantom_until = now + PHANTOM_QUIET_MS;
            deaf = true;
        }
        down_ms = now;
        tx0 = tx1 = t->x;
        ty0 = ty1 = t->y;
        drag_n = 1;
        path = 0;
        peek_sent = false;
        unlock_took = false;
        arc_ok = in_rim_band(t->x, t->y); arc_live = false; arc_swept = 0; arc_sent = -1;
        tk_ext[0] = tk_min[0] = tk_max[0] = t->x; tk_ext[1] = tk_min[1] = tk_max[1] = t->y;   /* THE TICKLE: this touch's count starts over */
        tk_dir[0] = tk_dir[1] = 0; tk_rev = 0; tk_head = 0; tk_live = false; memset(tk_stamp, 0, sizeof tk_stamp);
        arc_ang = rim_angle(t->x, t->y);
    } else if (down) {
        int sx = t->x - tx1, sy = t->y - ty1;
        if (sx || sy) {
            path += sqrtf((float)(sx * sx + sy * sy)); tx1 = t->x; ty1 = t->y;
            {   /* THE TICKLE: the reversals, each axis a peak detector with TICKLE_PX of hysteresis; the box the scrub stayed in */
                int p[2] = { t->x, t->y };
                bool turned = false;
                for (int i = 0; i < 2; i++) {
                    if (p[i] < tk_min[i]) tk_min[i] = p[i];
                    if (p[i] > tk_max[i]) tk_max[i] = p[i];
                    int d = p[i] - tk_ext[i];
                    if (tk_dir[i] == 0) { if (abs(d) >= TICKLE_PX) { tk_dir[i] = d > 0 ? 1 : -1; tk_ext[i] = p[i]; } }
                    else if (d * tk_dir[i] > 0) tk_ext[i] = p[i];                    /* still going its way: the extreme moves on */
                    else if (-d * tk_dir[i] >= TICKLE_PX) { tk_dir[i] = -tk_dir[i]; tk_ext[i] = p[i]; turned = true; }   /* turned back far enough: a reversal */
                }
                if (turned) {
                    tk_rev++;
                    tk_stamp[tk_head] = now ? now : 1;
                    tk_head = (tk_head + 1) % TICKLE_N;
                    if (tk_rev >= TICKLE_N && now - tk_stamp[tk_head] <= TICKLE_MS) tk_live = true;   /* the oldest of the last TICKLE_N inside the second */
                }
            }
            /* THE ARC: the sweep round the rim, while every sample stays in the band */
            if (arc_ok) {
                if (!in_rim_band(t->x, t->y)) { arc_ok = false; arc_live = false; }
                else {
                    float a = rim_angle(t->x, t->y);
                    float d = a - arc_ang;
                    if (d > 180.0f) d -= 360.0f; else if (d < -180.0f) d += 360.0f;
                    arc_swept += d;
                    arc_ang = a;
                    if (!arc_live && fabsf(arc_swept) >= ARC_MIN_DEG) arc_live = true;
                    int deg = (int)(a + 0.5f) % 360;
                    if (arc_live && deg != arc_sent && !locked && !deaf) {
                        arc_sent = deg;
                        n = push(out, cap, n, GES_ARC_MOVE, deg, t->x, t->y);
                        if (n <= cap) out[n - 1].dx = (int)arc_swept;
                    }
                }
            }
        }
        drag_n++;
        /* THE LOCK IS NEVER A TRAP: a plain finger still on the glass past
         * UNLOCK_HOLD_MS under any lock, not drawn anywhere: a hand, not
         * water -- the lock lets go now, while it presses; the lift is inert */
        if (locked && !unlock_took && now - down_ms >= UNLOCK_HOLD_MS) {
            int dx = tx1 - tx0, dy = ty1 - ty0;
            if (dx * dx + dy * dy < STROKE_PX * STROKE_PX) {
                unlock_took = true;
                locked = false;
                auto_lock = false;
                memset(storm_ring, 0, sizeof storm_ring);
                taps = 0;
                n = push(out, cap, n, GES_UNLOCKED, 3, tx0, ty0);
            }
        }
        /* PEEKABOO: a plain finger still on the glass past PEEKABOO_MS,
         * not drawn anywhere: a hand over the bowl, said once while it stays */
        if (!peek_sent && !locked && !deaf && !unlock_took && !tk_live && now - down_ms >= PEEKABOO_MS) {   /* (THE TICKLE: a finger scrubbing is not a hand resting over the bowl) */
            int dx = tx1 - tx0, dy = ty1 - ty0;
            if (dx * dx + dy * dy < STROKE_PX * STROKE_PX) {
                peek_sent = true;
                n = push(out, cap, n, GES_PEEK_DOWN, (int)(now - down_ms), tx0, ty0);
            }
        }
    } else if (down_prev) {
        /* a release */
        uint32_t held = now - down_ms;
        int dx = tx1 - tx0, dy = ty1 - ty0;
        float net = sqrtf((float)(dx * dx + dy * dy));
        bool travelled = net >= (float)STROKE_PX;
        bool flicked = held < SWIPE_MS && net >= (float)SWIPE_PX;   /* THE SWIPE's shape: quick and far enough (judged below with the drags) */
        /* THE RHYTHM: anything but a tap (a hold, a drag; a blip is not a
         * finger and changes nothing) ends the knocks' window and drops a
         * rhythm being tapped */
        if (!(locked || deaf || unlock_took) && held >= TAP_MIN_MS && (arc_live || tk_live || ((travelled || flicked) && held < STROKE_MS) || held > TAP_MAX_MS)) {
            knock_up_ms = 0;
            n = rhythm_drop(out, cap, n);
        }
        if (peek_sent) {                        /* PEEKABOO: the lift is the boo (the long press follows as ever) */
            peek_sent = false;
            if (!locked && !deaf) n = push(out, cap, n, GES_PEEK_UP, (int)held, tx0, ty0);
        }
        if (locked || deaf || unlock_took) {
            /* inert (the unlocking hold's lift too: it was the unlock, not a press) */
        } else if (arc_live) {
            /* THE ARC: the finger went round the rim -- the dial, whatever the time it took */
            n = push(out, cap, n, GES_ARC, (int)(arc_ang + 0.5f) % 360, tx0, ty0);
            if (n <= cap) out[n - 1].dx = (int)arc_swept;
        } else if (held < TAP_MIN_MS) {
            /* a blip: not a finger */
        } else if (tk_live) {
            /* THE TICKLE: one finger scrubbed back and forth, TICKLE_N reversals
             * inside a second -- whatever its net travel and however long it
             * stayed (a scrub was never a stroke, a hold or a tap); n = the
             * reversals, x, y, dx, dy = the box it stayed in (main: on him?) */
            n = push(out, cap, n, GES_TICKLE, tk_rev, tk_min[0], tk_min[1]);
            if (n <= cap) { out[n - 1].dx = tk_max[0] - tk_min[0]; out[n - 1].dy = tk_max[1] - tk_min[1]; }
        } else if ((travelled || flicked) && held < STROKE_MS) {
            /* a drag that lifted in time: the stroke, the feed, the page's
             * drag, the swipe, or nothing -- judged in THE GEL'S FRAME */
            int along, down;
            gel_frame(dx, dy, &along, &down);
            int d0 = depth_of(tx0, ty0), d1 = depth_of(tx1, ty1);
            if (drag_n < STROKE_MIN_N) {
                /* a jump, not a finger: two drops read as one */
            } else if (net < STROKE_STRAIGHT * path) {
                /* a scrub: no one direction, nothing (a QUICK one, three turns inside a second, was THE TICKLE above) */
            } else if (abs(down) > FEED_VERTICAL * abs(along)) {
                /* the feeding axis: down onto him, from the crown's third to his base's third */
                if (down > 0 && d0 < GLASS_PX / 3 && d1 > GLASS_PX * 2 / 3 && travelled)
                    n = push_drag(out, cap, n, GES_FEED, dx, dy);
                /* THE WATCH PAGE's drag: up off him, from his base's third to the crown's third */
                else if (down < 0 && d0 > GLASS_PX * 2 / 3 && d1 < GLASS_PX / 3 && travelled)
                    n = push_drag(out, cap, n, GES_DRAG_UP, dx, dy);
                /* down but not from the crown, up but not from the base, or a short quick one: nothing */
            } else if (flicked) {
                /* THE SWIPE: a flick along his wall (n = the ms it took: what made it a swipe) */
                n = push_drag(out, cap, n, GES_SWIPE, dx, dy);
                if (n <= cap) out[n - 1].n = (int)held;
            } else {
                n = push_drag(out, cap, n, GES_STROKE, dx, dy);
                stroke_ms_last = held;                     /* THE LULL: a slow stroke is one over LULL_STROKE_MS */
            }
        } else if (held <= TAP_MAX_MS) {
            if (rhythm_open && down_ms - rh_stamp[rh_n - 1] <= RHYTHM_GAP_MS) {
                /* THE RHYTHM: a tap landing inside the gap joins it; the sixth closes it at once */
                rh_stamp[rh_n++] = down_ms;
                if (rh_n >= RHYTHM_TAPS_MAX) { rh_full_until = down_ms + RHYTHM_GAP_MS; n = rhythm_close(out, cap, n, now); }
            } else if (rh_full_until && (int32_t)(down_ms - rh_full_until) < 0) {
                /* a seventh tap in the full rhythm's gap: swallowed (not a pet on top of the echo) */
            } else if (knock_up_ms && down_ms - knock_up_ms <= DOUBLE_WINDOW_MS && taps == 0) {
                /* THE RHYTHM OPENS: a fourth tap landing in the knocks' window
                 * (main un-arms the knocks' hold on this) -- the three knocks
                 * are its first three beats */
                if (rhythm_open) n = rhythm_close(out, cap, n, now);   /* (a rhythm left open across the gap by a finger that landed late: closed first) */
                rhythm_open = true; rh_n = 0; rh_full_until = 0;
                for (int i = 0; i < 3; i++) rh_stamp[rh_n++] = tap_down[i];
                rh_stamp[rh_n++] = down_ms;
                knock_up_ms = 0;
                n = push(out, cap, n, GES_RHYTHM_START, rh_n, tx0, ty0);
            } else {
                if (rhythm_open) n = rhythm_close(out, cap, n, now);   /* a tap landing past the gap: the rhythm closes with what it has, this tap is a new first */
                if (taps < 3) tap_down[taps] = down_ms;
                taps++;
                last_up_ms = now;
                if (taps >= 3) {
                    n = push(out, cap, n, GES_TRIPLE_TAP, taps, tx0, ty0);
                    taps = 0;
                    knock_up_ms = now ? now : 1;    /* THE RHYTHM: a fourth tap inside the window from here opens it */
                }
            }
        } else if (travelled || held >= LONG_MS) {
            /* the long press: a drag that never lifted inside STROKE_MS (wherever
             * it went), or a plain hold lifted past the lease's window */
            if (taps) n = push(out, cap, n, GES_TAPS_DROPPED, taps, tx0, ty0);
            taps = 0;
            n = push(out, cap, n, GES_LONG_PRESS, (int)held, tx0, ty0);
        } else if (taps && held >= TAP_HOLD_MS && held <= TAP_HOLD_MAX_MS) {
            /* THE STOPWATCH: a tap (or two), then a press held: the double-tap-and-hold */
            taps = 0;
            n = push(out, cap, n, GES_TAP_HOLD, (int)held, tx0, ty0);
        } else {
            if (taps) n = push(out, cap, n, GES_TAPS_DROPPED, taps, tx0, ty0);
            taps = 0;
            /* THE LEASE: a plain hold of about a second (no tap before it); any other plain hold is nothing */
            if (held >= LEASE_MIN_MS && held <= LEASE_MAX_MS)
                n = push(out, cap, n, GES_LEASE_HOLD, (int)held, tx0, ty0);
            else
                n = push(out, cap, n, GES_HOLD, (int)held, tx0, ty0);
        }
    }
    /* the window closes on what is pending: one tap is the pet, two the drink */
    if (taps && !down && now - last_up_ms > DOUBLE_WINDOW_MS) {
        n = push(out, cap, n, taps == 1 ? GES_TAP : GES_DOUBLE_TAP, taps, tx0, ty0);
        taps = 0;
    }
    /* THE RHYTHM: the gap passing with no finger down closes it (four to six taps: the echo is play.cpp's) */
    if (rhythm_open && !down && now - rh_stamp[rh_n - 1] > RHYTHM_GAP_MS) n = rhythm_close(out, cap, n, now);
    if (deaf && now >= phantom_until) phantom_until = 0;
    down_prev = down;
    return n;
}

/* --- the shake half -------------------------------------------------------- */

static int shake_step(uint32_t now, const vec3_t *a, bool screen_on, gesture_event_t *out, int cap, int n)
{
    if (a) {
        bool same = acc_prev_set && a->x == acc_prev.x && a->y == acc_prev.y && a->z == acc_prev.z;
        if (!same) {
            acc_prev = *a;
            acc_prev_set = true;
            if (!lp_set) { lp = *a; lp_set = true; sample_ms = now; }
            else {
                uint32_t ds = now - sample_ms;
                if (ds < 1) ds = 1;
                sample_ms = now;
                float k = (float)ds / (float)(SHAKE_TAU_MS + ds);
                lp.x += (a->x - lp.x) * k;
                lp.y += (a->y - lp.y) * k;
                lp.z += (a->z - lp.z) * k;
                float hp[3] = { a->x - lp.x, a->y - lp.y, a->z - lp.z };
                bool over = false;
                for (int i = 0; i < 3; i++) {
                    int s = hp[i] > SHAKE_G ? 1 : hp[i] < -SHAKE_G ? -1 : 0;
                    if (!s) continue;
                    over = true;
                    if (sgn[i] && s != sgn[i]) {
                        /* a reversal on this axis */
                        if (rev_ms[i] && now - rev_ms[i] <= SHAKE_WINDOW_MS) burst_shape = true;
                        rev_ms[i] = now ? now : 1;
                    }
                    sgn[i] = s;
                }
                if (over) {
                    if (!burst) { burst = true; burst_start_ms = now; burst_shape = false; }
                    burst_last_ms = now;
                }
            }
        }
    }
    /* the burst ends in quiet: then it is read, once */
    if (burst && now - burst_last_ms >= SHAKE_GAP_MS) {
        bool is_shake = burst_shape && (burst_last_ms - burst_start_ms) <= SHAKE_MAX_MS;
        burst = false;
        burst_shape = false;
        for (int i = 0; i < 3; i++) { sgn[i] = 0; rev_ms[i] = 0; }
        if (is_shake) {
            if (shake_pending && now - shake_ms <= DOUBLE_SHAKE_MS) {
                shake_pending = false;
                n = push(out, cap, n, GES_DOUBLE_SHAKE, 2, 0, 0);
                if (screen_on) {
                    locked = !locked;
                    auto_lock = false;              /* the hand's word: the wet glass's own lock ends here, and the storm's count starts over */
                    memset(storm_ring, 0, sizeof storm_ring);
                    n = push(out, cap, n, locked ? GES_LOCKED : GES_UNLOCKED, 1, 0, 0);
                }
            } else {
                shake_pending = true;
                shake_ms = now;
            }
        }
    }
    if (shake_pending && now - shake_ms > DOUBLE_SHAKE_MS) {
        shake_pending = false;
        n = push(out, cap, n, GES_SHAKE, 1, 0, 0);
    }
    return n;
}

/* --- the door ------------------------------------------------------------- */

void gesture_begin(uint32_t now_ms)
{
    locked = false;                         /* THE LOCK IS NEVER A TRAP: a boot is always unlocked */
    lock_pending = false;
    last_ms = now_ms;
    wall = 0;                               /* THE GEL'S FRAME: the bottom wall until main says (the screen's own frame) */
    down_prev = false; taps = 0; drag_n = 0; path = 0; peek_sent = false; unlock_took = false;
    memset(tap_down, 0, sizeof tap_down); knock_up_ms = 0; rhythm_open = false; rh_n = 0; rh_full_until = 0; last_ngaps = 0;
    arc_ok = arc_live = false; arc_swept = 0; arc_sent = -1;
    tk_dir[0] = tk_dir[1] = 0; tk_rev = 0; tk_head = 0; tk_live = false; memset(tk_stamp, 0, sizeof tk_stamp);   /* THE TICKLE */
    memset(press_ring, 0, sizeof press_ring); press_head = 0;
    phantom_until = 0; last_lock_note_ms = 0;
    memset(storm_ring, 0, sizeof storm_ring); storm_head = 0;
    last_phantom_ms = 0; auto_lock = false; auto_until = 0;
    lp_set = false; acc_prev_set = false;
    for (int i = 0; i < 3; i++) { sgn[i] = 0; rev_ms[i] = 0; }
    burst = false; burst_shape = false; shake_pending = false;
    lump_t = 0; water_t = 0; stroke_t = 0; drink_line = 0;
}

int gesture_tick(uint32_t now_ms, const vec3_t *acc, const touch_t *t, bool screen_on,
                 gesture_event_t *out, int cap)
{
    int n = 0;
    float dt = (now_ms - last_ms) * 1e-3f;
    if (dt < 0 || dt > 0.5f) dt = 0.5f;
    last_ms = now_ms;

    if (lock_pending) {
        lock_pending = false;
        if (lock_pending_on != locked) {
            locked = lock_pending_on;
            auto_lock = false;
            memset(storm_ring, 0, sizeof storm_ring);
            taps = 0;
            n = push(out, cap, n, locked ? GES_LOCKED : GES_UNLOCKED, 0, 0, 0);
        } else if (lock_pending_on && auto_lock) {
            /* `lock` typed while the wet glass holds it: now the hand's
             * lock (it no longer dries off by itself; the event surfaces
             * so it is logged as the hand's) -- still RAM only */
            auto_lock = false;
            n = push(out, cap, n, GES_LOCKED, 0, 0, 0);
        }
    }
    if (t) n = touch_step(now_ms, t, out, cap, n);
    n = shake_step(now_ms, acc, screen_on, out, cap, n);
    /* THE WET GLASS LOCKS ITSELF: the window passed and the glass dry
     * (no phantom read in AUTO_QUIET_MS): it lets go */
    if (auto_lock && locked && (int32_t)(now_ms - auto_until) >= 0 && now_ms - last_phantom_ms >= AUTO_QUIET_MS) {
        auto_lock = false;
        locked = false;
        n = push(out, cap, n, GES_UNLOCKED, 2, 0, 0);
    }

    /* what the gel shows, decaying */
    for (int i = 0; i < n && i < cap; i++) {
        if (out[i].kind == GES_FEED) lump_t = LUMP_S;      /* the top-up: the clock reset, never summed */
        if (out[i].kind == GES_DOUBLE_TAP) {
            water_t = WATER_S;
            water_amp = WATER_SQUASH * (warm ? HOT_DRINK_SETTLE_K : 1.0f);   /* A DRINK ON A HOT DAY: the settle deeper when drunk warm */
            drink_line += DRINK_LINE * (warm ? HOT_DRINK_LINE_K : 1.0f);     /* THE DRINK LINE: a hair higher (the rain's own on a hot day), capped */
            if (drink_line > DRINK_LINE_MAX) drink_line = DRINK_LINE_MAX;
        }
        if (out[i].kind == GES_STROKE) stroke_t = STROKE_LEAN_S;
    }
    if (lump_t > 0) { lump_t -= dt; if (lump_t < 0) lump_t = 0; }
    if (water_t > 0) { water_t -= dt; if (water_t < 0) water_t = 0; }
    if (drink_line > 0) { drink_line -= DRINK_LINE / DRINK_SINK_S * dt; if (drink_line < DRINK_LINE_GONE) drink_line = 0; }   /* a hair sinks over two hours; the last of it is nothing */
    if (stroke_t > 0) { stroke_t -= dt; if (stroke_t < 0) stroke_t = 0; }
    return n;
}

void gesture_set_wall(int w) { wall = w & 3; }
int  gesture_wall(void) { return wall; }
bool gesture_locked(void) { return locked; }
bool gesture_auto_locked(void) { return locked && auto_lock; }
void gesture_set_lock(bool on) { lock_pending = true; lock_pending_on = on; }
bool gesture_touch_live(void) { return !locked && last_ms >= phantom_until; }
int   gesture_rhythm_gaps(uint32_t *gaps, int cap)
{
    int n = last_ngaps < cap ? last_ngaps : cap;
    for (int i = 0; i < n; i++) gaps[i] = last_gaps[i];
    return n;
}
void  gesture_unshow(void) { lump_t = 0; water_t = 0; stroke_t = 0; drink_line = 0; }
void  gesture_unlump(void) { lump_t = 0; }   /* THE WISP EATEN: the feed was the wisp's, not a meal */

float gesture_lump(void)
{
    if (lump_t <= 0) return 0;
    float left = lump_t / LUMP_S;       /* 1 at the feed -> 0 when dissolved */
    return left * left;                 /* the ease-out: the bulge drops quickly, the last of it lingers */
}

float gesture_drink_line(void) { return drink_line; }
void  gesture_set_warm(bool w) { warm = w; }
bool  gesture_warm(void) { return warm; }

float gesture_squash(void)
{
    float s = drink_line;               /* THE DRINK LINE: the meniscus a hair up per drink, sinking */
    if (water_t > 0) s += water_amp * sinf((float)M_PI * water_t / WATER_S);
    float since = STROKE_LEAN_S - stroke_t;
    if (stroke_t > 0 && since < STROKE_SETTLE_S) s += STROKE_SQUASH * sinf((float)M_PI * since / STROKE_SETTLE_S);
    return s;
}

uint32_t gesture_stroke_ms(void) { return stroke_ms_last; }

float gesture_stroke(void)
{
    if (stroke_t <= 0) return 0;
    float since = STROKE_LEAN_S - stroke_t;
    if (since < STROKE_LEAN_IN_S) return since / STROKE_LEAN_IN_S;
    return stroke_t / (STROKE_LEAN_S - STROKE_LEAN_IN_S);
}

const char *gesture_name(gesture_kind_t k)
{
    switch (k) {
    case GES_TAP:          return "tap";
    case GES_DOUBLE_TAP:   return "double tap";
    case GES_TRIPLE_TAP:   return "three knocks";
    case GES_STROKE:       return "stroke";
    case GES_FEED:         return "feed";
    case GES_DRAG_UP:      return "drag up";
    case GES_SWIPE:        return "swipe";
    case GES_HOLD:         return "hold";
    case GES_LEASE_HOLD:   return "the lease's hold";
    case GES_TAP_HOLD:     return "tap and hold";
    case GES_LONG_PRESS:   return "long press";
    case GES_SHAKE:        return "shake";
    case GES_DOUBLE_SHAKE: return "double shake";
    case GES_LOCKED:       return "locked";
    case GES_UNLOCKED:     return "unlocked";
    case GES_PHANTOM:      return "phantom burst";
    case GES_TOUCH_LOCKED: return "touch under the lock";
    case GES_TAPS_DROPPED: return "taps dropped";
    case GES_PEEK_DOWN:    return "peekaboo (a hand over the bowl)";
    case GES_PEEK_UP:      return "peekaboo (boo)";
    case GES_ARC_MOVE:     return "arc (the finger round the rim)";
    case GES_ARC:          return "arc";
    case GES_RHYTHM_START: return "the rhythm opens (a fourth tap)";
    case GES_RHYTHM:       return "the rhythm";
    case GES_TICKLE:       return "tickle";
    default:               return "none";
    }
}
