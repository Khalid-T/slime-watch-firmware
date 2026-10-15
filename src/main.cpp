/* The slime watch, step 1 (Khalid's build order, 2026-09-06): "Board on
 * USB: gel in the round window, gravity/tilt/slosh/settle, sleeps when
 * still and dark, wakes on raise, touch = pet. No WiFi, no lease, no
 * brain."
 *
 * What lives here is the creature's OFFLINE half -- everything that must
 * work on a bus with no network: the gel (a port of render.py's dome),
 * the floor (THE FOUR WALLS -- Khalid 2026-09-10, reopening the
 * continuous bowl of 09-09: the gel sits at 6, 3, 12 or 9 o'clock,
 * always axis-aligned; the tilt between walls is worn as a lean, a
 * slosh and a slide; a pull past the diagonal changes the wall, and
 * every change is THE FALL across the face -- walls.h has the rule),
 * the slosh (the desk's two springs, k 220 / c 9), the resting breath,
 * the eyes that come when engaged and melt away, the pet, the dark
 * house it naps in and the wake on a raise.
 * See README.md "BEHAVIOUR SPLIT" for what only the soul on the box does.
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include "board.h"
#include "lcd.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe.cpp sets it) */
#include "wardrobe.h"   /* THE WRIST WARDROBE: every costume baked, morph only -- the veil, the costume where the gel stood */
#include "walls.h"      /* THE FOUR WALLS: the wall, the residual, the fall -- every tunable of the floor, in one place (plain C: the preview and the desk read it too) */
#include "imu.h"
#include "touch.h"
/* step 2 (Khalid, 2026-09-09: "bulk up the firmware while keeping it
 * light... connect it to WiFi... reach it from any device via the
 * tailnet"): the modules below only ADD to this file -- the store (NVS),
 * the radio and what rides on it, the link to the desk, the day log,
 * the glance, the console. They read ``app`` and ask for a pet; the
 * gel, the bowl and the fall are untouched. */
#include <driver/uart.h>
#include <sys/time.h>
#include <time.h>
#include "app.h"
#include "store.h"
#include "bus.h"        /* HARDWARE ROOM: the second bus, the reserved pins, the dark drivers */
#include "ppg.h"        /* THE PULSE ROW: a 30 s heart-rate read from the glass (SETTINGS 5), its count and its number on the row */
#include "net.h"
#include "link.h"
#include "daylog.h"
#include "daykeep.h"             /* THE STATURE WEEK: the week's drift off the day files (daykeep.cpp, pure), read beside the norm at the date's turn */
#include "nightsum.h"            /* THE NIGHT RING KNOWS HIS HEART (D33 / D34 / D36): the week's nights and resting heart off the same files (nightsum.c, pure) */
#include "glance.h"
#include "doctor.h"               /* MORE SETTINGS' ABOUT: the build id (doctor.cpp owns the generated header) */
#include "ear.h"                  /* THE EAR'S LEDGER AND ITS LOW GEAR: the nap's kind to the tally, the gear around the awake nap */
#include "word.h"
#include "question.h"             /* THE WORD'S QUESTION (E45): a word ending in ? -- a pet in 10 s is wrist:yes, the tap on it wrist:no (question.c) */
#include "tone.h"                 /* THE WORD'S TONE (2026-10-12, watch-night C5): a word ending in ! = one bright settle + happy eyes at its show, ... = one slow blink + the lids half-down a second (tone.c) */
#include "console.h"
#include "gesture.h"   /* the gestures + the water lock (stage 4+3): taps as words, shakes, the wet glass */
#include "cable.h"     /* THE CABLE SENSE, SETTLED: charging() as a band with a settle, never a flap at the float; THE RISING CELL: the closed minute's mV as a second witness */
#include "spent.h"     /* THE LAST DROP'S MANNERS: the spent line (DROP_MV 3550 / vigor's spent) -- no WiFi dial off the cable, LOW light, the SHORT spell */
#include "rest.h"                /* the night and the breath (stage 5+6): one call a frame, a few numbers back */
#include "wake.h"
#include "wdt.h"                 /* THE LOOP'S PULSE: the task watchdog on this thread, fed at the top of loop */
#include "lastdeath.h"           /* THE LAST DEATH: the reset reason, the RTC breadcrumb ticked from loop, the nap's kind */
#include "house.h"               /* THE HOUSE: holding / empty (one soul, one body); the arrival, the departure, the offline home */
#include "face.h"                /* THE WORDLESS WATCH FACE: the hour when empty, the day and night rings, the drop, the trace -- behind him */
#include "hours.h"               /* THE HOURS' LIGHT: the palette tinted by the hour, the backlight following the day */
#include "idle.h"                /* IDLE LIFE: the small things a creature at rest does (a look, a blink, a settle, a bubble, a glance up) */
#include "tell.h"                /* THE TELL: the glance that answers, the step bob, the night light -- the day's facts shown on the body, never said */
#include "grain.h"               /* THE GRAIN: the seed's knobs (the desk's formulas), regard worn as warmth, the mood's first face */
#include "sky.h"                 /* THE SKY: the weather word, as light on the gel and a few small habits (sky.cpp) */
#include "vigor.h"               /* VIGOR: the cell's level as the body's vigor -- the breath, the idle pace, the lids, the sooner nap, the spent look; never a number */
#include "gauge.h"               /* the MAX17048's percent for VIGOR when it is there (dark until soldered) */
#include "clockwork.h"           /* CLOCKWORK: the stopwatch (the held breath), the timer and the alarm (the bounces) -- the three things a wrist is for */
#include "gentle.h"              /* THE GENTLE WAKE (D35): the pulse's feed from the lit loop; the rule itself is clockwork's */
#include "haptic.h"              /* its bump and its triple hop (dark until the DRV2605 is soldered) */
#include "motor.h"               /* motor_present: `bumps` says which driver would play */
#include "bumps.h"               /* THE BUMPS (2026-09-23): the creature's own touches through the motor -- the wisp's landing, the sink, the rise, the purr under a pet; one pure gate (bumps.h) */
#include "pulse.h"               /* THE PULSE AS THE BREATH + THE HEARTBEAT HANDED BACK (2026-10-08): the last clean pulse's band and age; the purr under a pet as his heartbeat */
#include "dirty.h"               /* THE DIRTY RECT (the optimization pass): only what changed is pushed to the panel */
#include "mark.h"                /* THE MARK (reiterated): each drawer says whether it drew something different, and where */
#include "wisp.h"                /* THE WISP: the desk's doorbell (and the phone's, over BLE) as a mote beside him -- the air door on the bowl's angle */
#include "page.h"                /* THE WATCH PAGE: his tool page on a drag up -- the hour, the date, steps, the cell, the alarm, the sky, the link (page.cpp) */
#include "place.h"               /* THE PLACE: text never over the gel -- the hour's wall (the top; the bottom while he is on the top wall), the word's (opposite him), the backstop (place.cpp) */
#include "bored.h"               /* THE BORED ROLL: a pet winds the wrist's own bored clock back (bored.cpp; wardrobe.cpp rolls) */
#include "ble.h"                      /* BLUETOOTH (ruling 58): the stack up only while the WiFi radio is down; the row's switch; the iPhone's notices as wisps (ancs.cpp) */
#include "ancs.h"                     /* THE NOTICE CLEARED FROM THE WRIST (T17): ancs_clear -- the negative action for the notice whose card the hold landed on */
#include "play.h"                     /* PLAY: THE BEAD AND THE SPIN (watch-ideas F47 + F53, 2026-09-23): the pure arbiter and the bead's roll; the body's reactions are here */
#include "cramp.h"                    /* THE SMALL HOUSE (2026-10-09): the cramp, the nudge, the wrist's own bid -- pure; the stance, the push and the look are folded into draw() */
#include "petplace.h"                 /* THE PET'S PLACE (THE OPERATOR NIGHT C1): the tap's third on him -- the crown's press, a flank's lean away, the base's slide away (pure) */
#include "lookback.h"                 /* THE LOOK BACK AFTER THE FALL (C2): after the slump the gaze toward the wall he came from, a second (pure) */
#include "drowse.h"                   /* THE DIM'S DROWSE (2026-10-12, watch-night C7): the dim edge as a look -- droopy lids, a hair of slump, the breath a notch slower; pure */
#include "wispeye.h"                  /* THE WISP WATCHED (C3): the gaze follows the newest wisp in and rests on it at its post; a swell is one blink (pure) */
#include "fall0.h"                    /* THE WEIGHTLESS BEAT (watch-night C8): |a| under 0.3 g for 120 ms while lit = free fall -- the loft, the wide eyes, the held breath; the impact's splat + the brace, or the ease; pure */
#include "stare.h"                    /* THE STARE RETURNED (watch-night C12): 8 s of watched glass, once a spell -- the look up at him, content, one settle; pure */
#include "hourlook.h"                 /* THE HOUR'S GLANCE (watch-night C15): the minute turning :00 -- one glance toward the hands' rim point; pure */
#include "shakeoff.h"                 /* THE SHAKE-OFF (watch-night C16): the wet glass dried and the lock let go by itself -- one jiggle of the harmonic, the squint; pure */
#include "knock.h"                    /* THE KNOCK FROM BEHIND (watch-night C13): a rap along the normal with no touch down -- eyes wide; a second inside 2 s the squint; pure */
#include "phonegone.h"                /* THE PHONE LEFT BEHIND (watch-night C17): the phone's BLE dropping on a walking wrist -- on the next raise one look toward the air door; pure */
#include "lull.h"                     /* THE LULL (watch-night C11): three slow strokes inside a minute -- droopy lids, the breath toward the sleeper's, idle quiet, 45 s; pure */
#include "near.h"                     /* THE DESK NEXT DOOR (watch-night C9): the air door's RSSI banded FAR / NEAR / AT HAND -- the cramp's "near", one glance toward the door; pure */
#include "listen.h"                   /* THE EAR TO THE WRIST (watch-night C19): the creature's own pulse read under the glass -- the gaze down to the strap, content, one settle after a clean one; pure */
#include "drum.h"                     /* THE DRUMMED TABLE (watch-night C18): his typing's fine jitter on the wrist -- the gel clock runs faster (the bubble's road), nothing else; pure */
#include "warm.h"                     /* THE WARM SKIN (watch-night C20): the pulse sensor's die temperature banded WARM / COLD -- one content settle on the raise, the cold shiver + the slow breath; pure */

/* --- the tunables ------------------------------------------------------- */

/* the gel's size and place on the face are gel.h's (GEL_SCALE 6: the
 * dome 156 px wide, 65% of the disc; GEL_FLOOR_DROP 106, the base on
 * the rim, bent to the bowl; GEL_ANGLE_STEP_DEG 90: the four walls). */

/* the desk's springs (Config.SPRING_K / SPRING_C: ~2.4 Hz, settled in
 * ~600 ms -- gel, not rubber) */
static const float SPRING_K    = 220.0f;
static const float SPRING_C    = 9.0f;
/* THE SKY'S WEIGHT (sky.h, watch-ideas E38): rain or storm held makes
 * the gel heavier -- the damping x sky_heavy_k (9 -> 11 when the weight
 * is in) and the lean's / tilt's eases slowed by the same; set once a
 * frame in animate() from sky_heavy(), 1.0 with no wet sky */
static float spring_c_k = 1.0f;
/* the resting breath (Config.BREATH_AMPLITUDE / BREATH_HZ) */
/* THE WRIST'S POSTURE (Khalid, 23:10: at 6x on the round glass "the slime
 * reads as a sludge, too much fat on the sides"): the desk's dome is
 * 1.6 wide to 1 tall; here it stands a little taller and narrower -- a
 * resting squash, the same state the stretch uses, never a new shape */
static const float WRIST_SQUASH = -0.15f;
/* THE STATURE WEEK (daykeep.h, 2026-09-23): a body carries its weeks --
 * the resting squash drifts up to a quarter of an art pixel taller after
 * a week over his norm, as much rounder after a week under it, read off
 * the day files at the date's turn (face_refresh_norm) and turned into
 * the one value the raster takes (gel_stature_squash: -0.0347 at the
 * quarter pixel). 0 until the files say otherwise: the preview and a
 * young watch stand at WRIST_SQUASH exactly */
static float stature_px = 0, stature_squash = 0;
static int   stature_over = 0, stature_under = 0;
static const uint32_t KNOCK_ARM_MS = 2500;   /* three knocks arm the hold that sends him away for this long */
/* THE STROKE (gesture.cpp): a finger drawn across the glass is a longer
 * pet; the gel leans INTO its travel by STROKE_LEAN of a1 (projected on
 * its own floor, so a stroke along the rim leans it that way and one
 * straight down gravity does not), easing back on gesture_stroke()'s
 * 2 s envelope; the eyes stay content STROKE_EYE_S; the feed's happy
 * eyes FEED_EYE_S */
static const float    STROKE_LEAN  = 0.15f;
static const float    STROKE_EYE_S = 4.0f;
static const float    FEED_EYE_S   = 1.5f;
static uint32_t knock_armed_until = 0;
static bool     lease_hand = true;      /* THE SETTINGS PAGE's LEASE row: may the hold / the knocks claim or release (NVS; serial `come` / `release` always may) */
/* THE LEASE, EASY (Khalid, 2026-09-10 afternoon: "the gestures for the
 * lease are way too hard to access -- I have to triple tap and then
 * some, and the slime still doesn't go away"): ONE gesture, always the
 * same -- a plain hold of about a second (gesture.cpp's LEASE window,
 * 0.5-1.9 s since 09-10 21:08, THE HOLD'S WINDOW, WIDER: 0.8-1.4 was
 * too tight for a thumb) TOGGLES the lease: `come` on the empty glass, `release`
 * while he is here (link_whistle picks by the house). The feedback is
 * the wrist's own, the instant the hold is read, before the box
 * answers: the glass PULSES once (the backlight up by LEASE_PULSE_ADD
 * for LEASE_PULSE_MS -- at the awake level that is the cap, at the
 * night's level a blink and not a lamp) and the gel, if here, GLANCES
 * UP for LEASE_GLANCE_S. On `release` the sink starts at once on the
 * wrist's own belief (house_release_now: `ok released` confirms, a
 * refusal un-sinks), so "the slime doesn't go away" never happens for
 * want of a reply. The knocks stay (`come`; and armed, the hold after
 * is the same release). */
static const uint32_t LEASE_PULSE_MS  = 150;
static const int      LEASE_PULSE_ADD = 60;
static const float    LEASE_GLANCE_S  = 0.8f;
static uint32_t lease_pulse_until = 0;
static float    lease_glance_t = 0;
static bool     trace_stir = false;     /* THE TRACE STIRRED (watch-night C14): a tap on the empty glass landed near the trace -- face_in_t.stir for one frame */
/* PEEKABOO (design/watch-ideas.md F49, 2026-09-10; gesture.cpp's
 * GES_PEEK_DOWN / GES_PEEK_UP): a plain hold past 2 s with nothing
 * armed -- HOLDING, no knock armed (the knocks' hold is the release;
 * the lease's and the stopwatch's 0.5-1.9 s holds are under it) -- is a hand over the
 * bowl: the gel SINKS under the rim while the finger stays (PEEK_PX,
 * the house's own rise depth, over PEEK_SINK_S; the eyes stay up and
 * go last with the crown); the lift is the boo: it POPS back up over
 * PEEK_POP_S, eyes WIDE for PEEK_WIDE_S then HAPPY for PEEK_HAPPY_S,
 * one bounce (the pet's). `wrist:play` once per PEEK_PLAY_GAP_MS. The
 * long press the lift also yields is swallowed when the boo took it. */
static const float    PEEK_PX      = 95.0f;
static const float    PEEK_SINK_S  = 0.6f;
static const float    PEEK_POP_S   = 0.3f;
static const float    PEEK_WIDE_S  = 0.5f;
static const float    PEEK_HAPPY_S = 1.0f;
static const uint32_t PEEK_PLAY_GAP_MS = 600000;
static bool     peek_hiding = false;      /* the hand is over the bowl */
static float    peek_k = 0;               /* 0 = up, 1 = under the rim (eased) */
static float    peek_pop_t = -1;          /* the boo's clock, -1 = none */
static bool     peek_took_press = false;  /* the boo took this lift: the long press after it is nothing */
static uint32_t peek_play_ms = 0;
/* THE SHOVE (design/watch-swipes.md 3B, 2026-09-23; gesture.cpp's
 * GES_SWIPE): a FLICK along his wall on the bare glass, HOLDING, is
 * play, not a move -- a hand nudging a blob in a bowl. The gel SKIDS
 * SHOVE_PX the flick's way along the wall (the compose's slide axis:
 * + = his right), the push in over SHOVE_OUT_S with a squash on it
 * (SHOVE_SQUASH, a half sine over SHOVE_SQUASH_S: the hand presses as
 * it pushes), then he slides back to his place over the rest of
 * SHOVE_S with ONE settle (a damped half-turn: through his place,
 * a little past it, back), eyes WIDE for SHOVE_WIDE_S then HAPPY for
 * SHOVE_HAPPY_S. `wrist:play` ("shoved") once per SHOVE_PLAY_GAP_MS,
 * the peekaboo's own gap. A second flick inside SHOVE_AGAIN_S is the
 * same shove again, no bigger (calm: the skid restarts, nothing adds).
 * He never changes wall by hand -- the walls are gravity's (a flick
 * that moved him to the next wall would fight the tilt within a frame;
 * ruled out in the design). On the empty glass a swipe is nothing;
 * with a page up it is the page's (page_touch). */
static const float    SHOVE_PX       = 10.0f;
static const float    SHOVE_S        = 0.5f;
static const float    SHOVE_OUT_S    = 0.08f;
static const float    SHOVE_SQUASH   = 0.12f;
static const float    SHOVE_SQUASH_S = 0.25f;
static const float    SHOVE_WIDE_S   = 0.4f;
static const float    SHOVE_HAPPY_S  = 1.0f;
static const float    SHOVE_AGAIN_S  = 2.0f;
static const uint32_t SHOVE_PLAY_GAP_MS = PEEK_PLAY_GAP_MS;
static float    shove_t = -1;             /* the shove's clock, -1 = none */
static float    shove_dir = 0;            /* the flick's way along the wall: +1 his right, -1 his left */
static float    shove_now = 0;            /* this frame's skid, face px along the wall (animate) */
static float    shove_squash = 0;         /* this frame's push (animate) */
static bool     shove_eyes_wide = true;   /* wide first, then happy */
static uint32_t shove_ms = 0;             /* when the last shove began (the AGAIN window) */
static uint32_t shove_play_ms = 0;
/* THE TICKLE (THE OPERATOR NIGHT, design/watch-night-2026-10-12.md C4;
 * gesture.cpp's GES_TICKLE): one finger scrubbed back and forth ON him
 * -- three turns inside a second, the whole scrub inside his painted
 * box (gel_face_box) -- is a tickle, HOLDING, not asleep. THE WRIGGLE:
 * the spin's slow-wobble drive (below) on both springs, but at
 * TICKLE_HZ -- over the springs' own 2.4 Hz, so the mass jiggles under
 * the force instead of swinging with it -- fading over TICKLE_S;
 * TICKLE_DRIVE is five times the spin's 40 because a 6 Hz push above
 * resonance buys a fifth of the sway the spin's 0.8 Hz push gets for
 * the same force (K 220, C 9: the amplitude goes as 1 / |K - w^2|).
 * The SQUINT through the wriggle, then the HAPPY eyes TICKLE_HAPPY_S;
 * and a LEAN AWAY from the finger -- the pet's place's own (PETPLACE_LEAN
 * 0.12, in over 0.3 s), eased back by TICKLE_LEAN_S. `wrist:play`
 * ("tickled") once per PLAY_GAP_MS (play's own gap). A scrub BESIDE him
 * is nothing (logged); on the empty glass nothing; asleep nothing. */
static const float    TICKLE_S        = 0.6f;
static const float    TICKLE_HZ       = 6.0f;
static const float    TICKLE_DRIVE    = 200.0f;
static const float    TICKLE_HAPPY_S  = 1.0f;
static const float    TICKLE_LEAN_S   = PETPLACE_S;   /* the lean's whole road, the pet's place's second */
static float    tickle_t = -1;            /* the tickle's clock, -1 = none */
static float    tickle_side = 0;          /* the lean's way: away from the finger (+ his right) */
static float    tickle_a1 = 0;            /* this frame's lean (animate) */
static bool     tickle_squint = true;     /* the squint first, then happy */
static uint32_t tickle_play_ms = 0;
/* THE FAR TAP (design/watch-ideas.md A4, 2026-09-10): a knock on a
 * bowl's far wall turns a head. A single tap that lands farther than
 * FAR_TAP_PX from the gel's centre (its base on the rim, half its
 * height in) is not a pet: the gaze turns toward it for FAR_TAP_S --
 * the tap's offset read in the gel's own frame (along its floor: the
 * body's right; across it: up), each axis past FAR_TAP_DEAD_PX -- eyes
 * open, then the ordinary face. No sense; the day log's taps +1. A tap
 * on him is the pet as ever. */
static const float FAR_TAP_PX      = 60.0f;
static const float FAR_TAP_S       = 1.5f;
static const float FAR_TAP_DEAD_PX = 20.0f;
static float far_t = 0;                   /* the look's clock */
static int   far_gx = 0, far_gy = 0;      /* the look's gaze, in the gel's frame */
/* THE PET'S PLACE (petplace.h; THE OPERATOR NIGHT C1): the tap's third
 * on him, in his frame -- the crown's press as ever, a flank's shallower
 * with a lean away from the finger, the base's with a 2 px slide away;
 * the eyes glance at the finger first (look_at), then the pet's face */
static petplace_t pet_place = { PETPLACE_CROWN, 0, 0, 0 };
static float pet_place_t = -1;            /* seconds since the placed pet, -1 = none */
static float pet_press_k = 1.0f;          /* this pet's press, of PET_SQUASH */
static float pet_place_a1 = 0, pet_place_slide = 0;   /* this frame's lean and slide (animate) */
/* THE LOOK BACK AFTER THE FALL (lookback.h; C2): after the slump the
 * gaze turns toward the wall he came from for LOOKBACK_S, under the
 * brace's squint if there is one */
static int   lookback_from = 0;           /* the wall he fell from (fall_to) */
static float lookback_t = -1;             /* seconds since the slump ended, -1 = no look */
/* THE WISP WATCHED (wispeye.h; C3): the gaze follows the newest wisp
 * through its bow in and rests on it at its post for WISPEYE_DWELL_S,
 * then the ordinary face; a swelling halo gets one blink; the eyes are
 * held up through it (engage) -- never a bump, never a sense */
static wispeye_fx_t wisp_eye = { 0, 0, false, false, false };   /* this frame's look (the tick below, read in draw) */
/* PLAY: THE BEAD AND THE SPIN (play.h has the games, their numbers and
 * the gate table; play.cpp the arbiter and the roll). What the BODY does
 * with them lives here, through the existing springs and eyes only:
 *   the bead -- the gaze follows it round the bowl (look_at, each
 *   frame), the bite is the happy eyes for BEAD_HAPPY_S, half the pet's
 *   bounce and the small lump (play_lump, on the dome's own belly term
 *   with the meal's); the bead itself is a 3x3 mark in the SHADE colour
 *   (draw_bead, through fb_block like the lock's drop: THE PLACE's
 *   marks-in-the-shade);
 *   the spin -- the squint for SPIN_DIZZY_S, one kick against the turn
 *   and then THE SLOW WOBBLE: a slow sine (SPIN_WOBBLE_HZ) drives both
 *   springs with SPIN_DRIVE fading over the spell -- slow because the
 *   drive is, the springs' own 2.4 Hz under it; the gaze lags the turn
 *   (-2 against it for the first half, -1 coming back);
 *   the drop (F50, 2026-10-08) -- the gaze follows it down as the
 *   bead's; CAUGHT is the happy eyes for CATCH_HAPPY_S, half the pet's
 *   bounce and the MEAL's lump (play_lump carries it, 1.0); MISSED a
 *   look at the wet mark; the drop, the mark and the course's gates are
 *   draw_play_marks' (the lock's drop glyph small, in the shine);
 *   the course (F52) -- a gate lit keeps the eyes up; all three the
 *   content eyes for COURSE_CONTENT_S and the drink's settle.
 * The gyro is on only while the glass is lit (wake_up / go_dark). */
static const float SPIN_DRIVE     = 40.0f;   /* the slow wobble's force on the springs, per second, at the spell's start (JOLT_CAP is 80: half a jolt) */
static const float SPIN_WOBBLE_HZ = 0.8f;    /* the wobble's own rate: under the springs' 2.4 Hz, so it reads as the room still turning */
static const float SPIN_KICK      = 1.6f;    /* the first kick against the turn (KICK_CAP is 3.2) */
static float last_jerk = 0;                  /* sense()'s jerk this frame, for the bead's stillness */
/* PLAY: THE TOSS AND THE RHYTHM (watch-ideas F48 + F51, 2026-10-09; play.h
 * has the numbers, play.cpp the reading, the flight and the schedule):
 *   the toss -- the loft rides the compose's lift (play_toss_lift, along
 *   the wall's normal), the small splat its squash (play_toss_squash,
 *   half the fall's); at the landing the vertical spring takes
 *   TOSS_KICK (half the fall's landing kick) and the eyes squint for
 *   TOSS_SQUINT_S. A skid against the flick is NOT added: the slosh
 *   already kicks the lateral spring on every jerk (the mass lags), so
 *   the body leans against the throw on its own;
 *   the rhythm -- the detector's RHYTHM_START un-arms the knocks' hold
 *   (HOLDING; on the empty glass the knocks were `come` and the beat
 *   has nobody to echo it), RHYTHM hands the gaps to play_rhythm; each
 *   pulse rides the squash (play_rhythm_squash), the end is the happy
 *   eyes for RHYTHM_HAPPY_S. */
static const float TOSS_KICK      = 1.6f;    /* the landing's kick on the vertical spring: half the fall's KICK_CAP 3.2 (the small splat) */
static float last_jerk_x = 0, last_jerk_y = 0;   /* sense()'s jerk this frame, its in-plane parts in the glass's frame (the toss's direction) */
static bool  rhythm_for_gel = false;         /* THE RHYTHM opened while he was here (the close is judged on it too) */
static const float BREATH_AMP  = 0.04f;
static const float BREATH_HZ   = 0.55f;
/* the sleep crumple and the wake's ease (Config.SLEEP_SQUASH / WAKE_EASE_MS) */
static const float SLEEP_SQUASH = 0.30f;
static const float WAKE_EASE_S  = 0.26f;
/* the face: snaps in, melts away over EYE_FADE_MS after its hold */
static const float EYE_FADE_S   = 0.55f;
static const float EYE_HOLD_PET_S  = 4.0f;
static const float EYE_HOLD_WAKE_S = 3.0f;
/* the pet: the flat press (behaviour.PET, 0.18 for 300 ms, melting off
 * over PET_EASE_MS) and the bounce that follows */
static const float PET_S        = 0.30f;
static const float PET_EASE_S   = 0.15f;
static const float PET_SQUASH   = 0.18f;
static const float PET_BOUNCE   = -2.4f;
/* THE FOUR WALLS (Khalid 2026-09-10 17:10, reopening the continuous
 * bowl: "I don't like how the slime shape is when it's between angles
 * like top-left"; "there must be a better way ... keeping it lean"):
 * the gel sits at one of four rim places -- THE WALL, 6 / 3 / 12 / 9
 * o'clock -- and is drawn axis-aligned there, always. Gravity's angle
 * past the wall's is THE RESIDUAL, worn as a lean (a1), a slosh (a2)
 * and a slide (px along the wall), all eased; the wall changes only
 * when the pull passes the diagonal by WALL_HYST_DEG for WALL_HOLD_S,
 * and a change IS the fall. Every number is walls.h's -- the preview
 * and the desk's watchface.py read the same file. */
/* the slosh: what a turn and a jolt do to the springs, and the caps
 * (calm: the project's anti-Sonic law) */
static const float TURN_KICK    = 6.0f;    /* lateral force per g/s of lean change */
static const float JOLT_FORCE   = 60.0f;   /* squash force per g of jerk (per second) */
static const float JOLT_MIN     = 0.06f;   /* g of jerk that counts as a jolt */
static const float JOLT_CAP     = 80.0f;   /* a 100 ms shake at the cap = the desk's landing kick (3.2) */
static const float KICK_CAP     = 3.2f;    /* the landing kick, the desk's largest impulse */
/* the dark house: the LIT SPELL. Two presets since MORE SETTINGS (Khalid
 * 2026-09-10 21:12; the LIT SPELL row on SETTINGS 4, NVS `spellsht`):
 * LONG = 60 s dim / 90 s sleep -- the DEBUG SETTING of 19:00 ("make the
 * screen stay on for longer so we can debug better"), the default because
 * it is what is flashed; SHORT = the ruled 12 s / 20 s (ruling 2). This
 * replaces the "put the ruled numbers back after the debugging week"
 * chore: he flips the row when the week is over (the cell pays for LONG). */
static const uint32_t SPELL_DIM_MS[2]   = { 60000, 12000 };   /* [0] LONG, [1] SHORT */
static const uint32_t SPELL_SLEEP_MS[2] = { 90000, 20000 };
static inline uint32_t spell_dim_ms(void)   { return SPELL_DIM_MS[store.spell_short ? 1 : 0]; }
static inline uint32_t spell_sleep_ms(void) { return SPELL_SLEEP_MS[store.spell_short ? 1 : 0]; }
/* VIGOR (vigor.cpp): a low cell naps sooner -- the still timers shorter
 * under VIGOR_LOW (a tired body does not sit up lit as long) */
static const uint32_t DIM_LOW_MS   = 8000;
static const uint32_t SLEEP_LOW_MS = 12000;
static const float MOTION_G     = 0.05f;   /* jerk that counts as "not still" */
/* the nap itself, the raise and the lower-to-sleep are wake.cpp's
 * (stage 1+2: the IMU's wake-on-motion engine, the burst's verdict) */
static const uint8_t BL_AWAKE   = 200;
static const uint8_t BL_DIM     = 40;
/* THE LAMP (page.cpp, watch-ideas B17): the glass white at FULL while it
 * burns -- the one time the backlight ignores the hour and the night;
 * when it goes out the gel squints at the glare for LAMP_SQUINT_S */
static const uint8_t BL_LAMP    = 255;
static const float   LAMP_SQUINT_S = 1.2f;
/* BATTERY MANNERS (vigor.cpp, 2026-09-10 evening): under 20 % the awake
 * glass is a step dimmer -- BL_AWAKE_LOW in place of BL_AWAKE wherever
 * the awake level is chosen (the hour's factor and the dim floor apply
 * as ever; the lamp ignores it: he asked); the glance holds shorter
 * (glance.cpp); under 10 % the radio's claim comes half as often
 * (radio.cpp) and the page shows the cell in the shade (page.cpp) */
static const uint8_t BL_AWAKE_LOW = 150;
/* BRIGHTNESS (MORE SETTINGS, 09-10 21:12; the row on SETTINGS 4, NVS
 * `bright`): the awake level in three steps -- LOW / MID (BL_AWAKE, what
 * it always was) / HIGH (the panel's full). The hour's factor
 * (hours_level) and the night's levels (BL_SLEEP, NIGHT_BL) apply over
 * it as ever; BATTERY MANNERS' step down caps it under 20 % */
static const uint8_t BL_STEPS[3] = { 120, BL_AWAKE, 255 };
static inline uint8_t bl_awake(void)
{
    uint8_t lv = BL_STEPS[store.bright > 2 ? 1 : store.bright];
    lv = spent_level(spent_line(), lv, BL_STEPS[0]);   /* THE LAST DROP'S MANNERS: the LOW step whatever the row says */
    return (vigor_thrift() && lv > BL_AWAKE_LOW) ? BL_AWAKE_LOW : lv;
}
/* THE NIGHT (rest.cpp): asleep, the lit glass is at its lowest -- a
 * raise in the dark must not be a lamp */
static const uint8_t BL_SLEEP   = 16;
static const float   STRETCH_SQUASH = -0.30f;   /* the morning stretch: tall, eyes wide */
/* THE SMALL MANNERS (design/watch-ideas.md A3, B18; 2026-09-23). THE HANG
 * SLUMP: a raise from an arm that hung (wake.cpp: the nap's polls in
 * the hanging posture a minute or more) finds the gel spread on the
 * wall -- HANG_SQUASH, a touch of the splat's LAND_SQUASH 0.48 -- and
 * standing up over HANG_S, the eyes coming after (their rise waits for
 * it). SHOWING SOMEONE: the glass turned away and held (wake.cpp) is
 * company: the gel's most presentable thing -- a look up, content eyes,
 * one settle (SHOW_SETTLE_SQUASH over SHOW_SETTLE_S, idle's own settle)
 * -- for SHOW_S, his rings and ticks dim meanwhile (face.cpp). Neither
 * on the shelf, neither over the sleep look. */
static const float   HANG_SQUASH    = 0.16f;
static const float   HANG_S         = 0.5f;
static const float   SHOW_S         = 2.0f;
static const float   SHOW_SETTLE_SQUASH = 0.06f;
static const float   SHOW_SETTLE_S  = 1.5f;
static const rest_out_t *rest = NULL;           /* rest_tick's numbers, this frame */
static bool rest_woke_raise = false;            /* THE WAKING BREATH (watch-night C6): wake_up's word that a RAISE lit the glass, holding -- true for the one rest_tick after it */
static bool rest_chilled = false;               /* THE WARM SKIN (watch-night C20): warm_tick's word that the skin's COLD band is paid this frame -- true for the one rest_tick after it (the shiver, the slow breath) */
static float warm_settle_t = 0;                 /* THE WARM SKIN: the content settle's seconds left (WARM_SETTLE_S on the raise after the WARM band's entry; folded into draw()) */
static uint32_t warm_seq_seen = 0;              /* THE WARM SKIN: ppg_die_temp_seq() as last fed to warm.c */
static float itch_rad = 0;                     /* THE ITCH IN THE BOWL (rest.cpp): a long sit as a few degrees off gravity's angle, added to the bowl's target */
/* THE NIGHT LIGHT (tell.cpp): asleep and a raise in the quiet hours --
 * the DIMMEST glass the panel still shows (under BL_SLEEP: the hour is
 * all there is to see), the hour in the shade, dark again in 3 s */
static const uint8_t NIGHT_BL   = 8;
static const tell_out_t *tell = NULL;           /* tell_tick's numbers, this frame */
static int tell_cue = TELL_CUE_NONE;            /* a raise's wake or the knock's arrival, handed to the tell once */
static const clockwork_out_t *cw = NULL;        /* clockwork_tick's numbers, this frame */
/* THE WATCH PAGE's week (page.cpp, HIS WEEK): the six days before today,
 * oldest first, read from the day files with the norm (once at boot and
 * at the date's turn, never per frame); PAGE_NO_DAY = no file that day */
static uint32_t week_hist[6] = { PAGE_NO_DAY, PAGE_NO_DAY, PAGE_NO_DAY, PAGE_NO_DAY, PAGE_NO_DAY, PAGE_NO_DAY };
/* THE FRAME PUSH (the optimization pass, 2026-09-10; Khalid 23:55:
 * "optimize it, make it more efficient -- just the watch/screen"). The
 * frame is composed at FRAME_MS as before, then DIFFED (dirty.c: tile
 * hashes) and only the changed rect goes over the SPI bus; a frame
 * with nothing changed pushes nothing. After IDLE_AFTER_FRAMES such
 * frames in a row the loop composes at FRAME_IDLE_MS instead (20 Hz)
 * until something moves -- the first changed frame is back at 40 fps.
 *
 * REITERATED (the second pass; README "reiterated"): the frame is diffed
 * only under the HINT -- the gel's own box when the raster cache missed
 * or the compose moved it (gel_face_box), and each overlay's mark
 * (mark.h: a drawer's marks differ from last frame's) -- so a frame in
 * which nothing could have changed hashes nothing; a whole-frame scan
 * runs every SAFETY_SCAN_MS as the net under any drawer that forgets
 * to mark. THE STAGE: a rect of STAGE_PX or fewer pixels is copied into
 * a second, small DMA buffer and pushed from there, so the next frame
 * composes over fb while that push flies; a bigger rect is pushed from
 * fb itself and the next compose waits for it (the raster, which never
 * touches fb, runs before the wait). Why 32 KB and not a second
 * framebuffer: LovyanGFX's ESP32-S3 path is asynchronous only for the
 * LAST 32 KB of a transfer (the SPI's 2^18-bit length register; the
 * library waits in-call between chunks), so a 115 KB second buffer
 * would leave ~17 KB of heap with the TLS session held and its
 * full-frame push would still block the loop for ~8 ms -- 32 KB is the
 * size at which a push both fits the heap rule (>= 90 KB held) and
 * flies whole. And the rect is pushed as it is: the narrow rect and the
 * full-width band go through the same DMA engine at the same bytes per
 * second (one descriptor a row costs nothing measurable), so the tighter
 * rect is always fewer bytes; PUSH_WHOLE_PCT is gone. */
static const uint32_t FRAME_MS          = 25;    /* ~40 fps while anything moves */
static const uint32_t FRAME_IDLE_MS     = 50;    /* 20 Hz while nothing does */
static const int      IDLE_AFTER_FRAMES = 3;     /* unchanged frames before the loop slows */
static const uint32_t SAFETY_SCAN_MS    = 1000;  /* a whole-frame diff this often whatever the marks say */
static const int      STAGE_PX          = 16384; /* the push stage: 32 KB of internal DMA RAM (LovyanGFX's asynchronous limit) */

/* how the IMU's axes land on the screen (VERIFY on the board: the log
 * prints the raw vector; with the USB port held down, down should read
 * (0, +1)). Screen x right, y down. */
/* The IMU axes -> the screen's down, as a MAP index (bit0: negate x,
 * bit1: negate y, bit2: swap x/y), switchable live over serial ('0'..'7')
 * so the right one can be found on the wrist without a reflash. Map 1
 * (= -x, +y) was the first guess; Khalid read the markers on the wrist
 * (2026-09-09): every direction came out swapped x<->y, so map 6
 * (down = +y, -x) is the board's truth. */
static int axis_map = 6;
static inline float DOWN_X(const vec3_t &a)
{
    float v = (axis_map & 4) ? a.y : a.x;
    return (axis_map & 1) ? -v : v;
}
static inline float DOWN_Y(const vec3_t &a)
{
    float v = (axis_map & 4) ? a.x : a.y;
    return (axis_map & 2) ? -v : v;
}
/* the ORIENTATION MARKERS (Khalid, 2026-09-09: "add markers on what the
 * orientation is on each corner of the screen"): a letter at each wall
 * (B R T L, the current floor bright), a red dot at the rim where the
 * IMU's gravity points. 'd' over serial toggles them. */
static bool show_markers = false;      /* off since the orientation settled (map 6); `markers on` / 'd' */

/* --- the parts ------------------------------------------------------------ */

static WatchLCD lcd;
static uint16_t *fb;
const uint16_t *app_fb = NULL;
static uint16_t *cell = NULL;           /* the 1x art cell: 8 KB, in PSRAM since the optimization pass (only the CPU reads it; the DMA reads fb) */
static dirty_t  *dirty = NULL;          /* THE DIRTY RECT's tile hashes (900 B, PSRAM) */
static uint16_t *stage = NULL;          /* THE STAGE: the push source for a rect of STAGE_PX or fewer (32 KB, internal DMA) */
static bool     fb_in_flight = false;   /* a push is reading fb itself: the next compose waits for it */
static uint32_t pushes = 0, push_px = 0, busy_us = 0;   /* the report line: pushes a window, pixels pushed, the loop's busy time */
static uint32_t stage_pushes = 0, fb_pushes = 0, tiles_hashed = 0;   /* pushes by source, tiles hashed, this window */
static uint32_t moving_frames = 0, moving_us = 0;       /* frames composed at the moving rate, and the time spent at it */
/* the frame's clocks, this window (us): the compose's longest, its parts' sums, the waits */
static uint32_t t_frame_max = 0, t_raster = 0, t_gel = 0, t_face = 0, t_over = 0, t_hash = 0, t_push = 0, t_wait = 0, t_wait_max = 0;
static uint32_t t_wait_frame = 0;       /* this frame's wait, left out of its compose time */
static int      quiet_frames = 0;       /* frames in a row that pushed nothing */
static uint32_t report_ms = 0;          /* the 5 s report window's start */
static float    fps_moving = 0, fps_moving_s = 0;   /* DOCTOR: the last window's moving rate and its seconds (app_frame_stats) */
static uint32_t safety_ms = 0;          /* the last whole-frame diff */
static mark_t   mk_main;                /* THE MARK of this file's own overlays: the Zs, the lock's drop, the markers */
static dirty_rect_t gel_box_last = { 0, 0, 0, 0 };   /* the gel's rect last frame (the hint's other half when it moves) */
static bool     had_gel = false;

/* face.cpp's ring tables: PSRAM (the CPU alone reads them) */
void *face_mem(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    return p ? p : malloc(n);
}

struct Spring {
    float a = 0, v = 0;
    void kick(float amt) { v += amt; }
    void step(float dt) {
        v += (-SPRING_K * a - SPRING_C * spring_c_k * v) * dt;
        a += v * dt;
        if (fabsf(a) < 1e-4f && fabsf(v) < 1e-4f) a = v = 0;
    }
};

enum State { AWAKE, DIM, DARK };
static const char *STATE_NAMES[] = { "awake", "dim", "dark" };

static State state = AWAKE;
static Spring spring1, spring2;         /* lateral sway, vertical wobble */
/* THE FOUR WALLS' state: the wall it sits on (and that wall's angle,
 * which every overlay reads as the floor's), gravity's own angle (the
 * pull), the residual worn as the lean and the tilt, the hold before a
 * wall change, the fall in progress */
static int   wall = GEL_WALL_BOTTOM;    /* THE WALL: where he sits (walls.h) */
static float bowl_phi = 0;              /* the wall's angle (rad from straight down, + toward the right edge): the floor every overlay reads; only fall_swap moves it */
static float phi_target = 0;            /* gravity's angle on the face (the pull) */
static bool  phi_valid = false;         /* false while the watch lies level (no pull along the glass) */
static float g_along = 0, g_along_prev = 0;   /* gravity along the wall (+ = body's right) */
static float g_into = 0;                /* gravity into the wall (in-plane) */
static float flat_s = 0;                /* how long the watch has lain level */
static float lean = 0;                  /* the eased lean (a1): the residual's, LEAN_G x g along the wall */
static float tilt_k = 0, tilt_target = 0;   /* the residual's sine, eased at TILT_EASE: the slosh (a2) and the slide (px) along the wall */
static float wall_over_s = 0;           /* how long the pull has been past the diagonal (WALL_HYST_DEG); WALL_HOLD_S of it changes the wall */
static float lift = 0, lift_vel = 0;    /* a fall in progress: face px short of the rim along the new wall's gravity */
static float slide = 0, slide_vel = 0;  /* the drop's part across it (face px, + = the body's right) and its lateral speed: the arc to the new wall */
enum FallPhase { FALL_NONE, FALL_PEEL, FALL_FLIGHT, FALL_IMPACT, FALL_LAND };
static FallPhase fall_phase = FALL_NONE;
static float fall_t = 0, fall_squash = 0;
static int   fall_wall = GEL_WALL_BOTTOM;   /* the wall it falls to */
static float fall_to_phi = 0;           /* that wall's angle */
static float fall_sgn = 1.0f;           /* which way along the old wall the pull leans */
static bool  fall_centre = false;       /* THE BRACED FALL (walls.h): this fall is a half turn -- the drop through the centre */
static uint32_t brace_until = 0;        /* ...and for BRACE_S after its landing the gel braces (millis; 0 = not braced) */
static float hang_t = 0;                /* THE HANG SLUMP: the stand-up's seconds left after a raise from a hanging arm */
static float show_t = 0;                /* SHOWING SOMEONE: the presentable thing's seconds left */
static int   gaze_x = 0;                /* the eyes toward the pull while it leans or peels */
static vec3_t acc = {0, 0, 1}, acc_lp = {0, 0, 1};
static float clock_s = 0;
static uint32_t last_motion_ms = 0, last_touch_ms = 0;
static float eye_hold = 0, eye_alpha = 0, eye_rise_s = 0.35f;   /* THE GRAIN: engage()'s rise, from regard */
static int eye_kind = GEL_EYE_OPEN;
static float eye_kind_t = 0;
static float pet_t = 0;
static float stroke_along = 0;          /* the last stroke's travel along the gel's floor, -1..1 (+ = the body's right) */
static float wake_t = 0;
static bool touch_was_down = false;
/* THE BUMPS (bumps.h): the gate's clocks and the tallies; the purr is
 * ARMED at the pet and ridden on the breath's next inhale (purr_tick) */
static uint32_t bump_last_ms = 0, purr_last_ms = 0;
static bool     purr_armed = false;
/* THE NOTICE'S OWN REACTION (notice.h, 2026-10-06): a wisp's landing may
 * ask for TWO plays (a missed call, a voicemail) -- the second one
 * BUMP_AGAIN_MS after the first, one small timer; dropped if the glass
 * goes dark or he leaves between them (the touch was for him, lit) */
#define BUMP_AGAIN_MS 250u
static int      bump_again_kind = -1;
static int      bump_again_left = 0;
static uint32_t bump_again_ms = 0;
static bool     pulse_row_read = false;   /* THE PULSE ROW (2026-10-03): the running spot is the row's (a console `ppg spot` lands on the row too, but is not bumped) */
static uint32_t pulse_seen = 0;           /* ppg_spot_done() as last read by the loop */
/* THE PULSE AS THE BREATH / THE HEARTBEAT HANDED BACK (pulse.h; 2026-10-08):
 * the last CLEAN spot in the honest band, whoever asked for it (the row,
 * the console, the sampler), and millis() when it landed. THE ONE SEAM:
 * when the sampler's ring lands in ppg.h, the two lines in THE PULSE
 * ROW's block that fill these read its newest clean entry instead --
 * rest_tick and purr_tick never know */
static float    pulse_bpm_last = 0;       /* 0 = none yet */
static uint32_t pulse_at_ms = 0;
static uint32_t pulse_ring_seen = 0;      /* ppg_recent_seq() as last taken: THE PULSE IN THE DAY's own reads feed the breath and the purr too (THE MERGE, 2026-10-09) */
static uint32_t bump_beats = 0;           /* the purrs under a pet that played as his heartbeat */
static float pulse_age_s(void) { return pulse_at_ms ? (millis() - pulse_at_ms) / 1000.0f : -1.0f; }   /* -1 = never: pulse_fresh says no */
static float    breath_phase_prev = 0;
static uint32_t bump_played[BUMP_OCCASIONS] = { 0, 0, 0, 0, 0 };
static uint32_t bump_kept[BUMP_KEPT_N] = { 0 };
static int      bump_last_why = -1;                 /* the last decision, for `bumps` */
static uint32_t frames = 0;

static void log_state(const char *why)
{
    Serial.printf("[%8lu] state %s (%s)\n", (unsigned long)millis(),
                  STATE_NAMES[state], why);
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
/* an angle into (-pi, pi]: the short way round the bowl */
static float wrap_pi(float a)
{
    while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a <= -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}
static inline float deg(float rad) { return rad * 180.0f / (float)M_PI; }
static inline bool braced(void) { return brace_until && (int32_t)(brace_until - millis()) > 0; }   /* THE BRACED FALL's minute */

/* --- the seam (app.h): what the added modules read and may ask ------------- */

app_state_t app;
const char *const APP_STATE_NAMES[3] = { "awake", "dim", "dark" };
void app_frame_stats(float *moving_fps, float *moving_s, bool *idle_now)   /* DOCTOR */
{
    if (moving_fps) *moving_fps = fps_moving;
    if (moving_s) *moving_s = fps_moving_s;
    if (idle_now) *idle_now = quiet_frames >= IDLE_AFTER_FRAMES;
}
static void pet(void);

void app_pet(void) { if (house_holding()) pet(); else LOG("pet: the house is %s -- nothing", house_state_name()); }   /* THE HOUSE: no pet on an empty glass */
void app_keep_awake(void) { last_motion_ms = millis(); }
bool app_markers(void) { return show_markers; }
void app_set_markers(bool on)
{
    show_markers = on;
    store.markers = on;
    store_save_markers();
    LOG("markers %s", on ? "on" : "off");
}
void app_set_axis_map(int m)
{
    axis_map = m & 7;
    LOG("axis map %d: down = (%s%s, %s%s)", axis_map,
        (axis_map & 1) ? "-" : "+", (axis_map & 4) ? "y" : "x",
        (axis_map & 2) ? "-" : "+", (axis_map & 4) ? "x" : "y");
}

/* the clock: UTC in the RTC (NTP / the desk / `clock`), local by
 * store.tz_min. A soft reset keeps the RTC; a power cycle loses it, and
 * until someone tells the hour again the day log has nowhere to file
 * its minutes (it counts them as unlogged). */
static bool clock_set_flag = false;
static const char *clock_who = "nobody";   /* DOCTOR: who set it last (every caller passes a literal) */
bool clock_is_set(void) { return clock_set_flag; }
const char *clock_source(void) { return clock_who; }
void clock_local(struct tm *out, uint32_t *epoch_out)
{
    time_t now = time(NULL);
    if (epoch_out) *epoch_out = (uint32_t)now;
    time_t loc = now + (time_t)store.tz_min * 60;
    gmtime_r(&loc, out);
}
void clock_set_epoch(uint32_t epoch, const char *who)
{
    time_t before = time(NULL);
    struct timeval tv = { (time_t)epoch, 0 };
    settimeofday(&tv, NULL);
    clock_set_flag = true;
    clock_who = who;
    struct tm lt;
    clock_local(&lt, NULL);
    LOG("clock: %04d-%02d-%02d %02d:%02d:%02d local (tz %+d min) from %s, was %+ld s off",
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec,
        (int)store.tz_min, who, (long)((time_t)epoch - before));
}
uint32_t clock_local_date(void)
{
    if (!clock_set_flag) return 0;
    struct tm lt; clock_local(&lt, NULL);
    return (uint32_t)(lt.tm_year + 1900) * 10000u + (uint32_t)(lt.tm_mon + 1) * 100u + (uint32_t)lt.tm_mday;
}
int clock_local_minute(void)
{
    if (!clock_set_flag) return -1;
    struct tm lt; clock_local(&lt, NULL);
    return lt.tm_hour * 60 + lt.tm_min;
}

/* the battery: Vbat through the board's 200k/100k onto GPIO1, read
 * twice a second. The board has no VBUS pin: with USB in, the ETA6096
 * holds VBAT at its float (a full cell or no cell reads ~4.2 V), so
 * CHARGING is "at the float or above" -- honest for a watch on the
 * cable, blind to the first hour of a charge from empty. THE CABLE
 * SENSE, SETTLED (cable.h, 09-10 night): on at CABLE_ON_MV 4050, off
 * only under CABLE_OFF_MV 3980, each edge held CABLE_SETTLE_MS 10 s
 * before it is believed -- the 21:28 flap at the threshold is gone;
 * every reader of charging() (the claim rule, THE SHELF RULE, the day
 * log's flag, vigor, the drop, the page) sees the one settled value. */
static cable_t cable;
uint16_t batt_mv(void)
{
    static uint32_t at = 0; static uint16_t mv = 0;
    uint32_t now = millis();
    if (!at || now - at >= 500) {
        at = now;
        uint32_t sum = 0;
        for (int i = 0; i < 4; i++) sum += analogReadMilliVolts(PIN_BAT_ADC);
        mv = (uint16_t)(sum / 4 * 3);
    }
    return mv;
}
bool charging(void)
{
    uint16_t mv = batt_mv(); uint32_t now = millis();
    static uint32_t minute_at = 0;                               /* THE RISING CELL (cable.h): the closed minute's mV, a minute apart -- a cell never rises by itself */
    if (!minute_at || now - minute_at >= 60000) { minute_at = now; cable_minute(&cable, mv); }
    return cable_sense(&cable, mv, now);
}
/* THE LAST DROP'S MANNERS (spent.h): the spent line, read each frame
 * beside vigor's tick (below); link.cpp's radio_in and bl_awake() ask it */
static spent_t spent;
bool spent_line(void) { return spent.mv_on || vigor_spent(); }

/* --- the senses ----------------------------------------------------------- */

static void engage(float hold_s);

/* THE FALL (the walls' one event): the pull passed the diagonal and
 * stayed, so the wall changes -- the gel peels off the wall it sits on
 * and drops across the face to the new one (the phases: animate) */
static void fall_to(int new_wall)
{
    Serial.printf("[%8lu] bowl: %s -> %s (the pull at %.0f): the fall\n", (unsigned long)millis(),
                  GEL_WALL_NAMES[wall], GEL_WALL_NAMES[new_wall & 3], deg(phi_target));
    fall_wall = new_wall & 3;
    fall_to_phi = walls_phi(fall_wall);
    fall_sgn = g_along >= 0 ? 1.0f : -1.0f;    /* the pull's lean along the old wall */
    fall_centre = fall_wall == ((wall + 2) & 3);   /* THE BRACED FALL: a half turn goes through the centre */
    lookback_from = wall; lookback_t = -1;         /* THE LOOK BACK AFTER THE FALL: where he came from; a look still running is dropped (he is falling again) */
    fall_phase = FALL_PEEL;
    fall_t = 0;
    wall_over_s = 0;
    gaze_x = fall_sgn > 0 ? 2 : -2;
    eye_kind = GEL_EYE_WIDE;
    eye_kind_t = 0.6f;
    engage(2.5f);
}

/* the arrival: the mass reaches the glass long and compresses into it
 * (IMPACT), then the splat (LAND). The arc ends AT the new wall: the
 * drop's slide reaches 0 as its lift does (fall_swap gave it the
 * lateral speed for that), so the base lands on the wall's own rim
 * point; the slump after is the lean and the slosh easing in there. */
static void fall_land(void)
{
    lift = 0;
    lift_vel = 0;
    slide = 0;
    slide_vel = 0;
    fall_phase = FALL_IMPACT;
    fall_t = 0;
    fall_squash = IMPACT_SQUASH;
    Serial.printf("[%8lu] bowl: landed on the %s wall (the pull at %.0f)\n", (unsigned long)millis(),
                  GEL_WALL_NAMES[wall], deg(phi_target));
}

static void fall_splat(void)
{
    fall_phase = FALL_LAND;
    fall_t = 0;
    fall_squash = LAND_SQUASH;
    spring2.kick(KICK_CAP);                 /* the desk's landing kick */
    eye_kind = GEL_EYE_SQUINT;
    eye_kind_t = 0.45f;
    engage(2.0f);
    Serial.printf("[%8lu] bowl: splat at %.0f\n", (unsigned long)millis(), deg(bowl_phi));
    if (fall_centre && !rest_on_shelf()) {  /* THE BRACED FALL (walls.h): through the centre -- the squint held, the lean and the slosh reined in for a minute; a body that fell braces */
        brace_until = millis() + (uint32_t)(BRACE_S * 1000.0f);
        if (!brace_until) brace_until = 1;
        eye_kind_t = BRACE_S;
        Serial.printf("[%8lu] bowl: a fall through the centre: braced for %.0f s (the squint held, the lean cap x %.1f, the slosh x %.1f)\n",
                      (unsigned long)millis(), BRACE_S, BRACE_LEAN_K, BRACE_SLOSH_K);
    }
    fall_centre = false;
}

/* the swap: the mass is where it is. The puddle's CENTROID on the glass
 * is kept; a drop hanging straight down the new wall's gravity is hung
 * from it (its base half its height toward the pull), read in the new
 * frame as a lift short of the rim and a slide across -- never a jump,
 * and the frame turns to the new wall at this instant. THE ARC: the
 * drop must land ON the new wall, so it is given the lateral speed that
 * brings its slide to 0 in the time the lift takes to fall under
 * FALL_G -- a parabola across the face (for a half turn the centroid
 * is on the axis and the drop goes straight through the centre, as
 * before; for a quarter turn it is a short arc from beside the old
 * wall to the new one). */
static void fall_swap(void)
{
    const float D = GEL_FLOOR_DROP;
    /* the puddle's base on the old wall (slid along it by the creep) and
     * its centroid half its height up */
    float so = sinf(bowl_phi), co = cosf(bowl_phi);
    float hp = GEL_SCALE * GEL_BASE_RY * (1.0f - 0.45f * PEEL_SQUASH);
    float bx0 = D * so + slide * co, by0 = D * co - slide * so;
    float cx = bx0 - so * hp * 0.5f, cy = by0 - co * hp * 0.5f;
    /* the drop: its base half its height down the new gravity from there */
    float sn = sinf(fall_to_phi), cn = cosf(fall_to_phi);
    float H = GEL_SCALE * GEL_BASE_RY * (1.0f - 0.45f * DROP_SQUASH);
    float bx = cx + sn * H * 0.5f, by = cy + cn * H * 0.5f;
    float along = bx * sn + by * cn;             /* down the new gravity */
    float across = bx * cn - by * sn;            /* toward the new body's right */
    lift = D - along;
    slide = across;
    lift_vel = 0;
    /* the arc: across -> 0 over the fall's own time sqrt(2 lift / g) */
    float T = lift > 1.0f ? sqrtf(2.0f * lift / FALL_G) : 0.0f;
    slide_vel = T > 0 ? -slide / T : 0.0f;
    if (T <= 0) slide = 0;
    wall = fall_wall;
    bowl_phi = fall_to_phi;
    lean = 0;
    tilt_k = tilt_target = 0;
    g_along_prev = g_along = 0;
    fall_phase = FALL_FLIGHT;
    fall_squash = DROP_SQUASH;
    Serial.printf("[%8lu] bowl: the drop, %.0f px short of the %s wall, %.0f across (%.2f s of arc)\n",
                  (unsigned long)millis(), lift, GEL_WALL_NAMES[wall], slide, T);
}

/* one IMU sample folded into gravity, the lean, the bowl and the slosh */
static void sense(float dt)
{
    vec3_t a;
    bool got = imu_read(&a);
    bus0_note(imu_answered(), touch_alive());   /* THE BUS UNSTUCK (bus.cpp): the read noted; 50 unanswered in a row unstick bus 0 right here */
    if (!got) return;
    vec3_t prev_lp = acc_lp;
    acc = a;
    wrist_sample(&a, true);                  /* the day log's senses */
    /* gravity is the slow part; the fast part is the jolt */
    const float k = 0.18f;
    acc_lp.x += (a.x - acc_lp.x) * k;
    acc_lp.y += (a.y - acc_lp.y) * k;
    acc_lp.z += (a.z - acc_lp.z) * k;
    float jx = a.x - acc_lp.x, jy = a.y - acc_lp.y, jz = a.z - acc_lp.z;
    float jerk = sqrtf(jx * jx + jy * jy + jz * jz);
    last_jerk = jerk;                        /* PLAY: the bead's stillness reads it */
    last_jerk_x = DOWN_X(a) - DOWN_X(acc_lp); last_jerk_y = DOWN_Y(a) - DOWN_Y(acc_lp);   /* PLAY: THE TOSS reads the flick's way on the glass (the slosh's own `side`, both axes) */
    float dlx = acc_lp.x - prev_lp.x, dly = acc_lp.y - prev_lp.y, dlz = acc_lp.z - prev_lp.z;
    float drift = sqrtf(dlx * dlx + dly * dly + dlz * dlz);
    if (jerk > MOTION_G || drift > 0.02f) last_motion_ms = millis();

    /* THE FOUR WALLS: gravity's angle on the face is the pull; its parts
     * along and into the wall are read in the body frame at the wall's
     * angle (x toward the body's right, y into the wall). Flat = no
     * pull: the wall kept, the residual gone. */
    float gx = DOWN_X(acc_lp), gy = DOWN_Y(acc_lp);
    float inplane = sqrtf(gx * gx + gy * gy);
    float rxw = cosf(bowl_phi), ryw = -sinf(bowl_phi);
    g_along_prev = g_along;
    g_along = inplane > FLAT_G ? (gx * rxw + gy * ryw) : 0.0f;
    g_into = inplane > FLAT_G ? (gx * sinf(bowl_phi) + gy * cosf(bowl_phi)) : 0.0f;
    if (inplane > FLAT_G) {
        phi_target = atan2f(gx, gy);
        phi_valid = true;
        flat_s = 0;
    } else {
        /* level (as worn, looked at; lying on the desk): after FLAT_HOLD_S
         * the pull is HOME, the bottom wall -- from any other wall that is
         * a wall change, and the fall is the only road (walls.h) */
        flat_s += dt;
        if (flat_s >= FLAT_HOLD_S) {
            if (!(phi_valid && phi_target == 0.0f) && wall != GEL_WALL_BOTTOM)
                Serial.printf("[%8lu] bowl: level -> home (on the %s wall)\n", (unsigned long)millis(), GEL_WALL_NAMES[wall]);
            phi_target = 0.0f;
            phi_valid = true;
        } else phi_valid = false;
    }
    /* THE WALL CHANGE: the pull past the diagonal by WALL_HYST_DEG (60
     * deg from the wall it sits on) and STAYING there WALL_HOLD_S is a
     * turn, not a fidget: the wall changes to the one nearest the pull,
     * and the change is the fall. Level, or falling: the clock rests. */
    if (phi_valid && fall_phase == FALL_NONE) {
        float res = wrap_pi(phi_target - bowl_phi);
        if (fabsf(res) > (45.0f + WALL_HYST_DEG) * (float)M_PI / 180.0f) wall_over_s += dt;
        else wall_over_s = 0;
        if (wall_over_s >= WALL_HOLD_S) fall_to(gel_wall_of(phi_target));
    } else wall_over_s = 0;
    /* the eyes toward the pull while the residual is large */
    if (fall_phase == FALL_NONE)
        gaze_x = (phi_valid && fabsf(wrap_pi(phi_target - bowl_phi)) > 0.3f) ? (g_along > 0 ? 2 : -2) : 0;

    /* the slosh: a turn of the floor leans the gel against it (the
     * lateral spring), a jolt shakes it vertically (the wobble). The
     * mass lags the turn, so the kick is the turn's own rate, opposed. */
    /* Both are FORCES integrated over dt, never per-sample impulses: a
     * hand fidgeting with the board must read as a faint jiggle, and a
     * real shake as one wobble that settles -- the spring's own damping
     * (c 9) is what keeps a sustained drive small. */
    if (state == AWAKE || state == DIM) {
        float dg = (g_along - g_along_prev) / (dt > 1e-4f ? dt : 1e-4f);
        if (fabsf(dg) > 0.05f)
            spring1.kick(clampf(-dg * TURN_KICK, -KICK_CAP, KICK_CAP) * dt);
        if (jerk > JOLT_MIN) {
            float sgn = (DOWN_Y(a) - DOWN_Y(acc_lp)) >= 0 ? 1.0f : -1.0f;
            float f = clampf(sgn * (jerk - JOLT_MIN) * JOLT_FORCE, -JOLT_CAP, JOLT_CAP);
            spring2.kick(f * dt);
            float side = DOWN_X(a) - DOWN_X(acc_lp);
            spring1.kick(clampf(side * JOLT_FORCE, -JOLT_CAP, JOLT_CAP) * 0.5f * dt);
        }
    }
}

static void engage(float hold_s)
{
    /* the desk's rule: eyes snap in, and only melt away after the hold.
     * THE GRAIN colours the snap: they come up over regard's eye_rise_s
     * (0.35 s at regard 1, 0.9 s at -1; animate() does the rising) */
    eye_rise_s = grain_knobs()->eye_rise_s;
    if (hold_s > eye_hold) eye_hold = hold_s;
}

/* the gaze toward a point on the glass for ``s`` seconds, read in the
 * gel's own frame (along its floor: the body's right, gel_compose's
 * slide axis (cos phi, -sin phi); across it: up, against gravity);
 * returns the point's distance from the gel's centre */
static float look_at(int x, int y, float s)
{
    /* the gel's centre on the glass: the base centre GEL_FLOOR_DROP - lift
     * along gravity (sin phi, cos phi), then half its height back in */
    float gx = sinf(bowl_phi), gy = cosf(bowl_phi);
    float rx, ry; gel_blob_radii(WRIST_SQUASH, &rx, &ry);
    float d = (float)GEL_FLOOR_DROP - lift - ry * GEL_SCALE * 0.5f;
    float cx = SCREEN_W / 2.0f + gx * d, cy = SCREEN_H / 2.0f + gy * d;
    float dx = (float)x - cx, dy = (float)y - cy;
    float right = dx * gy - dy * gx;
    float up = -(dx * gx + dy * gy);
    far_gx = right > FAR_TAP_DEAD_PX ? 2 : right < -FAR_TAP_DEAD_PX ? -2 : 0;
    far_gy = up > FAR_TAP_DEAD_PX ? -1 : up < -FAR_TAP_DEAD_PX ? 1 : 0;
    far_t = s;
    return sqrtf(dx * dx + dy * dy);
}

/* the same distance without the look (PLAY: is the tap on him?) */
static float gel_dist(int x, int y)
{
    float gx = sinf(bowl_phi), gy = cosf(bowl_phi);
    float rx, ry; gel_blob_radii(WRIST_SQUASH, &rx, &ry);
    float d = (float)GEL_FLOOR_DROP - lift - ry * GEL_SCALE * 0.5f;
    float dx = (float)x - (SCREEN_W / 2.0f + gx * d), dy = (float)y - (SCREEN_H / 2.0f + gy * d);
    return sqrtf(dx * dx + dy * dy);
}

/* PLAY: THE SPIN -- the chip's z rate as the glass's turn, + clockwise
 * as seen. The axis map's swap and each negation are reflections of the
 * plane, and the glass's y runs DOWN (one more): an odd count flips the
 * sign. Only the board can prove which way is which (play.h's ruling). */
static inline float yaw_sign(void)
{
    int refl = ((axis_map & 4) ? 1 : 0) + ((axis_map & 1) ? 1 : 0) + ((axis_map & 2) ? 1 : 0) + 1;
    return (refl & 1) ? -1.0f : 1.0f;
}

/* THE FAR TAP: true = the tap landed off him and the gaze took it (not a pet) */
static bool far_tap(int x, int y)
{
    int gx0 = far_gx, gy0 = far_gy; float t0 = far_t;
    float dist = look_at(x, y, FAR_TAP_S);
    if (dist <= FAR_TAP_PX) { far_gx = gx0; far_gy = gy0; far_t = t0; return false; }   /* on him: the pet, the look untouched */
    eye_kind = GEL_EYE_OPEN;
    eye_kind_t = FAR_TAP_S;
    engage(FAR_TAP_S + 0.5f);
    Serial.printf("[%8lu] far tap at %d,%d (%.0f px off him): a look %+d,%+d, not a pet\n", (unsigned long)millis(), x, y,
                  dist, far_gx, far_gy);
    return true;
}

/* --- THE BUMPS (bumps.h; 2026-09-23) ------------------------------------------
 * The creature's own touches through the motor, each occasion ONCE:
 * the wisp's landing, the sink (the goodbye's settle), the rise, the
 * purr under a pet. The gate is pure (bumps.h; tools/bumps_test.cpp
 * pins its table); this is the state it reads -- the glass, the quiet
 * hours, the sleep look, the cable, the breath -- and the one call
 * that plays. Nothing else in this file may call haptic_play for the
 * creature: the clockwork's kicks are its own (the alarm, the
 * stopwatch: his tools, not his touches). */
static bumps_in_t bumps_state(void)
{
    bumps_in_t in;
    in.on = !store.bumps_off;
    in.lit = state != DARK;
    in.quiet = rest_night_now();
    in.asleep = rest && rest->dozing;
    in.on_cable = charging() || link_docked();      /* THE DOCK: a bump on a desk rattles the table (ruling 65) */
    in.breath_hz = rest ? rest->breath_hz : BREATH_HZ;
    in.breath_rest_hz = rest ? rest->breath_rest_hz : BREATH_HZ;
    in.now_ms = millis();
    in.last_bump_ms = bump_last_ms;
    in.last_purr_ms = purr_last_ms;
    return in;
}

/* one occasion: the gate decides, the motor plays (or the DRV2605),
 * one log line per bump; the purr is armed here and played by purr_tick.
 * THE NOTICE'S OWN REACTION: the wisp's landing may name its own kind
 * and plays (notice.h's table; kind_asked -1 = the gate's own kind, as
 * every other occasion) -- the gate still says whether anything plays
 * AT ALL; this only says what, once it opens */
static void bump_as(int occasion, int kind_asked, int repeats)
{
    bumps_in_t in = bumps_state();
    int why = 0;
    int kind = bumps_decide(occasion, &in, &why);
    bump_last_why = why;
    if (kind < 0) { if (why > 0 && why < BUMP_KEPT_N) bump_kept[why]++; return; }
    bump_last_ms = in.now_ms;
    if (occasion == BUMP_PET) {
        /* A8: the purr at the breath's rate -- ridden on the next inhale, not this frame */
        purr_last_ms = in.now_ms;
        purr_armed = true;
        breath_phase_prev = rest ? rest->breath_phase : 0;
        return;
    }
    if (kind_asked >= 0 && kind_asked < HAPTIC_KINDS) kind = kind_asked;
    bump_played[occasion]++;
    bool played = haptic_play(kind);
    if (played && repeats > 1) { bump_again_kind = kind; bump_again_left = repeats - 1; bump_again_ms = in.now_ms + BUMP_AGAIN_MS; }
    LOG("bump: %s (%s%s%s)%s", bumps_name(occasion), state == AWAKE ? "lit" : "dim",
        kind == HAPTIC_PURR ? ", a purr" : kind == HAPTIC_HOP3 ? ", the triple hop" : "", repeats > 1 ? ", twice" : "",
        played ? "" : " -- no motor on the BOM: nothing felt");
}
static void bump(int occasion) { bump_as(occasion, -1, 1); }

/* the second play, BUMP_AGAIN_MS after the first; dropped if the glass went dark or he left first */
static void bump_again_tick(void)
{
    if (bump_again_left <= 0) return;
    if (state == DARK || !house_holding()) { bump_again_left = 0; return; }
    if ((int32_t)(millis() - bump_again_ms) < 0) return;
    bump_again_left--;
    haptic_play(bump_again_kind);
    bump_again_ms += BUMP_AGAIN_MS;
}

/* the purr rides the breath: played the instant the phase wraps (the
 * inhale's start); dropped if the glass went dark or he left first */
static void purr_tick(void)
{
    if (!purr_armed) return;
    if (state == DARK || !house_holding()) { purr_armed = false; return; }
    float ph = rest ? rest->breath_phase : 0;
    if (bumps_purr_due(breath_phase_prev, ph)) {
        purr_armed = false;
        bump_played[BUMP_PET]++;
        float age = pulse_age_s();
        if (pulse_fresh(pulse_bpm_last, age)) {
            /* THE HEARTBEAT HANDED BACK (pulse.h, D32): a pulse known and fresh -- the purr is his own beat, one soft tap per pulse for HEARTBEAT_S */
            bump_beats++;
            bool played = haptic_beats(pulse_bpm_last, HEARTBEAT_S);
            LOG("bumps: the purr under a pet -- his heartbeat, %.0f bpm, %.0f s (on the inhale; read %.0f min ago)%s", pulse_bpm_last, HEARTBEAT_S, age / 60.0f,
                played ? "" : " -- no motor on the BOM: nothing felt");
        } else {
            bool played = haptic_play(HAPTIC_PURR);
            LOG("bump: the purr under a pet (on the inhale, the breath at %.2f Hz%s)%s", rest ? rest->breath_hz : BREATH_HZ,
                pulse_bpm_last > 0 ? "; the pulse is stale, the purr's own rate" : "; no pulse known, the purr's own rate",
                played ? "" : " -- no motor on the BOM: nothing felt");
        }
    }
    breath_phase_prev = ph;
}

/* THE TRACE ANSWERED (app.h): the wall and the residual's eased lean, -1..1 of LEAN_CAP (ONE BODY ACROSS THE HOUSES, 2026-10-09) */
void app_trace(int *wall_out, float *lean_out)
{
    if (wall_out) *wall_out = wall;
    if (lean_out) *lean_out = clampf(lean / LEAN_CAP, -1.0f, 1.0f);
}

void app_bumps_status(char *out, int cap)
{
    bumps_in_t in = bumps_state();
    uint32_t played = bump_played[0] + bump_played[1] + bump_played[2] + bump_played[3] + bump_played[BUMP_NUDGE], kept = 0;
    for (int i = 1; i < BUMP_KEPT_N; i++) kept += bump_kept[i];
    int n = snprintf(out, cap, "bumps %s: %lu played (the wisp %lu, the sink %lu, the rise %lu, the purr %lu -- %lu of them his heartbeat, the nudge %lu)%s; kept back %lu (dark %lu, quiet %lu, asleep %lu, cable %lu, breath %lu, minute %lu, gap %lu, off %lu); now %s%s%s%s, breath %.2f/%.2f Hz; motor %s",
                     in.on ? "on" : "OFF (NVS)", (unsigned long)played,
                     (unsigned long)bump_played[BUMP_WISP], (unsigned long)bump_played[BUMP_SINK], (unsigned long)bump_played[BUMP_RISE], (unsigned long)bump_played[BUMP_PET], (unsigned long)bump_beats, (unsigned long)bump_played[BUMP_NUDGE],
                     purr_armed ? " (a purr armed for the inhale)" : "", (unsigned long)kept,
                     (unsigned long)bump_kept[BUMP_KEPT_DARK], (unsigned long)bump_kept[BUMP_KEPT_QUIET], (unsigned long)bump_kept[BUMP_KEPT_ASLEEP], (unsigned long)bump_kept[BUMP_KEPT_CABLE],
                     (unsigned long)bump_kept[BUMP_KEPT_BREATH], (unsigned long)bump_kept[BUMP_KEPT_MINUTE], (unsigned long)bump_kept[BUMP_KEPT_GAP], (unsigned long)bump_kept[BUMP_KEPT_OFF],
                     in.lit ? "lit" : "dark", in.quiet ? ", the quiet hours" : "", in.asleep ? ", asleep" : "", in.on_cable ? ", on the cable" : "",
                     in.breath_hz, in.breath_rest_hz,
                     haptic_present() ? "the DRV2605" : motor_present() ? "bare, GPIO 33" : "NONE on the BOM (`bom motor on`)");
    if (n > 0 && n < cap && bump_last_why > 0) n += snprintf(out + n, cap - n, "; the last kept back: %s", bumps_kept_name(bump_last_why));
    if (n > 0 && n < cap) {                         /* THE HEARTBEAT HANDED BACK: what a pet would play now */
        float age = pulse_age_s();
        if (pulse_fresh(pulse_bpm_last, age)) snprintf(out + n, cap - n, "; his pulse %.0f bpm, %.0f min old: a pet is his heartbeat (%.0f s)", pulse_bpm_last, age / 60.0f, HEARTBEAT_S);
        else if (pulse_bpm_last > 0) snprintf(out + n, cap - n, "; his pulse %.0f bpm is %.0f min old (stale past %d): a pet is the purr", pulse_bpm_last, age / 60.0f, PULSE_STALE_MIN);
        else snprintf(out + n, cap - n, "; no pulse known: a pet is the purr");
    }
}

static void pet(void)
{
    if (wardrobe_worn()) {                  /* THE WRIST WARDROBE: a pet on a costume is its one act (the gel's happy eyes are under the costume) */
        Serial.printf("[%8lu] pet (worn: the act)\n", (unsigned long)millis());
        wardrobe_act();
        return;
    }
    Serial.printf("[%8lu] pet\n", (unsigned long)millis());
    pet_t = PET_S;
    lull_pet();                             /* THE LULL: a pet ends it (the happy eyes are the pet's own) */
    idle_pet();                             /* IDLE LIFE: nothing of its own for 10 s after a pet */
    bored_pet();                            /* THE BORED ROLL: attention is boredom's antidote (the desk's rule) */
    eye_kind = GEL_EYE_HAPPY;
    eye_kind_t = 0.9f;
    engage(EYE_HOLD_PET_S);
}

/* QUIET FROM / QUIET TO (MORE SETTINGS, 09-10 21:12; SETTINGS 5, NVS
 * `qfrom` / `qto`): the quiet hours rest.cpp (the night's onset, the
 * stir) and hours.cpp (the cool tint) read. The desk's grain `quiet=`
 * RULES while it says one (the box knows his nights; the precedence
 * stands); else his own whole hours from NVS -- "23-7" as the grain
 * would say it, and rest's own default when both are silent. */
static char quiet_own[12] = "";
static bool quiet_desk_rules(void) { const link_grain_t *g = link_grain(); return g->have && g->quiet[0] && strcmp(g->quiet, "-") != 0; }
static const char *quiet_hours_str(void)
{
    if (quiet_desk_rules()) return link_grain()->quiet;
    snprintf(quiet_own, sizeof quiet_own, "%d-%d", (int)store.quiet_from, (int)store.quiet_to);
    return quiet_own;
}

/* THE LEASE, EASY: the one toggle, from the hold, the knocks or serial */
static void lease_toggle(const char *how)
{
    bool here = house_holding();
    lease_pulse_until = millis() + LEASE_PULSE_MS;             /* the glass pulses once, now */
    if (here) { lease_glance_t = LEASE_GLANCE_S; engage(EYE_HOLD_PET_S); }   /* the gel glances up, now */
    LOG("lease: %s -> %s (the glass pulses%s)", how,
        here ? "release: the goodbye -- the sink starts now on the wrist's word" : "come: the gel rises on `ok held`",
        here ? ", the gel glances up" : "");
    link_whistle();                                            /* reads the house: come / release (the radio comes up for it) */
    if (here) house_release_now();                             /* after the whistle: the sink at once; `ok released` confirms, a `no ...` un-sinks */
}
void app_lease_toggle(const char *how) { lease_toggle(how); }  /* `come` / `release` over serial: the same road */

/* THE ALARM'S ONE ROAD (THE OPERATOR NIGHT T13, 2026-10-13): the ALARM row,
 * and the desk's `alarm HH:MM` / `alarm off` ask (link.cpp, alarmask.h),
 * set it the same way -- a minute sets it; -1 clears it and REMEMBERS the
 * hour (alarm_last) so the row's next tap turns it back on (an `off` on an
 * alarm already off keeps the hour remembered); NVS both; clockwork told
 * (it lights nothing before its minute, as ever). The console's own
 * `alarm` verb keeps its road (no hour remembered there, as before). */
void app_alarm_set(int minute, const char *how)
{
    if (minute < 0 && clockwork_alarm() >= 0) { store.alarm_last = (int16_t)clockwork_alarm(); store_save_alarm_last(); }
    store.alarm_min = (int16_t)(minute < 0 ? -1 : minute); store_save_alarm();
    clockwork_set_alarm(store.alarm_min, how);
}

/* THE SETTINGS PAGE (page.cpp; Khalid 2026-09-10 18:55: "a SETTINGS
 * MENU on the watch so I can do things myself"): the page hands over a
 * row and this does the real thing -- the module's own call, then NVS
 * only when the value changed. want: 1 on, 0 off, -1 toggle (a tap).
 * The same door serves `set <row> on|off` over serial. */
static char setting_line[160];
const char *app_setting(int row, int want, const char *how)
{
    setting_line[0] = 0;
    switch (row) {
    case SET_SLIME: {
        /* THE SLIME ROW (Khalid 09-10 21:08): an ACTION, not a switch -- the
         * lease's own road, whatever the LEASE switch says (that one governs
         * the hold and the knocks; this button always works). The page
         * CLOSES on the tap: the theatre needs the glass -- the goodbye and
         * the sink, or the rim's ripple and the rise, are drawn where the
         * page would be, and the gel is never drawn under a page; the
         * acknowledgement is the lease's own (the pulse, the glance up). */
        bool here = house_holding();
        if (page_up()) page_hide("the SLIME row");
        lease_toggle(how);
        snprintf(setting_line, sizeof setting_line, "settings: SLIME %s -> %s (%s) -- the lease's own road: %s", here ? "HERE" : "AWAY",
                 here ? "release" : "come", how,
                 here ? "the goodbye, the sink on the wrist's word, `release` to the box" : "`come` to the box, the rim ripples, the gel rises on `ok held`");
        break;
    }
    case SET_LEASE: {
        bool on = want < 0 ? !lease_hand : want != 0;
        bool changed = on != lease_hand;
        lease_hand = on; knock_armed_until = 0;
        if (changed) { store.lease_hand_off = !on; store_save_lease_hand(); }
        snprintf(setting_line, sizeof setting_line, "settings: LEASE %s (%s%s) -- the hold and the knocks %s claim or release; `come` / `release` over serial always may",
                 on ? "ON" : "OFF", how, changed ? ", stored" : "", on ? "may" : "may NOT");
        break;
    }
    case SET_QUIET: {
        bool on = want < 0 ? !wake_quiet_on() : want != 0;
        bool changed = on != wake_quiet_on();
        wake_quiet_set(on);
        if (changed) { store.quiet_off = !on; store_save_quiet(); }
        snprintf(setting_line, sizeof setting_line, "settings: QUIET TIME %s (%s%s) -- a raise in the quiet hours %s",
                 on ? "ON" : "OFF", how, changed ? ", stored" : "", on ? "lights only face-up (a turn in the sheets is not a raise)" : "lights the glass as by day");
        break;
    }
    case SET_FACE: {
        bool on = want < 0 ? !face_on() : want != 0;
        bool changed = on != face_on();
        face_set_on(on);
        if (changed) { store.face_off = !on; store_save_face(); }
        snprintf(setting_line, sizeof setting_line, "settings: FACE %s (%s%s) -- %s", on ? "ON" : "OFF", how, changed ? ", stored" : "",
                 on ? "the rings, the drop, the hour when empty" : "the black glass");
        break;
    }
    case SET_ALARM: {
        /* off: the hour is remembered (alarm_last) and the alarm cleared; on: the remembered hour set again (none = nothing: the rim dial sets it) */
        bool is_on = clockwork_alarm() >= 0;
        bool on = want < 0 ? !is_on : want != 0;
        if (on == is_on) {
            snprintf(setting_line, sizeof setting_line, "settings: ALARM %s already (%s)%s", on ? "ON" : "OFF", how,
                     on ? "" : "; a finger round the rim sets it");
        } else if (!on) {
            app_alarm_set(-1, how);                 /* THE ALARM'S ONE ROAD: cleared, the hour remembered */
            snprintf(setting_line, sizeof setting_line, "settings: ALARM OFF (%s, stored) -- %02d:%02d remembered: the next tap turns it back on", how,
                     store.alarm_last / 60, store.alarm_last % 60);
        } else if (store.alarm_last >= 0) {
            app_alarm_set(store.alarm_last, how);   /* the remembered hour, set again */
            snprintf(setting_line, sizeof setting_line, "settings: ALARM ON at %02d:%02d (%s, stored) -- the hour remembered", store.alarm_min / 60, store.alarm_min % 60, how);
        } else {
            snprintf(setting_line, sizeof setting_line, "settings: ALARM -- no hour remembered (%s): a finger round the rim sets one", how);
        }
        break;
    }
    case SET_LOCK: {
        bool on = want < 0 ? !gesture_locked() : want != 0;
        gesture_set_lock(on);                                  /* the event surfaces on the next tick as serial's does: logged there; RAM only (THE LOCK IS NEVER A TRAP: no NVS, a reboot unlocks) */
        snprintf(setting_line, sizeof setting_line, "settings: WATER LOCK %s (%s) -- %s", on ? "ON" : "OFF", how,
                 on ? "the glass ignores touch from the next tick; a double shake, a 3-s hold, `unlock` or a reboot frees it" : "the glass listens again");
        break;
    }
    case SET_LINK:
        link_now(how);
        snprintf(setting_line, sizeof setting_line, "settings: LINK NOW (%s) -- one dial, whatever the clocks", how);
        break;
    /* MORE SETTINGS (Khalid 2026-09-10 21:12: "just more from the watch -- bluetooth and wifi and what not") */
    case SET_WIFI: {
        /* HOLD = link_hold(true), the radio kept up (`link hold`'s road); AUTO = the budget (`link auto`). The
         * preference is NVS's: HOLD is applied again at every wake (link_dark ends a hold with the lit spell) */
        bool on = want < 0 ? !link_holding_radio() : want != 0;
        link_hold(on);
        bool changed = on != store.wifi_hold;
        if (changed) { store.wifi_hold = on; store_save_wifi_hold(); }
        snprintf(setting_line, sizeof setting_line, "settings: WIFI %s (%s%s) -- %s", on ? "HOLD" : "AUTO", how, changed ? ", stored" : "",
                 on ? "the radio kept up every lit spell (the cell pays; `link auto` or the row releases it)" : "the radio on its budget: a dial only for a reason");
        break;
    }
    case SET_NETWORK: case SET_ADDRESS: {
        char nl[160]; net_status(nl, sizeof nl);
        snprintf(setting_line, sizeof setting_line, "settings: %s (%s) -- read-only: %s", row == SET_NETWORK ? "NETWORK" : "ADDRESS", how, nl);
        break;
    }
    case SET_BT: {
        /* BLUETOOTH (ruling 58; THE PAIRING WINDOW 2026-10-06, his "turn on bluetooth then turn on pairing"): a tap goes
         * OFF -> ON -> PAIR, and a tap on the count RESTARTS the window (his taps of 14:54, fighting a cycle that went
         * PAIR -> OFF: the tap on a counting row means "turn on pairing", never "turn it all off"); the window closing by
         * itself returns the row to ON. OFF is a word, not a tap: `ble off` / `set bluetooth off` / the desk.
         * ON = the stack up whenever the WiFi radio is down (down for each dial, back after), OFF = never up (NVS);
         * PAIR = the 60 s window in which a stranger (the phone, the desk) may bond -- the row counts it down */
        int w = want;
        if (w < 0) w = !ble_enabled() ? 1 : 2;
        if (w == 2) {
            int left = ble_pair_open(0, how);
            if (left > 0) snprintf(setting_line, sizeof setting_line, "settings: BLUETOOTH PAIR (%s) -- the pairing window open for %d s: Settings > Bluetooth > slime-watch > Pair on the phone, or `slimectl watch ble pair` on the desk", how, left);
            else snprintf(setting_line, sizeof setting_line, "settings: BLUETOOTH PAIR (%s) -- the row is OFF: turn it ON first", how);
            break;
        }
        bool on = w != 0;
        bool changed = on != ble_enabled();
        if (!on && ble_pair_left_s() > 0) ble_pair_close(how);
        ble_set_enabled(on, how);                              /* logged and stored there; applied at once through the gate */
        snprintf(setting_line, sizeof setting_line, "settings: BLUETOOTH %s (%s%s) -- %s", on ? "ON" : "OFF", how, changed ? ", stored" : "",
                 on ? "up whenever the WiFi radio is down (never both); the phone's notices as wisps, the desk's lines over the air door -- a tap more opens the pairing window"
                    : "never up");
        break;
    }
    case SET_BRIGHT: {
        int step = want < 0 ? (store.bright + 1) % 3 : (want > 2 ? 2 : want);
        bool changed = step != store.bright;
        store.bright = (uint8_t)step;
        if (changed) store_save_bright();
        if (state == AWAKE) lcd.setBrightness(hours_level(bl_awake()));   /* at once, the hour's factor over it */
        snprintf(setting_line, sizeof setting_line, "settings: BRIGHTNESS %s (%s%s) -- the awake level %u (the hour's factor and the night's level over it)",
                 step == 0 ? "LOW" : step == 1 ? "MID" : "HIGH", how, changed ? ", stored" : "", (unsigned)BL_STEPS[step]);
        break;
    }
    case SET_SPELL: {
        bool sh = want < 0 ? !store.spell_short : want != 0;
        bool changed = sh != store.spell_short;
        store.spell_short = sh;
        if (changed) store_save_spell();
        snprintf(setting_line, sizeof setting_line, "settings: LIT SPELL %s (%s%s) -- the glass dims after %lu s and sleeps after %lu s of stillness%s",
                 sh ? "SHORT" : "LONG", how, changed ? ", stored" : "", (unsigned long)(spell_dim_ms() / 1000u), (unsigned long)(spell_sleep_ms() / 1000u),
                 sh ? " (ruling 2's numbers)" : " (the debugging week's; the cell pays)");
        break;
    }
    case SET_BUMPS: {
        /* THE BUMPS (bumps.h): the creature's touches through the motor, on or off; NVS `bumpsoff`, saved only on change */
        bool on = want < 0 ? store.bumps_off : want != 0;
        bool changed = on == store.bumps_off;
        store.bumps_off = !on;
        if (changed) store_save_bumps();
        if (!on) purr_armed = false;
        snprintf(setting_line, sizeof setting_line, "settings: BUMPS %s (%s%s) -- %s", on ? "ON" : "OFF", how, changed ? ", stored" : "",
                 on ? "the wisp's landing, the sink, the rise, the purr under a pet (never the night, the dock, a reminder)" : "the motor is the bench's only (`haptic buzz`); the creature never touches him");
        break;
    }
    case SET_LAMP:
        page_lamp(how);
        snprintf(setting_line, sizeof setting_line, "settings: LAMP (%s) -- the glass white for %.0f s, or until a tap", how, (double)LAMP_S);
        break;
    case SET_PULSE: {
        /* THE PULSE ROW (Khalid 2026-10-03 night, the watch solo: "trigger the heart rate checker for like 30
         * seconds so I can test it out"): an action -- a PPG_PULSE_S spot read by ppg.cpp, the sensor's own
         * AGC first; the row counts it down (page_in's pulse_left_s), the glass and the page are held up for
         * it, and when it lands the row shows the BPM, the motor bumps once (BUMPS permitting), the log says why */
        int left; bool settling;
        if (!ppg_present()) snprintf(setting_line, sizeof setting_line, "settings: PULSE (%s) -- no sensor (`bom ppg on`, and the MAX3010x on the second bus)", how);
        else if (ppg_spot_left(&left, &settling)) snprintf(setting_line, sizeof setting_line, "settings: PULSE (%s) -- already reading, %d s left", how, left);
        else if (ppg_spot_start_s(PPG_PULSE_S)) { pulse_row_read = true; snprintf(setting_line, sizeof setting_line, "settings: PULSE (%s) -- a %d s read once the wrist is still", how, PPG_PULSE_S); }   /* THE STILL SPOT (2026-10-08): the LEDs wait for the wrist; a read of the creature's own gives way to his (ppg.cpp) */
        else if (ppg_spot_busy()) snprintf(setting_line, sizeof setting_line, "settings: PULSE (%s) -- the raw stream has the sensor", how);
        else snprintf(setting_line, sizeof setting_line, "settings: PULSE (%s) -- NOT started (no heap for the buffer?)", how);
        break;
    }
    case SET_CLOCK: {
        bool h12 = want < 0 ? !store.h12 : want != 0;
        bool changed = h12 != store.h12;
        store.h12 = h12;
        if (changed) store_save_h12();
        glance_set_h12(h12);
        snprintf(setting_line, sizeof setting_line, "settings: CLOCK %s (%s%s) -- the face's hour, the glance's and the page's", h12 ? "12H" : "24H", how, changed ? ", stored" : "");
        break;
    }
    case SET_QFROM: case SET_QTO: {
        int8_t *h = row == SET_QFROM ? &store.quiet_from : &store.quiet_to;
        int v = want < 0 ? (*h + 1) % 24 : (want % 24);
        bool changed = v != *h;
        *h = (int8_t)v;
        if (changed) store_save_quiet_hours();
        snprintf(setting_line, sizeof setting_line, "settings: QUIET %s %02d:00 (%s%s) -- his quiet hours %d-%d; %s", row == SET_QFROM ? "FROM" : "TO", v, how,
                 changed ? ", stored" : "", (int)store.quiet_from, (int)store.quiet_to,
                 quiet_desk_rules() ? "the desk's grain rules for now (its `quiet=`); his own apply when it falls silent" : "in force (rest.cpp, hours.cpp)");
        break;
    }
    case SET_ABOUT:
        snprintf(setting_line, sizeof setting_line, "settings: ABOUT (%s) -- build %s, ip %s, heap %lu free, cell %u mV, up %lu s, steps %lu", how,
                 doctor_build_id(), net_ip()[0] ? net_ip() : "--", (unsigned long)ESP.getFreeHeap(), (unsigned)batt_mv(),
                 (unsigned long)(millis() / 1000u), (unsigned long)wrist_steps_today());
        break;
    /* SETTINGS 6, THE HIDDEN SWITCHES (THE OPERATOR NIGHT T15, 2026-10-12): each row calls exactly what its console verb calls */
    case SET_PAUTO: {
        /* PULSE AUTO = `ppg auto on|off` (bus.cpp): THE PULSE IN THE DAY's switch, NVS `ppgauto` */
        bool on = want < 0 ? store.ppg_auto_off : want != 0;
        bool changed = on == store.ppg_auto_off;
        store.ppg_auto_off = !on;
        if (changed) store_save_ppg_auto();
        snprintf(setting_line, sizeof setting_line, "settings: PULSE AUTO %s (%s%s) -- %s", on ? "ON" : "OFF", how, changed ? ", stored" : "",
                 on ? "a read of its own every 5 min at rest, never on the cable or the shelf" : "it reads only when asked (the PULSE row, `ppg spot`)");
        break;
    }
    case SET_DOCKHAND: {
        /* DOCK HAND = `dock hand on|off` (link.cpp: ONE BODY ACROSS THE HOUSES' switch, NVS `dockoff`; logged there) */
        bool on = want < 0 ? store.dockhand_off : want != 0;
        link_dockhand_set(on);
        snprintf(setting_line, sizeof setting_line, "settings: DOCK HAND %s (%s) -- %s", on ? "ON" : "OFF", how,
                 on ? "docking while holding releases him to the desk; undocking worn claims him back" : "the cable is a charger: a held slime stays through a dock");
        break;
    }
    case SET_EARGEAR: {
        /* EAR GEAR = `ear gear 80|160|240` (ear.cpp: NVS `eargear`; its line says what happened): a tap cycles LOW -> MID -> FULL -> LOW; `set eargear 160` names one, on = LOW, off = FULL */
        int mhz = want < 0 ? (store.ear_gear == EAR_GEAR_MHZ ? EAR_GEAR_MID_MHZ : store.ear_gear == EAR_GEAR_MID_MHZ ? EAR_GEAR_FULL_MHZ : EAR_GEAR_MHZ)
                : want == 0 ? EAR_GEAR_FULL_MHZ : want == 1 ? EAR_GEAR_MHZ : want;
        const char *l = ear_gear_set(mhz, how);
        snprintf(setting_line, sizeof setting_line, "settings: EAR GEAR %s (%s) -- %s", mhz == EAR_GEAR_MHZ ? "LOW" : mhz == EAR_GEAR_MID_MHZ ? "MID" : mhz == EAR_GEAR_FULL_MHZ ? "FULL" : "refused", how, l);
        break;
    }
    case SET_FORGET:
        /* FORGET = `ble forget` (every bond dropped; the phone must forget the watch too). From the page this is the SECOND tap: the first only
         * arms the row (SURE?, page.cpp's own 3 s) and hands main nothing (PAGE_ACT_KEEP); `set forget` over serial is the one tap `ble forget` is */
        ble_forget();
        snprintf(setting_line, sizeof setting_line, "settings: FORGET (%s) -- every bond dropped; the phone must forget slime-watch too (Settings > Bluetooth), then the pairing window again", how);
        break;
    /* THE BLUETOOTH PAGE (T16): its one row, and a tap on its disc */
    case SET_PAIRING: {
        /* exactly the BLUETOOTH row's third state: THE PAIRING WINDOW (ble_pair_open; the page's word turns OPEN while it runs, the row counts it) */
        int left = ble_pair_open(0, how);
        if (left > 0) snprintf(setting_line, sizeof setting_line, "settings: PAIRING OPEN (%s) -- the pairing window open for %d s: Settings > Bluetooth > slime-watch > Pair on the phone, or `slimectl watch ble pair` on the desk", how, left);
        else snprintf(setting_line, sizeof setting_line, "settings: PAIRING (%s) -- the BLUETOOTH row is OFF: turn it ON first (SETTINGS 3)", how);
        break;
    }
    case SET_BLE:
        snprintf(setting_line, sizeof setting_line, "settings: BLUETOOTH page (%s) -- phone %s, air door %s, ear %s, pairing %s, notices %s", how,
                 ble_phone_ready() ? "NEAR" : ble_bonded() ? "AWAY" : "NONE", ble_desk_open() ? "OPEN" : "SHUT",
                 ble_keeps_dark() ? "ON" : !ble_enabled() ? "OFF" : "NO BOND", ble_pair_left_s() > 0 ? "OPEN" : "SHUT", wisp_waiting() > 0 ? "SOME" : "NONE");
        break;
    default:
        snprintf(setting_line, sizeof setting_line, "settings: no such row %d", row);
        break;
    }
    LOG("%s", setting_line);
    return setting_line;
}

/* THE SWIPES: a drag's travel along the gel's own floor, -1..1 (+ = his
 * right: the body's right on the face is (cos phi, -sin phi),
 * gel_compose's slide axis -- the stroke's own projection) */
static float swipe_along(int dx, int dy)
{
    float len = sqrtf((float)(dx * dx + dy * dy));
    if (len <= 0) return 0;
    return clampf(((float)dx * cosf(bowl_phi) - (float)dy * sinf(bowl_phi)) / len, -1.0f, 1.0f);
}

/* THE NOTICE CLEARED FROM THE WRIST (T17): the lamp's hold (LAMP_HOLD_MS
 * or more, or a long press) landing on an OPEN notice card is the clear,
 * not the lamp -- the page closes the card (page_note_hold), the phone is
 * asked to take the notice off its shade (ancs_clear: the NEGATIVE action
 * only -- dismissed; a ringing call declined; ruling 139), and the wisp,
 * if it still hovers, is eaten into him the feed's way (read, not lost:
 * the ledger keeps it). A desk's or a box's notice has nothing on the
 * phone: the card closes, the wisp is eaten, the line says so. true =
 * the hold was the card's. */
static bool page_note_clear_hold(const char *how)
{
    int row = page_note_hold();
    if (row < 0) return false;
    const wisp_note_t *lg = NULL; int n = wisp_ledger(&lg);
    const char *app = row < n ? lg[row].app : "";
    bool asked = app[0] && ancs_clear(app);
    int bxx, byy, bw, bh; gel_face_box(&bxx, &byy, &bw, &bh);
    bool ate = app[0] && wisp_eat_app(app, bxx + bw / 2, byy + bh / 2);
    LOG("page: %s on the open notice (%s): %s%s", how, app[0] ? app : "?", asked ? "the phone asked to clear it" : "nothing to clear on the phone", ate ? "; its wisp eaten" : "");
    return true;
}

/* THE WATCH PAGE takes every TOUCH gesture while it is up (a tap or a
 * drag down closes it; a drag up re-arms it; the rest is the page's,
 * logged and nothing else). Shakes, the lock and the phantoms are the
 * body's, page or no page. true = taken. */
static bool page_touch(const gesture_event_t &e)
{
    switch (e.kind) {
    case GES_TAP: {
        wrist_count_tap();
        int row = page_tap(e.x, e.y);                          /* THE SETTINGS PAGE: on a row, the row's (the page restarts its 8 s); else the close as ever */
        if (row >= PAGE_NOTE_0) { /* THE NOTICE OPENED: the page's own (it opened the notice, or keeps it; its line says so) -- no module has a part */ }
        else if (row >= 0) app_setting(row, -1, "the page");
        else if (row == PAGE_ACT_KEEP) { /* SETTINGS 6's FORGET armed (T15): the page's own, its line says SURE? -- the second tap is the row's */ }
        else page_hide("a tap");
        return true;
    }
    case GES_DOUBLE_TAP: wrist_count_tap(); wrist_count_tap(); gesture_unshow(); page_hide("a double tap"); return true;
    case GES_FEED:       gesture_unshow(); page_hide("a drag down"); return true;
    case GES_DRAG_UP:    page_next("a drag up"); return true;      /* THE PAGE GROWS: the next page (the tool page, his week, the notices, settings 1-5, about) */
    case GES_SWIPE:      /* THE SWIPES (3A): a flick along his wall turns the page -- toward his left the next, toward his right the previous (the flick's way read on the wall he sits on, as the stroke's lean is) */
        if (swipe_along(e.dx, e.dy) < 0) page_next("a swipe left");
        else page_prev("a swipe right");
        return true;
    case GES_LONG_PRESS: gesture_unshow(); if (!page_note_clear_hold("a long press")) page_lamp("a long press"); return true;   /* THE LAMP (B17): the glass white for 20 s, on any page -- but on an open notice card the hold is THE CLEAR (T17) */
    case GES_LEASE_HOLD:                                             /* THE LAMP keeps its 1.5 s on the page: the detector names a plain hold of 0.5-1.9 s the lease's (THE HOLD'S WINDOW, WIDER), but on the page there is no lease -- a hold of LAMP_HOLD_MS or more is the lamp as it always was, under it the page's nothing */
        gesture_unshow();
        if (e.n >= LAMP_HOLD_MS) { if (!page_note_clear_hold("a hold")) page_lamp("a long press"); }   /* T17: on an open notice card the lamp's hold clears the notice instead */
        else LOG("page: a hold of %d ms on the page: the page's, not the creature's (%d ms or more is the lamp; a tap or a drag down closes it)", e.n, (int)LAMP_HOLD_MS);
        return true;
    case GES_TRIPLE_TAP: case GES_STROKE: case GES_HOLD: case GES_TAP_HOLD:
    case GES_TAPS_DROPPED:
    case GES_ARC_MOVE: case GES_ARC: case GES_PEEK_DOWN: case GES_PEEK_UP:   /* the rim's dial and the hand over the bowl are the creature's: nothing on the page */
    case GES_RHYTHM_START: case GES_RHYTHM:                                   /* PLAY: THE RHYTHM is the creature's too: nothing on the page */
    case GES_TICKLE:                                                          /* THE TICKLE (C4): the creature's: nothing on the page */
        gesture_unshow();
        if (e.kind != GES_ARC_MOVE && e.kind != GES_PEEK_UP)
            LOG("page: %s on the page: the page's, not the creature's (a tap or a drag down closes it)", gesture_name(e.kind));
        return true;
    default: return false;
    }
}

/* THE GESTURES (gesture.cpp, stage 4+3): the detector reads the glass
 * and the IMU's latest sample and hands back events; this switch is the
 * one place they become the body's own reactions, a log line and a
 * sense (dropped when not linked: the day log carries the count). A
 * single tap is the pet, played once the double-tap window has closed;
 * two taps are a drink, three the knocks (come; armed while he is here,
 * the hold after is release); a plain hold of about a second is THE
 * LEASE's toggle; a tap then a hold is the stopwatch; a finger drawn
 * across the glass is the STROKE (a long pet), drawn down it from the
 * top the FEED (the meal), drawn UP it THE WATCH PAGE -- all read on
 * THE WALL he sits on (THE SWIPES, 2026-09-23: gesture_set_wall) --
 * and FLICKED along his wall THE SWIPE: the page's turn with a page up,
 * THE SHOVE on the bare glass while he is here. Under the WATER
 * LOCK the glass is inert; on the empty glass the knocks, the lease's
 * hold and the page's drag mean something, nothing else does. The
 * README's GESTURES table is the one list of every gesture. */
/* THE TOUCH POLL (the optimization pass, pass 2): the controller was read
 * every pass of the loop -- a 7-byte I2C transaction ~1000 times a second
 * with no finger near the glass, most of the idle loop's bus traffic.
 * Now it is read every pass while a finger is on the glass or was within
 * TOUCH_HOLD_MS (the release and a drag stay exact), and every
 * TOUCH_IDLE_MS otherwise (125 Hz: a tap's 30 ms floor is still four
 * polls; the first sample of a touch lands within 8 ms). Between polls
 * the detector is handed the last reading. No reliance on the INT pin's
 * pulse width, so nothing the glass does can be missed. */
static const uint32_t TOUCH_IDLE_MS = 8, TOUCH_HOLD_MS = 200;
static uint32_t touch_poll_ms = 0, touch_down_ms = 0;
static touch_t  touch_last;
static void touch_tick(void)
{
    uint32_t now = millis();
    bool hot = touch_was_down || now - touch_down_ms < TOUCH_HOLD_MS;
    if (hot || now - touch_poll_ms >= TOUCH_IDLE_MS) {
        touch_poll_ms = now;
        touch_read(&touch_last);
        if (touch_last.down) touch_down_ms = now;
    }
    touch_t t = touch_last;
    if (t.down && gesture_touch_live()) {
        last_touch_ms = now;                  /* a hand keeps it awake; water on the glass does not */
        if (!touch_was_down) {
            engage(EYE_HOLD_PET_S);           /* a finger on the glass is attention */
            rest_touch();                     /* A TOUCH ENDS SLEEP (rest.cpp): a hand on the glass is a person awake */
        }
    }
    touch_was_down = t.down;
    gesture_event_t ev[4];
    gesture_set_wall(wall);                   /* THE GEL'S FRAME (THE SWIPES): every drag is judged on the wall he sits on -- the feed down onto him, the page's drag up off him, the swipe and the stroke along his wall */
    gesture_set_warm(sky_hot());              /* A DRINK ON A HOT DAY (A9): the sky's one word the drink reads -- the settle deeper, the hair the rain's */
    int n = gesture_tick(now, &acc, &t, state != DARK, ev, 4);
    if (n > 4) n = 4;
    if (n) cramp_touch(now);                  /* THE SMALL HOUSE: any touch gesture is attention (the wrist's bid's ignored clock restarts; a raise is not one) */
    for (int i = 0; i < n; i++) {
        const gesture_event_t &e = ev[i];
        if (page_up() && page_touch(e)) continue;     /* THE WATCH PAGE: the touch gestures are the page's while it is up */
        switch (e.kind) {
        case GES_TAP:
            if (word_tap()) {                                /* the tap took the word: not a pet (word.cpp) */
                wrist_count_tap();
                char q[QUESTION_ECHO_MAX + 1];
                if (question_no(now, q, sizeof q)) link_sense("wrist:no", q);   /* THE WORD'S QUESTION: the dismissal inside the window is his no */
                break;
            }
            if (clockwork_tap(now)) { wrist_count_tap(); break; }   /* CLOCKWORK: the stopwatch's stop, a ring's dismissal: not a pet */
            if (!house_holding()) { wrist_count_tap(); if (face_trace_near(e.x, e.y)) trace_stir = true; break; }   /* THE HOUSE: a tap on the empty glass does nothing (his day still counts it) -- THE TRACE STIRRED (C14): near the trace, the residue shivers once (no gel, no rise) */
            {   /* PLAY: THE BEAD (play.cpp): the watch flat and still 3 s, then a tap BESIDE him places the bead (2: the game starts, the sense goes once); in an open game another bead (1); on him, or too early, not the bead's -- the far tap or the pet as ever */
                int bxx, byy, bw, bh; gel_face_box(&bxx, &byy, &bw, &bh);   /* on him = within the far tap's radius OR inside his painted box (the dome is 156 px wide: a tap 65 px out would be eaten as it landed) */
                bool on_gel = gel_dist(e.x, e.y) <= FAR_TAP_PX || (bw > 0 && e.x >= bxx && e.x < bxx + bw && e.y >= byy && e.y < byy + bh);
                int r = play_tap(now, e.x, e.y, on_gel);
                if (r) {
                    wrist_count_tap();
                    engage(EYE_HOLD_PET_S);
                    idle_pet();
                    cramp_play(now);                     /* THE SMALL HOUSE: a game is an outlet */
                    if (r == 2) link_sense("wrist:play", "bead");
                    break;
                }
            }
            if (far_tap(e.x, e.y)) { wrist_count_tap(); break; }   /* THE FAR TAP: off him, the gaze turns that way -- not a pet, no sense */
            {   /* THE PET'S PLACE (petplace.c; C1): the tap's third on him, in his frame -- the glance at the finger first (the far tap's look, 0.4 s), then the press by its place (pet() below; under a costume the act is the costume's: the crown) */
                int bxx, byy, bw, bh; gel_face_box(&bxx, &byy, &bw, &bh);
                petplace_t crown = { PETPLACE_CROWN, 0, 0, 0 };
                pet_place = wardrobe_worn() ? crown : petplace_of(e.x, e.y, wall, bxx, byy, bw, bh);
                pet_press_k = petplace_press_k(pet_place.place);
                pet_place_t = 0;
                if (!wardrobe_worn()) look_at(e.x, e.y, PETPLACE_GLANCE_S);
                if (pet_place.place != PETPLACE_CROWN)
                    Serial.printf("[%8lu] pet's place: his %s%s (depth %d, along %+d)\n", (unsigned long)millis(), petplace_name(pet_place.place),
                                  pet_place.side ? (pet_place.side > 0 ? ", his right" : ", his left") : ", dead centre", pet_place.depth, pet_place.along);
            }
            pet();
            cramp_pet(now);                                  /* THE SMALL HOUSE: a pet spends the cramp; answers an open nudge / bid */
            bump(BUMP_PET);                                  /* THE BUMPS: the purr under a pet, ridden on the next inhale (A8) */
            wrist_count_tap();
            link_sense("wrist:pet", "petted");
            {   /* THE WORD'S QUESTION (E45): a pet inside the question's window is his yes -- the pet still played as a pet */
                char q[QUESTION_ECHO_MAX + 1];
                if (question_yes(now, q, sizeof q)) link_sense("wrist:yes", q);
            }
            break;
        case GES_DOUBLE_TAP:                  /* a drink: one contented settle (gesture_squash) and a soft wobble */
            if (!house_holding()) { gesture_unshow(); LOG("gesture: double tap on the empty glass: nothing (a hold of about a second brings him)"); break; }
            LOG("gesture: double tap at %d,%d: water%s", e.x, e.y, sky_hot() ? " (a hot day: welcome -- the settle deeper, the eyes longer, the hair the rain's)" : "");
            wrist_count_tap(); wrist_count_tap();
            wrist_count_mark(false);
            spring2.kick(-0.9f);
            eye_kind = GEL_EYE_CONTENT;
            eye_kind_t = 1.2f * (sky_hot() ? HOT_DRINK_EYES_K : 1.0f);   /* A DRINK ON A HOT DAY (A9): the content eyes held longer (1.8 s) */
            engage(EYE_HOLD_PET_S);
            link_sense("wrist:water", "drank");
            break;
        case GES_TRIPLE_TAP:
            /* THREE KNOCKS (Khalid 23:40: "three knocks brings the slime over, as if
             * I'm bringing him; three knocks and a hold of ~1 s makes him go away"):
             * on an empty glass the knock is `come`; while he is here it ARMS the
             * hold that follows (KNOCK_ARM_MS), which is `release` */
            if (!lease_hand) { LOG("gesture: three knocks at %d,%d: the lease by hand is OFF (SETTINGS) -- nothing", e.x, e.y); break; }
            if (!house_holding()) {
                LOG("gesture: three knocks at %d,%d: bringing him", e.x, e.y);
                lease_toggle("three knocks");
            } else {
                knock_armed_until = millis() + KNOCK_ARM_MS;
                LOG("gesture: three knocks: hold the glass to send him away (a plain hold of about a second does it alone)");
            }
            break;
        case GES_SWIPE: {
            /* THE SHOVE (THE SWIPES 3B): a flick along his wall on the bare
             * glass, HOLDING -- play: the skid the flick's way, the push's
             * squash, back to his place with one settle, eyes wide then happy */
            if (!house_holding()) { LOG("gesture: swipe %+d,%+d (%d ms) on the empty glass: nothing", e.dx, e.dy, e.n); break; }
            float along = swipe_along(e.dx, e.dy);
            bool again = shove_ms && now - shove_ms < (uint32_t)(SHOVE_AGAIN_S * 1000.0f);
            shove_dir = along < 0 ? -1.0f : 1.0f;
            shove_t = 0; shove_eyes_wide = true;
            shove_ms = now ? now : 1;
            eye_kind = GEL_EYE_WIDE; eye_kind_t = SHOVE_WIDE_S;
            engage(EYE_HOLD_PET_S);
            idle_pet();                             /* IDLE LIFE: nothing of its own for 10 s after play */
            cramp_play(now);                        /* THE SMALL HOUSE: the shove is an outlet */
            LOG("gesture: swipe %+d,%+d from %d,%d in %d ms: the shove on the %s wall, %.0f px toward his %s%s", e.dx, e.dy, e.x, e.y, e.n,
                GEL_WALL_NAMES[wall], SHOVE_PX, shove_dir > 0 ? "right" : "left", again ? " (again: the same shove, no bigger)" : "");
            if (!shove_play_ms || now - shove_play_ms >= SHOVE_PLAY_GAP_MS) { shove_play_ms = now ? now : 1; link_sense("wrist:play", "shoved"); }
            break;
        }
        case GES_STROKE: {
            /* THE STROKE: a long pet. The lean follows the finger along the
             * gel's own floor: the body's right on the face is (cos phi, -sin phi)
             * (gel_compose's slide), so the travel is projected on that */
            if (!house_holding()) { gesture_unshow(); LOG("gesture: stroke on the empty glass: nothing"); break; }
            float along = swipe_along(e.dx, e.dy);
            stroke_along = along;
            LOG("gesture: stroke %+d,%+d from %d,%d (%d samples): a long pet, leaning %.2f", e.dx, e.dy, e.x, e.y, e.n, stroke_along);
            idle_pet();                             /* IDLE LIFE: nothing of its own for 10 s after a pet */
            eye_kind = GEL_EYE_CONTENT;
            eye_kind_t = STROKE_EYE_S * grain_knobs()->linger;   /* THE GRAIN: content eyes linger longer when regard is warm */
            engage(eye_kind_t);
            link_sense("wrist:stroked", "stroked");
            {   /* THE LULL (lull.c, watch-night C11): a slow stroke (the finger down LULL_STROKE_MS or more) counts toward the three; the third inside a minute arms it, to begin after this stroke's own content face (never fights it) */
                int lr = lull_stroke(now, gesture_stroke_ms(), eye_kind_t);
                if (lr == LULL_STROKE_ARMED) LOG("lull: the third slow stroke (%lu ms) inside the minute -- the lull after this face's %.1f s", (unsigned long)gesture_stroke_ms(), eye_kind_t);
                else if (lr == LULL_STROKE_COUNTED) LOG("lull: a slow stroke (%lu ms), %d of %d", (unsigned long)gesture_stroke_ms(), lull_count(), LULL_STROKES);
                else if (lr == LULL_STROKE_RENEWED) LOG("lull: a slow stroke inside it -- its %.0f s again", LULL_S);
            }
            break;
        }
        case GES_TICKLE: {
            /* THE TICKLE (C4): one finger scrubbed back and forth ON him -- the
             * whole scrub inside his painted box -- is the wriggle (animate),
             * the squint then the happy eyes, a lean away from the finger.
             * Beside him nothing; on the empty glass nothing; asleep nothing */
            if (!house_holding()) { LOG("gesture: tickle (%d turns) on the empty glass: nothing", e.n); break; }
            if (rest && rest->asleep) { LOG("gesture: tickle (%d turns) on a sleeper: nothing (the night is his own)", e.n); break; }
            int bxx, byy, bw, bh; gel_face_box(&bxx, &byy, &bw, &bh);
            bool on_him = bw > 0 && e.x >= bxx && e.y >= byy && e.x + e.dx <= bxx + bw && e.y + e.dy <= byy + bh;
            if (!on_him) { LOG("gesture: tickle (%d turns) in %d,%d %dx%d beside him: a scrub, nothing", e.n, e.x, e.y, e.dx, e.dy); break; }
            petplace_t at = petplace_of(e.x + e.dx / 2, e.y + e.dy / 2, wall, bxx, byy, bw, bh);   /* THE PET'S PLACE's side: the scrub's centre on his right or his left, in his frame (under a costume no side: the act is the costume's) */
            tickle_side = wardrobe_worn() ? 0.0f : (float)-at.side;
            tickle_t = 0; tickle_squint = true;
            eye_kind = GEL_EYE_SQUINT; eye_kind_t = TICKLE_S;
            engage(EYE_HOLD_PET_S);
            idle_pet();                             /* IDLE LIFE: nothing of its own for 10 s after play */
            cramp_play(now);                        /* THE SMALL HOUSE: a tickle is an outlet */
            LOG("gesture: tickle (%d turns) in %d,%d %dx%d on him: the wriggle%s", e.n, e.x, e.y, e.dx, e.dy,
                tickle_side > 0 ? ", leaning to his right" : tickle_side < 0 ? ", leaning to his left" : "");
            if (!tickle_play_ms || now - tickle_play_ms >= PLAY_GAP_MS) { tickle_play_ms = now ? now : 1; link_sense("wrist:play", "tickled"); }
            break;
        }
        case GES_FEED:
            /* THE FEED: a finger drawn down the glass from the top, as if from
             * above him: the meal (the lump is gesture_squash's, 60 s) */
            if (!house_holding()) { gesture_unshow(); LOG("gesture: feed on the empty glass: nothing"); break; }
            if (wisp_feed_at(e.x + e.dx, e.y + e.dy)) {   /* THE WISP EATEN (E41, wisp.cpp): the feed's end on a hovering wisp -- the notice is the meal (the small lump is wisp_lump's), not his: no mark, no sense, no meal lump */
                gesture_unlump();
                LOG("gesture: feed ending at %d,%d on a wisp: eaten (the small lump, %lu so far)", e.x + e.dx, e.y + e.dy, (unsigned long)wisp_eaten());
                eye_kind = GEL_EYE_HAPPY; eye_kind_t = WISP_EAT_EYE_S; engage(EYE_HOLD_PET_S);
                break;
            }
            LOG("gesture: feed, %d,%d down %+d: a meal", e.x, e.y, e.dy);
            wrist_count_mark(true);
            eye_kind = GEL_EYE_HAPPY;
            eye_kind_t = FEED_EYE_S;
            engage(EYE_HOLD_PET_S);
            link_sense("wrist:meal", "ate");
            break;
        case GES_DRAG_UP:
            /* THE WATCH PAGE: a finger drawn up the glass turns it over to his tool page (page.cpp) */
            LOG("gesture: drag up %+d,%+d from %d,%d: the watch page", e.dx, e.dy, e.x, e.y);
            gesture_unshow();
            page_show("a drag up");
            break;
        case GES_LEASE_HOLD:
            /* THE LEASE, EASY: a plain hold of about a second -- come on the
             * empty glass, release while he is here (the knocks' armed hold
             * lands here too: the same release) */
            knock_armed_until = 0;
            if (!lease_hand) { LOG("gesture: a hold of %d ms: the lease by hand is OFF (SETTINGS) -- nothing", e.n); break; }
            {
                char how[40];
                snprintf(how, sizeof how, "a hold of %d ms", e.n);
                lease_toggle(how);
            }
            break;
        case GES_TAP_HOLD:
            /* CLOCKWORK: a tap then a hold (the double-tap-and-hold) starts
             * the stopwatch (the breath held), or sets the timer from the
             * final time showing */
            if (clockwork_hold(e.n, now)) { idle_pet(); engage(EYE_HOLD_PET_S); }
            else LOG("gesture: a tap then a hold of %d ms: nothing now (the stopwatch needs him here and no ring; a timer already running keeps it)", e.n);
            break;                            /* (09-10 21:08: this break was missing -- the tap-hold fell through into the dial below with its ms as degrees) */
        case GES_ARC_MOVE:                    /* THE RIM AS THE DIAL (clockwork.cpp): the finger round the rim turns the alarm's hour -- or, while the stopwatch's final time shows, THE TIMER FROM THE RIM; the gel watches it go round */
            clockwork_dial(e.n, e.dx, false, clock_local_minute(), now);
            if (house_holding()) { look_at(e.x, e.y, 0.4f); engage(1.0f); }
            break;
        case GES_ARC: {                       /* THE RIM AS THE DIAL: the lift sets the alarm (or clears the one just set; NVS as the console's `alarm`), or the timer from the final time (clockwork logs it) */
            int r = clockwork_dial(e.n, e.dx, true, clock_local_minute(), now);
            if (r == 1) { store.alarm_min = (int16_t)clockwork_alarm(); store_save_alarm(); }
            else if (r == 0) LOG("gesture: an arc round the rim to %d deg (%+d swept): %s", e.n, e.dx, clock_local_minute() < 0 ? "no clock to dial" : "nothing");
            break;
        }
        case GES_PEEK_DOWN:                   /* PEEKABOO: a plain hold past 2 s, HOLDING, no knock armed -- the hand over the bowl */
            if (house_holding() && (int32_t)(knock_armed_until - millis()) <= 0) {
                peek_hiding = true;
                engage(EYE_HOLD_PET_S);       /* the eyes stay up: they go last, with the crown */
                LOG("gesture: a hand over the bowl (held %d ms): peekaboo -- the gel hides", e.n);
            }
            break;
        case GES_PEEK_UP:                     /* PEEKABOO: the lift is the boo */
            if (peek_hiding) {
                peek_hiding = false;
                peek_took_press = true;       /* the long press this lift also yields is the boo's, not a whistle */
                if (house_holding()) {
                    peek_pop_t = 0;
                    eye_kind = GEL_EYE_WIDE; eye_kind_t = PEEK_WIDE_S;
                    engage(EYE_HOLD_PET_S);
                    spring2.kick(PET_BOUNCE);
                    idle_pet();
                    cramp_play(now);              /* THE SMALL HOUSE: the boo is an outlet */
                    LOG("gesture: boo (held %d ms): the gel pops back up", e.n);
                    if (!peek_play_ms || now - peek_play_ms >= PEEK_PLAY_GAP_MS) { peek_play_ms = now ? now : 1; link_sense("wrist:play", "peekaboo"); }
                }
            }
            break;
        case GES_LONG_PRESS:
        case GES_HOLD:
            if (peek_took_press) { peek_took_press = false; break; }   /* PEEKABOO took this lift */
            /* the hold after three knocks, while he is here: sending him away (release) */
            if (house_holding() && (int32_t)(knock_armed_until - millis()) > 0) {
                knock_armed_until = 0;
                LOG("touch held %d ms after three knocks: sending him away", e.n);
                lease_toggle("three knocks then a hold");
            }
            else LOG("touch held %d ms at %d,%d: nothing (a hold of about a second toggles the lease; a tap then a hold is the stopwatch)", e.n, e.x, e.y);
            break;
        case GES_RHYTHM_START:                /* PLAY: THE RHYTHM (play.cpp; watch-ideas F51): a fourth tap in the knocks' window un-arms them -- while he is here; on the empty glass the knocks were `come` and nobody is here to echo */
            rhythm_for_gel = house_holding();
            if (rhythm_for_gel) { knock_armed_until = 0; LOG("gesture: a fourth tap at %d,%d in the knocks' window: the knocks un-armed, a rhythm opens (up to %d taps; the gap is gesture.cpp's RHYTHM_GAP_MS)", e.x, e.y, GES_RHYTHM_MAX); }
            else LOG("gesture: a fourth tap at %d,%d: the knocks were `come` -- the rhythm has nobody to echo it", e.x, e.y);
            break;
        case GES_RHYTHM: {                    /* PLAY: THE RHYTHM closed (four to six taps): the gaps go to the arbiter, the echo a second after the last tap */
            uint32_t gaps[GES_RHYTHM_MAX - 1];
            int ng = gesture_rhythm_gaps(gaps, GES_RHYTHM_MAX - 1);
            if (!rhythm_for_gel || !house_holding()) { LOG("gesture: a rhythm of %d taps: nobody to echo it (the empty glass)", e.n); break; }
            if (play_rhythm(now, gaps, ng, (uint32_t)(e.dx > 0 ? e.dx : 0))) {
                idle_pet();
                engage(EYE_HOLD_PET_S);
                if (play_rhythm_sense_due()) link_sense("wrist:play", "rhythm");
            }
            if (play_log_new()) { LOG("%s", play_log()); play_log_taken(); }
            break;
        }
        case GES_SHAKE:                       /* "I'm up": the minute is flagged UP, a sleep span ends here */
            LOG("gesture: shake: up");
            wrist_count_shake();
            wrist_count_up();
            rest_shake();                     /* THE SHAKEN SLEEPER (rest.cpp): asleep or stirring, the rising -- never a stir */
            engage(EYE_HOLD_WAKE_S);
            link_sense("wrist:up", "up and about");
            break;
        case GES_DOUBLE_SHAKE:
            LOG("gesture: double shake%s", state == DARK ? " (dark: not the lock)" : "");
            wrist_count_shake();
            break;
        case GES_LOCKED:
        case GES_UNLOCKED: {
            bool on = e.kind == GES_LOCKED;   /* THE LOCK IS NEVER A TRAP (gesture.cpp): RAM only, whoever set it -- never NVS, a reboot unlocks; n 2 = THE WET GLASS LOCKED ITSELF, n 3 = the 3-s hold */
            if (e.n == 2) LOG("water lock %s", on ? "ON (the glass is wet); a double shake or a 3-s hold unlocks it sooner" : "off (the glass dried)");
            else if (e.n == 3) LOG("water lock off (a long hold): the glass listens again");
            else LOG("water lock %s (%s): the glass %s", on ? "ON" : "off", e.n ? "a double shake" : "serial or the page",
                     on ? "ignores touch; a drop shows at the rim -- a double shake, a 3-s hold or a reboot frees it" : "listens again");
            if (e.n == 2 && !on && shakeoff_trigger(state != DARK, house_holding(), rest && rest->dozing)) { eye_kind = GEL_EYE_SQUINT; eye_kind_t = SHAKEOFF_SQUINT_S; LOG("shake-off: rained on, dried -- one %.0f Hz jiggle %.1f s, the squint %.1f s", SHAKEOFF_HZ, SHAKEOFF_S, SHAKEOFF_SQUINT_S); }   /* THE SHAKE-OFF (shakeoff.c, watch-night C16): the dried edge only -- never a hand's unlock, the double shake or the hold; lit, holding, awake; the jiggle is folded into draw() beside cold_a2 */
            engage(2.0f);
            link_sense(on ? "wrist:locked" : "wrist:unlocked", on ? "glass locked for water" : "glass unlocked");
            break;
        }
        case GES_PHANTOM:
            LOG("gesture: %d touches in a second at %d,%d: a wet glass, dropped (deaf 1.5 s)", e.n, e.x, e.y);
            break;
        case GES_TOUCH_LOCKED:
            LOG("touch at %d,%d under the water lock: inert", e.x, e.y);
            break;
        case GES_TAPS_DROPPED:
            LOG("gesture: %d tap%s dropped (a hold or a burst followed)", e.n, e.n == 1 ? "" : "s");
            break;
        default: break;
        }
    }
}

/* --- the body ------------------------------------------------------------- */

static void animate(float dt)
{
    clock_s += dt;
    const float heavy_k = sky_heavy_k(sky_heavy());   /* THE SKY'S WEIGHT: 1.0 dry, 11/9 with rain or storm a minute in */
    spring_c_k = heavy_k;
    spring1.step(dt);
    spring2.step(dt);
    if (lease_glance_t > 0) { lease_glance_t -= dt; if (lease_glance_t < 0) lease_glance_t = 0; }   /* THE LEASE's glance up, easing off */
    shakeoff_tick(dt);                                                                              /* THE SHAKE-OFF (shakeoff.c): the dried glass's jiggle, read back in draw() */
    /* THE RESIDUAL, worn: the lean eases toward the pull's part along
     * the wall (not in the air: a body in flight leans nowhere); the
     * tilt -- the residual's sine, eased at TILT_EASE x the grain's own
     * slide knob -- is the slosh (a2) and the slide (px along the wall)
     * in draw(). THE ITCH (rest.cpp): a long sit rests a few degrees off
     * the pull, the side shifting -- a few px of slide, never a wall. */
    const float lean_cap = LEAN_CAP * (braced() ? BRACE_LEAN_K : 1.0f);   /* THE BRACED FALL: the cap halved for the minute */
    float lean_t = clampf(g_along * LEAN_G, -lean_cap, lean_cap);
    if (fall_phase == FALL_PEEL) lean_t = fall_sgn * PEEL_LEAN;
    if (fall_phase == FALL_FLIGHT) lean_t = 0;        /* a drop hangs straight */
    lean += (lean_t - lean) * clampf((fall_phase == FALL_PEEL ? 12.0f : LEAN_EASE / heavy_k) * dt, 0, 1);   /* THE SKY'S WEIGHT: a fuller gel leans slower (the peel keeps its own pace) */
    tilt_target = (phi_valid && fall_phase != FALL_PEEL && fall_phase != FALL_FLIGHT)
                  ? sinf(wrap_pi(phi_target + itch_rad - bowl_phi)) : 0.0f;
    tilt_k += (tilt_target - tilt_k) * clampf(TILT_EASE * grain_knobs()->slide / heavy_k * dt, 0, 1);   /* THE GRAIN: the tilt's ease, its own (+-15 %); THE SKY'S WEIGHT slows it the same */
    switch (fall_phase) {
    case FALL_PEEL: {
        /* the puddle: spread along the wall, creeping a little toward
         * the pull (the slide along the wall: the rim's degrees as px) */
        fall_t += dt;
        float t = clampf(fall_t / PEEL_S, 0, 1);
        fall_squash = PEEL_SQUASH * smooth(t);
        float creep = fall_sgn * PEEL_CREEP_DEG * (float)M_PI / 180.0f * GEL_FLOOR_DROP;
        slide += (creep - slide) * clampf(10.0f * dt, 0, 1);
        if (t >= 1.0f) fall_swap();
        break;
    }
    case FALL_FLIGHT:
        /* down the new wall's gravity under FALL_G, the slide running to
         * 0 at the lateral speed the swap gave it: the arc to the wall */
        lift_vel += FALL_G * dt;
        lift -= lift_vel * dt;
        slide += slide_vel * dt;
        if (slide_vel != 0 && (slide_vel > 0) == (slide > 0)) { slide = 0; slide_vel = 0; }   /* past the axis: it is there */
        fall_squash += (FLIGHT_SQUASH - fall_squash) * clampf(6.0f * dt, 0, 1);
        {
            float along = GEL_FLOOR_DROP - lift;
            if (lift <= 0 || (along > 0 && along * along + slide * slide >= (float)GEL_FLOOR_DROP * GEL_FLOOR_DROP))
                fall_land();
        }
        break;
    case FALL_IMPACT: {
        fall_t += dt;
        float t = clampf(fall_t / IMPACT_S, 0, 1);
        fall_squash = IMPACT_SQUASH + (LAND_SQUASH - IMPACT_SQUASH) * smooth(t);
        if (t >= 1.0f) fall_splat();
        break;
    }
    case FALL_LAND: {
        fall_t += dt;
        float t = clampf(fall_t / LAND_S, 0, 1);
        fall_squash = LAND_SQUASH * (1.0f - smooth(t));
        if (t >= 1.0f) {
            fall_phase = FALL_NONE; fall_squash = 0; gaze_x = 0;
            lookback_t = 0;                 /* THE LOOK BACK AFTER THE FALL (lookback.c): the slump is over -- the gaze toward the wall he came from for its second (the clock below, the gaze in draw; the brace's squint stays over it) */
            Serial.printf("[%8lu] bowl: a look back toward the %s wall (%s) for %.0f s%s\n", (unsigned long)millis(),
                          GEL_WALL_NAMES[lookback_from], lookback_name(lookback_from, wall), LOOKBACK_S, braced() ? " (under the brace's squint)" : "");
        }
        break;
    }
    default: break;
    }
    if (pet_t > 0) {
        pet_t -= dt;
        if (pet_t <= 0) {
            pet_t = 0;
            spring2.kick(PET_BOUNCE);       /* the release bounces, once */
        }
    }
    {   /* THE PET'S PLACE: the flank's lean away and the base's slide away on their roads (petplace.c); THE LOOK BACK's second */
        pet_place_a1 = 0; pet_place_slide = 0;
        if (pet_place_t >= 0) {
            pet_place_t += dt;
            if (petplace_done(pet_place_t) || !house_holding()) pet_place_t = -1;
            else petplace_fx(&pet_place, pet_place_t, &pet_place_a1, &pet_place_slide);
        }
        if (lookback_t >= 0) { lookback_t += dt; if (lookback_t >= LOOKBACK_S) lookback_t = -1; }
    }
    if (wake_t > 0) wake_t -= dt;
    if (hang_t > 0) { hang_t -= dt; if (hang_t < 0) hang_t = 0; }     /* THE HANG SLUMP's stand-up */
    if (show_t > 0) { show_t -= dt; if (show_t < 0) show_t = 0; }     /* SHOWING SOMEONE's presentable second */
    if (warm_settle_t > 0) { warm_settle_t -= dt; if (warm_settle_t < 0) warm_settle_t = 0; }   /* THE WARM SKIN's content settle */
    if (far_t > 0) { far_t -= dt; if (far_t <= 0) { far_t = 0; far_gx = far_gy = 0; } }   /* THE FAR TAP's look */
    {   /* PEEKABOO: the hide eases under the rim while the hand stays, the pop back up on the lift */
        if (peek_hiding && !house_holding()) peek_hiding = false;   /* the house went empty under the hand: nothing to hide */
        float target = peek_hiding ? 1.0f : 0.0f;
        float step = dt / (peek_hiding ? PEEK_SINK_S : PEEK_POP_S);
        if (peek_k < target) peek_k = fminf(peek_k + step, target);
        else if (peek_k > target) peek_k = fmaxf(peek_k - step, target);
        if (peek_pop_t >= 0) {
            peek_pop_t += dt;
            if (peek_pop_t >= PEEK_WIDE_S) { eye_kind = GEL_EYE_HAPPY; eye_kind_t = PEEK_HAPPY_S; peek_pop_t = -1; }   /* wide, then happy */
        }
    }
    {   /* THE SHOVE: the skid out, the slide back with one settle, the push's squash; wide eyes then happy */
        shove_now = 0; shove_squash = 0;
        if (shove_t >= 0 && !house_holding()) shove_t = -1;   /* the house went empty mid-skid: nothing to skid */
        if (shove_t >= 0) {
            shove_t += dt;
            float k;
            if (shove_t < SHOVE_OUT_S) k = smooth(shove_t / SHOVE_OUT_S);                     /* the push: out fast */
            else {
                float u = (shove_t - SHOVE_OUT_S) / (SHOVE_S - SHOVE_OUT_S);                 /* the return: through his place, a little past, back -- one settle */
                k = u >= 1.0f ? 0.0f : (1.0f - u) * cosf((float)M_PI * u);
            }
            shove_now = SHOVE_PX * shove_dir * k;
            if (shove_t < SHOVE_SQUASH_S) shove_squash = SHOVE_SQUASH * sinf((float)M_PI * shove_t / SHOVE_SQUASH_S);
            if (shove_eyes_wide && shove_t >= SHOVE_WIDE_S) { shove_eyes_wide = false; eye_kind = GEL_EYE_HAPPY; eye_kind_t = SHOVE_HAPPY_S; }
            if (shove_t >= SHOVE_S) { shove_t = -1; shove_now = 0; }
        }
    }
    {   /* THE TICKLE: the wriggle -- the spin's wobble drive on both springs at TICKLE_HZ, fading over TICKLE_S; the squint gives way to the happy eyes; the lean away eased back by TICKLE_LEAN_S */
        tickle_a1 = 0;
        if (tickle_t >= 0 && !house_holding()) tickle_t = -1;   /* the house went empty mid-wriggle: nothing to wriggle */
        if (tickle_t >= 0) {
            tickle_t += dt;
            if (tickle_t < TICKLE_S) {
                float f = TICKLE_DRIVE * (1.0f - tickle_t / TICKLE_S) * sinf(2.0f * (float)M_PI * TICKLE_HZ * tickle_t);
                spring1.kick(f * dt);
                spring2.kick(0.5f * f * dt);
            } else if (tickle_squint) { tickle_squint = false; eye_kind = GEL_EYE_HAPPY; eye_kind_t = TICKLE_HAPPY_S; }
            float k = tickle_t < PETPLACE_LEAN_IN_S ? smooth(tickle_t / PETPLACE_LEAN_IN_S)
                    : tickle_t < TICKLE_S ? 1.0f : 1.0f - (tickle_t - TICKLE_S) / (TICKLE_LEAN_S - TICKLE_S);
            tickle_a1 = tickle_side * PETPLACE_LEAN * clampf(k, 0.0f, 1.0f);
            if (tickle_t >= TICKLE_LEAN_S) { tickle_t = -1; tickle_a1 = 0; }
        }
    }
    if (eye_kind_t > 0) {
        eye_kind_t -= dt;
        if (eye_kind_t <= 0) eye_kind = GEL_EYE_OPEN;
    }
    if (eye_hold > 0) { eye_hold -= dt; if (eye_alpha < 1 && hang_t <= 0) eye_alpha = clampf(eye_alpha + dt / (eye_rise_s > 0.02f ? eye_rise_s : 0.02f), 0, 1); }   /* THE GRAIN: the rise; THE HANG SLUMP: the eyes come after the stand-up */
    else if (eye_alpha > 0) eye_alpha = clampf(eye_alpha - dt / EYE_FADE_S, 0, 1);
}

/* 3x5 glyphs: B R T L */
static const uint8_t GLYPH_B[5] = { 0b110, 0b101, 0b110, 0b101, 0b110 };
static const uint8_t GLYPH_R[5] = { 0b110, 0b101, 0b110, 0b101, 0b101 };
static const uint8_t GLYPH_T[5] = { 0b111, 0b010, 0b010, 0b010, 0b010 };
static const uint8_t GLYPH_L[5] = { 0b100, 0b100, 0b100, 0b100, 0b111 };
static const uint8_t *const GLYPHS[4] = { GLYPH_B, GLYPH_R, GLYPH_T, GLYPH_L };

static inline void fb_px(int x, int y, uint16_t rgb565)
{
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    fb[y * SCREEN_W + x] = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
}

static void fb_block(int x, int y, int w, int h, uint16_t c)
{
    mark_rect(&mk_main, x, y, w, h, c);
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) fb_px(xx, yy, c);
}

static void draw_markers(void)
{
    const int R = 96, S = 4;                    /* the letters' radius and pixel size */
    static const int MDX[4] = { 0, 1, 0, -1 }, MDY[4] = { 1, 0, -1, 0 };   /* B R T L on the glass */
    for (int w = 0; w < 4; w++) {
        int cx = SCREEN_W / 2 + MDX[w] * R, cy = SCREEN_H / 2 + MDY[w] * R;
        uint16_t c = (w == wall) ? 0xFFE0 : 0x7BEF;    /* yellow = the floor, grey = the rest */
        const uint8_t *g = GLYPHS[w];
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (g[row] & (1 << (2 - col)))
                    fb_block(cx - 6 + col * S, cy - 10 + row * S, S, S, c);
    }
    /* the red dot: gravity on the glass -- at the centre when the watch
     * lies flat, out to the rim as it tilts (not normalised: no dash
     * across the face as the tilt passes through flat) */
    float gx = DOWN_X(acc_lp), gy = DOWN_Y(acc_lp);
    float l = sqrtf(gx * gx + gy * gy);
    if (l > 1.0f) { gx /= l; gy /= l; }
    int dx = SCREEN_W / 2 + (int)(gx * 110.0f), dy = SCREEN_H / 2 + (int)(gy * 110.0f);
    fb_block(dx - 4, dy - 4, 9, 9, 0xF800);
    /* the map index, as a row of dots top-left: map+1 dots */
    for (int i = 0; i <= axis_map; i++) fb_block(70 + i * 8, 40, 5, 5, 0x07E0);
}

/* THE WATER LOCK's drop (gesture.cpp): a 5x7 drop in the shine colour at
 * the rim opposite the floor (outside the glance's ring at 70) while
 * the glass is locked -- the one mark that the gel is not listening */
static const uint8_t GLYPH_DROP[7] = { 0b00100, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110 };
static void draw_drop(void)
{
    const int PX = 3, R = 98;
    const gel_rgba_t *sh = &theme_pal()->shine;
    uint16_t c = (uint16_t)(((int)(sh->r * 31 + 0.5f) << 11) | ((int)(sh->g * 63 + 0.5f) << 5) | (int)(sh->b * 31 + 0.5f));
    int cx = SCREEN_W / 2 - (int)(sinf(bowl_phi) * R), cy = SCREEN_H / 2 - (int)(cosf(bowl_phi) * R);   /* opposite the floor, on the bowl */
    int x0 = cx - (5 * PX) / 2, y0 = cy - (7 * PX) / 2;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (GLYPH_DROP[row] & (1 << (4 - col)))
                fb_block(x0 + col * PX, y0 + row * PX, PX, PX, c);
}

/* PLAY: THE BEAD (play.cpp): a 3x3 bead in the SHADE colour (THE PLACE:
 * marks in the shade), each cell BEAD_BLOCK_PX square, where the roll
 * has it this frame -- over the frame through fb_block like the lock's
 * drop (its mark is mk_main's). Never under the page; gone the instant
 * it is eaten */
static void draw_bead(void)
{
    float x, y;
    if (!play_bead_on(&x, &y)) return;
    const gel_rgba_t *sh = &theme_pal()->shade;
    uint16_t c = (uint16_t)(((int)(sh->r * 31 + 0.5f) << 11) | ((int)(sh->g * 63 + 0.5f) << 5) | (int)(sh->b * 31 + 0.5f));
    int x0 = (int)lroundf(x) - (BEAD_SIZE * BEAD_BLOCK_PX) / 2, y0 = (int)lroundf(y) - (BEAD_SIZE * BEAD_BLOCK_PX) / 2;
    for (int row = 0; row < BEAD_SIZE; row++)
        for (int col = 0; col < BEAD_SIZE; col++)
            fb_block(x0 + col * BEAD_BLOCK_PX, y0 + row * BEAD_BLOCK_PX, BEAD_BLOCK_PX, BEAD_BLOCK_PX, c);
}

/* PLAY: THE DROP AND THE COURSE (play.cpp, F50 / F52). Three small
 * marks, through fb_block as the bead and the lock's drop are:
 *   the DROP falling -- the lock's own 5x7 drop glyph at DROP_PX 1 (5x7
 *     px, the bead's size) in the SHINE: the drop's look, a third of the
 *     lock's, where the fall has it this frame;
 *   the WET MARK -- a MARK_PX square in the SHADE where the drop met the
 *     glass, its colour sinking to the black glass over MARK_S;
 *   the COURSE's GATES -- a GATE_PX square on the rim at each gate wall
 *     (GATE_R out along the wall's angle), the shade until passed, the
 *     SHINE after. Never under the page; gone with the course. */
static uint16_t pal565(const gel_rgba_t *p, float k)
{
    return (uint16_t)(((int)(p->r * k * 31 + 0.5f) << 11) | ((int)(p->g * k * 63 + 0.5f) << 5) | (int)(p->b * k * 31 + 0.5f));
}
static void draw_play_marks(void)
{
    float x, y, k;
    if (play_drop_on(&x, &y)) {
        uint16_t c = pal565(&theme_pal()->shine, 1.0f);
        int x0 = (int)lroundf(x) - (5 * DROP_PX) / 2, y0 = (int)lroundf(y) - (7 * DROP_PX) / 2;
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (GLYPH_DROP[row] & (1 << (4 - col)))
                    fb_block(x0 + col * DROP_PX, y0 + row * DROP_PX, DROP_PX, DROP_PX, c);
    }
    if (play_mark_on(&x, &y, &k)) {
        if (k > 0.04f)   /* the last of it is the glass's own black: nothing to draw */
            fb_block((int)lroundf(x) - MARK_PX / 2, (int)lroundf(y) - MARK_PX / 2, MARK_PX, MARK_PX, pal565(&theme_pal()->shade, k));
    }
    if (play_course_on()) {
        int gw[COURSE_GATES]; bool gl[COURSE_GATES];
        play_course_gates(gw, gl);
        uint16_t dim = pal565(&theme_pal()->shade, 1.0f), lit = pal565(&theme_pal()->shine, 1.0f);
        for (int i = 0; i < COURSE_GATES; i++) {
            float phi = walls_phi(gw[i]);
            int gx = SCREEN_W / 2 + (int)lroundf(sinf(phi) * GATE_R), gy = SCREEN_H / 2 + (int)lroundf(cosf(phi) * GATE_R);
            fb_block(gx - GATE_PX / 2, gy - GATE_PX / 2, GATE_PX, GATE_PX, gl[i] ? lit : dim);
        }
    }
}

/* THE ZS (watch-body 1: "it sleeps too, Zs -- dreams only with visible
 * Zs, his rule"): 3x5 Z glyphs in the shine colour, rising from the
 * gel's crown (the side away from the floor wall) and fading; rest.cpp
 * times them, this only draws them. */
static const uint8_t GLYPH_Z[5] = { 0b111, 0b001, 0b010, 0b100, 0b111 };
static void draw_zs(float squash, float slide, float rise)
{
    float rx, ry;
    gel_blob_radii(squash, &rx, &ry);
    float crown = GEL_FLOOR_DROP - rise - ry * GEL_SCALE;   /* the crown, face px from the centre toward the wall */
    const gel_rgba_t *sh = &theme_pal()->shine;
    for (int i = 0; i < REST_ZS; i++) {
        const rest_z_t *z = &rest->z[i];
        if (!z->on || z->alpha <= 0) continue;
        float a = clampf(z->alpha, 0, 1);
        int Rc = (int)(sh->r * a * 31 + 0.5f), Gc = (int)(sh->g * a * 63 + 0.5f), Bc = (int)(sh->b * a * 31 + 0.5f);
        uint16_t c = (uint16_t)((Rc << 11) | (Gc << 5) | Bc);
        /* along the wall's outward direction, negated = up; along the floor = the body's right */
        float up = crown - 6.0f - z->rise, along = slide + rx * GEL_SCALE * 0.55f + z->drift;
        /* gravity's direction on the glass is (sin phi, cos phi); the body's right is a quarter back */
        float gx = sinf(bowl_phi), gy = cosf(bowl_phi);
        int cx = SCREEN_W / 2 + (int)lroundf(gx * up + gy * along);
        int cy = SCREEN_H / 2 + (int)lroundf(gy * up - gx * along);
        int S = z->px;
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (GLYPH_Z[row] & (1 << (2 - col)))
                    fb_block(cx - S * 3 / 2 + col * S, cy - S * 5 / 2 + row * S, S, S, c);
    }
}

/* THE FRAME PUSH: the dirty rect to the panel. A rect of STAGE_PX or
 * fewer pixels is copied into the stage (its rows packed, contiguous)
 * and pushed from there as one transfer -- asynchronous in full, and fb
 * is free for the next compose the moment this returns. A bigger rect
 * goes from fb itself: a full-width band as the framebuffer's own rows,
 * a narrower one through the panel's writeImage with the source's real
 * stride (LovyanGFX queues one DMA descriptor a row and fires them as
 * one transfer; its public pushImage forces the stride to the width, so
 * the panel is asked directly); the DMA reads fb after this returns and
 * draw() waits for it (fb_in_flight) before composing over the memory. */
static void push_rect(const dirty_rect_t &r)
{
    uint32_t t0 = micros();
    pushes++;
    push_px += (uint32_t)r.w * r.h;
    if (stage && r.w * r.h <= STAGE_PX) {
        lcd.waitDMA();                          /* the stage's last push (a compose has passed since: done) */
        uint16_t *d = stage;
        for (int y = 0; y < r.h; y++, d += r.w) memcpy(d, fb + (r.y + y) * SCREEN_W + r.x, (size_t)r.w * 2);
        lcd.pushImageDMA(r.x, r.y, r.w, r.h, (const lgfx::swap565_t *)stage);
        stage_pushes++;
        t_push += micros() - t0;
        return;
    }
    lcd.waitDMA();
    fb_in_flight = true;
    fb_pushes++;
    if (r.w == SCREEN_W) {
        lcd.pushImageDMA(0, r.y, SCREEN_W, r.h, (const lgfx::swap565_t *)(fb + r.y * SCREEN_W));
        t_push += micros() - t0;
        return;
    }
    lgfx::pixelcopy_t pc(fb, lcd.getColorDepth(), lgfx::rgb565_2Byte, false, nullptr);
    if (!pc.no_convert) {                       /* the panel's depth is not ours (it is: 16-bit swapped): the band, never a conversion */
        lcd.pushImageDMA(0, r.y, SCREEN_W, r.h, (const lgfx::swap565_t *)(fb + r.y * SCREEN_W));
        t_push += micros() - t0;
        return;
    }
    pc.src_bitwidth = SCREEN_W;
    pc.src_x = r.x;
    pc.src_y = r.y;
    lcd.panel()->writeImage(r.x, r.y, r.w, r.h, &pc, true);
    t_push += micros() - t0;
}

/* before the first write into fb: the push reading it, if one is */
static inline void wait_fb(void)
{
    if (!fb_in_flight) return;
    uint32_t t0 = micros();
    lcd.waitDMA();
    fb_in_flight = false;
    uint32_t tw = micros() - t0;
    t_wait += tw;
    t_wait_frame += tw;
    if (tw > t_wait_max) t_wait_max = tw;
}

/* one frame composed into fb; true when something of it reached the panel */
static bool draw(void)
{
    gel_cell_params_t p;
    memset(&p, 0, sizeof p);
    const uint32_t t_start = micros();
    uint32_t t0;
    t_wait_frame = 0;
    /* the breath is rest.cpp's now: the day's rate and depth, the phase
     * integrated (BREATH_AMP / BREATH_HZ are its resting numbers) */
    float squash = WRIST_SQUASH + stature_squash                /* THE STATURE WEEK: the week's drift on the resting stance (0 until the files say) */
                   + (cw ? cw->breath_k : 1.0f) * fall0_breath_k()   /* CLOCKWORK: the breath held (0) while the stopwatch runs; THE WEIGHTLESS BEAT: held (0) in the air (1 otherwise) */
                   * (rest ? rest->breath_amp * sinf(drowse_breath_phase(sky_breath_phase(rest->breath_phase)))   /* THE SKY: the breath slower in the heat (rest's own rate otherwise); THE DIM'S DROWSE: a notch slower under the dim (x1 otherwise) */
                        : BREATH_AMP * sinf(2.0f * (float)M_PI * BREATH_HZ * clock_s));
    if (pet_t > 0) {
        /* the flat press for most of the stay, melting off at the end */
        float t = clampf(pet_t / PET_EASE_S, 0, 1);
        squash += PET_SQUASH * pet_press_k * smooth(t);   /* THE PET'S PLACE: a flank's press is shallower (petplace_press_k) */
    }
    if (wake_t > 0 && !(rest && rest->stretch > 0))
        squash += SLEEP_SQUASH * smooth(clampf(wake_t / WAKE_EASE_S, 0, 1));
    if (hang_t > 0) squash += HANG_SQUASH * smooth(clampf(hang_t / HANG_S, 0, 1));   /* THE HANG SLUMP: spread on the wall, standing up over the first half second */
    if (show_t > 0 && SHOW_S - show_t < SHOW_SETTLE_S)                             /* SHOWING SOMEONE: one settle, idle's own */
        squash += SHOW_SETTLE_SQUASH * sinf((float)M_PI * (SHOW_S - show_t) / SHOW_SETTLE_S);
    if (warm_settle_t > 0)                                                          /* THE WARM SKIN (warm.c, watch-night C20): the raise after the skin read WARM -- one content settle, the same settle */
        squash += WARM_SETTLE_SQUASH * sinf((float)M_PI * (WARM_SETTLE_S - warm_settle_t) / WARM_SETTLE_S);
    /* THE NIGHT: asleep with him, the crumple held; the morning stretch, tall */
    if (rest && rest->asleep) squash += SLEEP_SQUASH;
    if (rest && rest->stretch > 0) squash += STRETCH_SQUASH * rest->stretch;
    if (rest) squash += rest->pant_squash;      /* THE RUN AND THE PANT (rest.cpp, C10): the crown a hair lower after a run, easing out over its 90 s */
    squash += fall_squash;                      /* the peel, the drop, the splat */
    squash += gesture_squash();                 /* the meal's lump, the drink's and the stroke's settles (gesture.cpp) */
    squash += shove_squash;                     /* THE SHOVE: the push's press */
    squash += play_toss_squash() + play_rhythm_squash();   /* PLAY: THE TOSS's small splat, THE RHYTHM's pulses (play.cpp) */
    squash += fall0_squash();                   /* THE WEIGHTLESS BEAT: the dropped bowl's landing, the fall's own splat (fall0.c) */
    cramp_fx_t cx; cramp_fx(&cx);               /* THE SMALL HOUSE: pressed a touch wider against its wall as the cramp grows (a stance) */
    squash += cx.squash;
    sky_fx_t sx; sky_fx(&sx);                   /* THE SKY (sky.cpp): rain's fullness, cold's shiver, hot's droop, storm's laps -- all nothing when no sky is known */
    squash += sx.squash;
    vigor_fx_t vx; vigor_fx(&vx);               /* VIGOR (vigor.cpp): the cable's contented settle; spent = the sleep crumple eased in (the eyes below) */
    squash += vx.squash + SLEEP_SQUASH * vx.spent_k;
    /* THE HOUSE (house.cpp): the arrival's rise out of the rim and the
     * departure's sink into it add to the squash, the lift and the
     * face's presence; while EMPTY there is no gel at all */
    house_fx_t hx; house_fx(&hx);
    squash += hx.squash;
    p.squash = clampf(squash + spring2.a, -0.55f, 0.70f);
    p.lump = fmaxf(fmaxf(fmaxf(gesture_lump(), tell_lump()), fmaxf(grain_lump(), play_lump())), wisp_lump());   /* THE TRUE LUMP: the feed's meal, the walk's, the desk's belly that came down the grain line (THE BELLY TRAVELS), the bead he ate (PLAY), or the wisp he was fed (E41), the largest (one belly), on the dome's own term */
    p.gaze_x = gaze_x;
    if (lease_glance_t > 0) p.gaze_y -= 1;      /* THE LEASE, EASY: the hold was read -- the gel glances up at him (idle's own glance-up, the same px) */
    if (far_t > 0) { p.gaze_x += far_gx; p.gaze_y += far_gy; }   /* THE FAR TAP: the look toward where the finger landed (and PLAY's bead, followed round the bowl) */
    if (lookback_t >= 0) { int lgx, lgy; if (lookback_gaze(lookback_from, wall, lookback_t, &lgx, &lgy)) { p.gaze_x += lgx; p.gaze_y += lgy; } }   /* THE LOOK BACK AFTER THE FALL (lookback.c): toward the wall he came from, its second; under the brace's squint */
    if (wisp_eye.watching) { p.gaze_x += wisp_eye.gaze_x; p.gaze_y += wisp_eye.gaze_y; }   /* THE WISP WATCHED (wispeye.c): toward the newest mote, through its flight and its two seconds at the post */
    if (play_dizzy() > 0) p.gaze_x += -play_dizzy_sign() * (play_dizzy() > 0.5f ? 2 : 1);   /* PLAY: THE SPIN -- the gaze lags the turn, coming back through the spell */
    if (show_t > 0) p.gaze_y -= 1;              /* SHOWING SOMEONE: the look up */
    p.gaze_y += cx.gaze_y;                      /* THE SMALL HOUSE: the nudge's / the bid's look up at him */
    if (rest && rest->cold_look > 0) p.gaze_y -= 1;   /* PUT ON COLD (rest.cpp): the look up after hours on the shelf */
    float sway = clampf(spring1.a * 0.5f, -0.25f, 0.25f);
    sway += stroke_along * STROKE_LEAN * gesture_stroke();   /* THE STROKE: the lean into the finger's travel */
    sway += pet_place_a1;                                    /* THE PET'S PLACE: a flank's lean AWAY from the finger (petplace.c; the stroke's mechanism reversed) */
    sway += tickle_a1;                                       /* THE TICKLE (C4): the lean away from the scrubbing finger, the same road */
    p.a1 = clampf(lean + sway, -0.30f, 0.30f);
    /* the mass sloshing shoulder to shoulder with the lean; THE RESIDUAL's
     * slosh: the low shoulder bulks (a2 negative = the right one) */
    p.a2 = clampf(0.12f * spring1.a + sx.a2 - SLOSH_A2 * (braced() ? BRACE_SLOSH_K : 1.0f) * tilt_k   /* THE BRACED FALL: the slosh damped for the minute */
                  + (rest ? rest->cold_a2 : 0.0f)                                                      /* PUT ON COLD: the shiver, the cold sky's own jiggle */
                  + shakeoff_a2(),                                                                     /* THE SHAKE-OFF (watch-night C16): the dried glass shaken off, the same jiggle */
                  -0.10f, 0.10f);
    p.phase = (float)M_PI / 2.0f;
    p.gel = fmodf(clock_s * GEL_GEL_HZ + sx.gel_extra, 1.0f);
    p.eye_alpha = eye_alpha;
    p.eye_kind = (wake_t > WAKE_EASE_S * 0.5f) ? GEL_EYE_SLEEP : eye_kind;
    if (rest && rest->asleep) { p.eye_kind = GEL_EYE_SLEEP; p.eye_alpha = 1.0f; }      /* the deep sleep look */
    else if (rest && rest->stretch > 0) { p.eye_kind = GEL_EYE_WIDE; p.eye_alpha = 1.0f; }
    else if (vx.spent_k >= 0.5f) { p.eye_kind = GEL_EYE_SLEEP; p.eye_alpha = 1.0f; }   /* VIGOR: spent -- the sleep look while lit */
    else if (sx.eye_kind >= 0) p.eye_kind = sx.eye_kind;   /* THE SKY: droopy lids on the raise in the heat */
    else if (vx.eye_kind >= 0) p.eye_kind = vx.eye_kind;   /* VIGOR: droopy on the raise's first second when the cell is low; content through the cable's settle (the tell's fact outranks it below, as it outranks the mood) */
    else if (rest && rest->run && p.eye_kind == GEL_EYE_OPEN) p.eye_kind = GEL_EYE_SQUINT;   /* THE RUN AND THE PANT (rest.cpp, C10): the wind in the eyes through a run; any other look wins */
    p.eye_alpha *= hx.eye_k;                    /* THE HOUSE: eyes fading out as it sinks, coming in as it rises */
    p.gaze_y += hx.gaze_y;                      /* THE GOODBYE (house.cpp): the look up before the sink, the eyes up for it */
    if (hx.eye_alpha > p.eye_alpha) p.eye_alpha = hx.eye_alpha;
    if (cx.eye_kind >= 0 && !(rest && rest->asleep)) p.eye_kind = cx.eye_kind;   /* THE SMALL HOUSE: content through his answer's second */
    if (cx.eye_alpha * hx.eye_k > p.eye_alpha) p.eye_alpha = cx.eye_alpha * hx.eye_k;
    {   /* IDLE LIFE (idle.cpp): the small things a creature at rest does,
         * folded in as deltas -- the look's and the glance-up's gaze, a
         * blink's or the glance's eye kind, the eyes brought up softly (the
         * larger presence wins), the settle's squash and lean, the
         * bubble's laps on the gel clock. Nothing plays unless the body is
         * free (loop() says); the shape is untouched */
        idle_fx_t ix; idle_fx(&ix);
        p.squash = clampf(p.squash + ix.squash, -0.55f, 0.70f);
        p.a1 = clampf(p.a1 + ix.a1, -0.30f, 0.30f);
        p.gaze_x += ix.gaze_x;
        p.gaze_y += ix.gaze_y;
        p.gel = fmodf(p.gel + ix.gel_extra + drum_extra(), 1.0f);   /* THE DRUMMED TABLE (drum.c, C18): the typing's ripple on the same road, the bubble's pace x the drumming */
        if (ix.eye_kind >= 0 && vx.spent_k < 0.5f) p.eye_kind = ix.eye_kind;   /* VIGOR: a spent body's sleep bar stays (idle rolls nothing spent; a thing in progress ends under it) */
        if (ix.eye_alpha * hx.eye_k > p.eye_alpha) p.eye_alpha = ix.eye_alpha * hx.eye_k;
    }
    {   /* THE STARE RETURNED (stare.c): the look up at him, content, one soft settle -- idle's own glance-up and settle,
         * deterministic; the eyes are engage()'d at the event; under the drowse's lids below, as idle's is */
        stare_fx_t stx; stare_fx(&stx);
        p.squash = clampf(p.squash + stx.squash, -0.55f, 0.70f);
        p.gaze_y += stx.gaze_y;
        if (stx.eye_kind >= 0 && vx.spent_k < 0.5f) p.eye_kind = stx.eye_kind;
    }
    {   /* THE EAR TO THE WRIST (listen.c): the gaze down toward the strap, content, through the creature's own read; the settle
         * after a clean one -- idle's own settle; under the drowse's lids below, as the stare's */
        listen_fx_t lfx; listen_fx(&lfx);
        p.squash = clampf(p.squash + lfx.squash, -0.55f, 0.70f);
        p.gaze_y += lfx.gaze_y;
        if (lfx.eye_kind >= 0 && vx.spent_k < 0.5f) p.eye_kind = lfx.eye_kind;
    }
    {   /* THE DIM'S DROWSE (drowse.c): under the dim the lids droop and the stance slumps a hair; over idle's
         * blink and glance (the dusk holds), under the night light's and the clockwork's below */
        drowse_fx_t dx; drowse_fx(&dx);
        p.squash = clampf(p.squash + dx.squash, -0.55f, 0.70f);
        if (dx.eye_kind >= 0 && !(rest && rest->dozing) && vx.spent_k < 0.5f) { p.eye_kind = dx.eye_kind; if (dx.eye_alpha * hx.eye_k > p.eye_alpha) p.eye_alpha = dx.eye_alpha * hx.eye_k; }
    }
    {   /* THE LULL (lull.c): petted toward rest -- the lids droopy, drifting in and (at the time's end) away; over idle's and
         * the stroke's own content face (a slow stroke inside the lull keeps the lids down), beside the drowse's; under the
         * tone's, the night light's and the clockwork's below; never dozing, never spent */
        lull_fx_t lx; lull_fx(&lx);
        if (lx.eye_kind >= 0 && !(rest && rest->dozing) && vx.spent_k < 0.5f) { p.eye_kind = lx.eye_kind; if (lx.eye_alpha * hx.eye_k > p.eye_alpha) p.eye_alpha = lx.eye_alpha * hx.eye_k; }
    }
    {   /* THE WORD'S TONE (tone.c): the shown word's beat -- a glad word's settle and happy eyes, a trailing word's slow blink
         * and half-down lids; over idle's and the drowse's (the word's show is a raise or a tap: the lids are up), under the
         * night light's and the clockwork's below; never dozing, never spent */
        tone_fx_t tx; tone_fx(&tx, millis());
        p.squash = clampf(p.squash + tx.squash, -0.55f, 0.70f);
        if (tx.eye_kind >= 0 && !(rest && rest->dozing) && vx.spent_k < 0.5f) p.eye_kind = tx.eye_kind;
        if (tx.eye_alpha * hx.eye_k > p.eye_alpha && !(rest && rest->dozing)) p.eye_alpha = tx.eye_alpha * hx.eye_k;
    }
    {   /* THE KNOCK FROM BEHIND (knock.c): the rap's wide eyes, the second rap's squint -- over the drowse's lids (a knock
         * brings them up) and the word's tone, under the tell's and the clockwork's below; never dozing, never spent */
        int kx = knock_eye_kind();
        if (kx >= 0 && !(rest && rest->dozing) && vx.spent_k < 0.5f) p.eye_kind = kx;
    }
    if (tell) {   /* THE TELL (tell.cpp) folds in here: the answer's squash curve and its eyes for a second or two
                   * (never over the deep sleep or rest's own stretch), the step bob's pulse, and the night light's
                   * kept sleep look through a stir -- then the ordinary creature */
        float ts = tell->squash;
        bool stir_light = tell->night_light && rest && rest->dozing && !rest->asleep;
        if (stir_light) ts += SLEEP_SQUASH;      /* a stir under the night light keeps the crumple */
        p.squash = clampf(p.squash + ts, -0.55f, 0.70f);
        if (stir_light) { p.eye_kind = GEL_EYE_SLEEP; p.eye_alpha = 1.0f * hx.eye_k; }
        else if (tell->eye_kind >= 0 && !(rest && (rest->asleep || rest->stretch > 0)) && vx.spent_k < 0.5f) { p.eye_kind = tell->eye_kind; p.eye_alpha = hx.eye_k; }   /* VIGOR: spent, it does not answer */
    }
    if (cw) {   /* CLOCKWORK (clockwork.cpp): the held breath's intake, the exhale's settle, the bounces' pulses; wide / content eyes
                 * (over the sleep look too: the alarm is the one thing allowed to wake him) */
        p.squash = clampf(p.squash + cw->squash, -0.55f, 0.70f);
        if (cw->eye_kind >= 0) { p.eye_kind = cw->eye_kind; if (cw->eye_alpha * hx.eye_k > p.eye_alpha) p.eye_alpha = cw->eye_alpha * hx.eye_k; }
    }

    /* the 1x art cell drawn at the wall's angle (a quarter: the raster's
     * sin/cos snap to 0/1, the cell IS the quarter blit), then the hard
     * nearest blit to the wall's place on the rim: the blocks whole, the
     * base centre on a whole pixel; the residual's slide, the sway's few
     * px and a fall's drop ride along the wall */
    p.angle = gel_snap_angle(bowl_phi);
    float slide_now = slide + LEAN_PX * sway + SLIDE_PX * tilt_k + shove_now + pet_place_slide, lift_now = lift;   /* what is composed: the physics' place (THE SHOVE's skid along the wall and THE PET'S PLACE's base slide included), fitted inside the disc below */
    lift_now += cx.push_px;                     /* THE SMALL HOUSE: the push off the wall toward the open side (the fit below keeps it inside the disc) */
    /* THE HINT: the box the drawers say could have changed (the gel's
     * when the raster missed or the compose moved it; each overlay's
     * mark below); nothing in it = nothing hashed, nothing pushed */
    dirty_rect_t hint = { 0, 0, 0, 0 };
    dirty_rect_t gel_box = { 0, 0, 0, 0 };
    bool gel_changed;
    const bool page = page_up();                /* THE WATCH PAGE: the glass's other face -- no gel while it is up, the rings stay */
    const bool costume = wardrobe_covers_gel();  /* THE WRIST WARDROBE: a costume stands where the gel would -- the gel is not rastered, the costume is blitted below */
    const bool gel_now = hx.gel && !page && !costume;
    if (gel_now) {
        gel_pal_t pal = *hours_pal(); sky_tint(&pal);   /* THE HOURS' LIGHT through THE SKY: the hour's palette (GEL_PAL_BLUE at midday, byte for byte), the weather's factors on body/shade/shine (identity when none is known) */
        t0 = micros();
        int rastered = gel_raster_cell(cell, &pal, &p);    /* the cell in PSRAM: before the wait, while a push may still read fb */
        t_raster += micros() - t0;
        /* THE DISC IS THE GLASS (gel_fit_inside): a fall's drop hung from
         * the puddle's centroid, the peel's creep, the residual's slide --
         * none may put a block past the rim; the least move fixes it, and
         * a move in flight is the flight's own (the arc re-aimed at the
         * wall). The house's sink and the peekaboo go under on purpose. */
        {
            float l = lift, s = slide_now;
            if (gel_fit_inside(cell, bowl_phi, &l, &s)) {
                lift_now = l;                       /* the draw's: a splat as wide as the chord sits a hair inward */
                slide_now = s;
                if (fall_phase == FALL_FLIGHT || fall_phase == FALL_PEEL) {
                    lift = l;
                    slide = s - LEAN_PX * sway - SLIDE_PX * tilt_k - shove_now - pet_place_slide;
                    if (fall_phase == FALL_FLIGHT) {
                        float T = lift > 1.0f ? sqrtf(2.0f * lift / FALL_G) : 0.0f;
                        slide_vel = T > 0 ? -slide / T : 0.0f;
                        if (T <= 0) slide = 0;
                    }
                }
            }
        }
        lift_now += play_toss_lift();           /* PLAY: THE TOSS -- the loft off the rim along the wall's normal (8 px at most: inside the disc; the gel alone, a costume stands) */
        lift_now += fall0_lift();               /* THE WEIGHTLESS BEAT -- the same road: the free fall's loft, the toss's 8 px at most */
        wait_fb();
        t0 = micros();
        gel_compose(fb, cell, bowl_phi, lift_now + hx.lift - PEEK_PX * smooth(peek_k), slide_now);   /* PEEKABOO: under the rim while the hand stays */
        t_gel += micros() - t0;
        gel_face_box(&gel_box.x, &gel_box.y, &gel_box.w, &gel_box.h);
        gel_changed = rastered || !had_gel || gel_box.x != gel_box_last.x || gel_box.y != gel_box_last.y
                      || gel_box.w != gel_box_last.w || gel_box.h != gel_box_last.h;
    } else {
        wait_fb();
        memset(fb, 0, (size_t)SCREEN_W * SCREEN_H * sizeof *fb);   /* THE HOUSE is empty (or THE WATCH PAGE is up): the black glass */
        gel_changed = had_gel;
    }
    if (gel_changed) hint = dirty_union(hint, dirty_union(gel_box, gel_box_last));
    gel_box_last = gel_box;
    had_gel = gel_now;
    /* THE WRIST WARDROBE: the costume at the wall with the gel's own lift
     * and slide (the house's sink and the peekaboo included), then the
     * veil's motes over whichever stands; nothing on an empty glass or
     * under the page. Its mark joins the hint below. */
    t0 = micros();
    wardrobe_draw(fb, wall, lift_now + hx.lift - PEEK_PX * smooth(peek_k), slide_now, hx.gel && !page);
    t_gel += micros() - t0;
    /* THE PLACE (place.cpp): with the gel alone in the frame, where the
     * hour (and the clockwork's number) and the word go this frame --
     * the hour pinned to the top (the bottom while he is on the top
     * wall), the word opposite him, the backstop against his pixels */
    static place_t pl = { GEL_WALL_TOP, GEL_WALL_TOP, 0, "" };
    if (!page) {
        bool hour_on = (house_holding() && glance_on()) || clockwork_showing();
        place_decide(&pl, fb, SCREEN_W, SCREEN_H, (gel_now || (costume && hx.gel)) ? wall : GEL_WALL_NONE, hour_on, glance_box, word_showing(), word_box);
        if (pl.log[0]) LOG("%s", pl.log);
    }
    t0 = micros();
    if (!page && rest && rest->dozing && house_holding() && !costume) draw_zs(p.squash, slide_now, lift_now);   /* the visible Zs, from the crown (not on an empty glass; a stir is still the night; not over a costume) */
    face_draw(fb);                              /* THE WATCH FACE (face.cpp): the rings, the drop, the hour when empty, the trace -- only where the glass is still black, behind him */
    t_face += micros() - t0;
    t0 = micros();
    if (page) {
        /* THE WATCH PAGE (page.cpp): his tool page over the face's rings
         * (it clears inside them); the creature's overlays wait -- the
         * glance's hour, the clockwork's number, the lock's drop, the wisp
         * and the word are not drawn while it is up (their marks say so) */
        struct tm lt; uint32_t ep; clock_local(&lt, &ep);
        uint16_t gmv = 0; uint8_t gpct = 0;
        int pct = gauge_read(&gmv, &gpct) ? (int)gpct : (int)(vigor_level_of_mv(batt_mv()) * 100.0f + 0.5f);
        page_in_t pin;
        memset(&pin, 0, sizeof pin);
        pin.minute = clock_local_minute();
        pin.wday = clock_is_set() ? lt.tm_wday : -1; pin.mday = lt.tm_mday; pin.mon = lt.tm_mon;
        pin.steps = wrist_steps_today();
        pin.batt_pct = pct; pin.charging = charging();
        pin.alarm_min = clockwork_alarm();
        pin.sky = sky_known() ? sky_word(0) : "";
        pin.link = (link_linked() || link_resting()) ? PAGE_LINK_FINE : PAGE_LINK_NONE;
        pin.cell_lean = vigor_lean();             /* BATTERY MANNERS: under 10 % the cell's number in the shade */
        /* THE PAGE GROWS: the count on the tool page, the week, the notices */
        {
            uint32_t now = millis();
            if (clockwork_sw_state() != CW_SW_OFF) { pin.tool = PAGE_TOOL_STOPWATCH; pin.tool_ms = clockwork_elapsed_ms(now); }
            else if (clockwork_tm_state() != CW_TM_OFF) { pin.tool = PAGE_TOOL_TIMER; pin.tool_ms = clockwork_remaining_ms(now); }
            pin.laps = clockwork_laps(&pin.lap_ms);
            for (int i = 0; i < 6; i++) pin.week[i] = week_hist[i];
            pin.week[6] = wrist_steps_today();
            pin.night_min = rest_night_min();
            static page_notice_t notes[WISP_LEDGER];
            const wisp_note_t *lg = NULL;
            int n = wisp_ledger(&lg);
            for (int i = 0; i < n && i < WISP_LEDGER; i++) {
                notes[i].app = lg[i].app; notes[i].title = lg[i].title; notes[i].age_s = (now - lg[i].at_ms) / 1000u;
                notes[i].name = lg[i].name; notes[i].msg = lg[i].msg;   /* THE NOTICE OPENED: the ledger's name and message (ANCS fills them; the desk's ask has neither) */
            }
            pin.notice = notes; pin.notices = n;
            /* THE SETTINGS PAGE: the rows show the modules' own state, never the page's memory */
            pin.set_here = house_holding();       /* THE SLIME ROW: HERE / AWAY by the house, never the page's memory */
            pin.set_lease = lease_hand; pin.set_quiet = wake_quiet_on(); pin.set_face = face_on(); pin.set_lock = gesture_locked();
            pin.net_ssid = net_ssid(); pin.net_ip = net_ip();
            pin.link_word = link_docked() ? "DOCK" : link_air() ? "AIR" : link_linked() ? "LINKED" : link_resting() ? "RESTING" : net_radio_is_on() ? "DIALLING" : "RADIO OFF";
            /* MORE SETTINGS: SETTINGS 3-5's rows and ABOUT's lines, the modules' own state as ever */
            pin.wifi_hold = link_holding_radio(); pin.radio_on = net_radio_is_on();
            pin.bt_word = ble_word();          /* BLUETOOTH: OFF / ON -- / PHONE / DESK / BOTH / the pairing window's count (ble.cpp) */
            pin.bright = store.bright; pin.spell_short = store.spell_short; pin.h12 = store.h12;
            pin.bumps_on = !store.bumps_off;   /* THE BUMPS: the BUMPS row (SETTINGS 4) */
            pin.quiet_from = store.quiet_from; pin.quiet_to = store.quiet_to; pin.quiet_desk = quiet_desk_rules();
            pin.build = doctor_build_id(); pin.heap_free = ESP.getFreeHeap(); pin.up_s = now / 1000u; pin.cell_mv = batt_mv();
            pin.box = link_box_short();          /* THE BOX'S STATUS: the ABOUT row (holder, words waiting, the box's sky) */
            {   /* THE PULSE ROW: the sensor's own state */
                int left = 0; bool settling = false; float bpm = 0; bool ok = false;
                pin.pulse_absent = !ppg_present();
                pin.pulse_left_s = ppg_spot_left(&left, &settling) ? (left > 0 ? left : 1) : 0;
                if (ppg_spot(&bpm, &ok)) { pin.pulse_bpm = ok ? (int)(bpm + 0.5f) : 0; pin.pulse_fail = !ok; }
                pin.pulse_still = ppg_spot_waiting();   /* THE STILL SPOT: `STILL` in the shade while the LEDs wait for the wrist */
                pin.pulse_thin = ppg_spot_thin();       /* THE PERFUSION INDEX: `THIN` where NONE would be, when the strap was the cause */
            }
            /* SETTINGS 6, THE HIDDEN SWITCHES (T15) and THE BLUETOOTH PAGE (T16): the stores' and the stack's own words */
            pin.pauto_on = !store.ppg_auto_off; pin.dockhand_on = !store.dockhand_off; pin.ear_gear = store.ear_gear;
            pin.ble_on = ble_enabled(); pin.ble_bonded = ble_bonded(); pin.ble_phone = ble_phone_ready(); pin.ble_desk = ble_desk_open();
            pin.ble_ear = ble_keeps_dark(); pin.ble_pairing = ble_pair_left_s() > 0; pin.ble_notices = wisp_waiting() > 0;
        }
        page_draw(fb, &pin);
    } else {
        if (show_markers) draw_markers();
        if (house_holding() && !clockwork_showing()) glance_draw(fb, pl.hour_at);   /* the hour: pinned to the top (Khalid 23:15: "the time shouldn't move, just the slime"), the bottom while he is on the top wall (THE PLACE); EMPTY, the face's resting hour has it; the clockwork's number has the spot while it runs */
        clockwork_draw(fb, millis(), pl.hour_at);   /* CLOCKWORK: the elapsed / the remaining / the alarm's hour in the hour's spot, its arc over the day ring */
        if (gesture_locked()) draw_drop();          /* the water lock's drop at the rim */
        wisp_draw(fb, bowl_phi);                    /* THE WISP: beside the gel, on the wall's angle */
        draw_bead();                                /* PLAY: the bead rolling round the bowl, a mark in the shade */
        draw_play_marks();                          /* PLAY: THE DROP AND THE COURSE -- the drop falling (the shine), the wet mark (the shade, fading), the three gates on the rim */
        word_draw(fb, pl.word_at, pl.word_inset);   /* the mind's line: opposite him, inward of the hour when they share a wall (THE PLACE), until a tap */
    }
    t_over += micros() - t0;
    {   /* THE MARKS: each drawer, against its last frame */
        dirty_rect_t b;
        if (face_mark(&b)) hint = dirty_union(hint, b);
        if (glance_mark(&b)) hint = dirty_union(hint, b);
        if (clockwork_mark(&b)) hint = dirty_union(hint, b);
        if (word_mark(&b)) hint = dirty_union(hint, b);
        if (mark_take(&mk_main, &b)) hint = dirty_union(hint, b);
        if (wisp_mark(&b)) hint = dirty_union(hint, b);
        if (wardrobe_mark(&b)) hint = dirty_union(hint, b);   /* THE WRIST WARDROBE: the costume's box when its frame or place changed, the veil's motes */
        if (page_mark(&b)) hint = dirty_union(hint, b);   /* THE WATCH PAGE: its disc on the way up and down, its numbers as they turn */
    }

    {   /* THE HOURS' LIGHT (hours.cpp): the backlight follows the day -- the
         * level the state chose (awake / dim / the night's BL_SLEEP) through
         * the hour's eased factor, set only when the number changes; the
         * state block's own sets go through hours_level() too, so this is
         * only ever the slow ease at a phase edge */
        static int bl_set = -1;
        int lv = hours_level(rest && rest->asleep ? BL_SLEEP : state == DIM ? BL_DIM : bl_awake());
        if (page_lamp_on()) lv = BL_LAMP;   /* THE LAMP: full, whatever the hour or the night says -- he asked for it with his hand */
        if (state != DARK && lv != bl_set) { bl_set = lv; lcd.setBrightness((uint8_t)lv); }
    }
    {   /* THE LAMP going out while he is here: the gel squints at the glare
         * for a moment (main's own eye kind; the render is untouched) */
        static bool lamp_was = false;
        bool lamp_now = page_lamp_on();
        if (lamp_was && !lamp_now && house_holding()) { eye_kind = GEL_EYE_SQUINT; eye_kind_t = LAMP_SQUINT_S; engage(LAMP_SQUINT_S + 0.5f); }
        lamp_was = lamp_now;
    }
    frames++;
    if (quiet_frames < IDLE_AFTER_FRAMES) moving_frames++;
    {   uint32_t tf = micros() - t_start - t_wait_frame; if (tf > t_frame_max) t_frame_max = tf; }   /* the compose alone: the wait for a push left out */
    /* THE DIRTY RECT: what changed since the panel last saw the frame,
     * under the hint (the whole frame every SAFETY_SCAN_MS), and only
     * that over the bus; nothing changed = nothing sent */
    dirty_rect_t r;
    bool changed = false;
    uint32_t now = millis();
    bool whole = now - safety_ms >= SAFETY_SCAN_MS;
    if (whole) safety_ms = now;
    if (whole || hint.w > 0) {
        t0 = micros();
        changed = dirty_scan_rect(dirty, fb, whole ? NULL : &hint, &r);
        t_hash += micros() - t0;
        tiles_hashed += dirty->tiles_scanned;
        if (changed) push_rect(r);
    }
    return changed;
}

/* THE WATCH FACE's norm: the last 7 days' step totals from the day
 * files, once at boot and once a day (the median is face.cpp's); THE
 * STATURE WEEK off the same walk, two weeks deep (the week judged and
 * the week its norms come from) */
static uint32_t face_norm_date = 0;
static void face_refresh_norm(void)
{
    uint32_t t[STATURE_FILES], d[STATURE_FILES];
    face_norm_date = clock_local_date();
    int n = wrist_recent_days(face_norm_date, d, t, STATURE_FILES);
    face_set_history(t, n < 7 ? n : 7);         /* the norm as it was: the newest seven */
    stature_px = daykeep_stature(d, t, n, face_norm_date, &stature_over, &stature_under);
    stature_squash = gel_stature_squash(stature_px);
    if (stature_over || stature_under) LOG("face: the stature %+.2f art px (%d day%s over the norm, %d under, of the week)",
                                           stature_px, stature_over, stature_over == 1 ? "" : "s", stature_under);
    wrist_night_walk(face_norm_date);           /* THE NIGHT RING KNOWS HIS HEART: the week's nights and resting heart, the same once-a-day walk (nothing with no clock) */
    /* the week: the six days before today by date, a file or not */
    uint32_t day = face_norm_date;
    for (int back = 1; back <= 6; back++) {
        day = day ? rest_date_prev(day) : 0;
        uint32_t steps = PAGE_NO_DAY;
        for (int i = 0; i < n; i++) if (day && d[i] == day) { steps = t[i]; break; }
        week_hist[6 - back] = steps;
    }
    if (face_norm()) LOG("face: the norm %lu steps from %d day%s%s", (unsigned long)face_norm(), face_norm_days(),
                         face_norm_days() == 1 ? "" : "s", face_norm_date ? "" : " (no clock: every file)");
    else LOG("face: no norm yet (%d day%s of the 3 it needs%s): no day ring", face_norm_days(), face_norm_days() == 1 ? "" : "s",
             face_norm_date ? "" : "; no clock: every file");
}

/* --- the dark house --------------------------------------------------------- */

static void go_dark(const char *why)
{
    state = DARK;
    app.state = (int)DARK;            /* said now, not at the next frame: the nap's errand (link.cpp) reads it as "not lit" */
    log_state(why);
    Serial.flush();
    lcd.waitDMA();
    fb_in_flight = false;
    lcd.setBrightness(0);
    lcd.sleep();
    touch_let_sleep(true);
    eye_alpha = 0;
    eye_hold = 0;
    drowse_dark();                    /* THE DIM'S DROWSE: the dark takes it silently (no frame runs before the wake) */
    lull_dark();                      /* THE LULL: the same -- the dark takes it, nothing left for the next lit frame */
    { stare_in_t z; memset(&z, 0, sizeof z); stare_tick(&z); }   /* THE STARE RETURNED (the review, 2026-10-13 00:50): one dark tick -- loop() never runs DARK, so without it the stare played once per BOOT, not once per lit spell */
    lease_pulse_until = 0;
    if (page_up()) { page_hide("the dark house"); const char *pl = page_tick(0); if (pl) LOG("%s", pl); }   /* THE WATCH PAGE closes with the light */
    imu_gyro_set(false);              /* PLAY: the gyro sleeps with the glass (the nap's poll reads the accelerometer alone) */
    net_off();                        /* the radio never keeps the house lit */
}

static void wake_up(const char *why)
{
    wake_woke(why, &acc_lp);          /* the engine off, the cause noted */
    imu_gyro_set(true);               /* PLAY: THE SPIN -- the gyro rides the lit glass only (off again in go_dark) */
    lcd.wakeup();
    if (dirty) dirty_force(dirty);    /* THE DIRTY RECT: the panel slept; the first frame goes whole, whatever its memory kept */
    safety_ms = 0;                    /* and the first frame is scanned whatever the marks say (they compare with the frame before the nap) */
    quiet_frames = 0;
    frames = pushes = push_px = busy_us = 0; report_ms = millis();   /* the report window starts clean after a nap */
    stage_pushes = fb_pushes = tiles_hashed = moving_frames = moving_us = 0;
    t_frame_max = t_raster = t_gel = t_face = t_over = t_hash = t_push = t_wait = t_wait_max = 0;
    lcd.setBrightness(hours_level(bl_awake()));   /* THE HOURS' LIGHT: the awake level by the hour */
    touch_let_sleep(false);
    touch_was_down = false;
    state = AWAKE;
    wake_t = WAKE_EASE_S;             /* the crumple eases out, not snaps */
    eye_kind = GEL_EYE_OPEN;
    engage(EYE_HOLD_WAKE_S * grain_knobs()->wake_hold);   /* THE GRAIN: the raise's held time, its own */
    spring2.kick(0.6f);
    uint32_t now = millis();
    last_motion_ms = now;
    last_touch_ms = now;
    log_state(why);
    if (!strcmp(why, "a raise")) {
        wrist_count_raise();
        rest_woke_raise = house_holding();   /* THE WAKING BREATH: the breath finds the inhale on this raise (rest.cpp refuses it asleep / cold / stretching) */
        tell_cue = TELL_CUE_RAISE;    /* THE TELL: the first second answers (or the night light) */
        if (wake_raise_from_hang() && !rest_on_shelf() && house_holding()) {   /* THE HANG SLUMP: spread on the wall, standing up, the eyes after (never off the shelf: that is PUT ON COLD's moment) */
            hang_t = HANG_S;
            LOG("the hang slump: the raise came from a hanging arm -- spread +%.2f, standing up over %.1f s, the eyes after", HANG_SQUASH, HANG_S);
        }
        vigor_raise();                /* VIGOR: droopy lids for the first second when the cell is low (the sky's own hook is in link.cpp's sense) */
        glance_show();                /* the hour, if it is known */
        face_raised();                /* THE SWEEP: the rings arrive from 12 o'clock */
        link_sense("wrist:raised", "raised to look");
    } else if (!strcmp(why, "a shake")) {
        wrist_count_shake();
        rest_shake();                 /* THE SHAKEN SLEEPER: the shake that lit the glass ends the night as a rising (the Zs go with the wake's crumple) */
        link_sense("wrist:shaken", "shaken");
    } else if (!strcmp(why, "a touch")) {
        rest_touch();                 /* A TOUCH ENDS SLEEP: the tap that lit the glass ends the night (the Zs go with the wake's crumple) */
    }
    net_on();                         /* awake: BLE up (the WiFi radio is off); the radio itself only for a reason (THE RADIO BUDGET, link.cpp), and BLE goes down for it */
    if (store.wifi_hold) link_hold(true);   /* MORE SETTINGS' WIFI HOLD: his standing word, every lit spell (link_dark ended the last one) */
}

/* asleep: light sleep kept by the IMU's motion engine, the touch, the
 * UART and the minute's timer (wake.cpp); a raise or a shake wakes it,
 * a bump is judged and slept through */
static void nap_tally(void)        /* THE EAR'S LEDGER: the last spell's kind (the awake ear / light sleep) and its length, to ear.cpp */
{
    bool ear; uint32_t ms;
    wake_nap_last(&ear, &ms);
    ear_napped(ear, ms);
    death_nap(ear ? DEATH_NAP_EAR : DEATH_NAP_DEEP);   /* THE LAST DEATH: the crumb's nap kind */
}

static void nap(void)
{
    wake_nap_begin(&acc_lp);          /* the pose it falls asleep in */
    const char *why;
    /* A bump or a turn is judged INSIDE wake_nap_once (NULL) and the loop
     * goes straight back to sleep: wake_up() -- the screen, net_on(), the
     * link's redial -- runs only for a raise, a shake, a touch or a word on
     * the port. The radio is never woken by a bump (pinned, the optimization
     * pass; wake.cpp's judge() carries the same note). */
    ear_gear_enter();                 /* THE EAR'S LOW GEAR: 80 MHz while the nap idles awake for the ear (NVS `eargear`; off by default) */
    vec3_t pose_before = acc_lp;      /* THE PULSE IN THE DAY (ppg.cpp): the nap's poll is the dark house's tick -- the pose before and after each is the wrist's motion, the read runs across polls */
    while (!(why = wake_nap_once(&acc_lp))) {
        nap_tally();                  /* THE EAR'S LEDGER: the spell's kind and length to the tally */
        near_due(millis(), false);    /* THE DESK NEXT DOOR (the review): the dark ages the band, as the README promises -- else a reconnect after a nap glances */
        link_dark_minute();           /* THE NIGHT RELEASE: the night's onset lands in the nap's minute; the verb goes on a dark errand (link.cpp; it leaves the low gear for the dial) */
        ear_gear_enter();             /* ... and the low gear back after it (idempotent, silent when already low) */
        ppg_dark_poll(&pose_before, &acc_lp); pose_before = acc_lp;   /* THE PULSE IN THE DAY: the read across the polls */
        {   /* THE PHONE LEFT BEHIND (phonegone.c, C17): the phone leaves in the dark too -- READY's fall on a walking wrist, heard at the poll; the look waits for the raise */
            phonegone_in_t pgn = { millis(), ble_is_up(), ble_phone_ready(), wrist_walking(), false, house_holding(), false, false };
            if (phonegone_tick(&pgn) == PHONEGONE_EV_GONE) LOG("phone left behind: the phone's BLE dropped on a walking wrist in the dark -- a look toward the door on the next raise (walk-away %d)", phonegone_drops());
        }
    }
    nap_tally();
    ear_gear_leave("the wake");       /* THE EAR'S LOW GEAR: 240 MHz before the glass lights */
    /* under the water lock a touch is a raise attempt, never a pet (gesture.cpp) */
    if (gesture_locked() && !strcmp(why, "a touch")) why = "a raise";
    wake_up(why);
}

/* --- setup / loop ---------------------------------------------------------- */

void setup()
{
    Serial.setRxBufferSize(1024);          /* a burst of provisioning lines */
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("slime watch: step 2 (gel, gravity, slosh, pet, the dark house; wifi, the link, the day log, the glance)");
    app.boot_ms = millis();
    store_begin();
    death_begin();                         /* THE LAST DEATH: how the body last died (the reason, the crumb, NVS boots/crashes), one line */
    show_markers = store.markers;
    /* a soft reset (OTA, `reboot`) keeps the RTC's hour; a power cycle does not */
    if (time(NULL) > 1700000000 && esp_reset_reason() != ESP_RST_POWERON) {
        clock_set_flag = true;
        clock_who = "kept across a reset";
        struct tm lt; clock_local(&lt, NULL);
        Serial.printf("[clock] kept across the reset: %04d-%02d-%02d %02d:%02d local\n",
                      lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    } else Serial.println("[clock] unset until NTP or the desk says (`clock <epoch>` by hand)");
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
    spent_sense(&spent, batt_mv(), false, millis());   /* THE BROWNOUT LOOP (2026-10-14: 309 crashes off a 3.52 V cell): the spent line sampled BEFORE the boot's burst -- without it frame 1 dialled WiFi and lit the glass full, the sag reset the chip, and the reboot did it again */
    wrist_begin();
    net_begin();
    grain_begin();                             /* THE GRAIN: the nominal creature until link_begin re-reads the cached line */
    vigor_begin();                             /* VIGOR: 1.0 until the first sample (the loop's first tick snaps to it) */
    link_begin();
    console_begin();
    gesture_begin(millis());                     /* the gestures; the water lock is never remembered -- THE LOCK IS NEVER A TRAP */
    lease_hand = !store.lease_hand_off;          /* THE SETTINGS PAGE: the lease by hand and quiet time as NVS remembered them */
    wake_quiet_set(!store.quiet_off);
    glance_set_h12(store.h12);                   /* MORE SETTINGS: the clock's 12H / 24H (the face and the page read store each frame) */
    if (store.wifi_hold || store.bright != 1 || store.spell_short || store.h12 || store.quiet_from != 23 || store.quiet_to != 7 || store.ble_off || store.bumps_off)
        LOG("settings: wifi %s, bluetooth %s, brightness %s, lit spell %s, clock %s, quiet %d-%d, bumps %s (NVS)", store.wifi_hold ? "HOLD" : "auto", store.ble_off ? "OFF" : "on",
            store.bright == 0 ? "LOW" : store.bright == 1 ? "mid" : "HIGH", store.spell_short ? "SHORT" : "long", store.h12 ? "12H" : "24h",
            (int)store.quiet_from, (int)store.quiet_to, store.bumps_off ? "OFF" : "on");
    if (store.lease_hand_off || store.quiet_off) LOG("settings: lease by hand %s, quiet time %s (NVS)", lease_hand ? "on" : "OFF", store.quiet_off ? "OFF" : "on");
    rest_begin(BREATH_HZ, BREATH_AMP);         /* the night and the breath */
    tell_begin();                              /* THE TELL: nothing told yet, no walk, no light */
    house_begin();                             /* THE HOUSE: the last known state from NVS, else empty (the black glass) */
    /* THE WATCH FACE: the trace's mask in PSRAM (7.2 KB; none = no trace), the NVS switch, the norm from the day files */
    face_begin((uint8_t *)heap_caps_malloc(FACE_TRACE_BYTES, MALLOC_CAP_SPIRAM), !store.face_off);
    face_refresh_norm();
    hours_begin(BL_DIM);                       /* THE HOURS' LIGHT: the awake glass never under the dim level by the hour */
    hours_sun(store.sun_rise, store.sun_set, true);   /* THE REAL SUN: the pair the last `time` left in NVS (the RTC's clock lights the glass before any link), snapped; -1 = the fixed hours */
    wardrobe_begin();                          /* THE WRIST WARDROBE: the art partition mapped, the remembered theme and wear (no veil at a boot); nothing mounted = the gel as ever */
    sky_begin();                               /* THE SKY: none known until a door says (or `sky <word>`) */
    question_begin();                          /* THE WORD'S QUESTION: nothing asked until a word ending in ? shows */
    clockwork_begin(store.alarm_min);          /* CLOCKWORK: the alarm as NVS remembered it; nothing running */
    idle_begin((uint32_t)micros() ^ (uint32_t)millis());   /* IDLE LIFE: the die seeded from the boot's clock */
    play_begin();                                          /* PLAY: THE BEAD AND THE SPIN -- nothing armed, nothing played */
    cramp_begin((uint32_t)micros() * 2654435761u ^ (uint32_t)millis());   /* THE SMALL HOUSE: the roll's die from the boot's clock */
    ble_begin();                           /* BLUETOOTH: the switch, the ANCS worker; the stack itself comes up at the wake (net_on), only while the WiFi radio is down */
    wake_set_ear(ble_keeps_dark, ble_ear_tick);   /* THE DARK EAR: with a peer bonded the nap idles awake and the stack keeps listening (ruling 78, his way) */
    ear_begin();                               /* THE EAR'S LEDGER AND ITS LOW GEAR: the tally open, the gear as NVS has it (off) */

    fb = (uint16_t *)heap_caps_malloc(SCREEN_W * SCREEN_H * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    app_fb = fb;
    /* the art cell and the dirty rect's hashes in PSRAM (the CPU alone
     * reads them; fb stays internal: the DMA engine reads that) */
    bool cell_psram = true, dirty_psram = true;
    cell = (uint16_t *)heap_caps_malloc(GEL_CELL_W * GEL_CELL_H * sizeof *cell, MALLOC_CAP_SPIRAM);
    if (!cell) { cell_psram = false; cell = (uint16_t *)malloc(GEL_CELL_W * GEL_CELL_H * sizeof *cell); }
    dirty = (dirty_t *)heap_caps_malloc(sizeof *dirty, MALLOC_CAP_SPIRAM);
    if (!dirty) { dirty_psram = false; dirty = (dirty_t *)malloc(sizeof *dirty); }
    dirty_begin(dirty);
    /* THE STAGE: internal, DMA-capable (the engine reads it); without it
     * every push goes from fb and the next compose waits, as in pass 1 */
    stage = (uint16_t *)heap_caps_malloc((size_t)STAGE_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    Serial.printf("[mem] fb %s, cell %u B %s, dirty %u B %s, stage %u B %s; heap %u free, psram %u of %u free\n",
                  fb ? "internal DMA" : "FAILED", (unsigned)(GEL_CELL_W * GEL_CELL_H * sizeof *cell), cell_psram ? "psram" : "INTERNAL",
                  (unsigned)sizeof *dirty, dirty_psram ? "psram" : "INTERNAL",
                  (unsigned)(STAGE_PX * 2), stage ? "internal DMA" : "NONE (pushes from fb)",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
                  (unsigned)ESP.getPsramSize());

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, BUS0_HZ);
    Wire.setTimeOut(BUS0_TIMEOUT_MS);          /* THE BUS UNSTUCK: a stuck read costs a frame 20 ms, not the core's 50 */
    uint8_t ia = imu_begin();
    Serial.printf("[imu] QMI8658 %s\n", ia ? "found" : "NOT FOUND");
    if (ia) Serial.printf("[imu] at 0x%02X, revision 0x%02X, INT1 on GPIO %d\n", ia, imu_revision(), PIN_IMU_INT1);
    touch_begin();
    bus_begin();                               /* HARDWARE ROOM: Wire1 + the dark drivers, only what `bom` says is soldered */

    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(bl_awake());    /* THE BROWNOUT LOOP: the floor when spent (bl_awake clamps on the line sampled above) */
    lcd.startWrite();
    lcd.fillScreen(TFT_BLACK);
    Serial.printf("[lcd] GC9A01 %dx%d up, DMA push\n", lcd.width(), lcd.height());

    /* settle the gravity filter before the first frame */
    for (int i = 0; i < 40; i++) {
        vec3_t a;
        if (imu_read(&a)) acc_lp = a;
        delay(5);
    }
    Serial.printf("[imu] resting acc %.2f %.2f %.2f -> screen down %.2f %.2f\n",
                  acc_lp.x, acc_lp.y, acc_lp.z, DOWN_X(acc_lp), DOWN_Y(acc_lp));
    last_motion_ms = last_touch_ms = millis();
    wake_up("boot");
    wdt_begin();                               /* THE LOOP'S PULSE: armed last, after the disk's mount and the imu's settle (both may take seconds) */
}

void loop()
{
    static uint32_t last_us = 0, last_frame_ms = 0;
    uint32_t now_us = micros();
    float dt = last_us ? (now_us - last_us) * 1e-6f : 0.02f;
    if (dt > 0.1f) dt = 0.1f;
    last_us = now_us;
    wdt_feed(WDT_SPELL_LOOP);                   /* THE LOOP'S PULSE: the thread is alive (once a second to the hardware) */
    death_tick(millis());                       /* THE LAST DEATH: the breadcrumb's uptime and link, once a second */

    console_tick();                             /* serial lines; a word keeps it awake */
    sense(dt);
    touch_tick();
    animate(dt);
    if (wake_take_shown()) {                    /* SHOWING SOMEONE (wake.cpp): the glass held toward someone -- the gel's most presentable thing, once per look */
        if (house_holding() && !(rest && rest->dozing) && fall_phase == FALL_NONE) {
            show_t = SHOW_S;
            eye_kind = GEL_EYE_CONTENT; eye_kind_t = SHOW_S;
            engage(SHOW_S + 0.5f);
            LOG("shown: company -- a look up, content eyes, one settle for %.0f s; his rings dim while the glass is theirs", SHOW_S);
        } else LOG("shown: company, but %s -- nothing", !house_holding() ? "the house is empty" : rest && rest->dozing ? "he is asleep" : "the gel is falling");
    }
    /* the added halves, polled: none may block the frame */
    glance_tick(dt);
    {   /* THE WATCH PAGE (page.cpp): its 8 s; up, it keeps the glass lit */
        const char *pl = page_tick(dt);
        if (pl) LOG("%s", pl);
        if (page_up()) app_keep_awake();
    }
    if (ble_pair_left_s() > 0) { app_keep_awake(); page_hold(); }   /* THE PAIRING WINDOW: the glass lit and the row's count up while it is open (as the PULSE row) */
    {   /* THE PULSE ROW (2026-10-03): while its read runs the glass stays lit and the page stays up; when the number lands, one bump */
        int left; bool settling;
        ppg_feed(last_jerk, dt, true);          /* THE STILL SPOT + THE PULSE IN THE DAY (2026-10-08): the wrist's jerk this frame -- the read waits on it, the creature's own read starts on it */
        if (ppg_spot_left(&left, &settling)) { app_keep_awake(); page_hold(); }
        uint32_t done = ppg_spot_done();
        { float gb; bool gok; if (ppg_spot(&gb, &gok)) gentle_pulse_seen(done, gb, gok, millis(), rest && rest->dozing); }   /* THE GENTLE WAKE: a finished spot, taken once by its count (the nap's poll feeds the same way) */
        if (ppg_recent_seq() != pulse_ring_seen) {   /* THE PULSE IN THE DAY: the creature's own read landed -- the breath's, the purr's and the gentle wake's (his asked reads take the block below) */
            pulse_ring_seen = ppg_recent_seq();
            int rb = 0; uint32_t rage = 0;
            if (ppg_rest_now(&rb, &rage) && pulse_valid((float)rb)) {
                pulse_bpm_last = (float)rb; pulse_at_ms = millis() - rage * 1000u;
                gentle_pulse((float)rb, true, millis(), rest && rest->dozing);
                LOG("pulse: %d bpm (the creature's own read) -- the breath's and the purr's for %d min", rb, PULSE_STALE_MIN);
            }
        }
        if (done != pulse_seen) {
            pulse_seen = done;
            {   /* THE PULSE AS THE BREATH: every clean result, whoever asked, is the breath's and the purr's (pulse.h) */
                float b = 0; bool k = false;
                if (ppg_spot(&b, &k) && k) {
                    if (pulse_valid(b)) { pulse_bpm_last = b; pulse_at_ms = millis(); LOG("pulse: %.0f bpm -- the breath's and the purr's for %d min", b, PULSE_STALE_MIN); }
                    else LOG("pulse: %.0f bpm is past the honest band (%.0f-%.0f): the breath and the purr ignore it", b, PULSE_BPM_MIN, PULSE_BPM_MAX);
                }
            }
            if (pulse_row_read) {
                float bpm = 0; bool ok = false;
                ppg_spot(&bpm, &ok);
                pulse_row_read = false;
                page_hold(); app_keep_awake();
                if (!store.bumps_off && haptic_play(HAPTIC_BUMP)) LOG("pulse: %s -- the row shows it; one bump", ok ? "landed" : "no clean pulse");
                else LOG("pulse: %s -- the row shows it", ok ? "landed" : "no clean pulse");
            }
        }
    }
    wrist_tick(state != DARK);
    net_tick();
    link_tick();
    bus_tick();                                 /* HARDWARE ROOM: the dark drivers' ticks (no-ops until `bom` says) */
    { const char *sl = sky_tick(dt); if (sl) LOG("%s", sl); }   /* THE SKY: its hold and its shiver's clock */
    { const char *ql = question_tick(word_showing(), word_text(), millis()); if (ql) LOG("%s", ql); }   /* THE WORD'S QUESTION: the window opens at the show, closes on the vote, the word's going or 10 s of silence */
    { int tn = tone_tick(word_showing(), word_text(), millis()); if (tn) LOG("tone: the word is %s -- %s", tone_name(tn), tn == TONE_GLAD ? "one bright settle, happy eyes" : "one slow blink, the lids half-down a second"); }   /* THE WORD'S TONE (tone.c): once per shown word, at its show; a plain stop and a ? play nothing here */
    {   /* THE WARM SKIN (warm.c, watch-night C20): the die temperature the pulse sensor read at the end of its last spot, fed once
         * each (the proxy, the band); the band's debts paid here, BEFORE rest takes its frame and the tell takes the raise's cue --
         * WARM: on the raise (lit, holding, awake, no page) one content settle (draw() folds it; the eyes CONTENT the same 1.5 s);
         * COLD: on the first lit, holding, awake frame the shiver and the slow breath, PUT ON COLD's road (rest_in_t.chilled below).
         * Never a mark, never a number, never a word; a watch on the desk reads the room (the plate is thick: ruling 148) */
        if (ppg_die_temp_seq() != warm_seq_seen) {
            warm_seq_seen = ppg_die_temp_seq();
            if (warm_feed(ppg_die_temp_c()) == WARM_EV_BAND)
                LOG("warm: the skin reads %s (the die %.1f C, the proxy %.1f C)%s", warm_word(warm_band()), warm_last_c(), warm_c(),
                    warm_band() == WARM_WARM ? " -- one content settle on the next raise" : warm_band() == WARM_COLD ? " -- the shiver, the breath slow to warm" : "");
        }
        int wa = warm_tick(state != DARK, house_holding(), !(rest && rest->dozing), tell_cue == TELL_CUE_RAISE, page_up());
        if (wa == WARM_ACT_SETTLE) { warm_settle_t = WARM_SETTLE_S; eye_kind = GEL_EYE_CONTENT; eye_kind_t = WARM_SETTLE_S; engage(WARM_SETTLE_S + 0.5f);
                                     LOG("warm: the raise on a warm skin -- one content settle (%.2f over %.1f s)", WARM_SETTLE_SQUASH, WARM_SETTLE_S); }
        else if (wa == WARM_ACT_SHIVER) rest_chilled = true;   /* rest.cpp says its own line */
    }
    {   /* THE NIGHT AND THE BREATH (rest.cpp): one call a frame, its
         * numbers read in draw(); the sense and the log line it hands back */
        rest_in_t rin = { dt, clock_local_minute(), state != DARK, wrist_motion_events(), wrist_walking(),
                          link_linked(), quiet_hours_str(),              /* the desk's grain, else his own QUIET FROM / TO */
                          pulse_bpm_last, pulse_age_s(),                 /* THE PULSE AS THE BREATH: the last clean pulse and its age */
                          rest_woke_raise,                               /* THE WAKING BREATH: this frame only */
                          lull_level(),                                  /* THE LULL: the breath's target toward the sleeper's by its level */
                          rest_chilled,                                  /* THE WARM SKIN: this frame only -- the shiver, the breath snapped slow */
                          ppg_breath_now(NULL, NULL) };                  /* THE BREATH READ: the last clean respiration rate, carried, not followed (ruling 154) */
        rest = rest_tick(&rin);
        rest_woke_raise = false;
        rest_chilled = false;
        itch_rad = rest->itch_deg * (float)M_PI / 180.0f;   /* THE ITCH IN THE BOWL: animate() adds it to the bowl's target */
        grain_tick(dt);                                     /* THE BELLY TRAVELS: the desk's belly sinking over the hour ... */
        link_belly(fmaxf(gesture_lump(), tell_lump()));     /* ... and the wrist's own going back, once per change */
        if (rest->log_new) { LOG("%s", rest->log); rest_log_taken(); }
        if (rest->sense_new) { link_sense(rest->sense_kind, rest->sense_words); rest_sense_sent(); }
    }
    {   /* THE PHONE LEFT BEHIND (phonegone.c, watch-night C17): the phone's BLE dropping (the stack still up) on a wrist that walked
         * inside the last 60 s = he walked away from it; on his NEXT raise, once, a look toward the air door (the wisps' door, the rim
         * opposite the floor) on the far tap's own road for 1.5 s, then at him; cleared when the phone is back first; the raise's cue
         * is read here BEFORE the tell takes it; the nap loop feeds the same edge in the dark; never a bump, a word or a sense */
        phonegone_in_t pgn = { millis(), ble_is_up(), ble_phone_ready(), wrist_walking(), tell_cue == TELL_CUE_RAISE, house_holding(), state != DARK, page_up() };
        int pge = phonegone_tick(&pgn);
        if (pge == PHONEGONE_EV_LOOK) { float dpx, dpy; phonegone_point(bowl_phi, SCREEN_W / 2.0f, SCREEN_H / 2.0f, PHONEGONE_R, &dpx, &dpy); look_at((int)dpx, (int)dpy, PHONEGONE_S); engage(PHONEGONE_S + 0.5f);
                                        LOG("phone left behind: the raise -- a look toward the air door at %.0f,%.0f (%+d,%+d) for %.1f s, then at him", dpx, dpy, far_gx, far_gy, PHONEGONE_S); }
        else if (pge == PHONEGONE_EV_GONE) LOG("phone left behind: the phone's BLE dropped on a walking wrist -- a look toward the door on the next raise (walk-away %d)", phonegone_drops());
        else if (pge == PHONEGONE_EV_BACK) LOG("phone left behind: the phone is back before the raise -- nothing owed");
    }
    {   /* THE TELL (tell.cpp): the day's freshest fact on a raise or an
         * arrival, the step bob, the night light -- one call a frame with
         * rest's and the day log's numbers; its squash and eye are read
         * in draw(), its kicks land on the springs here, its light on the
         * backlight below */
        rest_span_t sp; bool have_sp = rest_last_span(&sp);
        tell_in_t tin;
        memset(&tin, 0, sizeof tin);
        tin.dt = dt; tin.cue = tell_cue; tell_cue = TELL_CUE_NONE;
        tin.holding = house_holding(); tin.lit = state != DARK;
        tin.asleep = rest->asleep; tin.dozing = rest->dozing; tin.night = rest_night_now();
        tin.dusk = clock_local_minute() >= 0 && !rest_night_at(clock_local_minute())
                   && rest_night_at((clock_local_minute() + TELL_DUSK_BEFORE_MIN) % 1440);   /* THE YAWN AT DUSK: the hour before the quiet hours */
        tin.minute = clock_local_minute(); tin.date = clock_local_date(); tin.rows = rest_rows();
        tin.span_known = have_sp;
        if (have_sp) { tin.span_min = sp.minutes; tin.span_end_minute = sp.end_minute; tin.span_awakenings = sp.awakenings;
                       tin.span_restless_min = sp.restless_min; tin.span_end_row = sp.end_row; tin.span_night = sp.night; }
        tin.sit_min = rest_sit_min(); tin.steps_hour = rest_steps_last(60); tin.rows_since_walk = rest_rows_since_walk();
        tin.steps_today = wrist_steps_today(); tin.norm = face_norm(); tin.step_seq = wrist_step_seq();
        { int wh, wn; if (nightsum_weather(&wh, &wn)) { tin.weather_hr = wh; tin.weather_norm = wn; } }   /* UNDER THE WEATHER: today's resting heart over the week's (nightsum.c) */
        link_grain_day(&tin.desk_sat_min, &tin.desk_pets);   /* ONE BODY ACROSS THE HOUSES (C21): the desk's sit and pets off a fresh grain line, 0 / 0 when stale or never */
        tell = tell_tick(&tin);
        if (tell->log_new) { LOG("%s", tell->log); tell_log_taken(); }
        if (tell->kick1 != 0) spring1.kick(tell->kick1);
        if (tell->kick2 != 0) spring2.kick(tell->kick2);
        if (tell->eye_kind >= 0) engage(EYE_HOLD_WAKE_S);       /* the answer is given with the eyes in */
        glance_set_shade(tell->night_light);                     /* the hour in the shade under the night light */
    }
    {   /* CLOCKWORK (clockwork.cpp): the stopwatch, the timer, the alarm --
         * one call a frame with the clock, the house and the sleep; its
         * squash, breath factor and eyes are read in draw(), a bounce's
         * landing kick lands on the spring here, its haptic is played
         * (dark until the part is soldered), a ring's first seconds keep
         * the glass lit */
        clockwork_in_t cin = { dt, millis(), clock_local_minute(), clock_local_date(), state != DARK, house_holding(), rest->dozing };
        cw = clockwork_tick(&cin);
        if (cw->log_new) { LOG("%s", cw->log); clockwork_log_taken(); }
        if (cw->kick2 != 0) spring2.kick(cw->kick2);
        if (cw->haptic >= 0) haptic_play(cw->haptic);
        if (cw->keep_awake) app_keep_awake();
    }
    ble_tick(dt);                           /* BLUETOOTH: the phone's notices -> wisp_arrive (below); the 30 s budget line while up */
    /* THE WISP (wisp.cpp): the desk's doorbell over the link, the phone's
     * over BLE; while he is home; it waits on a dark glass for the light
     * (never makes it); glanced by a touch, a wake, the hour showing */
    wisp_tick(millis(), dt, bowl_phi, house_holding(), state != DARK, touch_was_down || wake_t > 0 || glance_on());
    {   /* THE BUMPS: it landed beside him (E40) -- any number landing this frame is one touch; THE NOTICE'S OWN REACTION
         * (notice.h): the touch is the STRONGEST landed notice's kind and plays; none = the mote alone, nothing felt */
        int kind = -1, reps = 0;
        int n = wisp_take_landed_react(&kind, &reps);
        if (n && kind >= 0) bump_as(BUMP_WISP, kind, reps);
        else if (n) LOG("bump: the wisp landed -- the notice's own reaction is none (news, a silent one): nothing felt");
        int faded = wisp_take_door_faded();           /* THE WISP AT THE DOOR (E43): an hour unclaimed, it faded -- nothing fails quietly */
        if (faded) LOG("wisp: %d at the air door an hour with nobody home: faded (the notice stays on the page)", faded);
    }
    {   /* THE WISP WATCHED (wispeye.c; C3): the newest mote's point against the gel's centre (look_at's own reckoning)
         * -> the gaze through its flight and its two seconds at the post; a swelled halo one blink over the open face */
        wispeye_in_t win = { dt, state != DARK, house_holding(), false, 0, 0, false, 0, 0, 0, bowl_phi, wisp_take_swelled() };
        win.have = wisp_eye_point(&win.wx, &win.wy); win.seq = wisp_eye_seq(&win.flying);
        float wgx = sinf(bowl_phi), wgy = cosf(bowl_phi), wrx, wry; gel_blob_radii(WRIST_SQUASH, &wrx, &wry);
        float wd = (float)GEL_FLOOR_DROP - lift - wry * GEL_SCALE * 0.5f;
        win.cx = SCREEN_W / 2.0f + wgx * wd; win.cy = SCREEN_H / 2.0f + wgy * wd;
        wispeye_tick(&win, &wisp_eye);
        if (wisp_eye.began) { engage(WISPEYE_EYES_S); LOG("wisp: the newest watched -- the gaze follows it in, rests %.0f s at its post", (double)WISPEYE_DWELL_S); }
        if (wisp_eye.blink && eye_kind == GEL_EYE_OPEN && eye_kind_t <= 0) { eye_kind = GEL_EYE_CLOSED; eye_kind_t = WISPEYE_BLINK_S; }   /* the swell's one blink, over the ordinary face only */
    }
    {   /* THE WRIST WARDROBE: the band clocks, the veil, the act once a
         * wear, the Sage under the word; THE BORED ROLL on the bare glass
         * (no page, awake, the body free -- the idle block's own busy
         * list, the costume itself excepted) */
        bool bare = !page_up() && !(rest && rest->asleep) && !touch_was_down && pet_t <= 0 && fall_phase == FALL_NONE
                    && wake_t <= 0 && eye_kind_t <= 0 && !(rest && rest->stretch > 0) && !clockwork_busy() && !vigor_playing() && peek_k <= 0 && !play_busy();
        wardrobe_tick(dt, state != DARK, house_holding(), word_showing(), bare);
    }
    {   /* PLAY: THE BEAD AND THE SPIN (play.cpp): one call a frame with
         * the wrist (in-plane gravity in the glass's frame, the jerk, the
         * gyro's yaw) and the gates (lit, the quiet hours, the shelf, the
         * house, the page, the word); the spin's start and the bead's
         * roll come back. The body's reactions are the springs' and the
         * eyes' own; the bead is drawn in draw_bead. */
        play_in_t pin;
        memset(&pin, 0, sizeof pin);
        pin.now_ms = millis(); pin.dt = dt;
        pin.gx = DOWN_X(acc_lp); pin.gy = DOWN_Y(acc_lp); pin.jerk_g = last_jerk;
        pin.jx_g = last_jerk_x; pin.jy_g = last_jerk_y;   /* THE TOSS: the flick's way */
        vec3_t gr;
        if (imu_gyro(&gr)) { pin.yaw_dps = gr.z * yaw_sign(); pin.rate_dps = sqrtf(gr.x * gr.x + gr.y * gr.y + gr.z * gr.z); }
        pin.gates.lit = state != DARK; pin.gates.quiet = rest_night_now(); pin.gates.shelf = rest_on_shelf();
        pin.gates.holding = house_holding(); pin.gates.page = page_up(); pin.gates.word = word_showing();
        /* CATCH THE DROP / THE COURSE (F50 / F52): the wall he sits on (the air door and the gates are read off it), whether he is landed on it (a wall change read then is a fall taken), and his painted box (the drop inside it is caught) */
        pin.wall = wall; pin.on_wall = fall_phase == FALL_NONE || fall_phase == FALL_IMPACT || fall_phase == FALL_LAND;
        gel_face_box(&pin.box_x, &pin.box_y, &pin.box_w, &pin.box_h);
        if (!house_holding()) pin.box_w = 0;
        pin.gates.asleep = rest && rest->asleep; pin.gates.falling = fall_phase != FALL_NONE;   /* (2026-10-09) nobody tosses a sleeper; the fall is the fall */
        int played = play_tick(&pin); int play_started = played;   /* (the merge: the drop/course hand reads play_started, the toss hand played) */
        if (played == PLAY_TOSS) {            /* THE TOSS (F48): in the air -- the loft rides the compose's lift; the landing below */
            engage(TOSS_SQUINT_S + 0.5f);
            idle_pet();
            if (play_toss_sense_due()) link_sense("wrist:play", "tossed");
        }
        if (play_toss_landed()) {             /* ... and it lands: the small splat (half the fall's: the squash is play's, the kick and the squint here) */
            spring2.kick(TOSS_KICK);
            eye_kind = GEL_EYE_SQUINT; eye_kind_t = TOSS_SQUINT_S;
            engage(TOSS_SQUINT_S + 0.5f);
        }
        if (play_rhythm_done()) {             /* THE RHYTHM (F51): the beat echoed -- the happy eyes */
            eye_kind = GEL_EYE_HAPPY; eye_kind_t = RHYTHM_HAPPY_S;
            engage(EYE_HOLD_PET_S);
        }
        if (played == PLAY_SPIN) {            /* THE SPIN: dizzy -- the squint, one kick against the turn, the slow wobble below */
            eye_kind = GEL_EYE_SQUINT; eye_kind_t = SPIN_DIZZY_S;
            engage(SPIN_DIZZY_S + 0.5f);
            spring1.kick(-play_dizzy_sign() * SPIN_KICK);
            spring2.kick(SPIN_KICK * 0.5f);
            idle_pet();
            cramp_play(pin.now_ms);           /* THE SMALL HOUSE: the spin is an outlet */
            link_sense("wrist:play", "spin");
        } else if (play_started == PLAY_COURSE) {   /* THE COURSE (F52): three gates on the other walls -- the eyes up, the sense once */
            engage(EYE_HOLD_PET_S);
            idle_pet();
            link_sense("wrist:play", "course");
        }
        {   /* CATCH THE DROP (F50) and THE COURSE's moments: the body's reactions through the eyes and the springs that exist */
            uint32_t pev = play_events();
            float dxp, dyp;
            if (play_drop_on(&dxp, &dyp)) { look_at((int)dxp, (int)dyp, 0.2f); engage(0.5f); }   /* the gel watches the drop fall, as it watches the bead */
            if (pev & PLAY_EV_CAUGHT) {   /* caught: the meal's lump (play_lump, on the belly's own term below), the happy eyes, one settle */
                eye_kind = GEL_EYE_HAPPY; eye_kind_t = CATCH_HAPPY_S;
                engage(EYE_HOLD_PET_S);
                spring2.kick(PET_BOUNCE * 0.5f);
            }
            if (pev & PLAY_EV_MISSED) { float mxp, myp; if (play_mark_on(&mxp, &myp, NULL)) look_at((int)mxp, (int)myp, 1.0f); }   /* missed: a look at the wet mark, no more (nothing reads as a scolding) */
            if (pev & PLAY_EV_GATE) engage(EYE_HOLD_PET_S);   /* a gate lit: the eyes stay up */
            if (pev & PLAY_EV_COURSE_DONE) {   /* all three: the content eyes and one contented settle (the drink's) */
                eye_kind = GEL_EYE_CONTENT; eye_kind_t = COURSE_CONTENT_S;
                engage(EYE_HOLD_PET_S);
                spring2.kick(-0.9f);
            }
        }
        float dz = play_dizzy();
        if (dz > 0) {   /* THE SLOW WOBBLE on both springs: a slow sine's force, fading with the spell */
            float t = (1.0f - dz) * SPIN_DIZZY_S;
            float f = SPIN_DRIVE * dz * sinf(2.0f * (float)M_PI * SPIN_WOBBLE_HZ * t);
            spring1.kick(f * play_dizzy_sign() * dt);
            spring2.kick(0.5f * f * dt);
        }
        float bxp, byp;
        if (play_bead_on(&bxp, &byp)) {       /* THE BEAD: the gel watches it round the bowl; into him, it is eaten */
            look_at((int)bxp, (int)byp, 0.2f);
            engage(0.5f);
            int x, y, w, h; gel_face_box(&x, &y, &w, &h);
            if (w > 0 && house_holding() && bxp >= x && bxp < x + w && byp >= y && byp < y + h) {
                play_bead_eat(pin.now_ms);
                eye_kind = GEL_EYE_HAPPY; eye_kind_t = BEAD_HAPPY_S;
                engage(EYE_HOLD_PET_S);
                spring2.kick(PET_BOUNCE * 0.5f);
            }
        }
        if (play_log_new()) { LOG("%s", play_log()); play_log_taken(); }
    }
    {   /* THE HOUSE (house.cpp): the theatre's clocks and the offline
         * rule (no link, worn = the default home); the arrival's moment
         * is the body's own wake -- eyes in, a small settle */
        /* THE RADIO BUDGET: a radio resting by choice (no dial due) is
         * not offline -- only a due dial that found no door lets the
         * offline clock run */
        house_tick(dt, link_linked() || link_resting(), millis() - last_motion_ms);
        int ev = house_take_event();
        if (ev == HOUSE_EV_ARRIVED) { eye_kind = GEL_EYE_OPEN; engage(EYE_HOLD_WAKE_S); spring2.kick(0.6f); tell_cue = TELL_CUE_ARRIVAL; bump(BUMP_RISE); cramp_arrive(); }   /* THE TELL: a knock's arrival answers too; THE BUMPS: the rise; THE SMALL HOUSE: fresh */
        else if (ev == HOUSE_EV_LEFT) { eye_hold = 0; cramp_release("the house emptied"); }   /* THE SMALL HOUSE: the release by any road (the hold, `no desk`, a knock) zeroes the cramp -- a wish granted is logged */
        if (house_take_sink()) bump(BUMP_SINK);              /* THE BUMPS: the goodbye's settle done, the sink begins -- he feels it go */
        purr_tick();                                         /* THE BUMPS: a purr armed at a pet rides the breath's next inhale */
        bump_again_tick();                                   /* THE NOTICE'S OWN REACTION: a wisp's second play, 250 ms after its first */
        /* THE WATCH FACE (face.cpp): the numbers behind him this frame;
         * the departure leaves the trace, the arrival takes it back; the
         * norm is re-read when the date turns */
        house_fx_t hx; house_fx(&hx);
        uint32_t sync_sent = 0, sync_total = 0; bool sync_on = link_sync_progress(&sync_sent, &sync_total);   /* THE DOCK FACE: the day being carried */
        face_in_t fin = { dt, state != DARK, house_holding(), hx.gel, ev, clock_local_minute(), glance_on() && !tell->night_light,   /* the night light: the resting hour stays in the shade */
                          wrist_steps_today(), rest_night_min(), batt_mv(), charging(),
                          sync_on, sync_sent, sync_total, link_sync_done_seq(), link_docked(),
                          link_reaching(), wall,                    /* THE REACHING CUE: the rim ripples at his wall while a come is out */
                          clockwork_alarm(),                     /* THE ALARM'S TICK: the shine tick at its hour on the rim */
                          store.h12,                             /* MORE SETTINGS' CLOCK row: the resting hour as 7:41 or 07:41 */
                          wake_showing() && house_holding(),     /* SHOWING SOMEONE: his rings dim while the glass is held toward company */
                          nightsum_deep(rest_night_id()) && !rest->dozing,   /* THE NIGHT RING KNOWS HIS HEART: the last night's dip was there -> the shine (never the night growing now) */
                          nightsum_wander(NULL, NULL),           /* ... the week's sleep midpoints wander -> the ring's tip breathes */
                          trace_stir };                          /* THE TRACE STIRRED (C14): the empty-glass tap near the trace, this frame only */
        trace_stir = false;
        const char *fl = face_tick(&fin);
        if (fl) LOG("%s", fl);
        if (clock_local_date() != face_norm_date) face_refresh_norm();
    }
    {   /* THE HOURS' LIGHT (hours.cpp) + IDLE LIFE (idle.cpp): one tick each.
         * The hour's tint and backlight factor (the grain's quiet hours are
         * the night); the small things a creature at rest does -- only lit,
         * holding, awake, with nothing else on the body (a touch, a pet, a
         * fall, the theatre, the wake's ease, a gesture's face, the stretch)
         * and the gel at rest (the springs quiet, the bowl settled, the
         * wrist still for 2 s) */
        hours_tick(dt, clock_local_minute(), quiet_hours_str());   /* the same quiet hours as rest's */
        {   /* VIGOR (vigor.cpp): the cell's level, one tick -- the gauge's
             * percent when the MAX17048 answers (dark until soldered), else
             * the ADC's mV; rest and idle read its factors, draw() its look;
             * the cable's settle wants the eyes in, as the tell's answer does */
            uint16_t gmv; uint8_t gpct;
            vigor_tick(dt, batt_mv(), charging(), gauge_read(&gmv, &gpct) ? (int)gpct : -1, state != DARK, house_holding());
            spent_sense(&spent, batt_mv(), vigor_spent(), millis());   /* THE LAST DROP'S MANNERS: the spent line's settle */
            if (vigor_playing()) engage(EYE_HOLD_WAKE_S);
            glance_set_short(vigor_thrift());      /* BATTERY MANNERS: the glance holds shorter under 20 % */
        }
        house_fx_t ihx; house_fx(&ihx);
        idle_in_t iin;
        iin.dt = dt; iin.now_ms = millis();
        iin.lit = state != DARK; iin.holding = house_holding(); iin.asleep = rest && rest->asleep;
        iin.busy = touch_was_down || pet_t > 0 || fall_phase != FALL_NONE || ihx.squash != 0 || ihx.eye_k < 1.0f || wardrobe_worn()   /* THE WRIST WARDROBE: a costume has no idle life of the gel's */
                   || wake_t > 0 || eye_kind_t > 0 || (rest && rest->stretch > 0) || clockwork_busy()   /* CLOCKWORK: the held breath, the exhale, the bounces */
                   || vigor_playing()                                                                   /* VIGOR: the cable's settle has the body */
                   || peek_k > 0                                                                        /* PEEKABOO: hidden, or popping back */
                   || shove_t >= 0                                                                      /* THE SHOVE: skidding, or sliding back */
                   || play_busy()                                                                       /* PLAY: a bead in the bowl, or the spin's dizzy spell */
                   || hang_t > 0 || show_t > 0 || (rest && rest->cold_look > 0)                         /* THE SMALL MANNERS: the stand-up, the presentable second, the cold look up */
                   || lull_on();                                                                        /* THE LULL: petted toward rest -- idle life quiet for its 45 s */
        iin.moving = fabsf(spring1.a) > 0.02f || fabsf(spring2.a) > 0.02f
                     || fabsf(tilt_k - tilt_target) > 0.02f || wall_over_s > 0 || millis() - last_motion_ms < 2000;   /* the residual still easing in (the itch's offset included), or a wall change pending */
        iin.eye_alpha = eye_alpha;
        iin.cramp = cramp_level();                                                                          /* THE SMALL HOUSE: the roam's weight */
        idle_tick(&iin);
        {   /* THE STARE RETURNED (stare.c, watch-night C12): 8 s of lit, raised, still, untouched glass is him looking at it --
             * once a spell the gaze up, content, one soft settle (folded into draw()); it waits for idle's thing and hands idle
             * the pet's own quiet after; the dim has the drowse's look and a low cell's dim lands on the same second (neither stares) */
            stare_in_t stn = { dt, state != DARK, house_holding(), wake_face_of(&acc_lp) >= STARE_FACE, !iin.moving,
                               touch_last.down || touch_was_down, iin.busy || state == DIM || vigor_low() || (rest && rest->dozing), page_up(), word_showing(), idle_playing() };   /* the review: never over a sleeper's closed lids */
            if (stare_tick(&stn) == STARE_EV_LOOK) { idle_pet(); engage(STARE_LOOK_S + 0.5f); LOG("stare: %.0f s watched -- a look up at him, content, one settle (idle quiet %.0f s after)", STARE_S, STARE_QUIET_S); }
        }
        {   /* THE EAR TO THE WRIST (listen.c, watch-night C19): while the creature's OWN pulse read runs with its LEDs lit (THE PULSE
             * IN THE DAY; never his PULSE row's, which keeps its number on the page), the ear pressed to him -- the gaze down toward
             * the strap, content, the look only; a clean read ends with one settle (folded into draw()), a failed one with nothing.
             * The eyes are engaged for the read's length and idle handed the pet's quiet so no glance of its own lands on the look */
            int oleft; bool oglow;
            bool oread = ppg_own_left(&oleft, &oglow);
            listen_in_t lsn = { dt, oread, oglow, ppg_recent_seq(), state != DARK, house_holding(), iin.busy || state == DIM || vigor_low() || (rest && rest->dozing), page_up() };   /* the review: never over a sleeper's closed lids */
            int lev = listen_tick(&lsn);
            if (lev == LISTEN_EV_EAR) { idle_pet(); engage((float)oleft + 1.5f); LOG("listen: the pulse in the day under the glass -- the ear to the wrist, %d s", oleft); }
            else if (lev == LISTEN_EV_CLEAN && listen_heard()) LOG("listen: a clean read -- %s", listen_settling() ? "one settle" : "ended unseen, no settle");
            else if (lev == LISTEN_EV_FAILED && listen_heard()) LOG("listen: the read spoiled -- the ear lifts, nothing");
        }
        {   /* THE HOUR'S GLANCE (hourlook.c, watch-night C15): the minute turning :00 -- one glance toward the stacked hands' rim
             * point (the hour tick's angle, face.cpp's) on the far tap's own road for 1.5 s, then forward; no sound, no bump */
            hourlook_in_t hln = { dt, clock_local_minute(), state != DARK, house_holding(), iin.busy || idle_playing() || word_showing() || (rest && rest->dozing), page_up() };   /* the review: no glance under the Zs */
            if (hourlook_tick(&hln) == HOURLOOK_EV_GLANCE) {
                int hd = face_hour_tick_deg(); float hpx, hpy;
                if (hd >= 0) { hourlook_point(hd, SCREEN_W / 2.0f, SCREEN_H / 2.0f, HOURLOOK_R, &hpx, &hpy); look_at((int)hpx, (int)hpy, HOURLOOK_S); engage(HOURLOOK_S + 0.5f);
                               LOG("hour's glance: the minute turned :00 -- a look toward the hands at %d deg (%+d,%+d) for %.1f s", hd, far_gx, far_gy, HOURLOOK_S); }
            }
        }
        {   /* THE DESK NEXT DOOR (near.c, watch-night C9): the desk's RSSI over the air door asked every 2 s on the lit glass
             * (ble_desk_rssi: 0 with no desk on the air -- at home on WiFi there is none, ruling 58), banded FAR / NEAR / AT HAND;
             * the RISE into AT HAND from a held lower band, lit, holding, the body free, no page = one glance toward the air
             * door (the wisps' rim point) for NEAR_GLANCE_S, the far tap's road; never a bump, never a mark; the band feeds the cramp below */
            if (near_due(millis(), state != DARK)) {
                int nev = near_feed(ble_desk_rssi());
                if (nev == NEAR_EV_ATHAND && state != DARK && house_holding() && !(iin.busy || idle_playing() || word_showing()) && !page_up()) {
                    float nx, ny; wisp_door_point(bowl_phi, &nx, &ny); look_at((int)nx, (int)ny, NEAR_GLANCE_S); engage(NEAR_GLANCE_S + 0.5f);
                    LOG("near: the desk came AT HAND (%d dBm) -- a glance toward the air door (%+d,%+d) for %.1f s", near_dbm(), far_gx, far_gy, NEAR_GLANCE_S);
                } else if (nev) LOG("near: the desk over the air door is %s (%d dBm)%s", near_word(near_band()), near_dbm(), nev == NEAR_EV_ATHAND ? " -- the glance kept (dark / empty / busy / a page)" : "");
            }
        }
        {   /* THE SMALL HOUSE (cramp.c): the cramp's clock on lit, holding, lively minutes; the outlets
             * spend it (the hooks above); the nudge at fullness when the desk's own door is near (the
             * slime's own roll), the wrist's own bid after a long ignored spell; the stance, the push and
             * the look are folded into draw(). RESTING (a low cell, the hour before the quiet hours) pauses
             * the clock: the small warm bowl is the place for it */
            cramp_in_t cin;
            memset(&cin, 0, sizeof cin);
            cin.dt = dt; cin.now_ms = millis();
            cin.lit = state != DARK; cin.holding = house_holding(); cin.asleep = rest && rest->dozing;
            cin.shelf = rest_on_shelf(); cin.cable = charging() || link_docked(); cin.quiet = rest_night_now();
            { int m = clock_local_minute();
              bool dusk = m >= 0 && !rest_night_at(m) && rest_night_at((m + TELL_DUSK_BEFORE_MIN) % 1440);   /* THE YAWN AT DUSK's hour */
              cin.resting = vigor_low() || dusk; }
            cin.walking = wrist_walking();
            cin.busy = iin.busy || idle_playing() || page_up() || word_showing();
            { uint32_t d = link_desk_ok_ms(); cin.desk_age_s = d ? (float)((millis() - d) / 1000u) : -1.0f; }
            cin.regard = link_grain()->have ? link_grain()->regard : 0.0f;
            cin.desk_far = near_band() == NEAR_FAR;   /* THE DESK NEXT DOOR: reachable over the air door, but FAR by its signal = not near (no band = nothing said) */
            cramp_tick(&cin);
            int ce = cramp_take_event();
            if (ce == CRAMP_EV_NUDGE) { bump(BUMP_NUDGE); engage(CRAMP_NUDGE_LOOK_S + 0.5f); link_sense("wrist:cramped", "cramped in the small house, the desk near"); }   /* THE BUMPS' gate says whether; the wire tells the desk */
            else if (ce == CRAMP_EV_BID) { bump(BUMP_NUDGE); engage(BID_LOOK_S + 0.5f); }
            else if (ce == CRAMP_EV_STAY) { eye_kind = GEL_EYE_CONTENT; eye_kind_t = CRAMP_CONTENT_S; engage(EYE_HOLD_PET_S); spring2.kick(0.5f); }   /* stay a while: one settle */
            else if (ce == CRAMP_EV_BID_ANSWERED) { eye_kind = GEL_EYE_CONTENT; eye_kind_t = CRAMP_CONTENT_S; engage(EYE_HOLD_PET_S); }
            if (cramp_log_new()) { LOG("%s", cramp_log()); cramp_log_taken(); }
        }
    }
    app.wall = wall;
    app.phi_deg = deg(bowl_phi);
    app.state = (int)state;

    uint32_t now = millis();
    uint32_t idle = now - (last_motion_ms > last_touch_ms ? last_motion_ms : last_touch_ms);
    /* VIGOR: a low cell naps sooner (8 / 12 s for the 12 / 20) */
    const uint32_t dim_ms = spent_ms(spent_line(), vigor_low() ? DIM_LOW_MS : spell_dim_ms(), SPELL_DIM_MS[1]),      /* THE LAST DROP'S MANNERS: the SHORT spell whatever the row says */
                   sleep_ms = spent_ms(spent_line(), vigor_low() ? SLEEP_LOW_MS : spell_sleep_ms(), SPELL_SLEEP_MS[1]);
    if (state == AWAKE && idle > dim_ms) {
        state = DIM;
        lcd.setBrightness(BL_DIM);
        char why[48]; snprintf(why, sizeof why, "still %lu s: dimming%s", (unsigned long)(dim_ms / 1000u), vigor_low() ? " (a low cell)" : "");
        log_state(why);
    } else if (state == DIM && idle <= dim_ms) {
        state = AWAKE;
        lcd.setBrightness(hours_level(bl_awake()));   /* THE HOURS' LIGHT */
        log_state("moved");
    }
    {   /* THE DIM'S DROWSE (drowse.c, watch-night C7): the dim edge is a look -- droopy lids, a hair of slump, the
         * breath a notch slower; a touch or a raise brings the lids up at once with the pet's own engage; the dark
         * takes it silently; never asleep, on the empty glass or over a page; a look, never Zs (ruling 123) */
        drowse_in_t din = { dt, state != DARK, state == DIM, touch_last.down, house_holding(), rest && rest->dozing, page_up(),
                            rest ? rest->breath_hz : BREATH_HZ, rest ? rest->breath_rest_hz : BREATH_HZ };
        int dev = drowse_tick(&din);
        if (dev == DROWSE_EV_UP) { engage(EYE_HOLD_PET_S); LOG("drowse: the lids up (%s)", touch_last.down ? "a touch" : "a raise"); }
        else if (dev == DROWSE_EV_DOWN) LOG("drowse: the dim -- droopy lids, the breath a notch slower (a look, never Zs)");
    }
    {   /* THE LULL (lull.c, watch-night C11): three slow strokes inside a minute were a hand petting him toward rest --
         * after the third's content face the lids drift DROOPY, the breath's target toward the sleeper's (rest_in_t.lulled),
         * idle quiet (the busy line), for LULL_S or until a pet (pet()), a jolt, a wall change or the dark (go_dark);
         * never dozing, never on the empty glass, never the Zs, never a sense */
        lull_in_t lin = { dt, state != DARK, house_holding(), rest && rest->dozing, wall, last_jerk };
        int lev = lull_tick(&lin);
        if (lev == LULL_EV_BEGIN) LOG("lull: petted toward rest -- the lids droopy, the breath toward %.2f Hz, idle quiet, %.0f s (a look, never Zs)", 0.30f, LULL_S);
        else if (lev == LULL_EV_END) {
            int lc = lull_end();
            if (lc == LULL_END_JOLT || lc == LULL_END_WALL) engage(EYE_HOLD_PET_S);   /* startled up: the open eyes come in (a pet brings its own) */
            LOG("lull: over -- %s%s", lull_end_name(lc), lc == LULL_END_TIME ? " (the lids drift back)" : "");
        }
    }
    {   /* THE WEIGHTLESS BEAT (fall0.c, watch-night C8): |a| under FREEFALL_G for FREEFALL_MS on the lit glass is a bowl
         * in free fall -- the gel lofts off its wall (the toss's lift road, in draw), the eyes WIDE, the breath held; an
         * impact over IMPACT_G inside the second lands it with the fall's own splat (fall0's squash, in draw) and THE
         * BRACE's minute (it was dropped: it hugs the floor); none = it eases down, the ordinary face. Lit only (the
         * dark's 250 ms poll cannot read a drop); never asleep, never on the empty glass, never inside a fall */
        fall0_in_t fin0 = { dt, sqrtf(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z), state != DARK, house_holding(), rest && rest->asleep, fall_phase != FALL_NONE };
        int fev = fall0_tick(&fin0);
        if (fev == FALL0_EV_LOFT) { eye_kind = GEL_EYE_WIDE; eye_kind_t = IMPACT_WAIT_MS / 1000.0f + FALL0_EASE_S; engage(2.0f); LOG("fall0: weightless -- the gel lofts off the %s wall, eyes wide, the breath held", GEL_WALL_NAMES[wall]); }
        else if (fev == FALL0_EV_IMPACT) {   /* the kick, the squint and the brace are fall_splat's own, by hand (no fall ran: the wall is still his) */
            spring2.kick(KICK_CAP); eye_kind = GEL_EYE_SQUINT; eye_kind_t = 0.45f; engage(2.0f);
            if (!rest_on_shelf()) { brace_until = millis() + (uint32_t)(BRACE_S * 1000.0f); if (!brace_until) brace_until = 1; eye_kind_t = BRACE_S; }
            LOG("fall0: dropped -- the splat%s", rest_on_shelf() ? "" : ", braced for a minute (it hugs the floor)");
        }
        else if (fev == FALL0_EV_CLEAR) { if (eye_kind == GEL_EYE_WIDE) { eye_kind = GEL_EYE_OPEN; eye_kind_t = 0; } LOG("fall0: no landing inside the second -- it eases down"); }
    }
    {   /* THE KNOCK FROM BEHIND (knock.c, watch-night C13): a rap on the glass's back or the table it rests on is a jerk
         * along the NORMAL with no touch down -- someone knocked on the bowl: the eyes WIDE for KNOCK_WIDE_S, a second
         * knock inside KNOCK_AGAIN_S the SQUINT (the eye kind is knock's own clock, folded in at the draw; the wobble's
         * kick is sense()'s as ever). Lit only, HOLDING, never asleep, never inside a fall or the weightless beat;
         * a touch down, one lifted inside the touch's hold, or a touch that cannot be vouched for refuses it */
        uint32_t know = millis();
        knock_in_t kin = { dt, last_jerk_x, last_jerk_y, acc.z - acc_lp.z, state != DARK, house_holding(), rest && rest->asleep,
                           fall_phase != FALL_NONE, fall0_on() || fall0_easing(),
                           touch_last.down || know - touch_down_ms < TOUCH_HOLD_MS || !gesture_touch_live() };
        int kev = knock_tick(&kin);
        if (kev == KNOCK_EV_FIRST) { engage(KNOCK_WIDE_S + 0.5f); LOG("knock: a rap from behind (%.2f g along the normal) -- eyes wide", fabsf(acc.z - acc_lp.z)); }
        else if (kev == KNOCK_EV_AGAIN) { engage(KNOCK_SQUINT_S + 0.5f); LOG("knock: again inside %.0f s -- the squint (I heard you)", KNOCK_AGAIN_S); }
    }
    {   /* THE DRUMMED TABLE (drum.c, watch-night C18): his typing carried up the wrist as a fine jitter (0.02-0.08 g, 4-10
         * reversals a second: neither a walk nor a jolt) held 3 s is a bowl on a drummed table -- the gel clock runs faster
         * (the bubble's own gel_extra road, folded in at the draw), nothing else; 2 s out of the band ends it. Lit only,
         * HOLDING, never asleep, never walking, never inside a fall or the weightless beat; never a sense, never a mark */
        drum_in_t din0 = { dt, last_jerk_x, last_jerk_y, acc.z - acc_lp.z, state != DARK, house_holding(), rest && rest->asleep,
                           wrist_walking(), fall_phase != FALL_NONE || fall0_on() || fall0_easing() };
        int dev = drum_tick(&din0);
        if (dev == DRUM_EV_BEGIN) LOG("drum: the table is drummed (%.1f reversals a second) -- the gel ripples", drum_rate_hz());
        else if (dev == DRUM_EV_END) LOG("drum: the drumming stopped -- the ripple eases");
    }
    {   /* asleep (a stir included), the lit glass stays at its lowest (a
         * raise at 3 am is not a lamp); under THE NIGHT LIGHT (tell.cpp)
         * lower still; the dim / awake levels are back the moment it wakes */
        static bool bl_sleep = false; static State bl_state = DARK; static uint8_t bl_lvl = 0;
        bool want = rest && rest->dozing && state != DARK;
        uint8_t lvl = tell->night_light ? NIGHT_BL : BL_SLEEP;
        if (want != bl_sleep || (want && (state != bl_state || lvl != bl_lvl))) {     /* the state block's own sets are above */
            bl_sleep = want; bl_lvl = lvl;
            lcd.setBrightness(hours_level(want ? lvl : state == DIM ? BL_DIM : bl_awake()));   /* THE HOURS' LIGHT: a level under the floor (BL_SLEEP / NIGHT_BL) is left as it is */
        }
        bl_state = state;
    }
    {   /* CLOCKWORK on the EMPTY glass: no gel to bounce, so the glass's
         * LIGHT bounces -- the awake level dipping with each of the three,
         * the state's own level back after (the watch works without him) */
        static bool pulsing = false;
        uint8_t base = rest->dozing ? BL_SLEEP : state == DIM ? BL_DIM : bl_awake();   /* the level the blocks above chose */
        if (cw->light < 1.0f && state != DARK) { pulsing = true; lcd.setBrightness((uint8_t)(hours_level(base) * cw->light)); }
        else if (pulsing) { pulsing = false; if (state != DARK) lcd.setBrightness(hours_level(base)); }
    }
    {   /* THE LEASE, EASY: the glass pulses once the instant the hold is
         * read -- the level up by LEASE_PULSE_ADD (capped) for LEASE_PULSE_MS,
         * the state's own level back after (the same shape as the
         * clockwork's dip above) */
        static bool pulsed = false;
        uint8_t base = rest->dozing ? BL_SLEEP : state == DIM ? BL_DIM : bl_awake();
        bool want = state != DARK && (int32_t)(lease_pulse_until - now) > 0;
        if (want && !pulsed) { pulsed = true; int lv = hours_level(base) + LEASE_PULSE_ADD; lcd.setBrightness((uint8_t)(lv > 255 ? 255 : lv)); }
        else if (!want && pulsed) { pulsed = false; if (state != DARK) lcd.setBrightness(hours_level(base)); }
    }
    if (tell->night_done) {                 /* THE NIGHT LIGHT's seconds are up: dark now, not at the timers' 12/20 s */
        go_dark("the night light");
        nap();
        last_us = 0;
        return;
    }
    if (state != AWAKE && idle > sleep_ms) {
        char why[48]; snprintf(why, sizeof why, "still %lu s: lights out%s", (unsigned long)(sleep_ms / 1000u), vigor_low() ? " (a low cell)" : "");
        go_dark(why);
        nap();
        last_us = 0;
        return;
    }
    /* LOWER-TO-SLEEP (stage 2): the face turned from the eyes and held */
    if (wake_lowered(dt, &acc_lp, last_motion_ms, last_touch_ms, touch_was_down,
                     fall_phase != FALL_NONE)) {
        go_dark("lowered: lights out");
        nap();
        last_us = 0;
        return;
    }

    /* THE FRAME PUSH: 40 fps while anything moves, 20 Hz once nothing has
     * changed for IDLE_AFTER_FRAMES frames (the first change is back at 40) */
    uint32_t frame_ms = quiet_frames >= IDLE_AFTER_FRAMES ? FRAME_IDLE_MS : FRAME_MS;
    if (quiet_frames < IDLE_AFTER_FRAMES) moving_us += (uint32_t)(dt * 1e6f);   /* time at the moving rate: the moving fps's clock */
    if (now - last_frame_ms >= frame_ms) {
        last_frame_ms = now;
        if (draw()) quiet_frames = 0; else if (quiet_frames < 1000) quiet_frames++;
    }
    busy_us += micros() - now_us;             /* the loop's work this pass (the delay below is its idle) */
    if (now - report_ms >= 5000) {
        float win_s = (now - report_ms) / 1000.0f;
        report_ms = now;
        app.fps = frames / win_s;
        fps_moving = moving_us ? moving_frames / (moving_us * 1e-6f) : 0.0f; fps_moving_s = moving_us * 1e-6f;   /* DOCTOR */
        static uint32_t r_seen = 0, c_seen = 0;   /* the raster cache's counts, this window */
        uint32_t rastered = 0, cached = 0; gel_raster_stats(&rastered, &cached);
        /* fps: frames composed over the window (the idle 20 Hz spells
         * pull it down: a pet every 3 s reads ~27); moving: the frames
         * composed at the moving rate over the time spent at it -- THE
         * number the >= 35 rule is about. frame: the longest compose
         * this window (ms, before the hash and the push), then the
         * per-frame averages of its parts, the hash, the push call and
         * the wait for a push reading fb (its longest too). */
        float fpf = frames ? 1.0f / frames : 0;
        Serial.printf("[%8lu] %s fps %.1f (moving %.1f over %.1f s) push %.1f/s (%lu KB/s, %s; stage %lu fb %lu) raster %lu/%lu tiles %lu/f busy %d%% frame max %.1f ms (raster %.2f gel %.2f face %.2f over %.2f hash %.2f push %.2f wait %.2f max %.1f) acc %.2f %.2f %.2f wall %s (pull %.0f%s) fall %d g_along %.2f lean %.2f tilt %.2f sway %.2f squash %.2f eyes %.2f\n",
                      (unsigned long)now, STATE_NAMES[state], app.fps,
                      moving_us ? moving_frames / (moving_us * 1e-6f) : 0.0f, moving_us * 1e-6f,
                      pushes / win_s,
                      (unsigned long)(push_px * 2 / 1024 / (win_s > 0 ? win_s : 1)), quiet_frames >= IDLE_AFTER_FRAMES ? "idle 20 Hz" : "40 fps",
                      (unsigned long)stage_pushes, (unsigned long)fb_pushes,
                      (unsigned long)(rastered - r_seen), (unsigned long)(rastered - r_seen + cached - c_seen),
                      (unsigned long)(frames ? tiles_hashed / frames : 0),
                      (int)(busy_us / (win_s * 1e4f)),
                      t_frame_max * 1e-3f, t_raster * 1e-3f * fpf, t_gel * 1e-3f * fpf, t_face * 1e-3f * fpf, t_over * 1e-3f * fpf,
                      t_hash * 1e-3f * fpf, t_push * 1e-3f * fpf, t_wait * 1e-3f * fpf, t_wait_max * 1e-3f,
                      acc.x, acc.y, acc.z, GEL_WALL_NAMES[wall], deg(phi_target), phi_valid ? "" : " level",
                      (int)fall_phase, g_along, lean, tilt_k,
                      spring1.a, spring2.a, eye_alpha);
        r_seen = rastered; c_seen = cached;
        frames = pushes = push_px = busy_us = 0;
        stage_pushes = fb_pushes = tiles_hashed = moving_frames = moving_us = 0;
        t_frame_max = t_raster = t_gel = t_face = t_over = t_hash = t_push = t_wait = t_wait_max = 0;
    }
    delay(1);
}
