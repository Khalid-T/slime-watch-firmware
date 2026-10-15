/* VIGOR FROM THE CELL (design/watch-inspiration.md A, Infineat's
 * battery-as-emblem: "when the fuel gauge lands, the cell's level should
 * be the gel's VIGOR; the drop stays a mark for him"; B: the pet that is
 * under the weather because he is -- and here because IT is: its own
 * body running down; never dies, never says so).
 *
 * The desk's law (watch.py `vigor_for`, PowerWatch: "the battery never
 * earns a word -- it sets vigor, and the body's posture does the
 * talking"): the cell's level becomes ONE slow number 0..1 and the body
 * wears it. By the material -- a body running down, wordless:
 *
 *   the resting breath     rate x (0.7 + 0.3 v), depth x (0.8 + 0.2 v)
 *                          (rest.cpp folds them where THE GRAIN folds its own)
 *   idle life              the gap x (1 + (1 - v)): fewer small things
 *   under VIGOR_LOW        droopy lids for the raise's first second (the
 *                          sky's hot-day pattern, not the tell's: a low
 *                          cell is a STATE shown on every raise while it
 *                          lasts, not a fact told once); the still timers
 *                          shorter (main.cpp: DIM 8 s, SLEEP 12 s) -- it
 *                          naps sooner
 *   under VIGOR_EMPTY      spent: the sleep look while lit (SLEEP_SQUASH,
 *                          the sleep bar for eyes), no idle things at all
 *   the cable arriving     one contented settle (a one-shot; held for the
 *                          lit glass), and the vigor rises with the mV
 *
 * The SOURCE: the gauge's percent when the MAX17048 answers (dark until
 * it is soldered), else the ADC's mV mapped VIGOR_MV_EMPTY..FULL ->
 * 0..1; on the charger's float (>= VIGOR_MV_FLOAT: no cell, or a full
 * one) 1.0 -- today's board, a cable and no cell. The level goes
 * through the desk's shape: vigor_for's steps (1.0 above VIGOR_LOW_PCT
 * 25, 0.7 to VIGOR_CRIT_PCT 10, 0.4 under) read as the knees of one
 * continuous curve -- 1.0 at full, 0.7 at 25 %, 0.4 at 10 %, 0 at
 * empty -- so the desk's numbers are hit at the desk's percents and
 * nothing steps. Then a 60-s ease (95 %): a raise never finds a
 * different creature than the last one.
 *
 * Never a number, never a nag: the face's drop (face.cpp) remains the
 * mark for HIM. Each threshold has VIGOR_HYST above it on the way back
 * so nothing flaps at the edge. No Arduino: dt and the readings are
 * parameters, tools/vigor_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* the thresholds, with their cause */
#define VIGOR_LOW          0.35f    /* under this the body is visibly low: the lids, the sooner nap (the desk's 0.4 band, a touch under) */
#define VIGOR_EMPTY        0.12f    /* under this it is spent: the sleep look while lit (the gauge's own alert is 10 %) */
#define VIGOR_HYST         0.05f    /* back over a threshold only this much above it */
#define VIGOR_EASE_S       60.0f    /* a change is done (95 %) in about this: slow, like the day's breath */
/* the ADC's map: a LiPo's 3.3 V empty .. ~4.15 V full (4.2 at the float) */
#define VIGOR_MV_EMPTY     3300
#define VIGOR_MV_FULL      4150
#define VIGOR_MV_FLOAT     4100     /* the charger's float: a full cell or no cell -- the face's own "nothing to say" line */
/* the desk's knees: config.py VIGOR_LOW_PCT / VIGOR_CRIT_PCT and vigor_for's 0.7 / 0.4 */
#define VIGOR_LOW_PCT      25
#define VIGOR_CRIT_PCT     10
#define VIGOR_AT_LOW       0.7f
#define VIGOR_AT_CRIT      0.4f
/* the cable's settle: the idle settle's own shape (idle.cpp SETTLE_*), content eyes a little past it */
#define VIGOR_SETTLE_S     1.5f
#define VIGOR_SETTLE_SQUASH 0.06f
#define VIGOR_SETTLE_EYE_S 2.0f
#define VIGOR_SETTLE_HOLD_S 300.0f  /* a settle owed to a dark glass waits this long for the light, then is dropped */
/* the low cell's lids on a raise, and the spent look's ease */
#define VIGOR_LID_S        1.0f
#define VIGOR_SPENT_EASE_S 2.0f
/* BATTERY MANNERS (2026-09-10 evening): two percents of the CELL's level
 * (the gauge's %, else the ADC's map), read as the watch's own thrift --
 * not the creature's vigor curve above, which is the body's posture. Under
 * VIGOR_THRIFT_PCT the glance holds shorter and the awake backlight drops
 * a step (main.cpp, glance.cpp); under VIGOR_LEAN_PCT the radio's claim
 * comes half as often (radio.cpp) and the page shows the cell in the
 * shade. VIGOR_PCT_HYST back over each on the way up. On the cable the
 * level is 1.0 (100 %): no manners while it drinks. */
#define VIGOR_THRIFT_PCT   20
#define VIGOR_LEAN_PCT     10
#define VIGOR_PCT_HYST     2

typedef struct {
    float squash;       /* the cable's settle, added this frame */
    int   eye_kind;     /* GEL_EYE_DROOPY (the raise's first second, low) / GEL_EYE_CONTENT (the settle); -1 = leave the draw's */
    float spent_k;      /* 0..1, eased: the sleep look's weight while spent (main adds SLEEP_SQUASH x this; the sleep bar over 0.5) */
} vigor_fx_t;

void  vigor_begin(void);
/* one call a frame: the ADC's mV, main's charging() (the float inferred),
 * the gauge's percent or -1 (absent / no answer), the glass lit and the
 * house holding (the settle plays only where it can be seen). Returns
 * the eased vigor. The first sample snaps (no fade-in from a guess). */
float vigor_tick(float dt, uint16_t batt_mv, bool charging, int gauge_pct, bool lit, bool holding);
float vigor(void);                      /* the eased number, 0..1 */
float vigor_target(void);               /* where it is easing to */
const char *vigor_source(void);         /* "the gauge" / "the ADC" / "the cable" / "nothing yet" */
bool  vigor_low(void);                  /* under VIGOR_LOW (with the hysteresis) */
bool  vigor_spent(void);                /* under VIGOR_EMPTY (with the hysteresis) */
/* BATTERY MANNERS: the cell's percent as the source says it (the gauge's,
 * else the ADC's map; 100 on the cable), and the two thrift states */
int   vigor_pct(void);                  /* 0..100; 100 until a sample */
bool  vigor_thrift(void);               /* under VIGOR_THRIFT_PCT: the glance shorter, the backlight a step down */
bool  vigor_lean(void);                 /* under VIGOR_LEAN_PCT: the radio's claim half as often, the cell in the shade on the page */
/* the factors the body folds in */
float vigor_breath_hz_k(void);          /* 0.7 + 0.3 v */
float vigor_breath_amp_k(void);         /* 0.8 + 0.2 v */
float vigor_idle_k(void);               /* 1 + (1 - v) on the idle gap */
void  vigor_raise(void);                /* a raise: the lids for a second when low */
void  vigor_fx(vigor_fx_t *out);        /* what the draw folds in this frame */
bool  vigor_playing(void);              /* the settle in progress (idle counts it as busy) */
int   vigor_settles(void);              /* the cable's settles so far */
void  vigor_status(char *out, int cap); /* `vigor` over serial: the number, its source, what it is doing */

/* the pure pieces, for the test */
float vigor_level_of_mv(uint16_t mv);   /* the ADC's map, 0..1 (1.0 at the float and above) */
float vigor_of_level(float level);      /* the desk's shape: the knees at 25 % / 10 % */
