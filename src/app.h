/* The seam between main.cpp (the creature's offline half) and the
 * modules that only ADD to it: the state they may read, the few things
 * they may ask of the body. main.cpp fills ``app`` and owns the gel;
 * nothing here touches the gel, the bowl or the fall. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

extern const uint16_t *app_fb;   /* the face, RGB565 in the panel's byte order: /screen.bin (Khalid: "screenshot the watch") */

typedef struct {
    int      wall;          /* GEL_WALL_* : THE WALL he sits on (walls.h: one of four, always) */
    float    phi_deg;       /* the wall's angle on the rim (0 / 90 / 180 / -90) */
    int      state;         /* 0 awake, 1 dim, 2 dark */
    float    fps;           /* the last 5 s window */
    uint32_t boot_ms;
} app_state_t;

extern app_state_t app;

void app_pet(void);                     /* a pet from outside (the HTTP page) */
void app_keep_awake(void);              /* a word over serial keeps it awake */
void app_lease_toggle(const char *how); /* THE LEASE, EASY: the one toggle (come / release by the house) with the wrist's own feedback -- `come` / `release` over serial take this road too */
/* THE SETTINGS PAGE: one row set from outside -- `set <row> on|off` (want 1 / 0; -1 = toggle, what a tap on the row does); the line says what happened */
const char *app_setting(int row, int want, const char *how);
/* THE ALARM'S ONE ROAD (T13): the ALARM row's and the desk's `alarm` ask's -- a minute sets it, -1 clears it and remembers the hour; NVS; clockwork told */
void app_alarm_set(int minute, const char *how);
void app_bumps_status(char *out, int cap);   /* THE BUMPS (bumps.h): `bumps` over serial -- the switch, the tallies, the gate's state now */
/* THE TRACE ANSWERED (watch-ideas C26, 2026-10-09): the posture he rests in -- THE WALL he sits on (GEL_WALL_*) and the
 * residual's eased lean as a fraction of LEAN_CAP (-1..1, + = his right along the wall); link.cpp puts it on the release */
void app_trace(int *wall_out, float *lean_out);
void app_set_markers(bool on);          /* the orientation markers */
bool app_markers(void);
void app_set_axis_map(int m);           /* '0'..'7': how the IMU's axes land on the screen */
extern const char *const APP_STATE_NAMES[3];
/* DOCTOR: the last 5 s report window's frame numbers -- the rate while
 * moving (the >= 35 rule's), the seconds spent moving, and whether the
 * loop is at its idle 20 Hz now */
void app_frame_stats(float *moving_fps, float *moving_s, bool *idle_now);

/* the clock: UTC from NTP or the desk, local = UTC + store.tz_min.
 * "set" means someone told it the hour this power cycle (or a soft
 * reset kept the RTC's). */
bool  clock_is_set(void);
void  clock_set_epoch(uint32_t epoch, const char *who);
void  clock_local(struct tm *out, uint32_t *epoch_out);   /* now, local */
uint32_t clock_local_date(void);        /* YYYYMMDD, or 0 unset */
int   clock_local_minute(void);         /* minute of the local day, or -1 */
const char *clock_source(void);         /* who set it last: "ntp", "the desk", "the rtc", "serial", "kept across a reset", "nobody" (DOCTOR) */

/* what the body's battery says: the divider on GPIO1, Vbat/3 */
uint16_t batt_mv(void);
bool     charging(void);                /* VBUS inferred: the charger's float on VBAT (no VBUS pin on the board) -- or THE RISING CELL (cable.h) */
bool     spent_line(void);              /* THE LAST DROP'S MANNERS (spent.h): under DROP_MV 3550 or vigor's spent band -- no WiFi dial off the cable, LOW light, the SHORT spell */

/* the log's timestamp prefix, the same shape main.cpp prints */
#define LOG(fmt, ...) Serial.printf("[%8lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__)
