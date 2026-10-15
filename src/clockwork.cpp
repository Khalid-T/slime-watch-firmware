/* CLOCKWORK: the stopwatch, the timer and the alarm as the creature's
 * own gestures (clockwork.h has the whole account). No Arduino: the
 * clock comes in as milliseconds and the local minute, the draw is a
 * framebuffer; tools/clockwork_test.cpp runs it on the laptop. */
#include "clockwork.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */        /* the eye kinds, the shine */
#include "haptic.h"     /* HAPTIC_BUMP, HAPTIC_HOP3: the kinds, not the driver */
#include "face.h"       /* face_arc: the ring's own routine */
#include "mark.h"
#include "gentle.h"    /* THE GENTLE WAKE (D35): the window's rule, pure C; the alarm is still this file's */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* --- the tunables ------------------------------------------------------- */

/* the hold that starts the stopwatch (and sets the timer from the final
 * time): since THE LEASE, EASY (2026-09-10 afternoon) it is the
 * TAP-THEN-HOLD -- gesture.cpp's GES_TAP_HOLD, a tap then a press held
 * 0.5-1.9 s (the same window as the lease's plain hold, which this
 * length used to be; widened with it 09-10 21:08, THE HOLD'S WINDOW,
 * WIDER); the bounds here are the detector's, kept as the guard they
 * always were */
static const int   HOLD_MIN_MS    = 500;
static const int   HOLD_MAX_MS    = 1900;
/* the held breath: the breath's amplitude to 0, the mass held a hair
 * taller (negative squash: the intake), the eyes wide */
static const float HOLD_SQUASH    = -0.08f;
/* the exhale on the stop: one deep settle easing out; the final time lit */
static const float EXHALE_SQUASH  = 0.15f;
static const float EXHALE_S       = 1.5f;
static const float FINAL_S        = 6.0f;
static const float FINAL_FADE_S   = 0.5f;    /* the glance's own fade at the end */
/* the bounces off the glass at the timer's zero / the alarm's minute:
 * BOUNCE_N landings BOUNCE_S apart, each a squash pulse of BOUNCE_SQUASH
 * (a half sine) with the desk's landing kick (main's KICK_CAP 3.2) */
static const int   BOUNCE_N       = 3;
static const float BOUNCE_S       = 0.35f;
static const float BOUNCE_SQUASH  = 0.20f;
static const float BOUNCE_KICK    = 3.2f;
/* EMPTY: the glass's light dips with each bounce (a backlight factor) */
static const float LIGHT_DIP      = 0.65f;
/* a ring holds the glass lit this long, then the dark house's own
 * timers; undismissed it clears itself after RING_S (a missed alarm is
 * not there at dinner: never a nag) */
static const float RING_KEEP_S    = 15.0f;
static const float RING_S         = 60.0f;
/* THE ZERO HE SLEPT THROUGH (design/watch-map.md ruling 26, built
 * 2026-10-09): a timer at zero while he sleeps is a DEBT, not a ring --
 * the bounces are owed and the ring's two clocks (RING_KEEP_S, RING_S)
 * do not run while the sleep look stands (only the alarm wakes him);
 * `0:00` is the glass's word meanwhile, as today. They are paid on the
 * first LIT frame after the sleep look ends (rest's dozing false): the
 * three bounces and the HOP3 once, the glass kept its 15 s, the minute
 * to clear from THEN. A tap before that clears the debt (the ring's
 * tap, as ever). No cap on the debt's age: a zero at 02:00 bounces at
 * 07:00 -- he set it, it tells him once, once he is up (the minute's
 * self-clear is for a ring he could have seen, not one he slept past). */
/* THE LONG STARE (ruling 27, built 2026-10-09): the stopwatch's wide
 * eyes are the held breath's -- for a sprint. A count past STARE_S
 * SOFTENS: one blink of STARE_BLINK_S (the desk's own blink feel,
 * idle.cpp's BLINK_S) and the ordinary face after, the breath still
 * held, the arc still growing. A LAP (the tick left on the arc: a thing
 * to look at) widens them again for LAP_WIDE_S, the STOP for
 * STOP_WIDE_S before the exhale's content eyes (the number, seen; then
 * the relief). A count under the minute is untouched: wide to the stop,
 * content through the exhale, as built. */
static const float STARE_S        = 60.0f;
static const float STARE_BLINK_S  = 0.12f;
static const float LAP_WIDE_S     = 1.0f;
static const float STOP_WIDE_S    = 1.0f;
/* the number: the glance's font (3x5, 3 px blocks) in the glance's spot
 * (70 px above the centre) */
static const int   TEXT_PX        = 3;
static const int   TEXT_R         = 70;
/* THE RIM AS THE DIAL (design/watch-ideas.md B13, 2026-09-10): the
 * finger's angle round the rim as the hour hand, snapped to
 * DIAL_STEP_MIN (a quarter hour: what a fingertip on a 240 px rim can
 * set -- 7.5 deg a step); the same drag over an alarm set within
 * DIAL_CLEAR_S clears it (the second thought after the first). */
static const int   DIAL_STEP_MIN  = 15;
static const float DIAL_CLEAR_S   = 30.0f;
/* LAPS ON THE ARC (watch-ideas B16): a lap's tick is a radial mark
 * across the ring band -- LAP_TICK_IN inside the day ring's outer edge
 * out to the edge itself (over the night ring's band too: it is a mark
 * ON the arc), LAP_TICK_W wide, in the BODY colour: darker than the
 * shine arc it sits on, lit against the black the arc has not reached
 * yet this minute. Drawn over whatever is there (the arc is). */
static const float LAP_TICK_IN    = 6.0f;
static const int   LAP_TICK_W     = 2;
/* THE TIMER FROM THE RIM (B13's other half): the degrees swept round
 * the rim as minutes -- a full turn is the hour, so a quarter turn is a
 * quarter hour; TIMER_STEP_MIN is what a fingertip on a 240 px rim can
 * set (30 deg a step); TIMER_DIAL_MAX_MIN two turns (a longer timer is
 * `timer <min>` over serial). The running timer's END on the minute
 * dial: a 1 px tick in the shine, TIMER_TICK_LEN long, ending where the
 * face's own ticks end (6 px inside the day ring's edge). */
static const int   TIMER_STEP_MIN    = 5;
static const int   TIMER_DIAL_MAX_MIN = 120;
static const float TIMER_TICK_IN     = 6.0f;
static const int   TIMER_TICK_LEN    = 4;
static const int   TIMER_TICK_W      = 1;

/* --- the state ---------------------------------------------------------- */

static int      sw_state = CW_SW_OFF;
static uint32_t sw_start_ms = 0, sw_final_ms = 0;
static float    sw_final_t = 0;          /* the final time's seconds left lit */
static float    exhale_t = 0;            /* seconds into the exhale */
static int      tm_state = CW_TM_OFF;
static uint32_t tm_total_ms = 0, tm_end_ms = 0;
static int      alarm_min = -1;
static uint32_t alarm_fired_date = 0;    /* the last firing's id: once a day */
static int      alarm_fired_min = -1;
static int      ring = CW_RING_NONE;
static uint32_t ring_start_ms = 0;
static int      ring_minute = -1;        /* the alarm's, for the hour shown */
static bool     bounce_pending = false;  /* the bounces wait for a lit glass (and, for the timer, for him to be up) */
static float    bounce_t = -1;           /* seconds into the bounces, -1 = none */
static int      bounce_i = -1;
static int      pending_haptic = -1;     /* a gesture's haptic, played on the next tick */
static bool     last_holding = false, last_asleep = false;
static uint32_t last_now_ms = 0;
static int      dial_minute = -1;        /* THE RIM AS THE DIAL: the minute under the finger, -1 = not dialling */
static uint32_t alarm_set_ms = 0;        /* when the alarm was last set (the dial's clear window), 0 = never */
static uint32_t lap_ms[CW_LAPS];         /* LAPS ON THE ARC: the splits, ms from the start */
static int      laps = 0;
static int      tm_dial_min = -1;        /* THE TIMER FROM THE RIM: the minutes under the finger, -1 = not dialling */
static bool     owed = false;            /* THE ZERO HE SLEPT THROUGH: the timer's bounces owed from his sleep (the ring's clocks held) */
static uint32_t owed_ms = 0;             /* when the zero came, for the log */
static float    wide_t = 0;              /* THE LONG STARE: seconds of the wide stare left (STARE_S from the start, LAP_WIDE_S from a lap) */
static float    blink_t = 0;             /* the blink that ends a stare: GEL_EYE_CLOSED for STARE_BLINK_S */
static bool     softened = false;        /* the stare softened once this count: the lap's and the stop's wide second apply */
static float    stop_wide_t = 0;         /* the stop's wide second over the exhale's start */
static int      last_minute = -1;        /* the last tick's minute of the day (the timer's tick on the minute dial) */
static uint32_t stir_ms = 0;             /* THE GENTLE WAKE: when the wrist last stirred (clockwork_stir), 0 = never */
static bool     gentle_took = false;     /* THE GENTLE WAKE rang the alarm early: its minute, when it comes, is consumed in silence */
static clockwork_out_t out;

/* the glance's font: 3x5 digits and the colon, each row 3 bits, msb left */
static const uint8_t DIGITS[10][5] = {
    { 0b111, 0b101, 0b101, 0b101, 0b111 },  /* 0 */
    { 0b010, 0b110, 0b010, 0b010, 0b111 },  /* 1 */
    { 0b111, 0b001, 0b111, 0b100, 0b111 },  /* 2 */
    { 0b111, 0b001, 0b111, 0b001, 0b111 },  /* 3 */
    { 0b101, 0b101, 0b111, 0b001, 0b001 },  /* 4 */
    { 0b111, 0b100, 0b111, 0b001, 0b111 },  /* 5 */
    { 0b111, 0b100, 0b111, 0b101, 0b111 },  /* 6 */
    { 0b111, 0b001, 0b010, 0b010, 0b010 },  /* 7 */
    { 0b111, 0b101, 0b111, 0b101, 0b111 },  /* 8 */
    { 0b111, 0b101, 0b111, 0b001, 0b111 },  /* 9 */
};
static const uint8_t COLON[5] = { 0b000, 0b010, 0b000, 0b010, 0b000 };

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
    va_list a; va_start(a, fmt);
    vsnprintf(out.log, sizeof out.log, fmt, a);
    va_end(a);
    out.log_new = true;
}

static void mss(char *s, int cap, uint32_t ms, bool up)
{
    uint32_t sec = up ? (ms + 999) / 1000 : ms / 1000;
    snprintf(s, cap, "%lu:%02lu", (unsigned long)((sec / 60) % 100), (unsigned long)(sec % 60));
}
static void hhmm(char *s, int cap, int minute)
{
    snprintf(s, cap, "%02d:%02d", (minute / 60) % 24, minute % 60);
}

/* --- the ring: the timer's zero, the alarm's minute ------------------------ */

static void ring_start(int kind, uint32_t now_ms, int minute)
{
    ring = kind;
    ring_start_ms = now_ms;
    ring_minute = minute;
    bounce_pending = true;
    bounce_t = -1;
    bounce_i = -1;
    owed = false;                        /* a new ring (the alarm taking a timer's) owes nothing from before */
}

static void ring_end(const char *why)
{
    char hm[8];
    if (ring == CW_RING_ALARM) hhmm(hm, sizeof hm, ring_minute); else mss(hm, sizeof hm, tm_total_ms, true);
    say("clockwork: %s %s dismissed (%s)%s", ring == CW_RING_ALARM ? "alarm" : "timer", hm, why,
        owed ? " -- the bounces owed from his sleep are forgiven" : "");
    if (ring == CW_RING_TIMER) tm_state = CW_TM_OFF;
    ring = CW_RING_NONE;
    bounce_pending = false;
    bounce_t = -1;
    owed = false;
}

static bool alarm_fired(uint32_t date, int minute)
{
    return alarm_fired_date == date && alarm_fired_min == minute;
}

/* --- THE GENTLE WAKE (design/watch-ideas.md D35, 2026-10-08) ------------------
 * Only when HE set an alarm: inside the GENTLE_WINDOW_MIN before it,
 * dozing, the wrist stirred within GENTLE_STIR_S and the pulse risen
 * GENTLE_RISE_BPM over the night's floor (gentle.c's rule), the alarm
 * rings NOW -- its own theatre, the bounces and the HOP3 over the sleep
 * look as at its minute -- and is consumed: its minute passes in
 * silence. Once per alarm. The window never says = the ordinary ring. */

void clockwork_stir(uint32_t now_ms) { stir_ms = now_ms ? now_ms : 1; }

static void gentle_fill(gentle_in_t *g, uint32_t now_ms, int minute, bool dozing)
{
    g->alarm_minute = alarm_min; g->minute = minute; g->dozing = dozing;
    g->stir_age_s = stir_ms ? (int)((now_ms - stir_ms) / 1000u) : -1;
    g->now_ms = now_ms;
}

static bool gentle_try(uint32_t now_ms, int minute, uint32_t date, bool dozing)
{
    if (alarm_min < 0 || minute < 0 || ring == CW_RING_ALARM) return false;
    if (alarm_fired(date, alarm_min) && minute == alarm_min) return false;   /* rang today, at its minute */
    gentle_in_t g; gentle_out_t o;
    gentle_fill(&g, now_ms, minute, dozing);
    if (!gentle_check(&g, &o)) return false;
    gentle_spent();
    gentle_took = true;
    if (ring == CW_RING_TIMER) { tm_state = CW_TM_OFF; }
    ring_start(CW_RING_ALARM, now_ms, alarm_min);
    char hm[8], am[8]; hhmm(hm, sizeof hm, minute); hhmm(am, sizeof am, alarm_min);
    say("clockwork: the gentle wake -- %s for %s (stirring %d s ago, pulse %d -> %d)", hm, am, g.stir_age_s, o.floor_bpm, o.last_bpm);
    return true;
}

bool clockwork_gentle_due(uint32_t now_ms, int minute, uint32_t date, bool dozing)
{
    return gentle_try(now_ms, minute, date, dozing);
}

/* --- the frame ------------------------------------------------------------- */

void clockwork_begin(int alarm_minute)
{
    sw_state = CW_SW_OFF; tm_state = CW_TM_OFF; ring = CW_RING_NONE;
    sw_final_t = 0; exhale_t = 0; bounce_t = -1; bounce_pending = false; pending_haptic = -1;
    owed = false; owed_ms = 0; wide_t = 0; blink_t = 0; softened = false; stop_wide_t = 0;
    alarm_fired_date = 0; alarm_fired_min = -1;
    gentle_begin(); gentle_took = false; stir_ms = 0;
    alarm_min = (alarm_minute >= 0 && alarm_minute < 1440) ? alarm_minute : -1;
    memset(&out, 0, sizeof out);
    out.breath_k = 1.0f; out.eye_kind = -1; out.haptic = -1; out.light = 1.0f;
    if (alarm_min >= 0) { char hm[8]; hhmm(hm, sizeof hm, alarm_min); say("clockwork: alarm %s (stored)", hm); }
}

const clockwork_out_t *clockwork_tick(const clockwork_in_t *in)
{
    uint32_t now = in->now_ms;
    float dt = in->dt;
    last_holding = in->holding;
    last_asleep = in->asleep;
    last_now_ms = now;
    last_minute = in->minute;
    if (tm_dial_min >= 0 && sw_state == CW_SW_FINAL) sw_final_t = FINAL_S;   /* THE TIMER FROM THE RIM: the final time waits while the finger goes round */
    /* the one-shots and the frame's numbers */
    out.breath_k = 1.0f; out.squash = 0; out.eye_kind = -1; out.eye_alpha = 0;
    out.kick2 = 0; out.haptic = pending_haptic; pending_haptic = -1;
    out.keep_awake = false; out.light = 1.0f;

    /* THE STOPWATCH: the held breath; the exhale and the final time */
    if (sw_state == CW_SW_RUN) {
        if (!in->holding) {             /* he left mid-count: no gel to hold its breath -- the count is dropped */
            sw_state = CW_SW_OFF;
            say("clockwork: stopwatch dropped -- the house went empty");
        } else {
            out.breath_k = 0;
            out.squash += HOLD_SQUASH;
            /* THE LONG STARE: wide for the first minute (and a second after a
             * lap), one blink at the softening, the ordinary face after */
            if (wide_t > 0) {
                out.eye_kind = GEL_EYE_WIDE; out.eye_alpha = 1.0f;
                wide_t -= dt;
                if (wide_t <= 0) {
                    wide_t = 0; blink_t = STARE_BLINK_S;
                    if (!softened) { softened = true; say("clockwork: the stare softens (a minute of it) -- the ordinary face until a lap or the stop; the breath still held"); }
                }
            } else if (blink_t > 0) {
                out.eye_kind = GEL_EYE_CLOSED; out.eye_alpha = 1.0f;
                blink_t -= dt;
            }
        }
    } else if (sw_state == CW_SW_FINAL) {
        sw_final_t -= dt;
        if (exhale_t < EXHALE_S) {
            out.squash += EXHALE_SQUASH * (1.0f - smooth(clampf(exhale_t / EXHALE_S, 0, 1)));
            out.eye_kind = GEL_EYE_CONTENT; out.eye_alpha = 1.0f;
            exhale_t += dt;
        }
        if (stop_wide_t > 0) {              /* THE LONG STARE's stop: the number, seen (wide), before the relief (content) */
            out.eye_kind = GEL_EYE_WIDE; out.eye_alpha = 1.0f;
            stop_wide_t -= dt;
        }
        if (sw_final_t <= 0) { sw_final_t = 0; sw_state = CW_SW_OFF; }
    }

    /* THE TIMER's zero */
    if (tm_state == CW_TM_RUN && (int32_t)(now - tm_end_ms) >= 0) {
        tm_state = CW_TM_RING;
        char hm[8]; mss(hm, sizeof hm, tm_total_ms, true);
        ring_start(CW_RING_TIMER, now, -1);
        say("clockwork: timer %s at zero%s", hm, in->asleep ? " (asleep: the bounces wait for him)" : "");
    }
    /* THE ALARM's minute, once a day */
    if (alarm_min >= 0 && in->minute >= 0 && in->minute == alarm_min && !alarm_fired(in->date, in->minute)) {
        alarm_fired_date = in->date; alarm_fired_min = in->minute;
        char hm[8]; hhmm(hm, sizeof hm, alarm_min);
        if (gentle_took) {
            /* THE GENTLE WAKE already rang it, early: the minute passes in silence and the window is armed for the next day */
            gentle_took = false; gentle_rearm();
            say("clockwork: alarm %s -- its minute, already rung by the gentle wake; nothing", hm);
        } else {
            if (ring == CW_RING_TIMER) { tm_state = CW_TM_OFF; }   /* the alarm takes the ring; the timer's was heard or not */
            ring_start(CW_RING_ALARM, now, alarm_min);
            say("clockwork: alarm %s -- the bounces, once%s%s", hm,
                in->asleep ? " (asleep: the guardian's rule bends)" : "", in->holding ? "" : " (empty: the glass's light)");
        }
    }
    /* THE GENTLE WAKE on a lit glass: asleep but lit happens (THE NIGHT
     * LIGHT keeps the sleep look through a stir); the same rule as the
     * nap's, the stir being whatever lit it */
    if (in->asleep && ring == CW_RING_NONE) gentle_try(now, in->minute, in->date, true);

    /* THE RING: the bounces once, on a lit glass (the timer waits for
     * him to be up; the alarm is the one thing allowed to wake him) */
    if (ring != CW_RING_NONE) {
        /* THE ZERO HE SLEPT THROUGH: the timer's bounces, unpaid while the
         * sleep look stands, are a debt -- the ring's clocks are held at
         * now (they run from the paying), the glass is not kept lit for
         * a sleeper; `0:00` shows as today if the glass is lit */
        bool debt = ring == CW_RING_TIMER && bounce_pending && in->asleep;
        if (debt) {
            if (!owed) { owed = true; owed_ms = now; }
            ring_start_ms = now;
        }
        if (bounce_pending && in->lit && (!in->asleep || ring == CW_RING_ALARM)) {
            bounce_pending = false;
            bounce_t = 0;
            bounce_i = -1;
            out.haptic = HAPTIC_HOP3;
            if (owed) {
                uint32_t late_s = (now - owed_ms) / 1000u;
                say("clockwork: timer 0:00 -- the zero came in his sleep %lu min %lu s ago: the bounces now, once he is up; a minute to clear from here",
                    (unsigned long)(late_s / 60), (unsigned long)(late_s % 60));
                owed = false;
            }
        }
        if (bounce_t >= 0) {
            int i = (int)(bounce_t / BOUNCE_S);
            if (i < BOUNCE_N) {
                if (i != bounce_i) { bounce_i = i; out.kick2 = BOUNCE_KICK; }
                float ph = (bounce_t - i * BOUNCE_S) / BOUNCE_S;
                float pulse = sinf((float)M_PI * clampf(ph, 0, 1));
                out.squash += BOUNCE_SQUASH * pulse;
                out.eye_kind = GEL_EYE_WIDE; out.eye_alpha = 1.0f;
                if (!in->holding) out.light = 1.0f - LIGHT_DIP * pulse;
                bounce_t += dt;
            } else bounce_t = -1;
        }
        out.keep_awake = !debt && (now - ring_start_ms) < (uint32_t)(RING_KEEP_S * 1000.0f);
        if (!debt && (now - ring_start_ms) >= (uint32_t)(RING_S * 1000.0f)) ring_end("its minute is up");
    }
    return &out;
}

void clockwork_log_taken(void) { out.log_new = false; }

/* --- the gestures ---------------------------------------------------------- */

/* the stop: the exhale, the final time lit (the laps stay with it) */
static void sw_stop(uint32_t now_ms, const char *how)
{
    sw_final_ms = now_ms - sw_start_ms;
    sw_state = CW_SW_FINAL;
    sw_final_t = FINAL_S;
    exhale_t = 0;
    stop_wide_t = softened ? STOP_WIDE_S : 0;    /* THE LONG STARE: a softened stare pops wide at the stop (the number, seen) */
    wide_t = 0; blink_t = 0;
    pending_haptic = HAPTIC_BUMP;
    char hm[8]; mss(hm, sizeof hm, sw_final_ms, false);
    say("clockwork: stopwatch %s (%s%s) -- the exhale; a tap then a hold now makes it a timer", hm, how,
        laps ? (laps == 1 ? ", one lap on the arc" : ", the laps on the arc") : "");
}

bool clockwork_hold(int held_ms, uint32_t now_ms)
{
    if (held_ms < HOLD_MIN_MS || held_ms > HOLD_MAX_MS) return false;
    if (!last_holding) return false;                 /* no gel to hold its breath */
    if (ring != CW_RING_NONE) return false;          /* a tap dismisses; a hold is nothing */
    if (sw_state == CW_SW_RUN) {                     /* LAPS ON THE ARC: the taps are laps now, so the hold is the stop */
        sw_stop(now_ms, "a tap then a hold");
        return true;
    }
    if (sw_state == CW_SW_FINAL) {
        /* the final time showing: that long, as a timer */
        uint32_t ms = sw_final_ms < 1000 ? 1000 : sw_final_ms;
        sw_state = CW_SW_OFF;
        laps = 0;
        clockwork_set_timer(ms, now_ms, "the stopwatch's final time");
        return true;
    }
    if (sw_state == CW_SW_OFF && tm_state == CW_TM_OFF) {
        sw_state = CW_SW_RUN;
        sw_start_ms = now_ms;
        laps = 0;
        wide_t = STARE_S; blink_t = 0; softened = false; stop_wide_t = 0;   /* THE LONG STARE: wide for the first minute */
        pending_haptic = HAPTIC_BUMP;
        say("clockwork: stopwatch started (a hold of %d ms) -- the breath held; a tap is a lap, a tap then a hold stops it", held_ms);
        return true;
    }
    return false;
}

bool clockwork_tap(uint32_t now_ms)
{
    if (sw_state == CW_SW_RUN) {
        /* LAPS ON THE ARC: a tick where the count is, twelve at most */
        uint32_t at = now_ms - sw_start_ms;
        char hm[8]; mss(hm, sizeof hm, at, false);
        if (laps < CW_LAPS) {
            lap_ms[laps++] = at;
            if (softened && wide_t < LAP_WIDE_S) { wide_t = LAP_WIDE_S; blink_t = 0; }   /* THE LONG STARE: a lap's tick is a thing to look at -- wide again for a second */
            say("clockwork: lap %d at %s (a tap) -- a tick on the arc%s; a tap then a hold stops the count", laps, hm,
                softened ? ", the eyes wide again for a second" : "");
        } else say("clockwork: a tap at %s -- twelve laps on the arc already, no more; a tap then a hold stops the count", hm);
        return true;
    }
    if (ring != CW_RING_NONE) { ring_end("a tap"); return true; }
    return false;
}

int clockwork_laps(const uint32_t **ms)
{
    if (ms) *ms = lap_ms;
    return sw_state == CW_SW_OFF ? 0 : laps;
}

/* --- by hand ----------------------------------------------------------------- */

void clockwork_set_timer(uint32_t ms, uint32_t now_ms, const char *who)
{
    if (ring == CW_RING_TIMER) { ring = CW_RING_NONE; bounce_pending = false; bounce_t = -1; owed = false; }
    if (ms == 0) {
        bool was = tm_state != CW_TM_OFF;
        tm_state = CW_TM_OFF;
        say("clockwork: timer %s (%s)", was ? "off" : "was not set", who);
        return;
    }
    tm_state = CW_TM_RUN;
    tm_total_ms = ms;
    tm_end_ms = now_ms + ms;
    char hm[8]; mss(hm, sizeof hm, ms, true);
    say("clockwork: timer %s set (%s) -- the arc shrinks from full", hm, who);
}

void clockwork_set_alarm(int minute, const char *who)
{
    if (ring == CW_RING_ALARM) ring_end(who);
    alarm_min = (minute >= 0 && minute < 1440) ? minute : -1;
    alarm_fired_date = 0; alarm_fired_min = -1;
    gentle_rearm(); gentle_took = false;                 /* THE GENTLE WAKE: a new alarm, a new window */
    alarm_set_ms = alarm_min >= 0 ? (last_now_ms ? last_now_ms : 1) : 0;
    if (alarm_min < 0) say("clockwork: alarm off (%s)", who);
    else { char hm[8]; hhmm(hm, sizeof hm, alarm_min); say("clockwork: alarm %s (%s) -- the bounces at the minute, once", hm, who); }
}

int clockwork_alarm(void) { return alarm_min; }

/* --- THE RIM AS THE DIAL ------------------------------------------------------- */

/* the finger's angle -> the next such clock-face time after now */
static int dial_to_minute(int angle_deg, int minute_now)
{
    int a = ((angle_deg % 360) + 360) % 360;
    int m12 = (int)((float)a / 360.0f * 720.0f / DIAL_STEP_MIN + 0.5f) * DIAL_STEP_MIN % 720;
    int today = minute_now % 1440;
    int m = m12 > today ? m12 : m12 + 720;               /* the morning's or the afternoon's, whichever comes first */
    if (m <= today) m += 720;
    return m % 1440;
}

int clockwork_dial(int angle_deg, int sweep_deg, bool lift, int minute_now, uint32_t now_ms)
{
    /* THE TIMER FROM THE RIM: the final time showing (or the timer's
     * dial already turning), the sweep is a duration */
    if (sw_state == CW_SW_FINAL || tm_dial_min >= 0) {
        int deg = sweep_deg < 0 ? 0 : sweep_deg;
        int min = (int)((float)deg / 360.0f * 60.0f / TIMER_STEP_MIN + 0.5f) * TIMER_STEP_MIN;
        if (min > TIMER_DIAL_MAX_MIN) min = TIMER_DIAL_MAX_MIN;
        if (!lift) { tm_dial_min = min; return 0; }                        /* the arc and the number follow the finger */
        tm_dial_min = -1;
        if (min < TIMER_STEP_MIN) {
            say("clockwork: the dial let go under %d min (%d deg swept): no timer; the final time stays", TIMER_STEP_MIN, sweep_deg);
            return 0;
        }
        if (sw_state == CW_SW_FINAL) { sw_state = CW_SW_OFF; laps = 0; }
        clockwork_set_timer((uint32_t)min * 60000u, now_ms, "the rim");
        return 2;
    }
    if (minute_now < 0) { dial_minute = -1; face_dial(-1); return 0; }   /* no clock: no hour to dial */
    int m = dial_to_minute(angle_deg, minute_now);
    if (!lift) {
        if (m != dial_minute) { dial_minute = m; face_dial(m); }             /* the hour tick follows the finger */
        return 0;
    }
    dial_minute = -1;
    char hm[8]; hhmm(hm, sizeof hm, m);
    bool same = alarm_min == m && alarm_set_ms && (now_ms - alarm_set_ms) <= (uint32_t)(DIAL_CLEAR_S * 1000.0f);
    if (same) {
        /* the same drag over the alarm just set: cleared, the tick fades */
        if (ring == CW_RING_ALARM) ring_end("the dial");
        alarm_min = -1; alarm_fired_date = 0; alarm_fired_min = -1; alarm_set_ms = 0;
        gentle_rearm(); gentle_took = false;
        face_dial(m); face_dial_clear();
        say("clockwork: alarm %s cleared by the dial (dialled to it again within %.0f s): the tick fades", hm, DIAL_CLEAR_S);
        return 1;
    }
    if (ring == CW_RING_ALARM) ring_end("the dial");
    alarm_min = m; alarm_fired_date = 0; alarm_fired_min = -1;
    gentle_rearm(); gentle_took = false;                 /* THE GENTLE WAKE: a new alarm, a new window */
    alarm_set_ms = now_ms ? now_ms : 1;
    face_dial(m); face_dial_set();
    say("clockwork: alarm %s set by the dial (%d deg): the tick blinks twice; the same drag within %.0f s clears it", hm, ((angle_deg % 360) + 360) % 360, DIAL_CLEAR_S);
    return 1;
}

int clockwork_dial_minute(void) { return dial_minute; }
int clockwork_dial_timer_min(void) { return tm_dial_min; }

bool clockwork_due(uint32_t now_ms, int minute, uint32_t date)
{
    if (alarm_min >= 0 && minute >= 0 && minute == alarm_min && !alarm_fired(date, minute)) {
        if (!gentle_took) return true;
        /* THE GENTLE WAKE rang it early: its minute passes in silence, the nap goes on */
        alarm_fired_date = date; alarm_fired_min = minute;
        gentle_took = false; gentle_rearm();
        char hm[8]; hhmm(hm, sizeof hm, alarm_min);
        say("clockwork: alarm %s -- its minute, already rung by the gentle wake; nothing", hm);
    }
    if (tm_state == CW_TM_RUN && (int32_t)(now_ms - tm_end_ms) >= 0 && !last_asleep) return true;
    return false;
}

/* --- what shows -------------------------------------------------------------- */

uint32_t clockwork_elapsed_ms(uint32_t now_ms)
{
    if (sw_state == CW_SW_RUN) return now_ms - sw_start_ms;
    if (sw_state == CW_SW_FINAL) return sw_final_ms;
    return 0;
}

uint32_t clockwork_remaining_ms(uint32_t now_ms)
{
    if (tm_state != CW_TM_RUN) return 0;
    int32_t left = (int32_t)(tm_end_ms - now_ms);
    return left > 0 ? (uint32_t)left : 0;
}

float clockwork_arc(uint32_t now_ms)
{
    if (tm_dial_min >= 0) return tm_dial_min >= 60 ? 1.0f : (float)tm_dial_min / 60.0f;   /* THE TIMER FROM THE RIM: the arc follows the finger */
    if (ring == CW_RING_TIMER) return 0;
    if (tm_state == CW_TM_RUN) return tm_total_ms ? (float)clockwork_remaining_ms(now_ms) / (float)tm_total_ms : 0;
    if (sw_state != CW_SW_OFF) return (float)(clockwork_elapsed_ms(now_ms) % 60000u) / 60000.0f;
    return 0;
}

/* the number in the glance's spot: the ring's first, then the final,
 * then what runs. The alarm on the empty glass shows nothing here --
 * the face's own hour is at the centre. */
static void text_now(char *s, int cap, uint32_t now_ms)
{
    s[0] = 0;
    if (tm_dial_min >= 0) { mss(s, cap, (uint32_t)tm_dial_min * 60000u, true); return; }   /* THE TIMER FROM THE RIM: the duration under the finger */
    if (ring == CW_RING_ALARM) { if (last_holding) hhmm(s, cap, ring_minute); return; }
    if (ring == CW_RING_TIMER) { snprintf(s, cap, "0:00"); return; }
    if (sw_state == CW_SW_FINAL) { mss(s, cap, sw_final_ms, false); return; }
    if (sw_state == CW_SW_RUN) { mss(s, cap, now_ms - sw_start_ms, false); return; }
    if (tm_state == CW_TM_RUN) { mss(s, cap, clockwork_remaining_ms(now_ms), true); return; }
}

bool clockwork_showing(void)
{
    return sw_state != CW_SW_OFF || tm_state != CW_TM_OFF || (ring == CW_RING_ALARM && last_holding);
}

bool clockwork_busy(void)
{
    return sw_state == CW_SW_RUN || (sw_state == CW_SW_FINAL && exhale_t < EXHALE_S) || bounce_t >= 0;
}

static inline uint16_t ink(const gel_rgba_t *c, float k)
{
    int R = (int)(c->r * k * 31 + 0.5f), G = (int)(c->g * k * 63 + 0.5f), B = (int)(c->b * k * 31 + 0.5f);
    uint16_t v = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((v >> 8) | (v << 8));            /* the panel's byte order */
}

static mark_t mk;                       /* THE MARK (mark.h): what this frame drew (the arc marks in face's) */
bool clockwork_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

static void block(uint16_t *fb, int x, int y, uint16_t c)
{
    mark_rect(&mk, x, y, TEXT_PX, TEXT_PX, c);
    for (int yy = y; yy < y + TEXT_PX; yy++) {
        if (yy < 0 || yy >= FACE_H) continue;
        for (int xx = x; xx < x + TEXT_PX; xx++) {
            if (xx < 0 || xx >= FACE_W) continue;
            fb[yy * FACE_W + xx] = c;
        }
    }
}

void clockwork_draw(uint16_t *fb, uint32_t now_ms, int at)
{
    const gel_rgba_t *sn = &theme_pal()->shine;
    float arc = clockwork_arc(now_ms);
    if (arc > 0) face_arc(fb, arc, true, ink(sn, 1.0f));
    if (sw_state != CW_SW_OFF && laps > 0) {
        /* LAPS ON THE ARC: each lap's tick where the count was on the minute's dial */
        uint16_t lc = ink(&theme_pal()->body, 1.0f);
        float r_out = face_ring_r();
        for (int i = 0; i < laps; i++)
            face_tick(fb, (float)(lap_ms[i] % 60000u) / 60000.0f, r_out - LAP_TICK_IN, r_out, LAP_TICK_W, lc, true);
    }
    if (tm_state == CW_TM_RUN && last_minute >= 0) {
        /* THE TIMER'S TICK: where the minute hand will be when it rings (the
         * minute plus the remaining, on the hour's dial), in the shine, on black */
        float end_min = (float)(last_minute % 60) + (float)clockwork_remaining_ms(now_ms) / 60000.0f;
        float r_out = face_ring_r() - TIMER_TICK_IN;
        face_tick(fb, fmodf(end_min, 60.0f) / 60.0f, r_out - TIMER_TICK_LEN, r_out, TIMER_TICK_W, ink(sn, 1.0f), false);
    }
    char s[8];
    text_now(s, sizeof s, now_ms);
    if (!s[0]) return;
    /* the final time dims over its last half second, the glance's way */
    float k = 1.0f;
    if (sw_state == CW_SW_FINAL && ring == CW_RING_NONE && tm_state == CW_TM_OFF && sw_final_t < FINAL_FADE_S)
        k = 0.15f + 0.85f * (sw_final_t / FINAL_FADE_S);
    uint16_t c = ink(sn, k);
    int n = (int)strlen(s);
    int w = n * 4 - 1;
    /* the hour's spot AT the wall THE PLACE chose (the top, or the bottom while the gel is on the top wall) */
    static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { 1, 0, -1, 0 };
    int x0 = FACE_W / 2 + DX[at & 3] * TEXT_R - (w * TEXT_PX) / 2, y0 = FACE_H / 2 + DY[at & 3] * TEXT_R - (5 * TEXT_PX) / 2;
    for (int i = 0; i < n; i++) {
        const uint8_t *g = s[i] == ':' ? COLON : DIGITS[(s[i] - '0') % 10];
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (g[row] & (1 << (2 - col)))
                    block(fb, x0 + (i * 4 + col) * TEXT_PX, y0 + row * TEXT_PX, c);
    }
}

int clockwork_sw_state(void) { return sw_state; }
int clockwork_tm_state(void) { return tm_state; }
int clockwork_ring(void) { return ring; }

void clockwork_status(char *out_s, int cap)
{
    char a[16], b[16], c[16];
    if (sw_state == CW_SW_OFF) snprintf(a, sizeof a, "off");
    else mss(a, sizeof a, sw_final_ms, false);
    if (tm_state == CW_TM_OFF) snprintf(b, sizeof b, "off");
    else mss(b, sizeof b, tm_total_ms, true);
    if (alarm_min < 0) snprintf(c, sizeof c, "off");
    else hhmm(c, sizeof c, alarm_min);
    char d[24] = "", l[24] = "";
    if (dial_minute >= 0) snprintf(d, sizeof d, " (dialling %02d:%02d)", dial_minute / 60, dial_minute % 60);
    if (sw_state != CW_SW_OFF && laps) snprintf(l, sizeof l, ", %d lap%s on the arc", laps, laps == 1 ? "" : "s");
    if (tm_dial_min >= 0) snprintf(b, sizeof b, "dialling %d min", tm_dial_min);
    char g[96]; gentle_in_t gi; gentle_fill(&gi, last_now_ms, last_minute, last_asleep); gentle_status(&gi, g, sizeof g);   /* THE GENTLE WAKE: the window as of the last tick */
    snprintf(out_s, cap, "clockwork: stopwatch %s%s%s | timer %s%s | alarm %s%s%s | ring %s%s%s | %s",
             sw_state == CW_SW_RUN ? (softened ? "running, the breath held, the stare softened" : "running, the breath held, the eyes wide") : a,
             sw_state == CW_SW_FINAL ? " (the final time lit)" : "", l,
             b, tm_state == CW_TM_RUN ? " running" : tm_state == CW_TM_RING ? " at zero" : "",
             c, alarm_min >= 0 && alarm_fired_min >= 0 ? " (fired today)" : "", d,
             ring == CW_RING_ALARM ? "the alarm" : ring == CW_RING_TIMER ? "the timer" : "none",
             ring != CW_RING_NONE && bounce_pending ? (owed ? ", the bounces OWED from his sleep (paid on his first lit frame up; a tap forgives)" : ", the bounces waiting for a lit glass") : "",
             bounce_t >= 0 ? ", bouncing" : "", g);
}
