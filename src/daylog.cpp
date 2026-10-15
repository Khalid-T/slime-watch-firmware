#include "daylog.h"
#include "app.h"
#include "store.h"
#include "link.h"
#include "rest.h"                 /* the night and the breath read the closed minutes */
#include "ear.h"                  /* THE EAR'S LEDGER: the minute's nap kind (the EAR flag) and the cell handed to the drain */
#include "drain.h"
#include "tell.h"                 /* THE NIGHT AFTER A REBOOT: which night the glance told */
#include "daykeep.h"              /* DISK KEEPING: which old files go (the decision, pure) */
#include "nightsum.h"             /* THE NIGHT RING KNOWS HIS HEART: the week's nights and resting heart off the rows (pure) */
#include "minutekeep.h"           /* THE MINUTE KEPT ACROSS A RESET: the tally in RTC memory, the decision at boot (pure) */
#include "fsguard.h"              /* THE DISK ASKS BEFORE IT FORMATS: mount / format once / refuse / the hand, the table (pure) */
#include "minuteflags.h"          /* THE HOUSE IN THE ROW: the row's bits 5-7 (HELD / LINKED / NOTICE) from the house, the door and the wisps over the minute (pure) */
#include "house.h"                /* ... house_holding */
#include "wisp.h"                 /* ... wisp_sent */
#include <Arduino.h>
#include <stdlib.h>
#include <LittleFS.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "lastdeath.h"          /* THE LAST DEATH: the header's reserved bytes (boots / crashes today, the build's crc), stamped at open_day */
#include "ppg.h"                /* THE PERFUSION INDEX: the day's median perfusion into the header's reserved[6] at close_file */

/* LittleFS.exists() opens the file and the core logs an error when it
 * is not there; stat() through the VFS is quiet */
static bool fs_exists(const char *path)
{
    char full[48];
    snprintf(full, sizeof full, "/littlefs%s", path);
    struct stat st;
    return stat(full, &st) == 0;
}

/* --- the senses' tunables --------------------------------------------------- */
static const float STEP_TH_G     = 0.10f;   /* a peak of the high-passed |a| this high is a step */
static const uint32_t STEP_REFRACT_MS = 250; /* no two steps closer than this (4 Hz cadence cap) */
static const float STILL_G       = 0.02f;   /* a second whose |hp| never exceeds this is still */
static const float MOVE_G        = 0.05f;   /* motion that counts as "worn" (main's MOTION_G) */
static const uint32_t WORN_MS    = 10 * 60 * 1000;
static const float SHAKE_G       = 0.60f;   /* a jerk this big is a shake (main's WAKE_JERK_G is 0.18: a nudge) */
static const int   WALK_STEPS_10S = 8;      /* steps in 10 s that make "on the move" */
/* the bands: milli-g RMS over the minute (config.py WRIST_STILL_MG is 15) */
static const int BAND_LIGHT_MG = 15, BAND_MOD_MG = 60, BAND_VIG_MG = 180;

/* --- the minute in progress ------------------------------------------------- */
/* THE MINUTE KEPT ACROSS A RESET (minutekeep.h, 2026-10-12): the tally
 * (steps, raises, taps, shakes, the energy's sum of squares and its n,
 * the lit and still seconds, the posture tally, the marks, hr / hrv)
 * lives in RTC slow memory, which a soft reset does not clear, stamped
 * each tick with its date and minute; wrist_begin decides what a boot
 * finds (taken back / filed as its own row / discarded as garbage) */
static RTC_NOINIT_ATTR mk_keep_t keep;
static mk_tally_t &cur = keep.t;
static int  take_min = -1;                  /* the kept minute to file (SAME or CROSSED), -1 = none; consumed when today's file opens */
static bool fs_refused = false;             /* THE DISK ASKS BEFORE IT FORMATS: the mount failed and the format was refused */
static mf_t mf = { 0, 0 };                  /* THE HOUSE IN THE ROW: the minute's HELD / LINKED / NOTICE so far (.bss, 8 B; a reset loses the minute's bits, never a byte of the row) */

static float lp_mag = 1.0f, hp_s = 0;
static bool  armed = true;
static uint32_t last_step_ms = 0, last_move_ms = 0, last_sample_us = 0;
static float sec_max_hp = 0;
static uint32_t sec_start_ms = 0;
static bool  sec_screen = false;
static int   last_minute = -1;
static uint32_t last_unset_ms = 0;
static int   unlogged_min = 0;
static bool  fs_ok = false;
static uint32_t kept_date = 0;              /* DISK KEEPING: the date the keeper last ran for */
/* the walk: steps stamped in a 10 s ring */
static uint32_t step_stamps[32];
static int step_head = 0;
static bool walking = false;
static uint32_t motion_events = 0;          /* raises + shakes, for rest.cpp */
static uint32_t step_seq = 0;               /* every step counted since boot, for the bob (tell.cpp) */

/* --- the open day ----------------------------------------------------------- */
static struct {
    uint32_t date;
    uint16_t first_min;
    int      recs;
    uint32_t steps;          /* the file's sum */
    bool     open;
} day;

static uint32_t chip_body(void) { return (uint32_t)(ESP.getEfuseMac() & 0xffffffffu); }

bool wrist_file_path(uint32_t date, char *out, int cap)
{
    snprintf(out, cap, "/day/%08lu.bin", (unsigned long)date);
    return true;
}
static void ok_path(uint32_t date, char *out, int cap)
{
    snprintf(out, cap, "/day/%08lu.ok", (unsigned long)date);
}

static bool read_hdr(const char *path, daylog_hdr_t *h, size_t *size)
{
    if (!fs_exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    *size = f.size();
    bool ok = f.read((uint8_t *)h, sizeof *h) == sizeof *h && !memcmp(h->magic, DAYLOG_MAGIC, 4);
    f.close();
    return ok;
}

static void close_file(uint32_t date)
{
    char p[32];
    wrist_file_path(date, p, sizeof p);
    daylog_hdr_t h; size_t sz;
    if (!read_hdr(p, &h, &sz)) return;
    if (h.flags & DAYLOG_CLOSED) return;
    h.count = (uint16_t)((sz - sizeof h) / sizeof(daylog_min_t));
    h.flags |= DAYLOG_CLOSED;
    if (day.open && day.date == date) h.reserved[DAYLOG_HDR_PERF_MED] = ppg_perfusion_day_take(NULL);   /* THE PERFUSION INDEX: the day's median, once, as the day closes (a stale file's byte is left as it was) */
    File f = LittleFS.open(p, "r+");
    if (!f) return;
    f.seek(0);
    f.write((const uint8_t *)&h, sizeof h);
    f.close();
    LOG("day: %08lu closed, %u records", (unsigned long)date, h.count);
    link_sync_now();                    /* a closed day is one to bring home */
}

/* every open file for a date before ``today`` is a day that ended
 * while the watch was off or clockless: close it so it can be synced */
static void close_stale(uint32_t today)
{
    File dir = LittleFS.open("/day");
    if (!dir) return;
    File f;
    while ((f = dir.openNextFile())) {
        const char *n = f.name();
        uint32_t d = (uint32_t)strtoul(n, NULL, 10);
        bool bin = strstr(n, ".bin") != NULL;
        f.close();
        if (bin && d && d < today) close_file(d);
    }
    dir.close();
}

static bool open_day(uint32_t date, int minute)
{
    char p[32];
    wrist_file_path(date, p, sizeof p);
    daylog_hdr_t h; size_t sz;
    day.open = false;
    day.date = date;
    day.steps = 0;
    if (read_hdr(p, &h, &sz)) {
        if (h.flags & DAYLOG_CLOSED) {
            LOG("day: %08lu is already closed; today's minutes are not logged", (unsigned long)date);
            return false;
        }
        day.first_min = h.first_min;
        day.recs = (int)((sz - sizeof h) / sizeof(daylog_min_t));
        File f = LittleFS.open(p, "r");
        if (f) {
            f.seek(sizeof h);
            daylog_min_t m;
            for (int i = 0; i < day.recs; i++) {
                if (f.read((uint8_t *)&m, sizeof m) != sizeof m) break;
                day.steps += m.steps;
            }
            f.close();
        }
        day.open = true;
        death_hdr_stamp(h.reserved);                                   /* THE LAST DEATH: this boot (once) and its build in the header's reserved bytes */
        { File w = LittleFS.open(p, "r+"); if (w) { w.seek(0); w.write((const uint8_t *)&h, sizeof h); w.close(); } }
        LOG("day: %08lu reopened at minute %d (%d records, %lu steps so far)",
            (unsigned long)date, minute, day.recs, (unsigned long)day.steps);
        return true;
    }
    memset(&h, 0, sizeof h);
    memcpy(h.magic, DAYLOG_MAGIC, 4);
    h.version = DAYLOG_VERSION;
    h.rec_size = sizeof(daylog_min_t);
    h.first_min = (uint16_t)minute;
    h.date = date;
    h.tz_min = (int16_t)store.tz_min;
    h.body = chip_body();
    death_hdr_stamp(h.reserved);                                       /* THE LAST DEATH: a file begun at boot counts the boot; one begun at the day's turn counts nothing */
    File f = LittleFS.open(p, "w");
    if (!f) { LOG("day: cannot create %s", p); return false; }
    f.write((const uint8_t *)&h, sizeof h);
    f.close();
    day.first_min = (uint16_t)minute;
    day.recs = 0;
    day.open = true;
    LOG("day: %08lu begun at minute %d", (unsigned long)date, minute);
    return true;
}

static void append_min(int minute, const daylog_min_t *rec)
{
    if (!day.open) return;
    int idx = minute - day.first_min;
    if (idx < day.recs) {
        LOG("day: minute %d is behind the file (%d records): skipped", minute, day.recs);
        return;
    }
    char p[32];
    wrist_file_path(day.date, p, sizeof p);
    File f = LittleFS.open(p, "a");
    if (!f) { LOG("day: cannot append %s", p); return; }
    daylog_min_t zero;
    memset(&zero, 0, sizeof zero);
    while (day.recs < idx) { f.write((const uint8_t *)&zero, sizeof zero); day.recs++; }
    f.write((const uint8_t *)rec, sizeof *rec);
    f.close();
    day.recs++;
    day.steps += rec->steps;
}

/* --- the senses ------------------------------------------------------------- */

int wrist_posture_of(const vec3_t *a)
{
    float inpl = sqrtf(a->x * a->x + a->y * a->y);
    float mag = sqrtf(inpl * inpl + a->z * a->z);
    if (mag < 0.3f) return 0;
    float r = inpl / mag;
    /* the chip sits on the back of the board, +z toward the back: a
     * face-up watch reads z < 0 (main's FACE_UP_Z; `wake-cal` shows it) */
    if (r < 0.35f) return a->z < 0 ? 2 : 4;       /* flat: screen up / lying: screen down */
    if (r < 0.75f) return 3;                         /* raised: tilted to the face */
    return 1;                                        /* hanging at the side */
}

const char *wrist_posture_name(int p)
{
    static const char *N[5] = { "unknown", "hanging", "flat", "raised", "lying" };
    return N[p >= 0 && p < 5 ? p : 0];
}

void wrist_sample(const vec3_t *a, bool awake)
{
    uint32_t now_us = micros(), now = millis();
    float dt = last_sample_us ? (now_us - last_sample_us) * 1e-6f : 0.02f;
    last_sample_us = now_us;
    if (dt > 0.5f) dt = 0.5f;
    float mag = sqrtf(a->x * a->x + a->y * a->y + a->z * a->z);
    /* gravity is the slow part (~1 s); what is left is motion about 1 g */
    lp_mag += (mag - lp_mag) * fminf(dt / 1.0f, 1.0f);
    float hp = mag - lp_mag;
    /* a touch of smoothing for the peak detector */
    hp_s += (hp - hp_s) * fminf(dt / 0.05f, 1.0f);
    float ahp = fabsf(hp);
    cur.sumsq += (double)hp * hp;
    cur.n++;
    if (ahp > sec_max_hp) sec_max_hp = ahp;
    if (ahp > MOVE_G) last_move_ms = now;
    if (ahp > SHAKE_G && !cur.shaken) { cur.shaken = true; cur.shakes++; }
    /* the steps: a peak above STEP_TH_G, re-armed below a third of it,
     * never two within STEP_REFRACT_MS. Awake only: the nap samples at
     * 4 Hz, too slow to see a stride (a walk wakes the watch anyway). */
    if (awake) {
        if (!armed && hp_s < STEP_TH_G * 0.3f) armed = true;
        if (armed && hp_s > STEP_TH_G && now - last_step_ms >= STEP_REFRACT_MS) {
            armed = false;
            last_step_ms = now;
            cur.steps++;
            step_seq++;
            step_stamps[step_head] = now;
            step_head = (step_head + 1) % 32;
        }
    }
    int p = wrist_posture_of(a);
    cur.posture_n[p]++;
}

static int steps_last_10s(uint32_t now)
{
    int n = 0;
    for (int i = 0; i < 32; i++)
        if (step_stamps[i] && now - step_stamps[i] <= 10000) n++;
    return n;
}

bool wrist_worn(void) { return last_move_ms && millis() - last_move_ms < WORN_MS; }
uint32_t wrist_steps_today(void) { return day.steps + cur.steps; }
int wrist_unlogged_min(void) { return unlogged_min; }
bool wrist_disk_refused(void) { return fs_refused; }
void wrist_count_raise(void) { if (cur.raises < 255) cur.raises++; motion_events++; }
void wrist_count_tap(void) { if (cur.taps < 255) cur.taps++; }
void wrist_count_shake(void) { if (!cur.shaken) { cur.shaken = true; cur.shakes++; } motion_events++; }
uint32_t wrist_motion_events(void) { return motion_events; }
bool wrist_walking(void) { return walking; }
uint32_t wrist_step_seq(void) { return step_seq; }
void wrist_count_up(void) { cur.up = true; }
void wrist_count_mark(bool meal) { if (meal) { if (cur.meals < 15) cur.meals++; } else if (cur.drinks < 15) cur.drinks++; }
void wrist_note_pulse(int bpm, int rmssd_ms) { cur.hr = bpm < 0 ? 0 : bpm > 255 ? 255 : bpm; cur.hrv = rmssd_ms < 0 ? 0 : rmssd_ms > 255 ? 255 : rmssd_ms; }

/* THE RED AND THE INFRARED (ppg.cpp): the night's lowest into the open day's header byte, the header read back
 * and written whole (as open_day's stamp does), once a night; no open day = nowhere to put it, silently */
static bool hdr_set_byte(int idx, uint8_t v)
{
    if (!day.open) return false;
    char p[32]; daylog_hdr_t h; size_t sz;
    wrist_file_path(day.date, p, sizeof p);
    if (!read_hdr(p, &h, &sz)) return false;
    h.reserved[idx] = v;
    File w = LittleFS.open(p, "r+");
    if (!w) return false;
    w.seek(0); w.write((const uint8_t *)&h, sizeof h); w.close();
    return true;
}

void wrist_note_spo2_min(int spo2)
{
    if (hdr_set_byte(DAYLOG_HDR_SPO2_MIN, (uint8_t)(spo2 < 0 ? 0 : spo2 > 100 ? 100 : spo2)))
        LOG("day: %08lu header: the night's lowest spo2 ~%d (uncalibrated)", (unsigned long)day.date, spo2);
}

/* THE BREATH READ (ppg.cpp): the night's median breath into the header's next byte, the same way, once a night */
void wrist_note_resp_night(int bpm)
{
    if (hdr_set_byte(DAYLOG_HDR_RESP_NIGHT, (uint8_t)(bpm < 0 ? 0 : bpm > 255 ? 255 : bpm)))
        LOG("day: %08lu header: the night's median breath ~%d /min", (unsigned long)day.date, bpm);
}

static void finish_minute(int minute)
{
    daylog_min_t r;
    memset(&r, 0, sizeof r);
    r.steps = (uint16_t)(cur.steps > 65535 ? 65535 : cur.steps);
    double rms = cur.n ? sqrt(cur.sumsq / cur.n) : 0.0;
    uint32_t mg = (uint32_t)(rms * 1000.0 + 0.5);
    r.energy = (uint16_t)(mg > 65535 ? 65535 : mg);
    r.band = mg < BAND_LIGHT_MG ? 0 : mg < BAND_MOD_MG ? 1 : mg < BAND_VIG_MG ? 2 : 3;
    if (wrist_worn()) r.flags |= DAYLOG_WORN;
    if (cur.shaken) r.flags |= DAYLOG_SHAKEN;
    if (charging()) r.flags |= DAYLOG_CHARGING;
    if (cur.up) r.flags |= DAYLOG_UP;
    r.raises = (uint8_t)(cur.raises > 255 ? 255 : cur.raises);
    r.taps = (uint8_t)(cur.taps > 255 ? 255 : cur.taps);
    r.screen_s = (uint8_t)(cur.screen_s > 60 ? 60 : cur.screen_s);
    int best = 0;
    for (int i = 1; i < 5; i++) if (cur.posture_n[i] > cur.posture_n[best]) best = i;
    r.posture = (uint8_t)best;
    r.batt_mv = batt_mv();
    if (ear_minute(day.date, r.batt_mv, (r.flags & DAYLOG_CHARGING) != 0) == DRAIN_EAR) r.flags |= DAYLOG_EAR;   /* THE EAR'S LEDGER: the minute's kind to the drain, the awake ear on the row */
    r.still_s = (uint8_t)(cur.still_s > 60 ? 60 : cur.still_s);
    r.spare = (uint8_t)((cur.drinks & 15) | ((cur.meals & 15) << 4));   /* the marks */
    r.hr = (uint8_t)cur.hr; r.hrv = (uint8_t)cur.hrv;                   /* THE PULSE IN THE DAY: the reserved bytes, filled since 2026-10-08 */
    r.flags |= mf_take(&mf, wisp_sent());                               /* THE HOUSE IN THE ROW: bits 5-7, HELD / LINKED / NOTICE over the minute (sampled a second at a time in wrist_tick) */
    rest_set_date(day.date);                                  /* the row's day: the night's stable id */
    rest_minute(minute, r.steps, r.energy, r.still_s,         /* the night and the breath; THE SHELF RULE reads the flags */
                (r.flags & DAYLOG_WORN) != 0, (r.flags & DAYLOG_CHARGING) != 0);
    nightsum_row(day.date, minute, r.steps, r.energy, r.flags, r.raises, r.taps, r.hr, rest_night_at(minute));   /* THE NIGHT'S SUM: the live minute (the walk fed the files) */
    append_min(minute, &r);
    LOG("day: minute %d: steps %u energy %u mg band %u flags %02x raises %u taps %u screen %u s still %u s %s batt %u mV%s%u/%u",
        minute, r.steps, r.energy, r.band, r.flags, r.raises, r.taps, r.screen_s, r.still_s,
        wrist_posture_name(r.posture), r.batt_mv, r.hr ? " pulse " : "", r.hr, r.hrv);
    memset(&cur, 0, sizeof cur);
}

/* --- THE NIGHT AFTER A REBOOT --------------------------------------------------
 * rest.cpp forgets across power; the files do not. Once the clock is
 * set (NTP, the desk's hello, a soft reset's RTC) and before today's
 * file is reopened, yesterday's and today's rows go through rest.cpp's
 * own detector (rest_replay_*), REPLAY_SLICE rows a tick so no frame
 * stalls (two full days = 2880 rows = 15 ticks). Rows with no battery
 * reading are the zero padding append_min writes for minutes the watch
 * was off: a gap, skipped -- the live path never saw them either. Then
 * the told-id: the night the glance had told before the reboot
 * (store.told_night = rest_night_id(), stamped when tell.cpp tells one)
 * is marked told again, so the same night is not answered twice. */
static const int REPLAY_SLICE = 200;
static struct {
    int      state;             /* 0 not yet, 1 running, 2 done, 3 refused (the clockless minutes were fresher) */
    uint32_t date[2];
    int      di, row, rows[2];
} rp;

static void replay_slice(uint32_t today)
{
    if (rp.state == 0) {
        rp.date[0] = rest_date_prev(today); rp.date[1] = today;
        rp.di = 0; rp.row = 0; rp.rows[0] = rp.rows[1] = 0;
        if (!rest_replay_begin()) { rp.state = 3; return; }
        rp.state = 1;
        LOG("rest: replaying %08lu and %08lu from the files (%d rows a tick)", (unsigned long)rp.date[0], (unsigned long)rp.date[1], REPLAY_SLICE);
    }
    int budget = REPLAY_SLICE;
    while (budget > 0 && rp.di < 2) {
        char p[32]; wrist_file_path(rp.date[rp.di], p, sizeof p);
        daylog_hdr_t h; size_t sz;
        if (!read_hdr(p, &h, &sz)) { rp.di++; rp.row = 0; continue; }
        int n = (int)((sz - sizeof h) / sizeof(daylog_min_t));
        if (rp.row >= n) { rp.di++; rp.row = 0; continue; }
        File f = LittleFS.open(p, "r");
        if (!f) { rp.di++; rp.row = 0; continue; }
        f.seek(sizeof h + (size_t)rp.row * sizeof(daylog_min_t));
        daylog_min_t m;
        while (budget > 0 && rp.row < n && f.read((uint8_t *)&m, sizeof m) == sizeof m) {
            int minute = h.first_min + rp.row;
            rp.row++; budget--;
            if (!m.batt_mv) continue;                        /* the zero padding: the watch was off */
            rest_replay_row(rp.date[rp.di], minute, m.steps, m.energy, m.still_s, m.raises,
                            (m.flags & DAYLOG_SHAKEN) != 0, m.taps,
                            (m.flags & DAYLOG_WORN) != 0, (m.flags & DAYLOG_CHARGING) != 0,
                            (m.flags & DAYLOG_UP) != 0);      /* THE SHAKEN SLEEPER: the "I'm up" shake is the rising */
            rp.rows[rp.di]++;
        }
        f.close();
    }
    if (rp.di < 2) return;
    rp.state = 2;
    rest_replay_end(rp.date[0], rp.rows[0], rp.date[1], rp.rows[1]);
    rest_span_t sp;
    uint32_t id = rest_night_id();
    if (id && rest_last_span(&sp) && sp.night) {
        if (store.told_night == id) { tell_told_night(sp.end_row); LOG("rest: that night (id %lu) was told before the reboot: not again", (unsigned long)id); }
        else LOG("rest: the night rebuilt (id %lu) is untold: the glance may answer it", (unsigned long)id);
    }
}

/* once a second: the night the glance just told, stamped into NVS */
static void told_poll(void)
{
    uint32_t row = tell_night_told_row();
    if (!row) return;
    rest_span_t sp;
    if (!rest_last_span(&sp) || !sp.night || sp.end_row != row) return;
    uint32_t id = rest_night_id();
    if (!id || store.told_night == id) return;
    store.told_night = id;
    store_save_told_night();
    LOG("rest: the night told (id %lu) -- stored, so a reboot does not tell it twice", (unsigned long)id);
}

void wrist_flush(void)
{
    if (day.open && last_minute >= 0) { finish_minute(last_minute); mk_clear(&keep); }   /* filed: nothing to keep across the reset that follows (an OTA, `reboot`) */
}

void wrist_tick(bool screen_on)
{
    uint32_t now = millis();
    if (screen_on) sec_screen = true;
    /* the second -- every second that passed since the last call: the
     * nap wakes once a minute (or on the IMU's motion engine), and each
     * second slept through was still by the engine's own word (nothing
     * over its threshold), the glass dark */
    int secs = 0;
    while (now - sec_start_ms >= 1000 && secs < 120) {
        sec_start_ms += 1000;
        secs++;
        if (sec_max_hp < STILL_G) cur.still_s++;
        if (sec_screen) cur.screen_s++;
        sec_max_hp = 0;
        sec_screen = screen_on;
    }
    if (now - sec_start_ms >= 1000) sec_start_ms = now;    /* longer than two minutes: the rest is lost */
    if (secs) {
        told_poll();
        mf_sample(&mf, house_holding(), link_linked(), wisp_sent());   /* THE HOUSE IN THE ROW: the three states once a second (the nap's once-a-minute call is one sample) */
        bool w = steps_last_10s(now) >= WALK_STEPS_10S;
        if (w != walking) {
            walking = w;
            link_sense(w ? "wrist:moving" : "wrist:still", w ? "on the move" : "stopped");
        }
    }
    /* the minute */
    if (!clock_is_set() || !fs_ok) {          /* no clock -- or no disk (THE DISK ASKS BEFORE IT FORMATS): the minutes count as unlogged, the same path */
        if (now - last_unset_ms >= 60000) {
            last_unset_ms = now;
            unlogged_min++;
            /* no clock, no file -- but the breath and the night still read the minute */
            double rms = cur.n ? sqrt(cur.sumsq / cur.n) : 0.0;
            uint32_t mg = (uint32_t)(rms * 1000.0 + 0.5);
            rest_minute(-1, (uint16_t)(cur.steps > 65535 ? 65535 : cur.steps), (uint16_t)(mg > 65535 ? 65535 : mg),
                        (uint8_t)(cur.still_s > 60 ? 60 : cur.still_s), wrist_worn(), charging());
            ear_minute(0, batt_mv(), charging());          /* THE EAR'S LEDGER reads the clockless minute too (no day to tally under) */
            mf_begin(&mf, wisp_sent());                    /* THE HOUSE IN THE ROW: a clockless minute's bits go with it (no row to land in) */
            memset(&cur, 0, sizeof cur);
            take_min = -1;                                 /* a kept minute with nowhere to file goes with the rest */
        }
        return;
    }
    int minute = clock_local_minute();
    uint32_t date = clock_local_date();
    if (!fs_ok) return;
    /* THE NIGHT AFTER A REBOOT: the files first, a slice a tick, before today's file reopens */
    if (rp.state < 2) { replay_slice(date); if (rp.state == 1) return; }
    if (!day.open || day.date != date) {
        if (day.open && day.date != date) close_file(day.date);
        close_stale(date);
        open_day(date, minute);
        /* THE MINUTE KEPT ACROSS A RESET: the kept minute is the one in progress again; the next tick files it under ITS minute (a crossed one pads the gap as ever) */
        if (take_min >= 0 && day.open) { last_minute = take_min; LOG("day: the kept minute %d is in progress again (%lu steps, %lu samples)", take_min, (unsigned long)cur.steps, (unsigned long)cur.n); }
        else { last_minute = minute; memset(&cur, 0, sizeof cur); }
        take_min = -1;
        mk_stamp(&keep, day.date, last_minute);
        /* DISK KEEPING: once at boot (the clock just set) and once at the
         * date's turn -- not every tick of a day that refused to open */
        if (kept_date != date) { kept_date = date; day_keep(date); }
        return;
    }
    if (last_minute < 0) { last_minute = minute; return; }
    if (minute != last_minute) {
        if (minute > last_minute) finish_minute(last_minute);
        else LOG("day: the clock stepped back (%d -> %d); the minute is dropped", last_minute, minute);
        last_minute = minute;
    }
    mk_stamp(&keep, day.date, last_minute);   /* THE MINUTE KEPT ACROSS A RESET: the tally's stamp, each tick (three stores) */
}

/* --- reading back ----------------------------------------------------------- */

bool wrist_sum(uint32_t date, daylog_sum_t *out)
{
    char p[32];
    wrist_file_path(date, p, sizeof p);
    daylog_hdr_t h; size_t sz;
    if (!read_hdr(p, &h, &sz)) return false;
    memset(out, 0, sizeof *out);
    out->date = date;
    out->last_min = -1;
    File f = LittleFS.open(p, "r");
    if (!f) return false;
    f.seek(sizeof h);
    daylog_min_t m;
    int i = 0;
    while (f.read((uint8_t *)&m, sizeof m) == sizeof m) {
        out->minutes++;
        out->steps += m.steps;
        if (m.flags & DAYLOG_WORN) out->worn_min++;
        if (m.flags & DAYLOG_WORN) out->active_min[m.band & 3]++;
        if (m.flags & DAYLOG_SHAKEN) out->shakes++;
        if (m.flags & DAYLOG_UP) out->ups++;
        out->drinks += m.spare & 15;
        out->meals += (m.spare >> 4) & 15;
        out->screen_s += m.screen_s;
        out->raises += m.raises;
        out->taps += m.taps;
        if (m.energy < BAND_LIGHT_MG && m.steps == 0) out->still_min++;
        out->last_min = h.first_min + i;
        i++;
    }
    f.close();
    return true;
}

/* THE NIGHT RING KNOWS HIS HEART (nightsum.h; daylog.h says when): the
 * week's files oldest first, 32 rows a read (512 B on the stack, never
 * a file in RAM); rows with no battery reading are append_min's zero
 * padding for minutes the watch was off -- skipped, as the replay
 * skips them. The quiet hours per row are rest's (rest_night_at). */
int wrist_night_walk(uint32_t today)
{
    nightsum_begin(today);
    if (!fs_ok || !today) return 0;
    uint32_t dates[NS_WALK_DAYS];
    dates[NS_WALK_DAYS - 1] = today;
    for (int i = NS_WALK_DAYS - 2; i >= 0; i--) dates[i] = rest_date_prev(dates[i + 1]);
    int fed = 0, files = 0;
    for (int i = 0; i < NS_WALK_DAYS; i++) {
        char p[32]; wrist_file_path(dates[i], p, sizeof p);
        daylog_hdr_t h; size_t sz;
        if (!read_hdr(p, &h, &sz)) continue;
        File f = LittleFS.open(p, "r");
        if (!f) continue;
        files++;
        f.seek(sizeof h);
        daylog_min_t buf[32];
        int row = 0;
        size_t got;
        while ((got = f.read((uint8_t *)buf, sizeof buf)) >= sizeof buf[0]) {
            int n = (int)(got / sizeof buf[0]);
            for (int k = 0; k < n; k++, row++) {
                const daylog_min_t *m = &buf[k];
                if (!m->batt_mv) continue;
                int minute = h.first_min + row;
                nightsum_row(dates[i], minute, m->steps, m->energy, m->flags, m->raises, m->taps, m->hr, rest_night_at(minute));
                fed++;
            }
        }
        f.close();
    }
    char l[400]; nightsum_status(l, sizeof l);
    LOG("%s (%d rows of %d file%s walked)", l, fed, files, files == 1 ? "" : "s");
    return fed;
}

/* THE WATCH FACE's norm (face.cpp): the newest days' totals, once at
 * boot and once a day -- a few files read whole, never per frame */
int wrist_recent_totals(uint32_t today, uint32_t *out, int max)
{
    uint32_t dates[16];
    return wrist_recent_days(today, dates, out, max > 16 ? 16 : max);
}

/* THE WATCH PAGE's week: the dates beside the totals */
int wrist_recent_days(uint32_t today, uint32_t *dates_out, uint32_t *out, int max)
{
    if (!fs_ok || max <= 0) return 0;
    uint32_t dates[16]; int n = 0;              /* the dates, newest first */
    File dir = LittleFS.open("/day");
    if (!dir) return 0;
    File f;
    while ((f = dir.openNextFile())) {
        const char *nm = f.name();
        uint32_t d = (uint32_t)strtoul(nm, NULL, 10);
        bool bin = strstr(nm, ".bin") != NULL;
        f.close();
        if (!bin || !d || (today && d >= today)) continue;
        if (n == 16) { if (d <= dates[15]) continue; n--; }    /* full: the oldest falls off */
        int i = n++;
        while (i > 0 && dates[i - 1] < d) { dates[i] = dates[i - 1]; i--; }
        dates[i] = d;
    }
    dir.close();
    int m = 0;
    for (int i = 0; i < n && m < max; i++) {
        daylog_sum_t s;
        if (wrist_sum(dates[i], &s)) { if (dates_out) dates_out[m] = dates[i]; out[m++] = s.steps; }
    }
    return m;
}

static bool synced(uint32_t date)
{
    char p[32];
    ok_path(date, p, sizeof p);
    return fs_exists(p);
}

void wrist_mark_synced(uint32_t date)
{
    char p[32];
    ok_path(date, p, sizeof p);
    File f = LittleFS.open(p, "w");
    if (f) { f.write((const uint8_t *)"ok", 2); f.close(); }
}

bool wrist_next_unsynced(uint32_t *date_out)
{
    if (!fs_ok) return false;
    File dir = LittleFS.open("/day");
    if (!dir) return false;
    uint32_t best = 0;
    File f;
    while ((f = dir.openNextFile())) {
        const char *n = f.name();
        uint32_t d = (uint32_t)strtoul(n, NULL, 10);
        bool bin = strstr(n, ".bin") != NULL;
        f.close();
        if (!bin || !d || synced(d)) continue;
        char p[32]; daylog_hdr_t h; size_t sz;
        wrist_file_path(d, p, sizeof p);
        if (!read_hdr(p, &h, &sz) || !(h.flags & DAYLOG_CLOSED)) continue;
        if (!best || d < best) best = d;
    }
    dir.close();
    if (!best) return false;
    *date_out = best;
    return true;
}

/* --- DISK KEEPING ------------------------------------------------------------
 * ~23 KB a day into a 3456 KB partition: five months fill it, and a full
 * disk stops today's minutes. Once at boot after the clock is set and
 * once at the date's turn (wrist_tick's open-the-day branch), /day is
 * walked into a list and daykeep_decide (daykeep.cpp, pure) says what
 * goes: the newest DAY_KEEP files stay; older synced ones go first,
 * oldest first; an older unsynced one is kept until synced or until
 * the disk passes DAY_PRUNE_PCT, when the oldest goes regardless. The
 * list is malloc'd for the walk (up to KEEP_MAX_FILES x 12 B) and freed:
 * nothing static for a once-a-day job. */
static const int KEEP_MAX_FILES = 256;

static void rm_day(uint32_t d)
{
    char p[32];
    wrist_file_path(d, p, sizeof p); LittleFS.remove(p);
    ok_path(d, p, sizeof p);         if (fs_exists(p)) LittleFS.remove(p);
}

/* the files' own bytes and their count, for `day ls` and the keeper's line */
static int day_files_total(uint32_t *bytes)
{
    int n = 0; *bytes = 0;
    File dir = LittleFS.open("/day");
    if (!dir) return 0;
    File f;
    while ((f = dir.openNextFile())) {
        if (strstr(f.name(), ".bin")) { n++; *bytes += f.size(); }
        f.close();
    }
    dir.close();
    return n;
}

int day_keep(uint32_t today)
{
    if (!fs_ok) return 0;
    uint32_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    daykeep_file_t *files = (daykeep_file_t *)malloc(sizeof(daykeep_file_t) * KEEP_MAX_FILES);
    uint8_t *verdict = (uint8_t *)malloc(KEEP_MAX_FILES);
    if (!files || !verdict) { free(files); free(verdict); LOG("day: keep: no memory for the walk"); return 0; }
    int n = 0;
    File dir = LittleFS.open("/day");
    if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
            const char *nm = f.name();
            uint32_t d = (uint32_t)strtoul(nm, NULL, 10);
            bool bin = strstr(nm, ".bin") != NULL;
            uint32_t sz = f.size();
            f.close();
            if (!bin || !d || n >= KEEP_MAX_FILES) continue;
            files[n].date = d; files[n].size = sz; files[n].synced = synced(d);
            n++;
        }
        dir.close();
    }
    LOG("day: %d file%s, %u KB of %u, keeping %d days", n, n == 1 ? "" : "s",
        (unsigned)(used / 1024), (unsigned)(total / 1024), DAY_KEEP);
    int gone = daykeep_decide(files, n, used, total, today, verdict);
    for (int i = 0; i < n; i++) {
        if (verdict[i] == DAYKEEP_KEEP) continue;
        if (verdict[i] == DAYKEEP_OLD_SYNCED)
            LOG("day: dropped %08lu.bin synced -- older than the %d kept", (unsigned long)files[i].date, DAY_KEEP);
        else
            LOG("day: dropped %08lu.bin %s -- the disk was full", (unsigned long)files[i].date,
                files[i].synced ? "synced" : "unsynced");
        rm_day(files[i].date);
        if (day.open && day.date == files[i].date) day.open = false;
    }
    free(files); free(verdict);
    return gone;
}

void wrist_begin(void)
{
    /* THE DISK ASKS BEFORE IT FORMATS (fsguard.h): begin(false), then the table -- a disk that mounts is never
     * formatted (the key is set if it was not); a failed mount formats ONCE when no key says this firmware
     * formatted before (the first flash, his `uploadfs`), else it is refused and said (doctor leads with it) */
    fs_ok = LittleFS.begin(false);
    int v = fsg_decide(fs_ok, store.fs_formatted, false);
    if (v == FSG_FORMAT) fs_ok = LittleFS.format() && LittleFS.begin(false);
    if ((v == FSG_FORMAT && fs_ok) || v == FSG_SET_KEY) { store.fs_formatted = true; store_save_fs_formatted(); }
    fs_refused = (v == FSG_REFUSE);
    Serial.printf("[fs] %s\n", fsg_word(v));
    sec_start_ms = millis();
    last_unset_ms = millis();
    /* THE MINUTE KEPT ACROSS A RESET (minutekeep.h): what the RTC memory holds, against this boot's reset
     * reason and the clock (kept across a soft reset by the RTC, main.cpp); garbage is zeroed, never filed */
    {
        esp_reset_reason_t r = esp_reset_reason();
        bool soft = r != ESP_RST_POWERON && r != ESP_RST_UNKNOWN;
        int why, k = mk_decide(&keep, soft, clock_is_set(), clock_local_date(), clock_local_minute(), &why);
        if (k != MK_DISCARD) {
            take_min = keep.minute;
            LOG("day: the minute kept across the reset (reason %d): %02d:%02d %s%s -- %lu steps, %lu samples, %d s lit",
                (int)r, keep.minute / 60, keep.minute % 60, mk_why_word(why), k == MK_CROSSED ? ", filed as its own row" : "",
                (unsigned long)cur.steps, (unsigned long)cur.n, cur.screen_s);
        } else {
            if (soft) LOG("day: no minute kept across the reset (reason %d): %s", (int)r, mk_why_word(why));
            memset(&keep, 0, sizeof keep);
        }
    }
    if (!fs_ok) { Serial.println("[fs] no day log"); return; }
    if (!fs_exists("/day")) LittleFS.mkdir("/day");
    Serial.printf("[fs] LittleFS %u KB, %u KB used\n", (unsigned)(LittleFS.totalBytes() / 1024),
                  (unsigned)(LittleFS.usedBytes() / 1024));
    sec_start_ms = millis();
    last_unset_ms = millis();
}

/* --- serial ----------------------------------------------------------------- */

static void cmd_ls(void)
{
    File dir = LittleFS.open("/day");
    if (!dir) { Serial.println("day: no /day"); return; }
    File f;
    int n = 0;
    while ((f = dir.openNextFile())) {
        const char *name = f.name();
        if (strstr(name, ".bin")) {
            uint32_t d = (uint32_t)strtoul(name, NULL, 10);
            char p[32]; daylog_hdr_t h; size_t sz;
            wrist_file_path(d, p, sizeof p);
            if (read_hdr(p, &h, &sz))
                Serial.printf("day %08lu: %u records from minute %u, %u B, %s, %s%s\n",
                              (unsigned long)d, (unsigned)((sz - sizeof h) / sizeof(daylog_min_t)),
                              h.first_min, (unsigned)sz, (h.flags & DAYLOG_CLOSED) ? "closed" : "open",
                              synced(d) ? "synced" : "unsynced", d == day.date && day.open ? " (today)" : "");
            n++;
        }
        f.close();
    }
    dir.close();
    uint32_t bytes; day_files_total(&bytes);
    uint32_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    Serial.printf("day: %d file%s, %u KB of day files; %u KB used of %u, %u KB free; keeping %d days, pruning past %d%%\n",
                  n, n == 1 ? "" : "s", (unsigned)(bytes / 1024), (unsigned)(used / 1024), (unsigned)(total / 1024),
                  (unsigned)((total > used ? total - used : 0) / 1024), DAY_KEEP, DAY_PRUNE_PCT);
}

static void cmd_show(uint32_t d)
{
    daylog_sum_t s;
    if (!wrist_sum(d, &s)) { Serial.printf("day: no file for %08lu\n", (unsigned long)d); return; }
    Serial.printf("day %08lu: %d minutes logged (last minute %d), steps %lu, worn %d min, "
                  "active light %d / moderate %d / vigorous %d min, still %d min, screen %d s, "
                  "raises %d, taps %d, shakes %d\n",
                  (unsigned long)d, s.minutes, s.last_min, (unsigned long)s.steps, s.worn_min,
                  s.active_min[1], s.active_min[2], s.active_min[3], s.still_min, s.screen_s,
                  s.raises, s.taps, s.shakes);
    if (d == day.date && day.open)
        Serial.printf("day: the minute in progress: steps %lu, %u samples, raises %lu taps %lu\n",
                      (unsigned long)cur.steps, (unsigned)cur.n, (unsigned long)cur.raises,
                      (unsigned long)cur.taps);
}

static void cmd_dump(uint32_t d)
{
    char p[32];
    wrist_file_path(d, p, sizeof p);
    File f = LittleFS.open(p, "r");
    if (!f) { Serial.printf("day: no file for %08lu\n", (unsigned long)d); return; }
    Serial.printf("day-dump %08lu %u\n", (unsigned long)d, (unsigned)f.size());
    uint8_t b[32];
    int n;
    while ((n = f.read(b, sizeof b)) > 0) {
        for (int i = 0; i < n; i++) Serial.printf("%02x", b[i]);
        Serial.println();
    }
    f.close();
    Serial.println("day-dump end");
}

/* `day fake <YYYYMMDD> [minutes]`: a closed day of plausible numbers, to
 * prove the sync and the desk's reader without waiting a day */
static void cmd_fake(uint32_t d, int minutes)
{
    char p[32];
    wrist_file_path(d, p, sizeof p);
    daylog_hdr_t h; memset(&h, 0, sizeof h);
    memcpy(h.magic, DAYLOG_MAGIC, 4);
    h.version = DAYLOG_VERSION; h.rec_size = sizeof(daylog_min_t);
    h.first_min = 0; h.date = d; h.tz_min = (int16_t)store.tz_min; h.body = chip_body();
    h.count = (uint16_t)minutes; h.flags = DAYLOG_CLOSED;
    File f = LittleFS.open(p, "w");
    if (!f) { Serial.println("day: cannot write"); return; }
    f.write((const uint8_t *)&h, sizeof h);
    for (int m = 0; m < minutes; m++) {
        daylog_min_t r; memset(&r, 0, sizeof r);
        int hour = m / 60;
        bool night = hour < 7 || hour >= 23, walk = !night && (m % 90) < 12;
        r.flags = DAYLOG_WORN;
        r.steps = walk ? 95 + (m % 7) : 0;
        r.energy = night ? 4 + (m % 5) : walk ? 320 : 25 + (m % 20);
        r.band = r.energy < BAND_LIGHT_MG ? 0 : r.energy < BAND_MOD_MG ? 1 : r.energy < BAND_VIG_MG ? 2 : 3;
        r.posture = night ? 2 : walk ? 1 : 3;
        r.batt_mv = (uint16_t)(4100 - m / 4);
        r.still_s = night ? 58 : walk ? 0 : 30;
        r.raises = (!night && (m % 15) == 0) ? 1 : 0;
        f.write((const uint8_t *)&r, sizeof r);
    }
    f.close();
    ok_path(d, p, sizeof p);
    LittleFS.remove(p);
    Serial.printf("day: faked %08lu, %d minutes, closed, unsynced\n", (unsigned long)d, minutes);
}

/* THE DISK ASKS BEFORE IT FORMATS: `day format`, the one hand that may wipe a disk that will not mount */
static void cmd_format(void)
{
    int v = fsg_decide(fs_ok, store.fs_formatted, true);
    if (v != FSG_HAND_FORMAT) { Serial.printf("day: %s\n", fsg_word(v)); return; }
    bool ok = LittleFS.format() && LittleFS.begin(false);
    if (ok) { fs_ok = true; fs_refused = false; store.fs_formatted = true; store_save_fs_formatted(); if (!fs_exists("/day")) LittleFS.mkdir("/day"); }
    Serial.printf("day: formatted by hand: %s\n", ok ? "the disk mounts, empty; the minutes file again from the next one" : "the format FAILED (the partition itself?)");
}

void wrist_cmd(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "format")) { cmd_format(); return; }
    if (!fs_ok) { Serial.printf("day: no filesystem%s\n", fs_refused ? " -- DISK FAILED TO MOUNT (the files may be there; `day format` wipes it)" : ""); return; }
    const char *what = argc > 1 ? argv[1] : "show";
    uint32_t d = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : (day.open ? day.date : clock_local_date());
    if (!strcmp(what, "ls")) cmd_ls();
    else if (!strcmp(what, "show")) { if (!d) Serial.println("day: no clock, no day yet"); else cmd_show(d); }
    else if (!strcmp(what, "dump")) { if (!d) Serial.println("day: no clock, no day yet"); else cmd_dump(d); }
    else if (!strcmp(what, "close")) { if (d) { wrist_flush(); close_file(d); if (day.open && day.date == d) day.open = false; } }
    else if (!strcmp(what, "fake")) { if (argc > 2) cmd_fake(d, argc > 3 ? atoi(argv[3]) : 1440); else Serial.println("day fake <YYYYMMDD> [minutes]"); }
    else if (!strcmp(what, "rm")) { if (d) { rm_day(d); if (day.date == d) day.open = false; Serial.printf("day: %08lu removed\n", (unsigned long)d); } }
    else if (!strcmp(what, "prune")) {
        /* DISK KEEPING by hand: the keeper's own lines say what went */
        uint32_t today = day.open ? day.date : clock_local_date();
        int gone = day_keep(today);
        Serial.printf("day: prune: %d file%s dropped\n", gone, gone == 1 ? "" : "s");
    }
    else Serial.println("day show|ls|dump|close|rm|prune|fake|format [YYYYMMDD]");
}
