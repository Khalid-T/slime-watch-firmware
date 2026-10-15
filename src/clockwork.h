/* CLOCKWORK (Khalid, 2026-09-09: "make it a true home for the slime
 * and something I can use too, as a watch"): the three things people
 * use on a wrist (design/watch-inspiration.md section C) -- a
 * STOPWATCH, a TIMER, an ALARM -- as the creature's own gestures.
 * Wordless but for the one number allowed (design-zoom-out: the hour;
 * here the elapsed / the remaining / the alarm's hour, in the glance's
 * font, in the glance's spot, only while the tool is lit). Never a
 * beep (no speaker), never a repeat, never a nag (section D): each
 * fires ONCE and waits for a tap, or clears itself after RING_S.
 *
 *   THE STOPWATCH  a HOLD on the glass of HOLD_MIN_MS..HOLD_MAX_MS
 *                  (the existing GES_HOLD, not the whistle's 1.5 s
 *                  press, and not the hold armed by three knocks)
 *                  starts it: the gel HOLDS ITS BREATH -- the breath's
 *                  amplitude to 0, the squash held at HOLD_SQUASH, the
 *                  eyes WIDE -- and a thin arc in the shine grows from
 *                  12 o'clock once per minute over the day ring
 *                  (face_arc), the elapsed as M:SS at the top. A tap
 *                  ends it: the EXHALE (one deep settle EXHALE_SQUASH
 *                  easing out over EXHALE_S, eyes content) and the
 *                  final time lit FINAL_S, then gone. The haptic's BUMP
 *                  on the start and the stop. THE LONG STARE (ruling
 *                  27, 2026-10-09): the wide eyes soften after STARE_S
 *                  -- one blink, the ordinary face, the breath still
 *                  held; a lap or the stop widens them for a second.
 *   THE TIMER      a hold of the same length WHILE the final time
 *                  shows sets that duration as a timer (the arc now
 *                  shrinks from full; the remaining at the top);
 *                  `timer <min>` sets one by hand. At zero the gel
 *                  BOUNCES OFF THE GLASS -- BOUNCE_N soft bounces,
 *                  squash pulses of BOUNCE_SQUASH with the desk's
 *                  landing kick each, eyes wide -- and the haptic's
 *                  HOP3, once; a tap dismisses. THE ZERO HE SLEPT
 *                  THROUGH (ruling 26, 2026-10-09): at zero while he
 *                  sleeps the bounces are OWED -- 0:00 shows, the ring's
 *                  clocks wait -- and paid on the first lit frame after
 *                  the sleep look ends; a tap before that forgives.
 *   THE ALARM      serial-set (`alarm HH:MM` / `alarm off`, NVS): at
 *                  the minute, HOLDING, the same bounces and HOP3 once
 *                  and the hour at the top; ASLEEP per rest.cpp the
 *                  sleep guardian's rule BENDS -- the alarm is the one
 *                  thing allowed to wake him: the nap is broken
 *                  (clockwork_due), the bounces play at the dimmest
 *                  glass, then the ordinary rules; a tap dismisses.
 *                  EMPTY, the watch still works without him: the
 *                  face's own hour is there and the glass's LIGHT
 *                  pulses with the bounces (``light``).
 *
 * No Arduino here: the clock, the house and the sleep come in as
 * parameters, the draw is a framebuffer; tools/clockwork_test.cpp runs
 * it on the laptop. The outputs are a few numbers the draw folds in
 * (the breath's factor, a squash, an eye, a kick), a haptic kind to
 * play, a log line. The gel, the bowl and the fall are untouched. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* what main hands over each frame */
typedef struct {
    float    dt;
    uint32_t now_ms;
    int      minute;        /* the local minute of the day, -1 = no clock */
    uint32_t date;          /* YYYYMMDD, 0 = no clock (the alarm's once-a-day id) */
    bool     lit;           /* the screen is on */
    bool     holding;       /* house_holding(): a gel to hold its breath */
    bool     asleep;        /* rest->dozing: the sleep guardian (the timer waits; the alarm bends the rule) */
} clockwork_in_t;

/* what main folds in */
typedef struct {
    float breath_k;         /* the breath's amplitude factor: 0 while the breath is held, else 1 */
    float squash;           /* added to the authored squash: the held flat, the exhale, the bounces */
    int   eye_kind;         /* GEL_EYE_* to wear, or -1 = the ordinary face */
    float eye_alpha;        /* the eyes brought in for it: the draw takes the max */
    float kick2;            /* one-shot: the vertical spring's kick this frame (a bounce's landing) */
    int   haptic;           /* one-shot: HAPTIC_* to play this frame, or -1 */
    bool  keep_awake;       /* a ring's first seconds hold the glass lit */
    float light;            /* EMPTY: the glass's light pulsing with the bounces (a backlight factor; 1 = none) */
    bool  log_new;
    char  log[112];
} clockwork_out_t;

enum { CW_SW_OFF = 0, CW_SW_RUN, CW_SW_FINAL };
enum { CW_TM_OFF = 0, CW_TM_RUN, CW_TM_RING };
enum { CW_RING_NONE = 0, CW_RING_TIMER, CW_RING_ALARM };

void clockwork_begin(int alarm_minute);                 /* NVS: the alarm's minute of the day, -1 = off */
const clockwork_out_t *clockwork_tick(const clockwork_in_t *in);
void clockwork_log_taken(void);
/* the gestures (main's switch): true = taken. A HOLD of held_ms on the
 * glass (the tap-then-hold): the stopwatch's start (HOLDING, nothing
 * running), its STOP while it counts, or the timer set from the final
 * time showing. A TAP: a LAP while the stopwatch counts, or a ring's
 * dismissal.
 *
 * LAPS ON THE ARC (design/watch-ideas.md B16, 2026-09-10 evening): a
 * tap while the count runs is not the stop but a LAP -- a tick left on
 * the arc where the count was (the minute's dial), CW_LAPS at most (a
 * thirteenth is nothing, logged); the count holds its breath through
 * them. The ticks stay with the final time and go with it, or with the
 * next start. clockwork_laps hands the splits (ms from the start) to
 * THE WATCH PAGE. */
#define CW_LAPS 12
bool clockwork_hold(int held_ms, uint32_t now_ms);
bool clockwork_tap(uint32_t now_ms);
int  clockwork_laps(const uint32_t **ms);   /* the count; *ms = the splits, oldest first (NULL is fine) */
/* something in the glance's spot this frame: the glance yields */
bool clockwork_showing(void);
/* the body is taken (the held breath, the exhale, the bounces): IDLE LIFE waits */
bool clockwork_busy(void);
/* over the composed frame, after the face: the arc over the day ring
 * (the shine), the M:SS / HH:MM in the glance's font in the hour's
 * spot -- ``at`` the wall THE PLACE chose for the hour (place.h) */
void clockwork_draw(uint16_t *fb, uint32_t now_ms, int at);
#ifdef __cplusplus
#include "dirty.h"
bool clockwork_mark(dirty_rect_t *box); /* THE MARK: did this frame's number differ from last frame's; the box of both (the arc marks in face's) */
#endif
/* the nap (wake.cpp): break the light sleep for the alarm's minute, or
 * the timer's zero when he is not asleep */
bool clockwork_due(uint32_t now_ms, int minute, uint32_t date);
/* THE GENTLE WAKE (design/watch-ideas.md D35, 2026-10-08; gentle.h has
 * the rule): beside clockwork_due in the nap, and in the tick when the
 * glass is lit through a stir -- inside the twenty minutes before the
 * alarm, dozing (rest's asleep or stirring, live: rest_dozing_now),
 * the wrist stirred within two minutes and the pulse risen over the
 * night's floor, the alarm rings NOW with its own theatre and its
 * minute is consumed; once per alarm; never without one. true = the
 * nap breaks for it ("the clockwork", as the alarm's minute).
 * clockwork_stir: the wrist moved (wake.cpp's dark judge and poll, the
 * wake's cause) -- the stir's recency the rule reads. */
bool clockwork_gentle_due(uint32_t now_ms, int minute, uint32_t date, bool dozing);
void clockwork_stir(uint32_t now_ms);
/* by hand: `timer <min>` (0 = off), `alarm HH:MM` (-1 = off; the NVS
 * write is the console's) */
void clockwork_set_timer(uint32_t ms, uint32_t now_ms, const char *who);
void clockwork_set_alarm(int minute, const char *who);
int  clockwork_alarm(void);
/* THE RIM AS THE DIAL (design/watch-ideas.md B13; ruling 25 settled by
 * building it): the ARC drag (gesture.cpp) turns the alarm's dial. The
 * finger's angle from 12 o'clock is the hour hand on a 12-hour dial in
 * DIAL_STEP_MIN steps; the alarm is the next such time after now (the
 * same clock-face hour twice a day: the one that comes first). While
 * the finger drags (lift = false) the face's hour tick follows it
 * (face_dial); on the lift (lift = true) the alarm is SET and the tick
 * blinks twice -- or, dialled to the hour of an alarm set within
 * DIAL_CLEAR_S, CLEARED, the tick fading. Returns 1 when the alarm
 * changed on a lift (main writes NVS then); 0 with no clock
 * (minute_now < 0), or on a move. `alarm HH:MM` over serial unchanged.
 *
 * THE TIMER FROM THE RIM (the other half of B13, 2026-09-10 evening):
 * WHILE THE STOPWATCH'S FINAL TIME SHOWS the same arc sets the TIMER
 * instead -- the degrees SWEPT (sweep_deg, clockwise positive) are the
 * duration, a quarter turn a quarter hour, in TIMER_STEP_MIN steps, up
 * to TIMER_DIAL_MAX_MIN (two turns); the arc on the ring follows the
 * finger (that fraction of the hour), the number at the top reads the
 * duration; the final time is kept lit while the finger goes round.
 * The lift SETS it (returns 2; the stopwatch is put away, its laps
 * with it) -- under one step, or counter-clockwise, nothing (the final
 * stays). No clock needed. While a timer runs its END sits on the rim
 * as a 1 px tick in the shine on the MINUTE dial (where the minute
 * hand will be when it rings; none without a clock). */
int  clockwork_dial(int angle_deg, int sweep_deg, bool lift, int minute_now, uint32_t now_ms);
int  clockwork_dial_minute(void);        /* the minute under the finger while dialling the alarm, -1 = not dialling */
int  clockwork_dial_timer_min(void);     /* the minutes under the finger while dialling the timer, -1 = not dialling */
/* for the test and the status */
int  clockwork_sw_state(void);
int  clockwork_tm_state(void);
int  clockwork_ring(void);
uint32_t clockwork_elapsed_ms(uint32_t now_ms);        /* the stopwatch's, running or final */
uint32_t clockwork_remaining_ms(uint32_t now_ms);      /* the timer's */
float clockwork_arc(uint32_t now_ms);                  /* the arc's fraction, 0 = none drawn */
void clockwork_status(char *out, int cap);             /* `clockwork` over serial */
