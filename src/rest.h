/* The wrist's night and its breath (design/watch-body.md section 1:
 * "night, you asleep: it sleeps too, Zs (dreams only with visible Zs --
 * his rule)"; "you wake: it wakes with you: stretch, the morning face").
 *
 * Fed by the day log's minute rows (rest_minute: steps, energy, still
 * seconds -- the same numbers wrist.py reads) and by one call a frame
 * (rest_tick), read back as a few numbers (rest_out_t). Plain C++ with
 * no Arduino in it: the time and the rows are parameters, so
 * tools/rest_test.cpp runs a synthetic day on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* what main hands over each frame */
typedef struct {
    float       dt;             /* seconds since the last tick */
    int         minute;         /* the local minute of the day, -1 = no clock */
    bool        lit;            /* the screen is on (awake / dim) */
    uint32_t    motion_seq;     /* wrist_motion_events(): raises + shakes, cumulative */
    bool        walking;        /* wrist_walking(): steps in the last 10 s */
    bool        linked;         /* link_linked(): a pending sense may go now */
    const char *quiet;          /* the grain's quiet hours ("23-7"), NULL/"-" = the default */
    /* THE PULSE AS THE BREATH (pulse.h, watch-ideas D31; 2026-10-08): the
     * last CLEAN spot's bpm (0 = none yet) and its age in seconds; past
     * PULSE_STALE_MIN or out of the honest band it is ignored. Last on
     * purpose: the positional initialisers before it still hold (0, 0) */
    float       pulse_bpm;
    float       pulse_age_s;
    /* THE WAKING BREATH (design/watch-night-2026-10-12.md C6): true on
     * the ONE frame the glass lit by a RAISE, holding (main's wake_up
     * says; a touch-wake, a shake, a word, the boot: false) -- the breath
     * snaps to the inhale's start and its first cycle is WAKE_BREATH_X,
     * easing to 1 over WAKE_BREATH_EASE_S (rest.cpp). Refused asleep or
     * stirring (the sleeper's breath is the night's), off a cold shelf
     * (PUT ON COLD's moment) and under the morning stretch. Last on
     * purpose: the positional initialisers before it still hold (false) */
    bool        woke_raise;
    /* THE LULL (lull.c, design/watch-night-2026-10-12.md C11): the lull's
     * level 0..1 (lull_level) -- three slow strokes inside a minute, a
     * hand petting him toward rest: the breath's TARGET eases toward the
     * sleeper's SLEEP_HZ by this much (the band's own ease; the why
     * unchanged -- a look, not a band; nothing asleep). Last on purpose:
     * the positional initialisers before it still hold (0) */
    float       lulled;
    /* THE WARM SKIN (warm.c, design/watch-night-2026-10-12.md C20): true
     * on the ONE frame the skin proxy's COLD band is paid (main's
     * warm_tick said WARM_ACT_SHIVER: lit, holding, awake) -- PUT ON
     * COLD's road: the shiver as cold_a2 this frame and the breath
     * snapped to the sleeper's, warming over BREATH_EASE_S (no look up:
     * that is the shelf's). Refused dozing or replaying. Last on purpose:
     * the positional initialisers before it still hold (false) */
    bool        chilled;
    /* THE BREATH READ (resp.h, ppg.cpp's ppg_breath_now; 2026-10-14): the
     * last CLEAN respiration rate off a read on the wrist, breaths a
     * minute (0 = none fresh) -- CARRIED, NOT FOLLOWED: the gel's resting
     * breath keeps THE PULSE AS THE BREATH's road (the pulse's proportion)
     * tonight, by his ruling (154); this field is the door for the day he
     * says the real breath may lead it. Last on purpose: the positional
     * initialisers before it still hold (0) */
    float       breath_bpm;
} rest_in_t;

/* one rising Z */
typedef struct {
    bool  on;
    float age;                  /* seconds since it left the crown */
    float rise;                 /* face px above the crown */
    float drift;                /* face px along the floor, from the crown */
    float alpha;                /* 1 -> 0 */
    int   px;                   /* the glyph's pixel size on the glass */
} rest_z_t;

#define REST_ZS 3

/* what main reads back */
typedef struct {
    bool   asleep;              /* the deep sleep look, the Zs */
    bool   dozing;              /* asleep OR stirring: the night is not over (THE NIGHT LIGHT keeps the sleep look through a stir) */
    float  breath_hz, breath_amp;
    float  breath_rest_hz;      /* THE BUMPS (bumps.h): the resting rate the breath eases toward at rest (the grain's and vigor's), so "the breath is quick" is a ratio, not a number. THE PULSE AS THE BREATH leaves it the grain's: a pulse-led breath above it IS quick -- the purr is rightly kept */
    bool   breath_pulse;        /* THE PULSE AS THE BREATH (D31): the breath follows his pulse now (a fresh one, awake, not walking) */
    float  breath_phase;        /* radians, integrated: no jump when the rate changes */
    float  stretch;             /* 0..1 the morning stretch's phase, 0 = none */
    float  itch_deg;            /* THE ITCH IN THE BOWL (watch-ideas D28): a long sit, as degrees off gravity's angle (+-4 at 60 min to +-10 at 180, the side shifting every 3); 0 = none. main adds it to the bowl's target */
    /* PUT ON COLD (watch-ideas A6, 2026-09-23): the first raise after
     * hours on the shelf -- one shiver (the cold sky's own 0.3 s jiggle,
     * as a2 this frame; main adds it to the 2-lobe harmonic), a look up
     * (seconds left; main takes gaze_y -1 while > 0), and the breath
     * snapped to the sleeper's, warming over its own ease */
    float  cold_a2;
    float  cold_look;
    rest_z_t z[REST_ZS];
    /* a sense for the link, once per span; main sends it and clears it */
    bool   sense_new;
    char   sense_kind[16];
    char   sense_words[48];
    /* a log line for the serial port, one at a time */
    bool   log_new;
    char   log[128];               /* (128: the "up at" line carries "worn at the start" now) */
    /* THE RUN AND THE PANT (design/watch-night-2026-10-12.md C10): run =
     * the newest closed minute had RUN_SPM steps or more (the breath at
     * RUN_HZ, the walk's depth; main squints: the wind); pant_squash =
     * the crown a hair lower after the run ends, easing out over PANT_S
     * (main adds it to the squash; breath_amp carries the pant's x1.3
     * itself). Last on purpose: appended after the log, as every field is */
    bool   run;
    float  pant_squash;
} rest_out_t;

void rest_begin(float breath_hz, float breath_amp);     /* main's resting BREATH_HZ / BREATH_AMP */
/* a closed minute of the day log: minute = the local minute of the day,
 * or -1; worn / charging are the row's WORN and CHARGING flags (THE SHELF
 * RULE reads them: a still run on the dock, or one the wrist never moved
 * in, is the shelf, not his night) */
void rest_minute(int minute, uint16_t steps, uint16_t energy_mg, uint8_t still_s, bool worn, bool charging);
const rest_out_t *rest_tick(const rest_in_t *in);
/* A TOUCH ENDS SLEEP (Khalid, 2026-09-10 05:40: he tapped the glass and
 * the Zs stayed): a hand on the glass is a person awake -- asleep or
 * stirring, the span ends here as a rising (the stretch and the sense
 * as for motion); on the shelf, the run ends. main calls it for the
 * touch that wakes the glass and for a finger landing on a lit one. */
void rest_touch(void);
/* THE SHAKEN SLEEPER (design/watch-ideas.md A1): a shake is "I'm up" --
 * asleep or stirring, the span ends here as a rising, never a stir; on
 * the shelf, the run ends. main calls it for the shake that wakes the
 * glass (wake_up("a shake")) and for the lit shake (GES_SHAKE). */
void rest_shake(void);
bool rest_on_shelf(void);                              /* THE SHELF: a still run judged not worn */
bool rest_cold(void);                                  /* PUT ON COLD: the breath still warming from the shelf (under the day's rate, easing up) */
/* THE NIGHT RELEASE (ruling 52, Khalid 2026-09-10 18:10: "if the watch
 * is sleeping, send the slime to the next thing, reclaimed by a simple
 * wake"): the two moments the LEASE cares about, taken once by whoever
 * polls (link.cpp in the loop; main's nap() between light sleeps, where
 * the night's onset actually lands) -- REST_EV_NIGHT when the verdict
 * becomes ASLEEP for the NIGHT (the quiet-hours still, worn at the
 * start: never the shelf, never the cable, never a day nap; a shelf run
 * promoted by a fidget in the quiet hours counts, late), REST_EV_RISING
 * when a night span ends, however it ended and however long it was (the
 * raise / shake / touch / steps: he is up, the wrist claims back).
 * Nothing in a replay. */
enum { REST_EV_NONE = 0, REST_EV_NIGHT = 1, REST_EV_RISING = 2 };
int  rest_take_lease_event(void);
void rest_sense_sent(void);                             /* main took the sense */
void rest_log_taken(void);                              /* main printed the log line */
void rest_status(char *out, int cap);                   /* `rest` over serial: one line */
/* for the test: the span's minutes so far, or the last span's */
int  rest_span_min(void);
/* THE WATCH FACE's night ring: the last night's minutes -- a span that
 * was the night (>= 3 h), or the one growing now if it began in the
 * quiet hours; 0 = no night known (a nap is not a night; a reboot
 * forgets: rest.cpp keeps nothing across power) */
int  rest_night_min(void);
const char *rest_state_name(void);                     /* awake / asleep / stirring / on the shelf */
bool rest_dozing_now(void);                            /* THE GENTLE WAKE (clockwork): asleep OR stirring as of the last closed minute -- live through the nap, where rest_tick does not run */
/* THE NIGHT AFTER A REBOOT: rest.cpp keeps nothing across power, but the
 * day files do (daylog.cpp: /day/YYYYMMDD.bin, one row a minute).
 * Once the clock is set, daylog.cpp replays yesterday's and today's
 * rows through the SAME detector, in slices off the frame loop:
 * rest_replay_begin() (false = refused: the wrist has already lived
 * more than a night's onset clockless -- what it saw is fresher than
 * the files), rest_replay_row() per row (a raise / a shake in the row
 * is the motion the live path would have seen, a tap the hand on the
 * glass, the UP flag the "I'm up" shake -- THE SHAKEN SLEEPER's rising
 * -- WORN / CHARGING the shelf rule's flags), rest_replay_end()
 * once. Replayed: no sense goes up, no stretch plays, no log line per
 * span -- one summary line at the end; the last span, the night ring,
 * the tell's facts and the breath's runs are what they would have been. */
bool rest_replay_begin(void);
void rest_replay_row(uint32_t date, int minute, uint16_t steps, uint16_t energy_mg, uint8_t still_s,
                     uint8_t raises, bool shaken, uint8_t taps, bool worn, bool charging, bool up);
void rest_replay_end(uint32_t date_a, int rows_a, uint32_t date_b, int rows_b);
bool rest_rebuilt(void);                                /* `rest`: "rebuilt from the files" */
/* the last night's STABLE id -- its wake's absolute minute (days since
 * 1970 x 1440 + the end minute), the same number live and replayed, so
 * a night told before a reboot is known told after it (store.told_night);
 * 0 = no night, or no date known when it ended */
uint32_t rest_night_id(void);
/* the row clock's date: daylog.cpp says which day the closed minute
 * belongs to (0 = no clock); the replay sets it per row */
void rest_set_date(uint32_t yyyymmdd);
/* the civil calendar, for the id and for "yesterday" (no libc TZ needed) */
uint32_t rest_date_ordinal(uint32_t yyyymmdd);          /* days since 1970-01-01 */
uint32_t rest_date_prev(uint32_t yyyymmdd);             /* the day before, YYYYMMDD */
/* THE GLANCE THAT ANSWERS (tell.cpp) reads the wrist's freshest facts
 * here -- plain numbers, dated by the row clock (rest_rows: one per
 * closed minute, with or without a clock) so a fact tells once */
typedef struct {
    int      minutes;           /* the span's length */
    int      end_minute;        /* the local minute it ended on, -1 = no clock */
    int      awakenings;
    int      restless_min;      /* rows in the span that were not still (the stirs included) */
    uint32_t end_row;           /* rest_rows() when it ended: the fact's id, its age = rows since */
    bool     night;             /* >= 3 h: the night (a nap is not one) */
} rest_span_t;
bool     rest_last_span(rest_span_t *out);   /* false = no span has ended yet */
uint32_t rest_rows(void);                    /* closed minutes seen since boot */
int      rest_sit_min(void);                 /* the open sit: consecutive rows under 60 mg with under 30 steps (a night counts: cap it at the wake) */
int      rest_steps_last(int rows);          /* steps in the last ``rows`` minutes (up to 60) */
int      rest_rows_since_walk(void);         /* rows since the last minute with 30+ steps; -1 = none in the hour */
bool     rest_night_now(void);               /* the quiet hours now (false with no clock) */
bool     rest_night_at(int minute);          /* QUIET TIME (wake.cpp): the quiet hours at a minute of the day; -1 = no clock: the grain's night=1 */
