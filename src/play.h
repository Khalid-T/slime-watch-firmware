/* PLAY: THE BEAD AND THE SPIN (design/watch-ideas.md F47 + F53,
 * 2026-09-23), THE TOSS AND THE RHYTHM (F48 + F51, 2026-10-09, THE
 * OVERHAUL WAVE). Four calm games a gel can play in its bowl, after the
 * peekaboo (F49, the first play): never required, never unasked, never
 * scored, never counted on the glass.
 *
 *   THE BEAD (F47)  the watch held FLAT and STILL for BEAD_STILL_S, then
 *       a tap on the glass beside him (not on him: that is the pet, or
 *       the far tap): a 3x3 bead in the SHADE colour appears where the
 *       finger landed and ROLLS ROUND THE BOWL with the tilt -- gravity
 *       is already the floor, the bead is one more thing in it. The gel
 *       watches it (the gaze); when it rolls into him it is EATEN (a
 *       small lump, the happy eyes). While the game is open and no bead
 *       is on the glass, another tap beside him places another (no
 *       stillness owed again: he is still playing). Flat again for
 *       BEAD_OVER_S with no bead and it is over.
 *   THE SPIN (F53)  a fast FLAT spin of the wrist -- the gyro's yaw
 *       (about the glass's normal) over SPIN_DPS, a turn a second, for
 *       SPIN_SPAN_MS (a flick is shorter), the turn's axis near the
 *       normal (SPIN_AXIS_K) -- leaves him dizzy for SPIN_DIZZY_S: the
 *       squint, a slow wobble on both springs, the gaze lagging the
 *       turn. A gel spun in its bowl. The body's reaction is main.cpp's;
 *       here only the reading.
 *
 *   CATCH THE DROP (F50, 2026-10-08)  inside the bead's game, every
 *       second bead eaten brings a DROP: CATCH_DELAY_S after the bite a
 *       5x7 drop in the shine leaves the air door (the rim opposite the
 *       floor) and FALLS straight down gravity's in-plane direction --
 *       re-read each frame: it does not roll, he tilts to steer where it
 *       lands. Into him (the painted box) it is CAUGHT: the meal's lump,
 *       the happy eyes. Onto the glass it is MISSED: a wet mark in the
 *       shade at the landing that fades over a minute.
 *   THE COURSE (F52, 2026-10-08)  in an open bead game with no bead on
 *       the glass, a wall change (he fell) opens the course: the other
 *       three walls become gates -- 2 px marks in the shade on the rim,
 *       in order round the bowl the way the fall went; landing on each
 *       in turn lights it in the shine; all three = the content eyes,
 *       the gates gone. No score, no clock; COURSE_IDLE_S at one wall
 *       and the course is left in silence.
 *
 * THE GATES, the same for all (peekaboo's, made a table): never in the
 * dark house, never in the quiet hours, never on the shelf, never on
 * the empty glass, never with a page or a word bubble up; each game
 * ONCE PER PLAY_GAP_MS (10 min, peekaboo's own gap). A game refused by
 * a gate does NOTHING visible (a log line, no more). A gate closing
 * while the bead's game is open ends it at once, the bead gone.
 *
 *    THE TOSS (F48)  a FLICK along the glass's plane -- the in-plane jerk
 *       over TOSS_G (under the shake's 1.0 g) for TOSS_SPAN_MS, mostly
 *       in the plane (a knock on the glass is along the normal), with
 *       NO REVERSAL inside TOSS_REVERSAL_MS before or after it (a shake
 *       goes out-back-out; a toss goes once) -- lofts the gel
 *       TOSS_LIFT_PX off the rim along the wall's normal; it falls back
 *       under the fall's own FALL_G and lands with THE SMALL SPLAT (half
 *       the fall's) and the squint. He threw it; it did not hop. Never
 *       the wall change: the fall is the fall. EVERY toss plays (as the
 *       shove does); the gap is the sense's alone.
 *   THE RHYTHM (F51)  four to six taps in a rhythm (gesture.cpp: a
 *       fourth tap in the knocks' window un-arms them and opens the
 *       rhythm; the stamps, the cap, the window's close are the
 *       detector's) -- the gel ECHOES it RHYTHM_DELAY_MS after the last
 *       tap as squash pulses at the same intervals, the eyes happy at
 *       the end. A gel taught a beat. The schedule is pure here: the
 *       gaps in, the pulses out, one squash term a frame. EVERY rhythm
 *       is echoed (he is teaching it; a mute second lesson would read
 *       as deaf); the gap is the sense's alone.
 *
 * ONE PURE ARBITER: the IMU's and the touch's state + the gates come in
 * as parameters, the game that starts comes out; the bead's roll is a
 * few lines of physics in the same file. No Arduino, no clock of its
 * own, no drawing: tools/play_test.cpp pins the whole table on the
 * laptop (the still-flat window, a tap too early, a tilt that breaks
 * the stillness, the spin's threshold and its speed line, the 10-min
 * repeat, each gate). main.cpp feeds it, draws the bead through its own
 * fb_block (a mark in the shade, as the lock's drop is), and plays the
 * reactions through the existing springs and eyes -- no new render
 * path; the preview never plays. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- the items' own numbers, named -------------------------------------- */

/* F47 THE BEAD */
#define BEAD_STILL_S     3.0f     /* "held flat and still for 3 s": flat (in-plane g under walls.h's FLAT_G) and still (the jerk under PLAY_STILL_G) this long arms the tap */
#define BEAD_OVER_S      10.0f    /* "flat again for 10 s with no bead and it is over" */
#define BEAD_SIZE        3        /* "a 3x3 bead in the shade" (cells; main.cpp draws each cell BEAD_BLOCK_PX square) */
#define BEAD_BLOCK_PX    2        /* the cell's size on the glass: 6 px across -- the wisp's mote is 3 px with a halo; 3 px alone is dust at arm's length (his eye's: 1 for the item's literal 3 px) */
#define BEAD_G_PX        600.0f   /* face px/s^2 per g of in-plane gravity: the roll (the gel's own fall is FALL_G 900: a bead rolls, it does not drop) */
#define BEAD_DAMP        1.2f     /* 1/s: the rolling loss -- on a level glass it comes to rest in about a second */
#define BEAD_RIM_R       114.0f   /* the bead's centre never past this from the glass's centre: the rim at 120, the bead's half-size and a block inside it */
#define BEAD_RIM_KEEP    0.85f    /* the speed along the rim kept when it meets the bowl's side (the part into it is lost: it rolls, it does not bounce) */
#define BEAD_LUMP        0.30f    /* "eaten (a small lump)": the belly at the bite -- the meal's is 1.0 */
#define BEAD_LUMP_S      30.0f    /* the small lump sinks away over this (the meal's 60) with the meal's own ease-out */
#define BEAD_HAPPY_S     1.0f     /* the happy eyes at the bite (main.cpp) */

/* F53 THE SPIN */
#define SPIN_DPS         360.0f   /* "the gyro's yaw over a turn a second" */
#define SPIN_SPAN_MS     120u     /* the yaw over SPIN_DPS across this span before it is a spin (a flick of the wrist peaks for a few ms; a third of a turn at the threshold is 120 ms) */
#define SPIN_AXIS_K      0.866f   /* cos 30 deg: a FLAT spin -- the yaw at least this much of the whole rate (the turn's axis within 30 deg of the glass's normal); a swing of the arm turns about another axis */
#define SPIN_DIZZY_S     2.0f     /* "dizzy for two seconds" */
#define SPIN_STILL_DPS   180.0f   /* the yaw back under this (half the threshold) before another spin can be read: one spin, one dizzy spell */

/* F50 CATCH THE DROP (2026-10-08): "now and then in play (never unasked --
 * after a bead was eaten) a drop falls from the air door straight down
 * gravity; tilt so the gel is under it: caught is the meal's lump,
 * missed a small wet mark on the glass that fades in a minute. The feed
 * from above, made into a catch." */
#define CATCH_EVERY_BITES 2       /* every SECOND bead eaten in a game brings a drop (the first bite is the bead's own); no dice in a pure module -- "now and then" as a rhythm his bowl keeps and he can learn */
#define CATCH_DELAY_S     2.0f    /* the drop leaves the air door this long after the bite: the bead swallowed, then the sky's answer */
#define DROP_G_PX         400.0f  /* face px/s^2 per g of in-plane gravity: slower than the bead's 600 (a drop, not a stone) -- a full tilt crosses the glass in about a second */
#define DROP_SLIDE_G      0.25f   /* the fall's LEAST pull, in g: a level glass still drips (under it the drop would hang at the door for ever -- a mark he cannot play is a nag) */
#define DROP_DIR_G        0.05f   /* in-plane g under this has no direction worth reading: the drop falls toward the wall he sits on (level, down is the floor) */
#define DROP_DOOR_R       116.0f  /* the air door: the rim opposite the floor (wisp.cpp's DOOR_R) */
#define DROP_RIM_R        114.0f  /* the landing: the drop's centre at the bowl's side (the bead's own rim), heading out, after it came IN past it */
#define DROP_OUT_PX       2.0f    /* a drop that never came in (gravity toward the door: the glass turned over) and is this far past the door fell out of it: missed, the mark at the door */
#define DROP_PX           1       /* the drop's cell: the lock's 5x7 drop glyph at 1 px (5x7 px, the bead's size) in the SHINE -- the drop's own look, a third of the lock's */
#define CATCH_LUMP        1.0f    /* caught = the meal's lump (gesture.cpp's feed: 1.0; the bead's is 0.30) */
#define CATCH_LUMP_S      60.0f   /* sinking over the meal's own minute (gesture.cpp's LUMP_S), the meal's ease-out */
#define CATCH_HAPPY_S     1.5f    /* the happy eyes at the catch (main.cpp): longer than the bead's 1 s -- a whole meal */
#define MARK_PX           3       /* the wet mark: a 3 px mark in the shade where the drop landed (THE PLACE's marks-in-the-shade) */
#define MARK_S            60.0f   /* "fades in a minute": the mark's shade dims to the glass over this */

/* F52 THE COURSE (2026-10-08): "three dim gates (2 px marks in the shade)
 * at three places on the rim; tilt the gel through them in order and
 * each lights in the shine; through all three, the content eyes and the
 * gates gone. No score, no clock." He sits on FOUR WALLS only, so the
 * gates are three of the four walls -- the three he is not on -- and
 * "through" is landing there (the wall change is the fall). The entry:
 * in an open bead game with NO bead on the glass, a wall change (the
 * bowl tilted past the diagonal: he fell) starts it -- the one free
 * gesture inside play, no new touch, nothing of the GESTURES table taken. */
#define COURSE_GATES      3       /* the other three walls, in the order round the bowl the first fall went */
#define COURSE_IDLE_S     30.0f   /* no gate passed this long: the course is left, the gates gone (no clock on the glass: the leaving is silent) */
#define GATE_PX           2       /* "2 px marks": each gate a 2x2 mark on the rim at its wall, the shade until passed, the shine after */
#define GATE_R            116.0f  /* the gate's place: the rim at the wall (the air door's radius -- the gel's base centre sits at GEL_FLOOR_DROP 106, so a lit gate shows a hair past his base) */
#define COURSE_CONTENT_S  1.5f    /* the content eyes at the end (main.cpp) */


/* F48 THE TOSS (2026-10-09) */
#define TOSS_G           0.6f     /* the in-plane jerk over this is a flick: under gesture.cpp's SHAKE_G 1.0 (a shake is harder and reverses), over a walk's high-passed peaks (0.2-0.4 g; the day log's SHAKEN is 0.6 too) */
#define TOSS_PLANE_K     0.8f     /* the in-plane part at least this much of the whole jerk (within ~37 deg of the plane): a knock on the glass or the table is along the normal and is a bump, never a toss */
#define TOSS_SPAN_MS     50u      /* the flick over TOSS_G this long (two frames at 40 fps): a knuckle's bump rings for ~10 ms; the wake's shake asks 150 ms of 0.18 g */
#define TOSS_REVERSAL_MS 250u     /* no opposite excursion over TOSS_G this long before OR after the flick: a shake's halves come every 100-250 ms (SHAKE_WINDOW_MS 500 for two); so the toss is read 250 ms after the flick ends (the shake's own SHAKE_GAP_MS wait) */
#define TOSS_LIFT_PX     8.0f     /* "a few px off the rim": the loft along the wall's normal (the house's sink is 95, the peekaboo's hide 95; 8 is a hop's height without the hop) */
#define TOSS_UP_S        0.12f    /* the loft takes this (the fall's IMPACT_S: the same quickness, the other way); the fall back is under FALL_G 900 (8 px: 0.13 s) */
#define TOSS_SPLAT       0.24f    /* the small splat: half the fall's LAND_SQUASH 0.48 (play.cpp checks the half at compile time), easing off over TOSS_LAND_S */
#define TOSS_LAND_S      0.38f    /* the splat's ease, the fall's own LAND_S */
#define TOSS_SQUINT_S    1.0f     /* "the squint": held this long from the landing (the fall's is 0.45; a thrown thing takes a moment longer) */

/* F51 THE RHYTHM (2026-10-09) */
#define RHYTHM_MAX       6        /* "four to six taps": the cap (gesture.cpp's RHYTHM_TAPS_MAX is the same 6; the detector closes the rhythm at the sixth) */
#define RHYTHM_DELAY_MS  1000u    /* "a second later": the first pulse this long after the LAST TAP (the detector says how long ago that was at the close) */
#define RHYTHM_PULSE     0.10f    /* each pulse of squash: the stroke's settle (0.10), more than the drink's 0.08, under the pet's 0.18 -- a beat, not a press */
#define RHYTHM_PULSE_MS  120u     /* each pulse's length, a half sine: five frames at 40 fps; the beats are 250 ms apart at the fastest (the phantom's rule: five touches in a second is water) so the pulses never overlap */
#define RHYTHM_HAPPY_S   1.0f     /* the happy eyes at the end of the echo (the bead's own second) */

/* all */
#define PLAY_GAP_MS      600000u  /* once per 10 min, each game (peekaboo's PEEK_PLAY_GAP_MS): the bead's GAME and the spin's spell; for the toss and the rhythm the SENSE only (every one plays) */
#define PLAY_STILL_G     0.05f    /* the jerk under this is still (main's MOTION_G) */

/* --- the wire ----------------------------------------------------------- */

enum { PLAY_NONE = 0, PLAY_BEAD = 1, PLAY_SPIN = 2, PLAY_COURSE = 3, PLAY_TOSS = 4, PLAY_RHYTHM = 5 };

/* what happened this frame (play_events(), a mask, cleared each tick) */
enum {
    PLAY_EV_DROP        = 1 << 0,   /* a drop left the air door */
    PLAY_EV_CAUGHT      = 1 << 1,   /* the drop fell into him: the meal's lump */
    PLAY_EV_MISSED      = 1 << 2,   /* the drop met the glass: a wet mark */
    PLAY_EV_GATE        = 1 << 3,   /* a gate of the course lit */
    PLAY_EV_COURSE_DONE = 1 << 4,   /* all three: the content eyes, the gates gone */
    PLAY_EV_COURSE_GONE = 1 << 5,   /* the course left (idle, or a gate closed) */
};

/* why the last would-be start did not: the gate that refused it (or
 * PLAY_OK: it started) */
enum {
    PLAY_OK = 0,
    PLAY_REFUSED_DARK,        /* the dark house */
    PLAY_REFUSED_QUIET,       /* the quiet hours */
    PLAY_REFUSED_SHELF,       /* on the shelf (a still run judged not worn) */
    PLAY_REFUSED_EMPTY,       /* the empty glass: nobody to play with */
    PLAY_REFUSED_PAGE,        /* THE WATCH PAGE is up */
    PLAY_REFUSED_WORD,        /* the word bubble is up */
    PLAY_REFUSED_GAP,         /* inside PLAY_GAP_MS of the last game of its kind */
    PLAY_REFUSED_NOT_ARMED,   /* the bead: the tap came before the 3 s of stillness */
    PLAY_REFUSED_ON_GEL,      /* the bead: the tap landed on him (the pet's) */
    PLAY_REFUSED_ASLEEP,      /* the gel is asleep (rest's night): nobody tosses a sleeper, nobody drums on his glass -- every game (2026-10-09) */
    PLAY_REFUSED_FALLING,     /* the toss only: a fall is in progress -- the fall is the fall, never the toss */
};

typedef struct {
    bool lit;        /* not the dark house (main: state != DARK) */
    bool quiet;      /* the quiet hours (rest_night_now) */
    bool shelf;      /* rest_on_shelf() */
    bool holding;    /* house_holding(): a gel to play with */
    bool page;       /* page_up() */
    bool word;       /* word_showing() */
    bool asleep;     /* rest->asleep (2026-10-09: a seventh gate, every game) */
    bool falling;    /* main's fall_phase != FALL_NONE (the toss only) */
} play_gates_t;

typedef struct {
    uint32_t now_ms;
    float    dt;
    float    gx, gy;     /* in-plane gravity in the glass's frame (main's DOWN_X / DOWN_Y of the low-passed sample: +x toward the glass's right, +y toward its bottom), in g */
    float    jerk_g;     /* the fast part of the sample (main's jerk): the stillness, and the toss's whole */
    float    jx_g, jy_g; /* THE TOSS: the jerk's in-plane parts in the glass's frame (main's DOWN_X / DOWN_Y of the sample less the low-passed one, as the slosh's `side`), in g; 0 = not known */
    float    yaw_dps;    /* the gyro about the glass's normal, signed in the glass's frame (+ = clockwise as seen); 0 with no gyro */
    float    rate_dps;   /* the gyro's whole magnitude */
    play_gates_t gates;
    /* F50 / F52 (at the end: the old initialisers hold) */
    int      wall;       /* THE WALL he sits on (GEL_WALL_*: 0 bottom, 1 right, 2 top, 3 left) -- the air door and the gates are read off it */
    bool     on_wall;    /* landed: not in the air (main: fall_phase none, impact or land -- the wall is his); a wall change is read the frame this is true again */
    int      box_x, box_y, box_w, box_h;   /* the gel's painted box this frame (gel_face_box; w 0 = none): the drop inside it is CAUGHT */
} play_in_t;

void play_begin(void);
/* one call a frame. Returns PLAY_SPIN on the frame a spin is read and
 * allowed (once per spin), PLAY_COURSE on the frame the course starts,
 * PLAY_TOSS on the frame a toss is read and allowed (every toss), else
 * PLAY_NONE; steps the bead's roll, the stillness, the game's clocks, the
 * dizzy spell, the drop's fall, the mark's fade, the course's gates and
 * its idle, the toss's flight and splat, the rhythm's pulses. */
int  play_tick(const play_in_t *in);
/* PLAY_EV_* of the last tick (a mask; cleared at each tick's start) */
uint32_t play_events(void);
/* a single tap on the glass (after the word's, the clockwork's and the
 * house's own takes; before the far tap and the pet). on_gel = it landed
 * on him (main's FAR_TAP_PX test). Returns 0 = not the bead's (the tap
 * goes on to be the far tap or the pet), 1 = a bead placed in the game
 * already open, 2 = THE GAME STARTED with this bead (the sense goes up
 * once, here). Refusals leave play_refused() saying why. */
int  play_tap(uint32_t now_ms, int x, int y, bool on_gel);
/* the bead rolled into him (main reads the gel's painted box): eaten --
 * the lump starts sinking, the bead is gone, the game stays open */
void play_bead_eat(uint32_t now_ms);

bool play_bead_on(float *x, float *y);    /* a bead on the glass, and where (face px) */
bool play_bead_armed(void);               /* flat and still for BEAD_STILL_S: the next tap beside him is the bead's */
bool play_bead_open(void);                /* the game is on (a bead placed, not yet over) */
float play_still_s(void);                 /* how long flat and still so far */
float play_lump(void);                    /* play's lump, 0..1 of the belly: the eaten bead's small one (BEAD_LUMP at the bite) or the caught drop's meal (CATCH_LUMP), the larger, sinking */
/* F50 CATCH THE DROP */
bool  play_drop_on(float *x, float *y);   /* a drop falling, and where (face px) */
float play_drop_due_s(void);              /* a drop owed: seconds until it leaves the air door; 0 = none pending */
bool  play_mark_on(float *x, float *y, float *k);   /* the wet mark on the glass, where, and how much of it is left (1 at the landing -> 0 at MARK_S) */
void  play_catch_tally(uint32_t *drops, uint32_t *caught, uint32_t *missed);   /* since boot: the log and the test, never the glass */
/* F52 THE COURSE */
bool  play_course_on(void);
int   play_course_gates(int walls[COURSE_GATES], bool lit[COURSE_GATES]);   /* the three gate walls in order and which are lit; returns how many are lit (0 with no course) */
int   play_course_next(void);             /* the next gate's wall, -1 = no course */
float play_course_idle_s(void);           /* since the last gate (or the start) */
/* `play` over serial: the whole state in one line */
float play_dizzy(void);                   /* the spin's dizzy spell, 1 -> 0 over SPIN_DIZZY_S; 0 = none */
int   play_dizzy_sign(void);              /* the spin's way (+1 / -1), for the gaze that lags it */
float play_spin_dps(void);                /* the last spin's peak yaw (the speed line) */
/* THE TOSS: what the body shows (main adds them to the compose's lift
 * and the frame's squash); the direction is the flick's, a unit vector
 * in the glass's frame (+x right, +y down) -- the mass lags it, so a
 * skid goes AGAINST it; the landing and the sense are takes (true once) */
float play_toss_lift(void);               /* px off the rim along the wall's normal, 0 when not in the air */
float play_toss_squash(void);             /* the small splat's envelope, TOSS_SPLAT easing off over TOSS_LAND_S; 0 otherwise */
bool  play_toss_dir(float *x, float *y);  /* the last toss's direction; false = never tossed */
bool  play_toss_landed(void);             /* the frame it landed (once): the kick and the squint are main's */
bool  play_toss_sense_due(void);          /* this toss's `wrist:play` "tossed" is due (once per PLAY_GAP_MS; once) */
/* THE RHYTHM: the detector's gaps (ms between the taps, n_gaps of
 * them = taps - 1, at most RHYTHM_MAX - 1; more are cut) and how long
 * ago the last tap was at the close; the gates apply (holding, lit,
 * the quiet hours, the shelf, the page, a word, asleep); returns the
 * pulses scheduled (taps) or 0 refused (play_refused says why) */
int   play_rhythm(uint32_t now_ms, const uint32_t *gaps_ms, int n_gaps, uint32_t since_last_ms);
float play_rhythm_squash(void);           /* this frame's pulse, RHYTHM_PULSE on a half sine over RHYTHM_PULSE_MS; 0 between and after */
int   play_rhythm_pending(void);          /* pulses still to come (0 = no echo on) */
bool  play_rhythm_done(void);             /* the frame the last pulse ended (once): the happy eyes are main's */
bool  play_rhythm_sense_due(void);        /* this rhythm's `wrist:play` "rhythm" is due (once per PLAY_GAP_MS; once) */
bool  play_busy(void);                    /* a bead on the glass, a dizzy spell, a drop falling, a course open, a toss in the air or splatting, an echo pending: idle life waits */
int   play_refused(void);                 /* PLAY_* of the last would-be start */
const char *play_refused_name(int r);
uint32_t play_count(int game);            /* games played since boot, by kind (PLAY_BEAD / PLAY_SPIN / PLAY_COURSE / PLAY_TOSS / PLAY_RHYTHM) -- for the log and the test, never the glass */
void  play_status(char *out, int cap);    /* `play` over serial: the tallies, what is on now, the last refusal */
/* the log line, one at a time (main prints it and calls play_log_taken) */
bool  play_log_new(void);
const char *play_log(void);
void  play_log_taken(void);

#ifdef __cplusplus
}
#endif
