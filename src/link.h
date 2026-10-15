/* The link: design/watch-link.md spoken from the wrist. One engine, three
 * transports carrying the same bytes -- a non-blocking lwIP socket to a
 * plain door (the laptop's LAN door), a WiFiClientSecure session to a
 * TLS door (the box behind Tailscale Funnel; the dial on a short-lived
 * task, the session polled from loop()), or the USB dock (the desk's
 * SerialLink driving this same serial port: a `hi <nonce>` from the
 * console starts it). The doors are store.doors, tried in order; one
 * that fails is skipped for a growing while. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool  have;
    int   seed;
    float regard;
    int   night;
    char  quiet[12];
    char  mood[20];
    bool  have_belly;   /* THE BELLY TRAVELS (watch-ideas C20): the line carried belly= */
    float belly;        /* the desk's stomach fullness at the line, 0..1 */
    bool  have_day;     /* HE ARRIVES WITH THE OTHER HOUSE'S DAY (watch-ideas C21, 2026-10-09): a LIVE line carried sat= / pets= */
    int   sat_min;      /* minutes he has sat at the desk this session (0 = none) */
    int   pets;         /* pets today at the desk (0 = none) */
} link_grain_t;

void link_begin(void);
void link_tick(void);                   /* from loop() */
void link_net_up(bool up);              /* the WiFi manager says */
/* THE RADIO BUDGET (radio.h): the link dials only for a reason, and
 * switches the radio itself (net_radio_on / off) */
void link_wake(void);                   /* the wake (from net_on): the clocks run; a reason brings the radio up */
void link_dark(void);                   /* the dark house (from net_off): the clocks stop, a hold ends */
/* THE NIGHT RELEASE (ruling 52): main's nap() calls this between light
 * sleeps -- the night's onset (rest.cpp's verdict, landed by the nap's
 * own minute) sends `release asleep` on a DARK ERRAND (the radio up with
 * the glass dark, one verb, the radio off; bounded), the house EMPTY
 * with no theatre; the rising (the raise / shake / touch that ends the
 * span) claims back on the wake's own dial. Returns at once when rest
 * has nothing to say. */
void link_dark_minute(void);
/* THE LAN KNOCK: the desk's `POST /lease` body (`lost` / `free`, signed
 * with the watch key when one is remembered here -- radio.h has the
 * line). 0 = taken (why = the house's state after), 1 = refused (why =
 * "no key" / "stale"), 2 = not a knock. It only ever makes the wrist
 * ask the box: never the lease by itself. */
int  link_knock(const char *body, char *why, int cap);
/* THE DOOR'S AUTH, lent to `POST /art` (ART OVER THE AIR, net.cpp): the
 * knock's own verdict over another message -- mac = HMAC(key, "<stamp>
 * <what>"), the stamp the UTC minute within KNOCK_STALE_MIN of the
 * wrist's clock (stamp_s / mac_s NULL or "" = unsigned). 0 = taken (why
 * "ok", or "unsigned" on a bare board), 1 = refused (why "no key" /
 * "stale"). radio_knock_judge decides, as for the knock. */
int  link_sign_check(const char *what, const char *stamp_s, const char *mac_s, char *why, int cap);
/* THE REACHING CUE (face.cpp): a come / a claim by hand (the hold, the
 * knocks, the desk's `free`, the rising) is on its way to the box --
 * from the ask until the answer or the give-up; the face ripples the
 * rim at the wall meanwhile, so the dial does not feel dead */
bool link_reaching(void);
void link_now(const char *why);         /* `link now`: one dial now, whatever the clocks (a test aid) */
void link_hold(bool on);                /* `link hold` / `link auto`: the radio kept up this lit spell (OTA, the page, provisioning) */
bool link_holding_radio(void);          /* a hold is on */
void link_hold_for(uint32_t s);         /* THE FLASH FROM THE DESK (T14): the desk's `hold <s>` -- a timed hold (holdask.h caps it); 0 releases */
uint32_t link_hold_left_s(void);        /* seconds left on the desk's hold (0: none, or the console's untimed one) */
bool link_resting(void);                /* awake with no reason to dial: NOT offline (THE HOUSE's clock does not run) */
bool link_radio_wanted(void);           /* a reason stands (a due dial, met or not) */
void link_radio_line(char *out, int cap);   /* "resting: last link 4 min ago, next due in 6 min" -- `link`, `status`, `doctor` */
/* the dock: the console hands over the desk's lines; true = taken */
bool link_serial_line(const char *line);
bool link_docked(void);                 /* the serial link is the held door (THE DOCK FACE's plug) */
/* THE DOCK FACE: a day sync in flight over any door -- bytes sent so far, the day's size */
bool link_sync_progress(uint32_t *sent, uint32_t *total);
uint32_t link_sync_done_seq(void);      /* bumped on each `ok day` end: the ring's one blink */
void link_serial_ask(const char *line);     /* a `wifi add ...` with no dock open */
void link_undock(const char *why);
/* THE AIR DOOR (bleuart.cpp): the desk's lines over BLE, the dock without the cable */
bool link_ble_line(const char *line);   /* a line from the desk's BLE central; true = taken (a `hi` opens the session) */
void link_air_closed(const char *why);  /* the central left / unsubscribed / the stack went down */
bool link_air(void);                    /* the air door is the held door */
/* the body's events, as senses (dropped when not linked; one a kind per 30 s) */
void link_sense(const char *kind, const char *words);
/* THE BELLY TRAVELS (watch-ideas C20): the wrist's own lump (the feed's
 * or the walk's, the larger, 0..1) once a frame; it goes back as `sense
 * wrist:belly <0.00-1.00>` once per change over BELLY_STEP, a ride-along
 * (never a dial by itself: the meal it follows is the reason) */
void link_belly(float lump);
/* the long press: come, then release -- with the radio resting, the
 * radio comes up for it (THE RADIO BUDGET: a whistle is a reason) */
void link_whistle(void);
void link_sync_now(void);               /* ask for a day sync as soon as linked */
bool link_linked(void);
bool link_holding(void);                /* the lease is ours, as far as the desk said */
const char *link_state_name(void);
const link_grain_t *link_grain(void);
/* ONE BODY ACROSS THE HOUSES (C21): the desk's sit and pets off a live grain line younger than GRAIN_DAY_FRESH_MS
 * (30 min: the line is asked at every hello; an offline arrival hours later wears nothing); 0 / 0 when stale or
 * never. true = fresh. */
bool link_grain_day(int *sat_min, int *pets);
/* THE DOCK HANDS HIM OVER (dockhand.h, C24): the switch (NVS `dockoff`) and the console's line */
void link_dockhand_set(bool on);
void link_dockhand_line(char *out, int cap);
uint32_t link_last_ok_ms(void);
/* THE SMALL HOUSE (cramp.h, 2026-10-09): the last `ok` from a door that IS
 * the desk -- the LAN door (plain), the USB dock, the air door -- or a
 * knock of his taken (`POST /lease`, the dock's `knock`); the box's TLS
 * door is not the desk. 0 = never. The honest evidence that the bigger
 * house is reachable right now */
uint32_t link_desk_ok_ms(void);
int  link_days_synced(void);
void link_status(char *out, int cap);  /* one line for `status` */
/* THE BOX'S STATUS (SAGE-WEARABLE-CLAW-REPLY section 4): `status` asked
 * once per link after the day sync -- `ok status holder=<body|none>
 * last_day=<date|none> words=<n> sky=<w[,w]|none>`; kept for the
 * console (`box: holder=kaptop day=2026-09-11 words=2 sky=clear`), the
 * doctor and the page's ABOUT row (`KAPTOP 2 CLEAR`; "" = not asked) */
void link_box_line(char *out, int cap);
const char *link_box_short(void);
/* THE REFUSAL SAID ONCE (refusal.h, 2026-10-12 T5): a door's repeated
 * word (`no gate` on every beat, `no box`, `no soul`, `none` to word /
 * sky, `no verb` to status / look) is logged once per verb, again when
 * the word changes, counted between -- this line carries the count for
 * `link` and doctor: `refusals: no gate x 212 since 09:12 (beat)`; ""
 * while nothing was refused this session (reset at every `ok hello`) */
void link_refusal_line(char *out, int cap);
/* THE TLS SESSION IN PSRAM (the optimization pass): where mbedtls's
 * sessions are allocated from the next dial on -- PSRAM (the default)
 * or internal RAM (the core's own); `tls-mem psram|internal` */
void link_tls_mem(bool psram);
bool link_tls_psram(void);
void link_door_line(int i, char *out, int cap);   /* one door for `desk list`: index, host, port, tls, last result */
/* WORN ON THE BEAT (SAGE-WATCH-LINK-SLIME-ACK section 8, behind
 * store.beat_worn): the minute's `beat` / honest `claim` carry one word,
 * `worn` / `still` (house_worn()); `beat-worn on|off` over serial sets
 * the flag (NVS) and re-arms the word for this link */
void link_beat_worn(bool on);
