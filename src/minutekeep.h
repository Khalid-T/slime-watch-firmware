/* THE MINUTE KEPT ACROSS A RESET (2026-10-12, the operator night, T3).
 * The day log's minute in progress -- the tally daylog.cpp fills from
 * every IMU sample and every touch -- used to live in .bss and die with
 * every reset: an OTA, a `reboot`, a watchdog's bite, a panic, each one
 * took up to a minute of his day with it (the OTA and the reboot flush
 * first; a bite and a panic never could). Now the tally lives in RTC
 * slow memory (RTC_NOINIT_ATTR in daylog.cpp: the one region a reset
 * does not clear, ~90 B), stamped each tick with the date and the
 * minute it belongs to, and at the next boot this module says what to
 * do with what it finds:
 *
 *   SAME     a soft reset in the same minute: the tally is taken back
 *            as the minute in progress and goes on filling;
 *   CROSSED  the reset crossed a minute boundary (the same day): the
 *            kept minute is filed as its own row before the new one
 *            (daylog.cpp sets last_minute to it; the next tick files it
 *            and the live minute begins from zero);
 *   DISCARD  a power-on (the RTC memory is garbage), a reset reason
 *            the SDK could not name, no clock yet (a kept minute with
 *            no clock to check it against), a stamp from another day
 *            (a reset across midnight: yesterday's file is closed by
 *            then), a stamp AHEAD of the clock, a wrong magic, or any
 *            field out of its bounds.
 *
 * Garbage never files: the magic is the first gate and every field has
 * a bound (a tally is at most a minute of samples at the loop's rate;
 * a double that is not a number is not a tally). Pure: no Arduino, no
 * clock of its own -- the reset kind, the clock and the stamp are
 * parameters; tools/minute_keep_test.cpp runs the table on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MK_MAGIC        0x314B4D53u   /* "SMK1" little-endian */

/* the bounds: a minute of the wrist at the loop's rate, with room */
#define MK_MAX_N        20000         /* samples a minute (the loop runs ~50 Hz awake: 3000; the nap 4 Hz) */
#define MK_MAX_STEPS    1000          /* STEP_REFRACT_MS 250 caps a minute at 240 */
#define MK_MAX_SECS     200           /* screen_s / still_s: wrist_tick counts at most 120 s a call */
#define MK_MAX_SHAKES   60
#define MK_MAX_SUMSQ_G2 256.0         /* |hp| never past the IMU's 16 g: hp*hp <= 256 per sample */
#define MK_DATE_MIN     20200101u
#define MK_DATE_MAX     20991231u

/* the tally daylog.cpp fills (its `cur`): one minute in progress */
typedef struct {
    uint32_t steps, raises, taps, shakes;
    double   sumsq; uint32_t n;
    int      screen_s, still_s;
    int      posture_n[5];
    uint8_t  shaken;                /* bool, kept a byte so a garbage value can be checked */
    uint8_t  up;                    /* the "I'm up" shake landed this minute */
    int      drinks, meals;         /* the marks: double / triple taps */
    int      hr, hrv;               /* THE PULSE IN THE DAY: the minute's clean read, 0 = none */
} mk_tally_t;

/* what the RTC memory holds: the stamp around the tally */
typedef struct {
    uint32_t   magic;               /* MK_MAGIC while a minute is in progress; 0 = nothing kept */
    uint32_t   date;                /* YYYYMMDD the tally belongs to */
    int32_t    minute;              /* its minute of the day, 0..1439 */
    mk_tally_t t;
} mk_keep_t;

enum { MK_DISCARD = 0, MK_SAME = 1, MK_CROSSED = 2 };

/* why a keep was discarded (or taken), for the one boot line */
enum {
    MK_WHY_TAKEN = 0,
    MK_WHY_POWERON,                 /* a power-on: the RTC memory is garbage by definition */
    MK_WHY_NO_CLOCK,                /* no clock to check the stamp against */
    MK_WHY_MAGIC,                   /* no stamp, or a wrong one */
    MK_WHY_BOUNDS,                  /* a field past its bound: garbage */
    MK_WHY_OTHER_DAY,               /* the stamp is from another date */
    MK_WHY_AHEAD                    /* the stamp is ahead of the clock */
};

static inline bool mk_date_ok(uint32_t d)
{
    if (d < MK_DATE_MIN || d > MK_DATE_MAX) return false;
    uint32_t m = (d / 100) % 100, day = d % 100;
    return m >= 1 && m <= 12 && day >= 1 && day <= 31;
}

/* every field inside its bound; the magic first */
static inline bool mk_check(const mk_keep_t *k)
{
    if (!k || k->magic != MK_MAGIC) return false;
    if (!mk_date_ok(k->date)) return false;
    if (k->minute < 0 || k->minute > 1439) return false;
    const mk_tally_t *t = &k->t;
    if (t->n > MK_MAX_N || t->steps > MK_MAX_STEPS) return false;
    if (t->raises > 255 || t->taps > 255 || t->shakes > MK_MAX_SHAKES) return false;
    if (!(t->sumsq == t->sumsq) || t->sumsq < 0.0 || t->sumsq > MK_MAX_SUMSQ_G2 * (double)t->n + 1.0) return false;   /* NaN, negative, more than every sample at the IMU's ceiling */
    if (t->screen_s < 0 || t->screen_s > MK_MAX_SECS || t->still_s < 0 || t->still_s > MK_MAX_SECS) return false;
    uint32_t sum = 0;
    for (int i = 0; i < 5; i++) { if (t->posture_n[i] < 0 || (uint32_t)t->posture_n[i] > t->n) return false; sum += (uint32_t)t->posture_n[i]; }
    if (sum > t->n) return false;
    if (t->shaken > 1 || t->up > 1) return false;
    if (t->drinks < 0 || t->drinks > 15 || t->meals < 0 || t->meals > 15) return false;
    if (t->hr < 0 || t->hr > 255 || t->hrv < 0 || t->hrv > 255) return false;
    return true;
}

/* the decision at boot. ``soft`` = the reset was anything the SDK names
 * but a power-on (esp_reset_reason: SW, PANIC, the watchdogs, BROWNOUT,
 * DEEPSLEEP); ``clock_set`` + ``date`` / ``minute`` = the clock as it
 * stands at boot (kept across a soft reset by the RTC, main.cpp).
 * ``why`` (may be NULL) gets a MK_WHY_* word for the log line. */
static inline int mk_decide(const mk_keep_t *k, bool soft, bool clock_set, uint32_t date, int minute, int *why)
{
    int w;
    int v = MK_DISCARD;
    if (!soft) w = MK_WHY_POWERON;
    else if (!clock_set || minute < 0) w = MK_WHY_NO_CLOCK;
    else if (!k || k->magic != MK_MAGIC) w = MK_WHY_MAGIC;
    else if (!mk_check(k)) w = MK_WHY_BOUNDS;
    else if (k->date != date) w = MK_WHY_OTHER_DAY;
    else if (k->minute > minute) w = MK_WHY_AHEAD;
    else { w = MK_WHY_TAKEN; v = k->minute == minute ? MK_SAME : MK_CROSSED; }
    if (why) *why = w;
    return v;
}

/* the stamp, each tick: the minute the tally belongs to */
static inline void mk_stamp(mk_keep_t *k, uint32_t date, int minute)
{
    k->date = date;
    k->minute = minute;
    k->magic = MK_MAGIC;
}

/* nothing kept: after a flush filed the minute (an OTA, `reboot`), or at a discard */
static inline void mk_clear(mk_keep_t *k) { k->magic = 0; }

static inline const char *mk_why_word(int why)
{
    switch (why) {
    case MK_WHY_TAKEN:     return "taken back";
    case MK_WHY_POWERON:   return "a power-on: nothing kept";
    case MK_WHY_NO_CLOCK:  return "no clock to check it against";
    case MK_WHY_MAGIC:     return "nothing was kept";
    case MK_WHY_BOUNDS:    return "garbage (a field past its bound)";
    case MK_WHY_OTHER_DAY: return "from another day (its file is closed)";
    case MK_WHY_AHEAD:     return "ahead of the clock";
    }
    return "?";
}

#ifdef __cplusplus
}
#endif
