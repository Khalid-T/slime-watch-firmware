/* MAX3010x PPG (0x57 on the second bus). 2026-09-12: the part on the
 * bench is the MAX30102 (MH-ET LIVE breakout: red + IR, no green), so
 * the spot reads by the IR LED by default; `ppg led green` the day a
 * 30101 lands (store.ppg_led). A DARK DRIVER: compiled in, inert until
 * `bom ppg on`. ONE thing: a SPOT read at rest -- one LED, the FIFO
 * polled from the tick, then a simple peak-interval average (and, since
 * 2026-10-08, the intervals' RMSSD). NOT continuous, NOT
 * motion-compensated: a spot with the arm still or the read says not
 * ok. Raw registers, no library; the sample buffer (7.2 KB) lives on
 * the heap only while a read runs.
 *
 * THE PULSE, THREE WAYS BETTER (2026-10-08, the overhaul wave):
 *   THE AGC PER LED    agc.h's agc2_t: the raw stream's two slots each
 *                      settle their own current (the wrist read of 09-25
 *                      pinned at 0xF8 judging one channel by the other's
 *                      skin). The spot's one LED is agc_step as before.
 *   THE STILL SPOT     still.h: every spot waits for the wrist to be
 *                      still before the LEDs light, and a burst mid-read
 *                      throws the samples away (a restart, or the verdict
 *                      "moved" with the IMU as the witness). main feeds
 *                      the motion: ppg_feed lit, ppg_dark_poll in the nap.
 *   THE PULSE IN THE DAY  a short spot (PPG_AUTO_S) every PPG_AUTO_EVERY_MIN
 *                      at rest, by itself, in both houses; a clean result
 *                      goes to the day log's minute (hr / hrv) and to a
 *                      RAM ring of the last PPG_RECENT_N -- ppg_recent /
 *                      ppg_rest_now / ppg_recent_span are what the breath,
 *                      the night ring and the gentle wake read. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "imu.h"                    /* vec3_t: the nap's poll hands two poses to ppg_dark_poll */

#define PPG_SPOT_S 10               /* the console's spot */
#define PPG_SPOT_MAX_S 30           /* the longest read (the buffer is sized for it: 900 samples, 7.2 KB on the heap while it runs) */
/* THE PULSE ROW (Khalid 2026-10-03 night, the watch solo on his wrist: "add some firmware where I can trigger
 * the heart rate checker for like 30 seconds so I can test it out"): SETTINGS 5's fourth row starts a read
 * this long from the glass -- the row counts it down, then shows the BPM (page.h SET_PULSE; main.cpp) */
#define PPG_PULSE_S 30
/* THE PULSE IN THE DAY (watch-ideas D31's sampling half, 2026-10-08): the creature's own read, short and
 * seldom -- a 10 s spot (the item's own number; six beats at 40 bpm fit it, the honesty check needs six)
 * every five minutes (the item's; 288 a day at most, ~48 min of LED a day if every one ran -- most will not:
 * the gates), the ring four hours deep (48 at 5 min: a night's worth of the slow half, and the breath wants
 * only the newest). The gap is counted from the last START, clean or not: a moving hour costs one try */
#define PPG_AUTO_S          10
#define PPG_AUTO_EVERY_MIN  5
#define PPG_RECENT_N        48
#define PPG_REST_FRESH_S    1800u   /* ppg_rest_now: a resting pulse older than half an hour is nobody's "now" (six gaps missed = he was moving, or the watch was off him) */

enum { PPG_LED_GREEN = 0, PPG_LED_IR = 1, PPG_LED_RED = 2 };
const char *ppg_led_name(int led);

void ppg_begin(void);               /* probe PART_ID 0x15, then shut down (LEDs off) */
void ppg_end(void);
void ppg_tick(void);                /* drains the FIFO while a spot runs; finishes it */
bool ppg_present(void);
bool ppg_spot_start(void);          /* begin a PPG_SPOT_S spot read; false = absent, the raw stream, or his own read already running (an auto read gives way) */
bool ppg_spot_start_s(int secs);    /* ...of a given length, 5..PPG_SPOT_MAX_S (THE PULSE ROW's 30) */
/* while HIS spot runs (the row's, the console's; not the raw stream, not the creature's own): the seconds it has
 * left (the whole length while the wrist is waited for or the AGC is still choosing the current -- *settling);
 * false = none of his running */
bool ppg_spot_left(int *left_s, bool *settling);
bool ppg_spot_waiting(void);        /* THE STILL SPOT: his spot waits for the wrist to be still, the LEDs dark (the row shows STILL) */
/* THE EAR TO THE WRIST (listen.h, watch-night C19): while the CREATURE'S OWN read is taken (waiting for the wrist, the
 * agc choosing, or reading; never his asked ones, never the raw stream): the seconds it has left (the whole length until
 * the read proper runs) and whether its LEDs glow now (dark while it waits for the wrist); false = no read of its own */
bool ppg_own_left(int *left_s, bool *glow);
uint32_t ppg_spot_done(void);       /* how many of HIS spots have FINISHED since boot (a change = a new result to read) */
const char *ppg_spot_why(void);     /* the last spot's verdict in words ("11 beats, cv 0.07, 24.9 Hz" / "nothing on the sensor (dc 312)" / "moved 12 s in (0.21 g), no time left") */
bool ppg_spot_busy(void);           /* the sensor is taken: any read, his or the creature's, or the raw stream */
bool ppg_raw_start(int secs);       /* the raw stream: red+ir at 100 Hz as `raw <i> <red> <ir>` serial lines for secs (5..120); each LED at its own AGC current */
/* the ONE call: the last of HIS spots' result. Returns false when none
 * has finished yet; *ok is false when it finished without a clean pulse
 * (nothing on the sensor, too few beats, an irregular interval, moved). */
bool ppg_spot(float *bpm, bool *ok);
float ppg_spot_rmssd(void);         /* the last clean spot's RMSSD, ms (the successive intervals' spread; 0 = none) */
/* THE PERFUSION INDEX (perfusion.h, 2026-10-14): the read's own quality -- AC/DC of the ir slot in percent, banded
 * NONE / THIN / FAIR / GOOD. His last spot failed for a THIN perfusion (the PULSE row says THIN where it said NONE);
 * the last measured read's index (-1 = none measured); the day's median as the header byte (daylog.h reserved[6]),
 * taken ONCE at the day's close by daylog.cpp, which clears the day's tally. Never a number on the glass */
bool ppg_spot_thin(void);
float ppg_perfusion_last(void);
uint8_t ppg_perfusion_day_take(int *n_out);
/* THE WARM SKIN (warm.c, watch-night C20): the MAX30102's die temperature, read ONCE at the end of every spot
 * (his or the creature's own: one conversion before the LEDs' shutdown, ~30 ms, no buffer). NAN until the first;
 * the seq counts readings so main feeds each to warm.c once. Never a number on the glass (`ppg` says it on serial) */
float ppg_die_temp_c(void);
uint32_t ppg_die_temp_seq(void);
void ppg_status(char *out, int cap);

/* --- THE STILL SPOT: the wrist's motion, from main ---------------------------- */
/* lit: once a frame with sense()'s jerk (g) and the frame's dt; this is also where THE PULSE IN THE DAY
 * decides to start a read of its own (the gates are read here: the cable, the shelf, the gap, the sensor) */
void ppg_feed(float jerk_g, float dt, bool lit);
/* dark: once per poll of the nap (after wake_nap_once said nothing) with the pose before and after -- the
 * jerk between two polls is the nap's stillness, as raise.cpp reads it; dt is measured here */
void ppg_dark_poll(const vec3_t *prev_pose, const vec3_t *pose);

/* --- THE PULSE IN THE DAY: what the others read ---------------------------------
 * The ring holds the last PPG_RECENT_N CLEAN results of the creature's own reads (never his asked spots:
 * a fingertip on the desk is not his resting pulse), newest first. Each has the minute of the local day
 * it landed in (-1 with no clock) and its age in seconds now. Nothing here lights the glass or says a
 * number to him: the breath (the resting breath follows the resting pulse / 4), the night ring, the
 * gentle wake read these and show nothing but themselves. */
uint32_t ppg_recent_seq(void);                                              /* +1 per result pushed into the ring: a consumer takes each once (the breath, the gentle wake) */
int  ppg_recent_count(void);                                                 /* 0..PPG_RECENT_N */
bool ppg_recent(int i, int *bpm, int *rmssd_ms, int *minute, uint32_t *age_s);   /* i = 0 the newest; false past the count (any out may be NULL) */
/* "the resting pulse now": the newest clean result if it is younger than PPG_REST_FRESH_S; false = none fresh */
bool ppg_rest_now(int *bpm, uint32_t *age_s);
/* "the pulse over the night": the results younger than within_s -- how many, the lowest, the highest, the
 * mean (bpm); false = none in the span (the outs untouched). within_s 0 = the whole ring */
bool ppg_recent_span(uint32_t within_s, int *n, int *lo, int *hi, int *mean);
/* THE BREATH READ (resp.h, 2026-10-14): the last clean respiration rate off a read on the wrist (never the raw
 * stream's), breaths a minute, if younger than PPG_REST_FRESH_S; 0 = none fresh. Coarse (a 10 s read holds
 * 2..4 breaths); its confidence 0..1 and age beside it. Carried to rest_in_t.breath_bpm, NOT followed by the
 * gel's breath tonight (ruling 154); never the glass, never a digit */
float ppg_breath_now(float *conf, uint32_t *age_s);
bool ppg_auto_on(void);                                                      /* the switch (store.ppg_auto_off): `ppg auto on|off` */
