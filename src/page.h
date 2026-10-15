/* THE WATCH PAGE (Khalid, 2026-09-10 afternoon: "split it: more firmware
 * WITHOUT the slime; the slime is nice to have, but build the watch
 * features too, an actual smartwatch").
 *
 * A finger drawn UP the glass (bottom third to top third -- the one
 * axis that meant nothing) turns the glass over to ITS OTHER FACE for
 * PAGE_S seconds: HIS TOOL PAGE. The hour big in the centre, the date
 * under it, the day's steps with a footprint, the cell as a percent
 * with the drop, the next alarm (or "--"), the sky's word if one is
 * held, and the link as one dot. Numbers are allowed HERE -- it is his
 * tool, the way the hour already is (design-zoom-out: the creature's
 * language stays wordless; this page is not the creature's). The gel is
 * not drawn while the page is up (the glass shows one face at a time);
 * the rings stay at the rim (the page draws only inside them). Pinned
 * to the watch frame like the hour: never rotates. A tap or a drag down
 * closes it; any other touch gesture on it is the page's, not the
 * creature's (main.cpp routes them). `page` over serial shows / hides
 * it.
 *
 * THE PAGE GROWS (2026-09-10 evening). A running stopwatch or timer
 * shows on the tool page (its M:SS with a mark, the lap count, the last
 * two laps' splits). A SECOND drag up while the page is up turns it to
 * HIS WEEK -- steps per day for seven days as bars with the numbers,
 * the night's hours (rest.cpp's last night: what the wrist already
 * knows, never a guess), the cell and whether it charges; a THIRD to
 * THE NOTICES -- the last three wisps from the desk with how long ago
 * (wisp.cpp's ledger: RAM, no persistence); a fourth is the tool page
 * again. Every drag restarts the PAGE_S; a tap or a drag down closes
 * whichever page is up. THE SWIPES (design/watch-swipes.md 3A,
 * 2026-09-23): with a page up a FLICK along his wall turns it too --
 * toward his left the next page (as the drag up), toward his right the
 * previous (the ring backwards, page_prev); main reads the flick's
 * direction on the wall he sits on and calls one or the other.
 *
 * THE SETTINGS PAGE (Khalid, 2026-09-10 18:55: "a SETTINGS MENU on the
 * watch so I can do things myself"). Two more pages in the ring,
 * SETTINGS 1 and 2 (3, 4, 5 and ABOUT since MORE SETTINGS, 21:12), turned by the same drag up after THE NOTICES:
 * rows he taps -- SLIME (HERE / AWAY: a tap sends him or calls him,
 * the lease's own road; 09-10 21:08), LEASE (may a hold / three
 * knocks claim or release), QUIET TIME (wake.cpp's rule), FACE (the
 * hour and rings on the empty glass); then WATER LOCK, LINK NOW (a
 * one-shot dial), ALARM (the hour; a tap turns it off and back on --
 * the rim dial sets it) -- and a status row (the link's word, the
 * SSID, the ip). A tap on a row is
 * the row's: the page hands main an ACTION (page_tap: the page knows
 * no module), highlights the row for SET_HILITE_S and starts its 8 s
 * over; a tap off the rows closes the page as ever. What each row
 * shows comes in through page_in_t (the modules' own state, read by
 * main each frame), so the page never lies about what it toggled.
 *
 * THE NOTICE OPENED (2026-10-06, with THE NOTICE'S OWN REACTION): a
 * tap on a row of THE NOTICES opens that notice for the page's stay --
 * the app's name in the shine, the title dimmed, the MESSAGE
 * word-wrapped over up to five rows of the round glass, the age at the
 * bottom; a tap on it keeps it 8 s more (he is reading, as ABOUT), a
 * drag up or a swipe returns to the list (the next page is a second
 * drag), a drag down closes as ever. The open notice is the page's own
 * memory (as hilite_row is: a view, not a state a module owns), so
 * page_in_t carries nothing for it; page_tap answers PAGE_NOTE_0 + the
 * row (main has nothing to do but let it be). THE NOTICES' rows show
 * the app's NAME when the ledger has one (ANCS asks the phone for it
 * after the notice; the id's last dotted word until then) and, under
 * it, the title -- or the message's start when the notice has no title.
 *
 * No Arduino: parameters in, a framebuffer out; tools/page_test.cpp
 * renders it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define SET_HILITE_S 0.6f               /* THE SETTINGS PAGE: a tapped row lit in the shine this long */

#define PAGE_S 8.0f                     /* the page's stay, re-armed by another drag up */
/* THE LAMP (design/watch-ideas.md B17, 2026-09-10 evening): the one
 * thing every wrist does in the dark -- the glass lit WHITE at full
 * backlight for LAMP_S, or until a tap. B17 had it on the drag up; that
 * axis is the page's now, so the lamp lives ON the page: a long press
 * (>= 1.5 s) on any page. It is the glass's trick, not the creature's:
 * the gel is not drawn under it (the page is up), and when it goes out
 * while he is HOLDING the gel squints at the glare (main's own eye
 * kind). The page's 8 s do not run while it burns; a tap or a drag
 * down ends both. */
#define LAMP_S 20.0f
/* the lamp's hold: 1.5 s or more on the page, as B17 had it. Since THE
 * HOLD'S WINDOW, WIDER (09-10 21:08) the detector names a plain hold of
 * 0.5-1.9 s the lease's; on the page there is no lease, so main reads
 * the hold's length against this: at or over it, the lamp (as any
 * GES_LONG_PRESS is); under it, the page's nothing */
#define LAMP_HOLD_MS 1500
#define PAGE_NOTICES 3                  /* the notices page lists this many */
#define PAGE_NO_DAY 0xFFFFFFFFu         /* a day of the week with no file */

enum { PAGE_LINK_NONE = 0, PAGE_LINK_FINE = 1 };   /* the dot: the shade (no door) / the green-ish shine (linked, or resting on the budget) */
/* the pages, in the order the drag up turns them. MORE SETTINGS (Khalid,
 * 2026-09-10 21:12: "I like the settings as it is right now, the layout
 * ... just more from the watch -- bluetooth and wifi and what not"):
 * SETTINGS 3, 4, 5 and ABOUT after the two he has, the same rows, the
 * same drag; the ring's order otherwise as it was */
/* THE BLUETOOTH PAGE (THE OPERATOR NIGHT T16, 2026-10-12; the page owed since MORE SETTINGS: "bluetooth and wifi
 * and what not"): after THE NOTICES, before the settings -- five lines in words (PHONE, AIR DOOR, EAR, PAIRING,
 * NOTICES), the PAIRING line a row (a tap opens THE PAIRING WINDOW); SETTINGS 6 (T15, THE HIDDEN SWITCHES) after 5 */
enum { PAGE_TOOL = 0, PAGE_WEEK = 1, PAGE_NOTICES_PAGE = 2, PAGE_BLE = 3, PAGE_SETTINGS = 4, PAGE_SETTINGS2 = 5,
       PAGE_SETTINGS3 = 6, PAGE_SETTINGS4 = 7, PAGE_SETTINGS5 = 8, PAGE_SETTINGS6 = 9, PAGE_ABOUT = 10, PAGE_N = 11 };
#define PAGE_SETTINGS_N 6               /* SETTINGS 1..6 (the pages PAGE_SETTINGS .. PAGE_SETTINGS6) */
/* THE SETTINGS PAGE's rows (the order on the glass: page 1 the first four, page 2 the next three, then SETTINGS 3-5's -- page.cpp's SET_PAGE_FIRST / SET_PAGE_ROWS), and the actions a tap hands main.
 * THE SLIME ROW (Khalid 2026-09-10 21:08: "make it easy access for the watch for it to leave"): the FIRST row of
 * SETTINGS 1 is an ACTION, not a switch -- `SLIME  HERE` while the house holds him (a tap = release, the hold's own
 * road) and `SLIME  AWAY` while it is empty (a tap = come, the knocks' road); it always works, LEASE or no LEASE
 * (that switch governs the hold and the knocks). Five rows do not fit at 34 px inside the disc (the fifth row's chord
 * at y 202 is 100 px; ALARM and its hour need 114), so ALARM moved to SETTINGS 2, before the status lines. */
/* MORE SETTINGS (21:12): SETTINGS 3 -- WIFI (HOLD / AUTO: the radio kept up every lit spell, or the budget), NETWORK
 * (the SSID, or OFF / JOINING; read-only), ADDRESS (the ip; read-only), BLUETOOTH (ON / OFF, ruling 58: the BLE stack
 * up only while the WiFi radio is down -- `ON --` advertising, `PHONE` while an iPhone is connected); SETTINGS 4 -- BRIGHTNESS (LOW / MID / HIGH), LIT SPELL (SHORT 12 / 20 s, LONG
 * 60 / 90 s), LAMP (an action: the lamp); SETTINGS 5 -- CLOCK (12H / 24H), QUIET FROM, QUIET TO (whole hours, a tap
 * cycles; the desk's grain overrides while it says, shown in the shade); ABOUT (read-only: build, ip, heap, cell,
 * uptime, steps -- a tap anywhere keeps it 8 s more). Read-only rows still hand main their SET_* (a log line). */
enum { SET_SLIME = 0, SET_LEASE, SET_QUIET, SET_FACE,                  /* SETTINGS 1 */
       SET_LOCK, SET_LINK, SET_ALARM,                                  /* SETTINGS 2 */
       SET_WIFI, SET_NETWORK, SET_ADDRESS, SET_BT,                     /* SETTINGS 3 */
       SET_BRIGHT, SET_SPELL, SET_LAMP, SET_BUMPS,                     /* SETTINGS 4 (BUMPS the fourth row, 2026-09-23: THE BUMPS' switch -- ON / OFF; the page's fourth row fits as SETTINGS 1 and 3 have it, nothing he has seen moved) */
       SET_CLOCK, SET_QFROM, SET_QTO, SET_PULSE,                       /* SETTINGS 5 (PULSE the fourth row, 2026-10-03: THE PULSE ROW -- a tap starts a 30 s heart-rate read; the row counts it down, then shows the BPM) */
       SET_ABOUT,                                                      /* ABOUT: the whole page as one row */
       /* SETTINGS 6, THE HIDDEN SWITCHES (T15, 2026-10-12; appended after ABOUT so no row he has moves): PULSE AUTO (ON / OFF:
        * `ppg auto`), DOCK HAND (ON / OFF: `dock hand`), EAR GEAR (LOW / MID / FULL, a tap cycles: `ear gear 80|160|240`),
        * FORGET (PHONE dimmed = a bond to drop, NONE in the shade = nothing; a tap arms it -- `SURE?` in the shine for
        * FORGET_ARM_S -- and a second tap inside that does `ble forget`; the arm is the page's own memory, as a lit row is) */
       SET_PAUTO, SET_DOCKHAND, SET_EARGEAR, SET_FORGET,
       /* THE BLUETOOTH PAGE (T16): PAIRING is its one row (a tap = THE PAIRING WINDOW, as the BLUETOOTH row's third state);
        * SET_BLE = a tap anywhere else on its disc (the page kept 8 s, as ABOUT; main logs the page's words) */
       SET_PAIRING, SET_BLE,
       SET_N };
#define FORGET_ARM_S 3.0f               /* SETTINGS 6's FORGET: the second tap must land inside this of the first */
enum { PAGE_ACT_NONE = -1,              /* page_tap: no row under the finger (main closes the page as ever); else a SET_* row, or PAGE_NOTE_0 + a notice's row */
       PAGE_ACT_KEEP = -2 };            /* the page took the tap itself (SETTINGS 6's FORGET armed): nothing for main, the page stays */
/* THE NOTICE OPENED: page_tap's answer for THE NOTICES -- PAGE_NOTE_0 + i = the i-th row's notice opened (or, open already, kept
 * 8 s more); well past SET_N so a SET_* and a notice never meet. The page keeps the open notice itself (page_note_open). */
enum { PAGE_NOTE_0 = 100 };
#define PAGE_NOTE_ROWS 5                /* the message's rows on the open notice, at most */
enum { PAGE_TOOL_NONE = 0, PAGE_TOOL_STOPWATCH, PAGE_TOOL_TIMER };          /* what counts on the tool page */

typedef struct {
    const char *app;        /* the app id as the desk said it (the page shows its last dotted name when no name is known) */
    const char *title;      /* may be NULL or "" */
    uint32_t    age_s;      /* how long ago it arrived */
    const char *name;       /* the app's display name ("Messages"); NULL or "" = unknown, the id's last word then */
    const char *msg;        /* the notice's message, NULL or "" = none (THE NOTICE OPENED wraps it; the list shows its start when there is no title) */
} page_notice_t;

typedef struct {
    int      minute;        /* the local minute of the day, -1 = no clock */
    int      wday;          /* 0 Sun .. 6 Sat, -1 = no clock */
    int      mday;          /* 1..31 */
    int      mon;           /* 0 Jan .. 11 Dec */
    uint32_t steps;         /* wrist_steps_today() */
    int      batt_pct;      /* the cell, 0..100; -1 = unknown */
    bool     charging;
    int      alarm_min;     /* clockwork_alarm(): the minute of the day, -1 = none */
    const char *sky;        /* sky_word(0) when known, "" when none (drawn uppercase) */
    int      link;          /* PAGE_LINK_* */
    bool     cell_lean;     /* BATTERY MANNERS (vigor_lean): under 10 % the cell's number is drawn in the shade; charging, it breathes (the cell filling) */
    /* THE PAGE GROWS: the tool page's count */
    int      tool;          /* PAGE_TOOL_*: a stopwatch (running or its final time) or a timer (running or at zero) */
    uint32_t tool_ms;       /* the elapsed (the stopwatch) or the remaining (the timer) */
    int      laps;          /* clockwork_laps(): the count... */
    const uint32_t *lap_ms; /* ...and the splits, oldest first (NULL when none) */
    /* HIS WEEK */
    uint32_t week[7];       /* steps per day, the oldest first, [6] = today (live); PAGE_NO_DAY = no file */
    int      night_min;     /* rest_night_min(): the last night's minutes, 0 = none known */
    /* THE NOTICES */
    const page_notice_t *notice;   /* newest first, `notices` of them (NULL = none) */
    int      notices;
    /* THE SETTINGS PAGE: what the rows show (the modules' own state; alarm_min above is the ALARM row's) */
    bool     set_here;      /* THE SLIME ROW: house_holding() -- HERE (a tap releases) / AWAY (a tap calls) */
    bool     set_lease;     /* the hold / the knocks may claim or release (main's lease_hand) */
    bool     set_quiet;     /* QUIET TIME's rule is on (wake_quiet_on) */
    bool     set_face;      /* face_on() */
    bool     set_lock;      /* gesture_locked() */
    const char *net_ssid;   /* net_ssid(): "" when the radio is down */
    const char *net_ip;     /* net_ip(): "" when down */
    const char *link_word;  /* the link in a word: DOCK / LINKED / DIALLING / RESTING / RADIO OFF (main composes it; LINK NOW's value) */
    /* MORE SETTINGS (21:12): SETTINGS 3-5's rows and ABOUT's lines */
    bool     wifi_hold;     /* link_holding_radio(): HOLD / AUTO */
    bool     radio_on;      /* net_radio_is_on(): NETWORK reads the SSID, JOINING, or OFF */
    const char *bt_word;    /* BLUETOOTH's value (ble_word): "OFF" (the shade) / "ON --" (dimmed: on, nobody) / "PHONE" "DESK" "BOTH" (the shine: who is on) / "42 S" (the shine: THE PAIRING WINDOW's count); "--" = no word */
    int      bright;        /* store.bright: 0 LOW / 1 MID / 2 HIGH */
    bool     spell_short;   /* store.spell_short: SHORT / LONG */
    bool     bumps_on;      /* THE BUMPS (bumps.h): !store.bumps_off -- the BUMPS row's ON / OFF */
    bool     h12;           /* store.h12: the CLOCK row, and every hour on the page (hourfmt.h) */
    int      quiet_from, quiet_to;   /* store's whole hours */
    bool     quiet_desk;    /* the desk's grain `quiet=` rules for now: the two values in the shade */
    const char *build;      /* doctor_build_id() */
    uint32_t heap_free;     /* ESP.getFreeHeap() */
    uint32_t up_s;          /* millis() / 1000 */
    uint16_t cell_mv;       /* batt_mv() */
    const char *box;        /* THE BOX'S STATUS (link_box_short): `KAPTOP 2 CLEAR` -- the holder, the words waiting, the box's sky; NULL / "" = not asked (`--` in the shade) */
    /* THE PULSE ROW (2026-10-03): the sensor's own state (ppg.h), never the page's memory */
    bool     pulse_absent;  /* no sensor on the BOM: `--` in the shade, a tap does nothing */
    int      pulse_left_s;  /* > 0 while a read runs: the seconds left, in the shine (the whole length while the AGC settles) */
    int      pulse_bpm;     /* the last read's BPM, 0 = none yet (or the last read failed: pulse_fail) */
    bool     pulse_fail;    /* the last read found no clean pulse: `NONE` in the shade */
    bool     pulse_still;   /* THE STILL SPOT (2026-10-08): the read waits for the wrist to be still, the LEDs dark: `STILL` in the shade (before the count) */
    bool     pulse_thin;    /* THE PERFUSION INDEX (2026-10-14): the failed read's cause was a THIN perfusion (the strap): `THIN` in the shade where NONE would be; never a number */
    /* SETTINGS 6, THE HIDDEN SWITCHES (T15): the stores' own words */
    bool     pauto_on;      /* !store.ppg_auto_off: PULSE AUTO ON / OFF */
    bool     dockhand_on;   /* !store.dockhand_off: DOCK HAND ON / OFF */
    int      ear_gear;      /* store.ear_gear: 80 LOW / 160 MID / 240 FULL (anything else reads FULL) */
    /* THE BLUETOOTH PAGE (T16): the stack's own state (ble.h and its tenants), never the page's memory; FORGET reads ble_bonded too */
    bool     ble_on;        /* ble_enabled(): the switch -- off, EAR reads OFF */
    bool     ble_bonded;    /* ble_bonded(): a bond in NVS -- PHONE AWAY (NEAR when connected), NONE without; FORGET's PHONE / NONE */
    bool     ble_phone;     /* ancs_connected(): the phone on and ready -- PHONE NEAR */
    bool     ble_desk;      /* bleuart_linked(): the desk subscribed over the air -- AIR DOOR OPEN / SHUT */
    bool     ble_ear;       /* ble_keeps_dark(): THE DARK EAR on -- EAR ON; else OFF (the switch) or NO BOND */
    bool     ble_pairing;   /* ble_pair_left_s() > 0: PAIRING OPEN / SHUT (the word turns, no count) */
    bool     ble_notices;   /* wisp_waiting() > 0: NOTICES SOME / NONE (waiting for the light or at the air door) */
} page_in_t;

void page_begin(void);
void page_show(const char *why);        /* the tool page, up for PAGE_S (again, if already up); the line is page_tick's */
void page_show_which(int which, const char *why);   /* a given page (`page week`) */
void page_next(const char *why);        /* the drag up (or, THE SWIPES 2026-09-23, a swipe toward his left) while it is up: the next page, PAGE_S again (down: the tool page) */
void page_prev(const char *why);        /* THE SWIPES: a swipe toward his right while it is up: the previous page (the ring backwards; from the tool page, ABOUT), PAGE_S again (down: the tool page) */
void page_hide(const char *why);
void page_lamp(const char *why);        /* THE LAMP: the glass white for LAMP_S on the page that is up (or the tool page); logged by page_tick */
bool page_lamp_on(void);                /* main: the backlight at full while it burns */
bool page_up(void);
int  page_which(void);                  /* PAGE_* while up, -1 down */
const char *page_name(int which);       /* "the tool page" / "his week" / "the notices" / "bluetooth" / "settings 1/6" .. "settings 6/6" / "about" */
/* THE SETTINGS PAGE: a tap at x,y on the page that is up -- a row's
 * index (SET_*) when the finger is on one of a settings page's rows
 * (the row highlights, the 8 s start over; main does the toggle),
 * PAGE_ACT_NONE otherwise (main closes the page as ever) */
int  page_tap(int x, int y);
bool page_forget_armed(void);           /* SETTINGS 6's FORGET: the first tap landed, the second would do it (FORGET_ARM_S; `page` prints it) */
int  page_note_open(void);              /* THE NOTICE OPENED: the open notice's row on THE NOTICES, -1 = the list (or another page) */
/* THE NOTICE CLEARED FROM THE WRIST (T17): the lamp's hold landing on an OPEN notice card -- the card closes to the list (the
 * 8 s again) and the row it showed is answered, for main to clear that notice on the phone (ancs_clear) and eat its wisp;
 * -1 = no card open (the list, another page, the lamp, no page): the hold is the lamp's as ever. Never the lamp on a card. */
int  page_note_hold(void);
const char *page_setting_name(int row);   /* "slime" / "lease" / "quiet" / "face" / "lock" / "link" / "alarm" / "wifi" / ... (the console's `set <row>`), NULL past SET_N */
const char *page_setting_label(int row);  /* the label on the glass ("QUIET FROM"), NULL past SET_N */
int  page_setting_by_name(const char *name);   /* -1 = not a row */
float page_left_s(void);
void page_hold(void);                   /* THE PULSE ROW: the page's stay topped back to PAGE_S while a read runs (and once more when its number lands), so the glass does not turn over mid-count; nothing when no page is up */
const char *page_tick(float dt);        /* one call a frame: the stay's clock; a log line or NULL */
void page_draw(uint16_t *fb, const page_in_t *in);   /* after face_draw: clears inside the rings, draws the page */
#ifdef __cplusplus
#include "dirty.h"
bool page_mark(dirty_rect_t *box);      /* THE MARK: did this frame's page differ from last frame's; the box of both */
#endif
void page_status(char *out, int cap);   /* `page` over serial prints this after the toggle */
