/* The day-log: what the wrist remembers, one 16-byte record a minute,
 * design/watch-daylog.md byte-for-byte (wrist.py reads it). Written to
 * LittleFS as /day/YYYYMMDD.bin; the offset IS the minute. Plus the
 * senses that fill it: a peak-detect step counter on |a|, the motion
 * energy, the wear inference, raises, taps, the lit seconds. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "imu.h"

#pragma pack(push, 1)
#define DAYLOG_MAGIC   "SLWD"
#define DAYLOG_VERSION 1

typedef struct {
    char     magic[4];      // "SLWD"
    uint8_t  version;       // 1
    uint8_t  rec_size;      // sizeof(daylog_min_t) == 16
    uint16_t first_min;     // minute-of-day of record 0 (0 unless the log began late)
    uint32_t date;          // YYYYMMDD, the watch's local date
    int16_t  tz_min;        // local offset, minutes east of UTC (informational)
    uint16_t count;         // records written; 0 = read the file length
    uint32_t body;          // the watch's chip id, low 32 bits
    uint8_t  flags;         // bit0 CLOSED: the day is done, the file will not grow
    uint8_t  reserved[11];
} daylog_hdr_t;             // 32 bytes

typedef struct {
    uint16_t steps;         // steps this minute
    uint16_t energy;        // motion energy: milli-g RMS about 1 g, capped
    uint8_t  band;          // 0 still  1 light  2 moderate  3 vigorous
    uint8_t  flags;         // bit0 WORN  bit1 SHAKEN  bit2 CHARGING  bit3 UP (the "I'm up" shake: a sleep span ends here)  bit4 EAR (2026-10-09: the minute napped AWAKE for THE DARK EAR, ear.h; clear = light sleep or the glass lit)  bit5 HELD  bit6 LINKED  bit7 NOTICE (2026-10-12, THE HOUSE IN THE ROW, minuteflags.h: the house on the wrist / a door answered / a wisp flew in, at any second of the minute)
    uint8_t  raises;        // wrist raises (the glance interrupt), capped 255
    uint8_t  taps;          // taps on the glass
    uint8_t  screen_s;      // seconds the screen was lit, 0..60
    uint8_t  posture;       // 0 unknown 1 hanging 2 flat 3 raised 4 lying (the minute's dominant)
    uint16_t batt_mv;       // battery, millivolts
    uint8_t  hr;            // RESERVED for the PPG: bpm at rest (0 = none)
    uint8_t  hrv;           // RESERVED for the PPG: RMSSD ms, or a quality byte
    uint8_t  still_s;       // seconds under the still threshold, 0..60
    uint8_t  spare;         // the MARKS (gestures, 2026-09-09): bits 0-3 drinks (double taps), bits 4-7 meals (the feed: a drag down the glass), each capped 15
} daylog_min_t;             // 16 bytes
#pragma pack(pop)

#define DAYLOG_CLOSED   0x01
#define DAYLOG_WORN     0x01
#define DAYLOG_SHAKEN   0x02
#define DAYLOG_CHARGING 0x04
#define DAYLOG_UP       0x08
#define DAYLOG_EAR      0x10    /* THE EAR'S LEDGER: the minute's nap was the awake ear (drain.h DRAIN_EAR), so the desk can judge its cost over days */
#define DAYLOG_HELD     0x20    /* THE HOUSE IN THE ROW (minuteflags.h MF_HELD): the house was on the wrist at any second of the minute */
#define DAYLOG_LINKED   0x40    /* ... a door answered (MF_LINKED) */
#define DAYLOG_NOTICE   0x80    /* ... a wisp flew in to him (MF_NOTICE) */

/* the day's sums, for `status`, `day show` and the HTTP page */
typedef struct {
    uint32_t date;
    int      minutes;       /* records in the file */
    uint32_t steps;
    int      worn_min, active_min[4], screen_s, raises, taps, shakes, still_min;
    int      ups, drinks, meals;   /* the gestures: "I'm up" minutes, the marks */
    int      last_min;      /* the last minute logged, -1 */
} daylog_sum_t;

void wrist_begin(void);                             /* mounts LittleFS, closes stale days */
/* one IMU sample (awake at the loop rate, asleep at the nap's 4 Hz) */
void wrist_sample(const vec3_t *a, bool awake);
/* the second and minute clocks; call from loop() and the nap loop */
void wrist_tick(bool screen_on);
void wrist_count_raise(void);
void wrist_count_tap(void);
void wrist_count_shake(void);
/* the gestures (gesture.cpp): the "I'm up" shake flags the minute UP; a
 * drink / a meal is a mark in the spare byte */
void wrist_count_up(void);
void wrist_count_mark(bool meal);
/* THE PULSE IN THE DAY (ppg.cpp, 2026-10-08): a clean resting read lands in the minute in progress -- the next
 * closed minute carries it as hr (bpm) and hrv (RMSSD ms, capped 255); 0 = no read that minute */
void wrist_note_pulse(int bpm, int rmssd_ms);
/* THE RED AND THE INFRARED (ppg.cpp, 2026-10-13, T19): the night's lowest UNCALIBRATED SpO2 into the open day's
 * header, reserved[DAYLOG_HDR_SPO2_MIN] (one byte, 0 = none; the header's [0..3] are THE LAST DEATH's), written
 * once at the sleep span's end -- the file of the morning the night ended on */
#define DAYLOG_HDR_SPO2_MIN 4
void wrist_note_spo2_min(int spo2);
/* THE PERFUSION INDEX (ppg.cpp + perfusion.h, 2026-10-14): the day's MEDIAN perfusion index into the day's header at
 * its close, reserved[DAYLOG_HDR_PERF_MED] -- tenths of a percent (capped 254); 0 = no read that day (or an older
 * file); 255 = reads taken, the median under a tenth (nothing on the sensor). close_file takes it from ppg.cpp */
#define DAYLOG_HDR_PERF_MED 6
/* THE BREATH READ (ppg.cpp, 2026-10-14, resp.h): the night's MEDIAN respiration rate, breaths a minute, into the
 * open day's header reserved[DAYLOG_HDR_RESP_NIGHT] (one byte, 0 = none), written once at the sleep span's end
 * beside the spo2 byte -- the file of the morning the night ended on */
#define DAYLOG_HDR_RESP_NIGHT 5
void wrist_note_resp_night(int bpm);
bool wrist_worn(void);                              /* motion in the last 10 min */
/* for rest.cpp (the night and the breath): raises + shakes since boot --
 * a bump is real motion now; and whether steps are coming in right now */
uint32_t wrist_motion_events(void);
bool wrist_walking(void);
uint32_t wrist_step_seq(void);                      /* steps counted since boot: THE STEP BOB hears each one (tell.cpp) */
uint32_t wrist_steps_today(void);                   /* the file's sum + the minute in progress */
bool wrist_sum(uint32_t date, daylog_sum_t *out);   /* reads a file back */
/* THE WATCH FACE's norm: the step totals of the newest ``max`` day files
 * dated before ``today`` (0 = every file), newest first; how many */
int  wrist_recent_totals(uint32_t today, uint32_t *out, int max);
/* THE WATCH PAGE's week: the same walk with each file's date beside its
 * total (newest first), so a day with no file can be told from a day of
 * nothing; read once a day beside the norm, never per frame */
int  wrist_recent_days(uint32_t today, uint32_t *dates, uint32_t *totals, int max);
void wrist_flush(void);                             /* write the minute in progress now (before an OTA) */
const char *wrist_posture_name(int p);
int  wrist_posture_of(const vec3_t *a);              /* 0 unknown 1 hanging 2 flat 3 raised 4 lying */
int  wrist_unlogged_min(void);                      /* minutes lived with no clock to file them under (or no disk, since 2026-10-12) */
bool wrist_disk_refused(void);                      /* THE DISK ASKS BEFORE IT FORMATS (fsguard.h): the mount failed and the format was refused -- doctor leads with it */
/* serial: `day show|ls|dump|close|rm|prune|fake [YYYYMMDD]` */
void wrist_cmd(int argc, char **argv);
/* DISK KEEPING (daykeep.h): drop what the keep says, log each; how many
 * went. Run once at boot after the clock is set and once at the date's
 * turn (wrist_tick does it); `day prune` runs it by hand */
int  day_keep(uint32_t today);
/* THE NIGHT RING KNOWS HIS HEART (nightsum.h): the newest NS_WALK_DAYS
 * day files, oldest first, through a 32-row buffer into nightsum_row --
 * once at boot after the clock and once at the date's turn (main's
 * face_refresh_norm, beside the norm); finish_minute feeds the live
 * minute between walks. Returns the rows fed; logs one `night:` line. */
int  wrist_night_walk(uint32_t today);
/* the sync: closed days not yet acked by the desk, oldest first */
bool wrist_next_unsynced(uint32_t *date_out);
void wrist_mark_synced(uint32_t date);
bool wrist_file_path(uint32_t date, char *out, int cap);
