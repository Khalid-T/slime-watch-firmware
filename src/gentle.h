/* THE GENTLE WAKE (design/watch-ideas.md D35; 2026-10-08, THE OVERHAUL
 * WAVE): "Only when HE set an alarm: the bounces may come up to twenty
 * minutes early if the wrist is stirring and the pulse is rising -- a
 * light phase, the sleep guardian bending the way it does for the
 * alarm. Never without an alarm."
 *
 * The rule, pure C, no Arduino, no clock of its own: CLOCKWORK owns the
 * alarm and asks this once a nap poll (clockwork_gentle_due from the
 * nap, clockwork_tick when the glass is lit through a stir). It keeps
 * the pulse's short memory -- the last GENTLE_HIST spot results with
 * their stamps -- and THE NIGHT'S FLOOR: the lowest clean reading taken
 * while he dozed, since the last reading taken awake (a reading awake
 * ends the night the floor belonged to). Fed by gentle_pulse where a
 * spot finishes (today main's PULSE ROW block and the nap's poll, both
 * through gentle_pulse_seen, which takes a result once by ppg's count;
 * the richer ring another hand is building -- a spot every ~5 min at
 * rest -- is the same call where its result lands: one line).
 *
 * "Wake now" = ALL of: an alarm is set and the clock known; now is in
 * the GENTLE_WINDOW_MIN before it (not at it: the minute itself is the
 * alarm's own); he dozes (rest's asleep or stirring: never the awake,
 * never the shelf); the wrist stirred within GENTLE_STIR_S; the last
 * two clean readings, the newest within GENTLE_PULSE_AGE_S, both sit
 * GENTLE_RISE_BPM over the night's floor, the newest no lower than the
 * one before (a rise, not a blip) -- the light phase. Once per alarm:
 * gentle_spent() when it fires, gentle_rearm() when the alarm is set
 * or its minute passes. The alarm's own ring at its minute is
 * CLOCKWORK's and untouched: if the window never said, it rings. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the window: twenty minutes, the item's own number -- a light phase
 * is 10-20 min long, so the window spans about one */
#define GENTLE_WINDOW_MIN 20
/* a stir this recent is "stirring": two minutes -- a turn in the sheets
 * is followed by stillness again within a minute or two if he was only
 * turning; a stir inside the window with the pulse up is a surfacing */
#define GENTLE_STIR_S 120
/* the rise over the night's floor that reads as the light phase: deep
 * sleep's rate to REM's / light's is ~5-10 bpm apart in the literature
 * on a resting adult; 5 is the floor of that, a smaller rise is noise
 * on a 10-30 s spot */
#define GENTLE_RISE_BPM 5
/* the newest reading must be this fresh: fifteen minutes -- three of
 * the coming ring's ~5-min spots; an older pulse is last hour's */
#define GENTLE_PULSE_AGE_S 900
/* the readings kept: four -- the last two the rule reads, two more so
 * `clockwork` can say where it has been; the night's floor is its own
 * number and remembers the whole night */
#define GENTLE_HIST 4
/* a floor needs this many clean dozing readings under it (the two the
 * rule reads and one more): with fewer, "no pulse known" */
#define GENTLE_FLOOR_N 3

typedef struct {
    int      alarm_minute;   /* the alarm's minute of the day, -1 = none */
    int      minute;         /* now, the local minute of the day, -1 = no clock */
    bool     dozing;         /* rest's asleep OR stirring (the night is on) */
    int      stir_age_s;     /* seconds since the wrist last stirred, -1 = never */
    uint32_t now_ms;
} gentle_in_t;

enum {
    GENTLE_NO_ALARM = 0,     /* nothing set: never */
    GENTLE_NO_CLOCK,         /* an alarm, no clock to measure the window by */
    GENTLE_SPENT,            /* it already woke him for this alarm (or the alarm rang) */
    GENTLE_ARMED,            /* outside the window: waiting for it */
    GENTLE_AWAKE,            /* in the window, but rest says he is up (or on the shelf) */
    GENTLE_NO_STIR,          /* in the window, the wrist still */
    GENTLE_NO_PULSE,         /* in the window, stirring, no pulse known (no floor, no fresh pair) */
    GENTLE_FLAT,             /* in the window, stirring, the pulse not risen */
    GENTLE_WAKE              /* say it now */
};

typedef struct {
    bool wake;               /* = state == GENTLE_WAKE */
    int  state;              /* GENTLE_* above */
    int  from_minute;        /* the window's first minute (alarm - GENTLE_WINDOW_MIN, wrapped); -1 with no alarm / clock */
    int  floor_bpm;          /* the night's floor, 0 = none */
    int  last_bpm, prev_bpm; /* the last two clean readings, 0 = none */
} gentle_out_t;

void gentle_begin(void);
/* a finished spot: its bpm and whether it was clean, when, and whether
 * he dozed as it was taken (the floor's night) */
void gentle_pulse(float bpm, bool ok, uint32_t now_ms, bool dozing);
/* the same through ppg's "spots finished" count: a result is taken once
 * however many callers poll (main's lit loop, the nap). seq 0 = none yet */
void gentle_pulse_seen(uint32_t seq, float bpm, bool ok, uint32_t now_ms, bool dozing);
/* the rule: pure, changes nothing; true = wake now */
bool gentle_check(const gentle_in_t *in, gentle_out_t *out);
void gentle_spent(void);     /* it woke him: not again for this alarm */
void gentle_rearm(void);     /* a new alarm, or the old one's minute passed */
bool gentle_is_spent(void);
/* `clockwork`: one phrase, "gentle: armed 07:10-07:30" / "gentle: in the
 * window, not stirring" / "gentle: in the window, no pulse known" ... */
void gentle_status(const gentle_in_t *in, char *out, int cap);
const char *gentle_state_name(int state);
int  gentle_count(void);     /* readings kept (for the test) */

#ifdef __cplusplus
}
#endif
