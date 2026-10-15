/* What the watch remembers across power: NVS (Preferences, namespace
 * "watch"). Never in the repo -- credentials and the link key are typed
 * over serial once (README "Provisioning") and live only in the chip. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define STORE_NETS      12          /* networks remembered */
#define STORE_DOORS     4           /* the desk doors, tried in order */
#define STORE_SSID_MAX  32
#define STORE_SECRET_MAX 64         /* a psk, an EAP password, an identity */

enum { NET_OPEN = 0, NET_PSK = 1, NET_EAP = 2 };

typedef struct {
    char    ssid[STORE_SSID_MAX + 1];
    uint8_t kind;                           /* NET_* */
    char    id[STORE_SECRET_MAX + 1];       /* EAP: the inner identity (username) */
    char    anon[STORE_SECRET_MAX + 1];     /* EAP: the outer/anonymous identity ("" = id) */
    char    pw[STORE_SECRET_MAX + 1];       /* the psk, or the EAP password */
} net_cred_t;

/* a door: where a watch-link daemon answers. Plain TCP (the laptop's
 * LAN door) or TLS (the box behind Tailscale Funnel: Let's Encrypt,
 * the ISRG roots pinned in link.cpp, SNI = host). */
typedef struct {
    char     host[64];
    uint16_t port;
    uint8_t  tls;                           /* 1 = WiFiClientSecure */
} door_t;

/* FAST JOIN (the snappy pass, 2026-09-10 evening): the last network
 * joined with its AP's BSSID and channel, so the next radio-on joins
 * it DIRECTLY (WiFi.begin(ssid, psk, channel, bssid): the driver probes
 * that one channel for that one AP instead of scanning thirteen) and
 * scans only when that fails or the network is no longer remembered.
 * Its own NVS key ("fast"), never a field of net_cred_t: a changed
 * shape of the nets blob would drop every network he typed. */
typedef struct {
    char    ssid[STORE_SSID_MAX + 1];       /* "" = none yet */
    uint8_t bssid[6];
    uint8_t channel;                        /* 1..13; 0 = unknown (a plain scan) */
} fast_join_t;

/* THE NEAR DOOR FIRST (the snappy pass): per network, the PLAIN door
 * that last answered `ok hello` on it -- the laptop's LAN door, which
 * proxies the claim to the box over ssh in ~1 s and needs no TLS
 * handshake (4-5 s over the funnel). Joined to that network the link
 * tries it first and the box's door second; elsewhere the list's
 * order as before. Its own NVS key ("near"); a door that fails is
 * skipped as ever, so a stale memory costs one failed connect. */
typedef struct {
    char   ssid[STORE_SSID_MAX + 1];        /* "" = an empty slot */
    int8_t door;                            /* the door's index in doors[] */
} near_door_t;

typedef struct {
    net_cred_t nets[STORE_NETS];
    uint8_t    n_nets;
    door_t     doors[STORE_DOORS];          /* tried in order; door 0 is what `desk <host> <port>` sets */
    uint8_t    n_doors;
    char       key[65];                     /* the link's HMAC key, as typed (hex or any string) */
    int32_t    tz_min;                      /* local offset, minutes east of UTC */
    bool       tz_said;                     /* THE PHONE'S HOUR (2026-10-12, cts.h): NVS has a "tz" key -- the desk's `time` (or his `tz`) said one; while false the bonded phone's zone may fill tz_min (RAM only, never saved: ruling 138) */
    char       ota_pw[33];                  /* "" = the first 8 chars of the key */
    bool       markers;
    char       grain[128];                  /* the cached grain line (96 until 2026-10-09: ONE BODY ACROSS THE HOUSES added `sat=` / `pets=`, the longest line now ~105 B) */
    uint8_t    house;                       /* THE HOUSE's last known state (house.h: 0 unknown, 1 holding, 2 empty) */
    bool       face_off;                    /* THE WATCH FACE switched off (`face off`); default on */
    uint8_t    bom;                         /* HARDWARE ROOM: which add-ons are soldered (bus.h BOM_*; `bom haptic on`) */
    uint8_t    haptic_cal[3];               /* the DRV2605's auto-calibration (comp, bemf, feedback); all 0 = never run */
    uint8_t    ppg_led;                     /* THE PPG's LED (ppg.h PPG_LED_*): 1 IR (the MAX30102 he has, default), 0 green (a 30101), 2 red; NVS "ppgled" */
    bool       ppg_auto_off;                /* THE PULSE IN THE DAY (2026-10-08, ppg.h): OFF = the creature never reads his pulse by itself; on (the default: the sensor is on his wrist) = a 10 s read every 5 min at rest, the day log's hr / hrv and the ring; NVS "ppgauto" (`ppg auto on|off`) */
    uint8_t    ppg_rail;                    /* THE PPG's RAIL (bench 2026-09-13: the adapter's 3V3 and VSYS pins are dead): a header GPIO driven HIGH as the module's VIN, 0 = none (the module on a real rail); NVS "ppgrail" */
    int16_t    alarm_min;                   /* CLOCKWORK: the alarm's minute of the local day, -1 = off (`alarm HH:MM`) */
    bool       beat_worn;                   /* WORN ON THE BEAT (SAGE-WATCH-LINK-SLIME-ACK section 8): the minute's beat / claim carry `worn` / `still`; default ON since 2026-09-17 (SAGE-WEARABLE-CLAW-REPLY section 1: the box arbitrates on it); NVS "bworn" unchanged, so a wrist that set it off keeps off (`beat-worn on|off`) */
    uint32_t   told_night;                  /* THE NIGHT AFTER A REBOOT: the last night the glance told (rest_night_id: its wake's absolute minute), 0 = none -- so a replayed night is not told twice */
    fast_join_t fast;                       /* FAST JOIN: the last AP (net.cpp keeps it at every JOINED) */
    near_door_t near[STORE_NETS];           /* THE NEAR DOOR FIRST: per network, the plain door that answered (link.cpp keeps it) */
    /* THE SETTINGS PAGE (2026-09-10 evening): his own switches, each its own key, saved only when changed */
    bool       lease_hand_off;              /* LEASE off: the hold and the knocks may not claim or release (serial `come` / `release` still may); NVS "leaseoff", default on */
    bool       quiet_off;                   /* QUIET TIME off: a raise in the quiet hours lights the glass as by day (wake.cpp); NVS "quietoff", default on */
    int16_t    alarm_last;                  /* ALARM: the hour remembered when the row turned it off, so the next tap turns it back on; NVS "alarmlst", -1 = none */
    /* MORE SETTINGS (2026-09-10 21:12, Khalid: "just more from the watch"): SETTINGS 3-5's rows, each its own key, saved only when changed */
    bool       wifi_hold;                   /* WIFI HOLD: the radio kept up every lit spell (link_hold(true) at each wake; AUTO = the budget); NVS "wifihold", default off */
    uint8_t    bright;                      /* BRIGHTNESS: the awake backlight's step, 0 LOW / 1 MID / 2 HIGH (main's BL_STEPS; the hour's factor and the night's level apply as ever); NVS "bright", default 1 */
    bool       spell_short;                 /* LIT SPELL: SHORT = the ruled 12 s dim / 20 s sleep; LONG (the default: what is flashed) = the debugging 60 / 90; NVS "spellsht" */
    bool       h12;                         /* CLOCK: 12H (true) / 24H for the face's hour, the glance's and the page's (hourfmt.h); NVS "h12", default 24H */
    int8_t     quiet_from;                  /* QUIET FROM: the quiet hours' start, a whole hour 0-23 (rest.cpp / hours.cpp; the desk's grain `quiet=` overrides while it says); NVS "qfrom", default 23 */
    int8_t     quiet_to;                    /* QUIET TO: their end; NVS "qto", default 7 */
    bool       ble_off;                     /* BLUETOOTH (ruling 58, 09-10 21:50): OFF = the BLE stack never up; on (the default) = up whenever the WiFi radio is down (ble.cpp); NVS "bleoff" */
    uint8_t    ear_gear;                    /* THE EAR'S LOW GEAR (2026-10-09, ear.h): the CPU's MHz while the nap idles awake for THE DARK EAR -- 80 (or 160) = a low gear, 240 = off (THE DEFAULT until the board proves it); NVS "eargear" (`ear gear 80|160|240`) */
    bool       bumps_off;                   /* THE BUMPS (2026-09-23, bumps.h): OFF = the creature never touches him through the motor; on (the default) = the wisp's landing, the sink, the rise, the purr under a pet; NVS "bumpsoff" (`bumps on|off`, SETTINGS 4's BUMPS row) */
    bool       dockhand_off;                /* THE DOCK HANDS HIM OVER (2026-10-09, dockhand.h): OFF = the cable is a charger, a held slime stays through a dock; on (the default) = docking while holding releases him to the desk, undocking worn claims him back; NVS "dockoff" (`dock hand on|off`) */
    /* THE WRIST WARDROBE (2026-09-17): what the body wore and the glass's theme when the power went, so a boot on the bus wears it again (no veil) */
    char       worn[16];                    /* the worn form's id ("-" / "" = the gel); NVS "worn" */
    char       theme[16];                   /* the theme's palette name ("" = blue); NVS "theme" */
    /* THE REAL SUN (2026-09-23, watch-ideas E39): today's sunrise and sunset as local minutes, from the wire's `time`
     * (the desk's / the box's `sunrise=HH:MM sunset=HH:MM`), -1 = unknown = the fixed hours; kept so a cold boot with
     * the RTC's clock lights the glass by the real sun before any link; written only when the pair changes (a minute
     * a day at most). NVS "sunrise" / "sunset" */
    int16_t    sun_rise;
    int16_t    sun_set;
    bool       fs_formatted;                /* THE DISK ASKS BEFORE IT FORMATS (2026-10-12, fsguard.h): this firmware has formatted the LittleFS partition once, or found it mounted; NVS "fsfmt"; set = a failed mount is NEVER formatted by itself again (`day format` is the one hand) */
} store_t;

extern store_t store;

void store_begin(void);                     /* loads everything (defaults when blank) */
bool store_net_add(const char *ssid, uint8_t kind, const char *id,
                   const char *anon, const char *pw);   /* replaces a same-ssid entry; false = full */
bool store_net_forget(const char *ssid);
const net_cred_t *store_net_find(const char *ssid);
void store_save_nets(void);
/* the doors: `desk add` appends (false = full or malformed), `desk rm`
 * drops one, `desk <host> <port>` sets door 0 plain. NVS blob "doors";
 * the old single "host"/"port" pair migrates into door 0 on the first
 * boot that finds no blob, and door 0 keeps writing that pair (a
 * firmware flashed back still finds its desk). */
bool store_door_add(const char *host, uint16_t port, bool tls);
bool store_door_rm(int n);
void store_door_set0(const char *host, uint16_t port);
void store_save_doors(void);
void store_save_key(void);
void store_save_tz(void);
void store_save_ota(void);
void store_save_markers(void);
void store_save_grain(void);
void store_save_house(void);
void store_save_face(void);
void store_save_bom(void);
void store_save_haptic_cal(void);
void store_save_ppg_led(void);
void store_save_ppg_rail(void);
void store_save_ppg_auto(void);             /* THE PULSE IN THE DAY's switch */
void store_save_alarm(void);
void store_save_beat_worn(void);
void store_save_fast(void);                 /* FAST JOIN: the last AP's ssid / bssid / channel (NVS "fast") */
/* THE NEAR DOOR FIRST: the plain door remembered for a network (-1 = none); set = remember (saved when changed) */
int  store_near_door(const char *ssid);
void store_near_set(const char *ssid, int door);
void store_save_told_night(void);
/* THE SETTINGS PAGE's keys */
void store_save_lease_hand(void);
void store_save_quiet(void);
void store_save_alarm_last(void);
/* MORE SETTINGS' keys */
void store_save_wifi_hold(void);
void store_save_bright(void);
void store_save_spell(void);
void store_save_h12(void);
void store_save_quiet_hours(void);          /* qfrom + qto together */
void store_save_sun(void);                  /* THE REAL SUN: sun_rise + sun_set together */
void store_save_ble(void);                  /* BLUETOOTH's switch */
void store_save_bumps(void);                /* THE BUMPS' switch */
void store_save_ear_gear(void);             /* THE EAR'S LOW GEAR: the MHz */
void store_save_dockhand(void);             /* THE DOCK HANDS HIM OVER's switch */
void store_save_worn(void);                 /* THE WRIST WARDROBE: the worn form's id */
void store_save_theme(void);                /* ... and the theme's palette name */
void store_save_fs_formatted(void);         /* THE DISK ASKS BEFORE IT FORMATS: the once */
/* the clock, kept coarse across a soft reset: the epoch every few minutes */
void store_save_clock(uint32_t epoch);
uint32_t store_load_clock(void);
