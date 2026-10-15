/* THE NIGHT'S SUM (the overhaul wave, 2026-10-08; design/watch-ideas.md
 * D33 THE DEEP NIGHT, D36 THE NIGHT'S STEADINESS, D34 UNDER THE WEATHER):
 * what the week's day files say about his nights and his resting heart,
 * read as a few verdicts the face and the tell wear -- never a number
 * on the glass.
 *
 * Fed ROWS, one per minute of the day log (daylog.h's daylog_min_t: the
 * same steps / energy / flags rest.cpp reads, plus the `hr` byte -- bpm
 * at rest, 0 = none; RESERVED today, the PPG's spots fill it another
 * hand's evening). Two feeders, the same call: THE WALK (daylog.cpp's
 * wrist_night_walk: the newest NS_WALK_DAYS files oldest first, through
 * a 32-row buffer, once at boot after the clock and once at the date's
 * turn, beside the norm -- the pattern of daykeep) and THE LIVE ROW
 * (daylog.cpp's finish_minute, the minute just closed), so the sum is
 * fresh between walks and a reboot rebuilds it. A row fed twice (the
 * walk's last minute, then the live one) is dropped by its minute.
 *
 * THE NIGHT, found again (rest.cpp's rule, simplified to a pure pass
 * over rows in time order -- rest's own detector is live state and
 * cannot be called twice): a run of STILL rows (energy under
 * NS_STILL_MG, no steps -- rest's line) that tolerates a toss (non-still
 * rows under NS_TOSS_MAX_MIN in a row, ending the run at the last still
 * row when they pass it), ends at steps of NS_STEPS_UP, the "I'm up"
 * flag, or a raise / a tap outside the quiet hours (inside them a raise
 * is a stir: the night light), and at a gap in the rows (the watch
 * off). It is HIS night when it ran NS_NIGHT_MIN or longer, touched the
 * quiet hours (a nap by day is not a night, whatever its length -- the
 * steadiness would be nonsense), a WORN row came NS_WORN_AFTER rows in
 * (the shelf rule: a still wrist moves a little; a shelf never does)
 * and it did not begin on the cable. One night per calendar day: the
 * longest that ENDED on it.
 *
 *   D33 THE DEEP NIGHT: the nocturnal dip -- the median resting bpm of
 *       the night's MIDDLE THIRD against the median of the DIP_REF_MIN
 *       minutes before it began (the evening at rest), DIP_BPM or more
 *       down, DIP_MIN_SAMPLES on each side; else flat; too few samples
 *       (every night today: the bytes are 0) = unknown, and unknown is
 *       silent. The face draws the night ring in the shine for a deep
 *       night (nightsum_deep: the newest night, matched to rest's own
 *       by its end within NS_MATCH_MIN -- the ring is rest's night).
 *   D36 THE NIGHT'S STEADINESS: the midpoints (bed + wake over two, on
 *       the absolute minute line) of the newest STEADY_NIGHTS nights;
 *       their range over STEADY_WANDER_MIN = the nights wander, and the
 *       night ring's tip breathes (face.cpp); under it, or fewer than
 *       STEADY_MIN_NIGHTS nights, steady. Needs no PPG: live from the
 *       third night of files.
 *   D34 UNDER THE WEATHER: the day's resting median (every non-zero hr
 *       byte outside a night -- the byte is a rest spot by contract) over
 *       the week's norm (the median of the WEATHER_DAYS previous days'
 *       medians, WEATHER_MIN_DAYS of them at least) by WEATHER_BPM or
 *       more, with WEATHER_MIN_SAMPLES today -- the tell wears the
 *       restless night's droopy first face once that day. The norm is
 *       the WRIST's (the idea said the box's; the wrist has the files
 *       and the box may be far): ruling 88.
 *
 * Pure C, no Arduino: tools/nightsum_test.cpp feeds it synthetic
 * weeks. ~1 KB of static state (the reference ring, the run's bins,
 * the day's histogram); nothing allocated. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the night's rule, rest.cpp's numbers (STILL_MG, TOSS_MAX_MIN, STEPS_UP, SLEPT_H, SHELF_WORN_MIN) */
#define NS_STILL_MG         15      /* a still row: energy under this and no steps (rest's STILL_MG) */
#define NS_TOSS_MAX_MIN     15      /* non-still rows in a row that end the run at its last still row (rest's TOSS_MAX_MIN) */
#define NS_STEPS_UP         30      /* steps in a row that are "he is up" (rest's STEPS_UP) */
#define NS_NIGHT_MIN        180     /* 3 h: the night, never a nap (rest's SLEPT_H) */
#define NS_WORN_AFTER       10      /* a WORN row this many rows in = a wrist, not a shelf (rest's SHELF_WORN_MIN) */
#define NS_GAP_MIN          3       /* a jump of more than this many minutes between rows = the watch was off: the run ends, the reference forgets */
#define NS_MATCH_MIN        120     /* D33: the file's night and rest's agree when their ends are within this (the toss, the wake minute) */
/* D33 THE DEEP NIGHT */
#define DIP_BPM             8       /* the dip: the literature's nocturnal dip is ~10 % of resting HR (6-8 bpm at 60-80); a flat 8 is the guess -- ruling 86 */
#define DIP_MIN_SAMPLES     3       /* spots on each side before a verdict (every ~5 min at rest: a quarter hour each) */
#define DIP_REF_MIN         180     /* the reference: the 3 h before the night began -- the evening at rest, the spots' usual hour */
#define NS_BIN_MIN          5       /* the run's hr kept one per 5-minute bin (the spot's own pace: a minute-by-minute byte only overwrites) */
#define NS_RUN_BINS         192     /* 16 h of bins: the longest run kept (a longer one keeps its first 16 h) */
/* D36 THE NIGHT'S STEADINESS */
#define STEADY_NIGHTS       7       /* the week's nights judged */
#define STEADY_MIN_NIGHTS   3       /* fewer = nothing to say, steady */
#define STEADY_WANDER_MIN   60      /* the midpoints' range over an hour = the nights wander -- ruling 87 */
/* D34 UNDER THE WEATHER */
#define WEATHER_BPM         7       /* today's resting median this far over the week's norm -- ruling 88 */
#define WEATHER_DAYS        7       /* the norm: the previous week's day medians */
#define WEATHER_MIN_DAYS    3       /* ... at least this many of them (face.cpp's NORM_MIN_DAYS: a norm from fewer is a guess) */
#define WEATHER_MIN_SAMPLES 6       /* a day's median stands on this many spots (half an hour of rest at ~5 min a spot) */
/* the walk */
#define NS_WALK_DAYS        8       /* today and the 7 before it: 7 nights' ends, 7 days' norms */
#define NS_DAYS             10      /* day slots from the walk's base (today - 7): the 8 walked + 2 lived past the last walk */
#define NS_HR_MAX           220     /* bpm bins; a byte past it is clipped */

enum { NS_UNKNOWN = -1, NS_FLAT = 0, NS_DEEP = 1 };     /* a night's dip verdict */

typedef struct {
    uint32_t end_abs;       /* the last still minute, absolute (days since 1970 x 1440 + the minute) */
    uint32_t mid_abs;       /* the midpoint, the same line */
    int      minutes;       /* its length */
    int      dip;           /* NS_UNKNOWN / NS_FLAT / NS_DEEP */
    int      ref_hr, mid_hr;        /* the two medians, 0 = none */
    int      ref_n, mid_n;          /* the samples each stood on */
} nightsum_night_t;

/* start over for a walk: ``today`` YYYYMMDD (0 = no clock: every row is
 * dropped until a walk with a date) -- the base is today - 7 */
void nightsum_begin(uint32_t today);
/* one row of the day log, in time order: its date and minute of the
 * day, the row's steps / energy / flags (DAYLOG_* bits: WORN, CHARGING,
 * UP), its raises and taps, its hr byte (0 = none), and whether the
 * minute is in the quiet hours (rest_night_at). A row at or before the
 * last one fed is dropped. */
void nightsum_row(uint32_t date, int minute, uint16_t steps, uint16_t energy_mg, uint8_t flags,
                  uint8_t raises, uint8_t taps, uint8_t hr, bool quiet);
/* the newest night found (false = none) */
bool nightsum_last_night(nightsum_night_t *out);
/* D33: the newest night is DEEP and is the night ``night_id`` names
 * (rest_night_id(): its wake's absolute minute; 0 = none: false) */
bool nightsum_deep(uint32_t night_id);
/* D36: the newest nights' midpoints wander (their range over
 * STEADY_WANDER_MIN); ``range_min`` / ``nights`` may be NULL */
bool nightsum_wander(int *range_min, int *nights);
/* D34: today's resting median stands WEATHER_BPM over the week's norm;
 * ``today_hr`` / ``norm_hr`` (either may be NULL) get the two medians
 * (0 = not known) whatever the answer */
bool nightsum_weather(int *today_hr, int *norm_hr);
/* for the test and `night`: the resting median of the day ``days_ago``
 * (0 = today, its samples so far), 0 = none; and today's sample count */
int  nightsum_day_hr(int days_ago);
int  nightsum_today_samples(void);
/* `night` over serial: the nights, the wander, the dip, the weather; one line */
void nightsum_status(char *out, int cap);
/* the civil calendar: days since 1970-01-01 (Hinnant's days_from_civil) */
uint32_t nightsum_ordinal(uint32_t yyyymmdd);

#ifdef __cplusplus
}
#endif
