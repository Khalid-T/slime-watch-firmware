#include "console.h"
#include "lastdeath.h"          /* THE LAST DEATH: the verb about to run, into the breadcrumb */
#include "app.h"
#include "store.h"
#include "net.h"
#include "link.h"
#include "daylog.h"
#include "glance.h"
#include "word.h"
#include "question.h"
#include "gesture.h"
#include "rest.h"
#include "wake.h"
#include "house.h"
#include "face.h"
#include "hours.h"
#include "idle.h"
#include "cramp.h"              /* THE SMALL HOUSE: `cramp` */
#include "tell.h"
#include "grain.h"
#include "bus.h"
#include "sky.h"
#include "vigor.h"
#include "clockwork.h"
#include "wisp.h"
#include "page.h"
#include "play.h"       /* PLAY: `play` -- the four games' tallies, what is on, the last refusal */
#include "doctor.h"
#include "ancs.h"
#include "ble.h"
#include "ear.h"        /* THE EAR'S LEDGER AND ITS LOW GEAR: `ear`, `ear gear 80|160|240` */
#include "wardrobe.h"   /* THE WRIST WARDROBE: list / wear / act / theme / check */
#include "artota.h"     /* ART OVER THE AIR: `wardrobe art [drop]` -- the push's state, a push dropped by hand */
#include "theme.h"
#include "nightsum.h"   /* THE NIGHT RING KNOWS HIS HEART: `night` */
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

static char line[300];
static size_t n = 0;

/* in place: unquoted tokens split on blanks; "quoted ones" keep their
 * blanks and a \" inside them is a literal quote */
static int tokenize(char *s, char **argv, int max)
{
    int argc = 0;
    char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || argc >= max) break;
        char *out = p;
        argv[argc++] = out;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1] == '"') { *out++ = '"'; p += 2; }
                else *out++ = *p++;
            }
            if (*p == '"') p++;
            *out = 0;
        } else {
            while (*p && *p != ' ' && *p != '\t') *out++ = *p++;
            if (*p) p++;
            *out = 0;
        }
    }
    return argc;
}

static void help(void)
{
    Serial.println(
        "slime watch -- commands (quote arguments with spaces: wifi \"my net\" \"pa ss\"; \\\" is a quote)\n"
        "  help | status | link | pet | glance | rest | house | reboot\n"
        "  link now | link hold | link auto          THE RADIO BUDGET: a dial now (a test aid); the radio kept up this lit spell (OTA, the page); back on the budget\n"
        "  doctor                                    ONE health report (build, heap, fps, house, link, clock, day log, night, bom, VERDICT); also GET /doctor\n"
        "  wifi <ssid> [<psk>]                       remember a WPA2-PSK (or open) network\n"
        "  wifi-eap <ssid> <identity> <password> [<anonymous identity>]   WPA2-Enterprise PEAP/MSCHAPv2\n"
        "  wifi-list | wifi-forget <ssid> | wifi-scan | wifi-join\n"
        "  desk <host> <port>                        door 0, plain (the laptop's LAN door, WATCH_PORT 8891)\n"
        "  desk add <host> <port> [tls] | desk list | desk rm <n>\n"
        "                                            the doors, tried in order (up to 4; tls = the box behind the funnel)\n"
        "  key <hex>                                 the link's HMAC key (the desk's WATCH_KEY)\n"
        "  ota <password>                            OTA password (default: the key's first 8)\n"
        "  tz <minutes east of utc>                  the local offset until the desk says (-240)\n"
        "  clock <epoch>                             set the hour by hand (the desk / NTP do it)\n"
        "  day show|ls|dump|close|rm|prune|format [YYYYMMDD] the day log (prune = DISK KEEPING by hand; ls shows the free space; format = THE DISK ASKS BEFORE IT FORMATS' one hand: wipes a disk that will not mount, refused on one that does)\n"
        "  come | release | sync | undock            the lease's toggle (by the house: a release sinks at once), a day sync now, end the dock\n"
        "  dock | dock hand on|off                   THE DOCK HANDS HIM OVER (dockhand.h): docking while holding = `release docked` (the desk claims at once), undocking worn = the claim back; the switch (NVS); bare = its state\n"
        "  page | page on|off|week|notices|ble|settings|about|next   THE WATCH PAGE: his tool page (the hour, date, steps, cell, alarm, sky, link, a running count) for 8 s -- a drag up the glass does it; again turns it: his week, the notices, bluetooth (T16: phone / air door / ear / pairing / notices in words), settings 1/6 .. 6/6, about\n"
        "  set | set <row> [on|off|<n>]               THE SETTINGS PAGE's rows by hand (bare = what a tap on the row does; NVS): slime (come / release), lease, quiet, face | lock, link, alarm | wifi (HOLD / AUTO), network, address, bt | bright (0-2), spell (SHORT / LONG), lamp, bumps | clock (12H / 24H), qfrom, qto (0-23) | about | pauto, dockhand, eargear (80 / 160 / 240; a tap cycles), forget (two taps on the glass), pairing, ble (SETTINGS 6 + THE BLUETOOTH PAGE, T15/T16)\n"
        "  play                                      PLAY (play.cpp): armed / the bead game / the games' tallies / a drop owed or falling / the wet mark / the course's gates and which are lit / the last refusal -- never on the glass\n"
        "  bumps [on|off]                            THE BUMPS (bumps.h): the creature's touches through the motor -- the wisp's landing, the sink, the rise, the purr under a pet; the switch (NVS); bare = the tallies and the gate's state\n"
        "  beat-worn [on|off]                        WORN ON THE BEAT: the minute's beat / claim carry `worn` / `still` (NVS; on since 09-17: the box arbitrates on it; the shelf never claims)\n"
        "  tls-mem [psram|internal]                  where mbedtls's TLS sessions live from the next dial (the optimization pass: PSRAM by default)\n"
        "  house | house holding|empty               THE HOUSE: the state, its cause and age; force it (a test aid: the arrival, the departure)\n"
        "  face | face on|off                        THE WATCH FACE: what is shown and the numbers (steps/norm, night, batt, the trace); the switch (NVS)\n"
        "  wardrobe [status|list|wear <id>|off|act|roll|theme [<pal>]|check|art [drop]]   THE WRIST WARDROBE: the baked forms; a costume through the veil (off = the gel); its act; the theme = the palette (NVS); the crc check; ART OVER THE AIR's push (its state; drop = abandon it)\n"
        "  hours | idle                              THE HOURS' LIGHT: the phase, the tint, the backlight; IDLE LIFE: the last thing and when\n"
        "  cramp                                     THE SMALL HOUSE: the cramp's level and clock, the last nudge / bid and its answer, why it is held back now\n"
        "  grain                                     THE GRAIN: the seed, regard, mood, night and every knob derived from them\n"
        "  tell                                      THE TELL: what the glance would answer now, what it has said, the walk's bob, the night light\n"
        "  night                                     THE NIGHT RING KNOWS HIS HEART: the week's nights off the files, the last one's dip, the midpoints' wander, today's resting heart vs the week's\n"
        "  sky | sky <word> [<word>] | sky none      THE SKY: the held word and its age; set it by hand (rain snow cold hot clear storm wind dark)\n"
        "  question                                  THE WORD'S QUESTION: the last word ending in ?, its window (10 s from the show), his vote (a pet = wrist:yes, the tap on it = wrist:no) or silence\n"
        "  vigor                                     VIGOR: the cell's level as the body's vigor -- the number, its source (gauge / ADC / the cable) and what it is doing\n"
        "  lock | unlock                             the water lock: the glass ignores touch (a double shake or a 3-s hold frees it; never NVS)\n"
        "  knock lost|free [<stamp> <mac>]           the desk's knock by hand (the dock's line; signed when a key is here): `ok <house>` / `no <why>`\n"
        "  clockwork | timer <min>|off | alarm HH:MM|off   CLOCKWORK: the state; a timer by hand (the gel bounces at zero); the alarm (NVS; the one thing that wakes him)\n"
        "  markers on|off (or d) | 0..7              the orientation markers, the IMU axis map\n"
        "  wake | wake-cal | wake-set d f [d f [q]] | wom-test    the wake: cause+tallies; resting pose; set raise deg/face (lower deg/face, QUIET TIME's face) live; the engine\n"
        "  bom | bom <haptic|gauge|rtc|ppg|button|motor> on|off   HARDWARE ROOM: which add-ons are soldered (NVS); the dark drivers wake\n"
        "  bus                                       scan both I2C buses: what answers, by name\n"
        "  haptic bump|purr|hop | haptic cal         a gesture (the DRV2605, or the bare motor on GPIO 33 when `bom motor on`); the LRA's auto-calibration (once, stored)\n"
        "  haptic buzz [<ms> [<pct> [<hz>|0]]] | haptic off   THE BARE MOTOR's bench test: one flat pulse (2 s cap) at a PWM frequency (0 = raw DC); stop it\n"
        "  haptic hold [<s>]                         the multimeter's test: GPIO 33 plain HIGH for s seconds (default 20, cap 60; the nap ends it)\n"
        "  gauge | rtc | rtc sync | ppg | ppg spot [secs] | ppg led ir|green|red | ppg auto on|off   the MAX17048 mV/%; the PCF85063's time / written from the clock; the MAX3010x's last spot + the ring of its own reads / a spot read (10 s; 5..30 -- `set pulse` is the row's 30; the LEDs wait for a still wrist) / which LED (ir = the 30102) / THE PULSE IN THE DAY's switch (a 10 s read every 5 min at rest, NVS)\n"
        "  wisp | wisp <app> [<title>]               THE WISP: what hovers (its ink: desk / phone / box), what waits (for the light, or at the air door while EMPTY), the faded, the eaten; one by hand, answered as the desk's ask would be (ok / ok door; `wisp jarvas <title>` = the box's voice)"
        "\n  ble | ble on|off | ble pair [secs] | ble forget   BLUETOOTH (ruling 58: BLE only while the WiFi radio is down): the state (the stack, the phone, the air door, the heap); the switch (NVS; = set bluetooth on|off); THE PAIRING WINDOW (60 s: a phone or the desk may bond); every bond dropped"
        "\n  ancs | ancs fake [<app> [<title> [<msg>]]] | ancs clear [<app>]   iPhone notices: the client's state; a wisp with no phone (ancs forget = ble forget); the newest notice (or that app's) dismissed on the phone -- the negative action, what a hold on an open notice card does"
        "\n  ear | ear gear 80|160|240                 THE EAR'S LEDGER: deep / awake-ear minutes today and since boot, the cell's mV/h by nap kind (the last hour, 20 min to speak), the gear; THE LOW GEAR: 80 (or 160) MHz while the ear naps (NVS; 240 = off, the default: unproven on the board)"
        );
}

static void status(void)
{
    char nl[160], ll[900];         /* THE RADIO BUDGET: the link line carries the radio's (and the box's status line since 09-17) */
    net_status(nl, sizeof nl);
    link_status(ll, sizeof ll);
    struct tm lt; uint32_t ep;
    clock_local(&lt, &ep);
    daylog_sum_t s; bool have = clock_local_date() && wrist_sum(clock_local_date(), &s);
    Serial.printf("[%8lu] status: %s house=%s fps %.1f wall %d phi %.0f | steps %lu worn %s logged %d min unlogged %d | "
                  "clock %s%04d-%02d-%02d %02d:%02d:%02d tz %+d | %s | %s | batt %u mV%s | up %lu s heap %u free\n",
                  (unsigned long)millis(), APP_STATE_NAMES[app.state], house_state_name(), app.fps, app.wall, app.phi_deg,
                  (unsigned long)wrist_steps_today(), wrist_worn() ? "yes" : "no", have ? s.minutes : 0,
                  wrist_unlogged_min(), clock_is_set() ? "" : "UNSET ", lt.tm_year + 1900, lt.tm_mon + 1,
                  lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, (int)store.tz_min, nl, ll, batt_mv(),
                  charging() ? " charging" : "", (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap());
}

static void wifi_list(void)
{
    Serial.printf("wifi: %u of %d remembered\n", store.n_nets, STORE_NETS);
    for (int i = 0; i < store.n_nets; i++) {
        const net_cred_t *c = &store.nets[i];
        Serial.printf("  %-32s %s\n", c->ssid, c->kind == NET_EAP ? "wpa2-enterprise" : c->kind == NET_PSK ? "psk" : "open");
    }
}

static void run(int argc, char **argv)
{
    const char *v = argv[0];
    death_verb(v);                              /* THE LAST DEATH: the breadcrumb's verb, before it runs */
    if (!strcmp(v, "help") || !strcmp(v, "?")) help();
    else if (!strcmp(v, "status")) status();
    else if (!strcmp(v, "doctor")) doctor_print();   /* DOCTOR: the morning's one report */
    else if (!strcmp(v, "link")) {
        /* THE RADIO BUDGET: `link now` forces a dial (a test aid), `link
         * hold` keeps the radio up this lit spell (OTA, the page), `link
         * auto` puts it back on the budget; bare, the status line */
        if (argc > 1 && !strcmp(argv[1], "now")) link_now("serial");
        else if (argc > 1 && !strcmp(argv[1], "hold")) link_hold(true);
        else if (argc > 1 && !strcmp(argv[1], "auto")) link_hold(false);
        else { char l[900]; link_status(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "pet")) app_pet();
    else if (!strcmp(v, "glance")) { glance_show(); word_glance(); }   /* a glance is a glance: a held word shows on it too (as on a raise) */
    else if (!strcmp(v, "rest")) { char l[320]; rest_status(l, sizeof l); Serial.println(l); }   /* (320: THE PULSE AS THE BREATH added the pulse's clause) */
    else if (!strcmp(v, "house")) {
        if (argc > 1 && !strcmp(argv[1], "holding")) house_force(HOUSE_HOLDING, "serial");
        else if (argc > 1 && !strcmp(argv[1], "empty")) house_force(HOUSE_EMPTY, "serial");
        else if (argc > 1) Serial.println("house [holding|empty]");
        else { char l[160]; house_status(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "wardrobe")) {
        /* THE WRIST WARDROBE: the baked forms, a wear by hand (the veil),
         * the act, the theme; status; the blobs' crc against the table */
        const char *a = argc > 1 ? argv[1] : "status";
        if (!strcmp(a, "list")) wardrobe_list();
        else if (!strcmp(a, "wear") && argc > 2) { if (!wardrobe_wear(argv[2], "serial")) Serial.printf("wardrobe: no art '%s' (wardrobe list)\n", argv[2]); }
        else if (!strcmp(a, "off") || !strcmp(a, "home")) wardrobe_wear("-", "serial");
        else if (!strcmp(a, "act")) { if (!wardrobe_act()) Serial.println("wardrobe: no act (nothing worn, none baked, or playing)"); }
        else if (!strcmp(a, "roll")) { wardrobe_roll_now(); Serial.println("wardrobe: the bored urge now -- the roll lands on the next lit, holding, bare frame (THE WRIST'S OWN MORPHS)"); }
        else if (!strcmp(a, "theme")) {
            if (argc > 2) { if (!wardrobe_theme(argv[2], "serial")) Serial.println("wardrobe: no such palette (wardrobe theme lists them)"); }
            else { Serial.printf("wardrobe: theme %s; the palettes:", theme_name()); for (int i = 0; i < theme_count(); i++) Serial.printf(" %s%s", theme_name_at(i), theme_reserved_at(i) ? "*" : ""); Serial.println(" (* = the dark band)"); }
        }
        else if (!strcmp(a, "check")) { char l[200]; wardrobe_check(l, sizeof l); Serial.println(l); }
        else if (!strcmp(a, "status")) { char l[320]; wardrobe_status(l, sizeof l); Serial.println(l); char t[160]; artota_status(t, sizeof t); Serial.println(t); }
        else if (!strcmp(a, "art")) {
            /* ART OVER THE AIR: the push's state (`wardrobe art`), or a push
             * dropped by hand (`wardrobe art drop`: the image is gone if a
             * sector was erased) */
            if (argc > 2 && !strcmp(argv[2], "drop")) { if (artota_active()) artota_abandon("dropped over serial"); else Serial.println("art: no push open"); }
            char t[160]; artota_status(t, sizeof t); Serial.println(t);
        }
        else Serial.println("wardrobe [status|list|wear <id>|off|act|roll|theme [<pal>]|check|art [drop]]");
    }
    else if (!strcmp(v, "face")) {
        if (argc > 1 && (!strcmp(argv[1], "on") || !strcmp(argv[1], "off"))) {
            bool on = argv[1][1] == 'n';
            face_set_on(on); store.face_off = !on; store_save_face();
            LOG("face: %s (stored)", on ? "on -- the rings, the drop, the hour when empty" : "off -- the black glass");
        } else if (argc > 1) Serial.println("face [on|off]");
        else { char l[320]; face_status(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "hours")) {
        if (argc >= 2 && !strcmp(argv[1], "sun")) {   /* THE REAL SUN, a test aid: `hours sun HH:MM HH:MM` / `hours sun off` (RAM only; the next `time` on the wire has its say) */
            int rh = 0, rm = 0, sh = 0, sm = 0;
            if (argc >= 4 && sscanf(argv[2], "%d:%d", &rh, &rm) == 2 && sscanf(argv[3], "%d:%d", &sh, &sm) == 2) {
                if (hours_sun(rh * 60 + rm, sh * 60 + sm, false)) LOG("hours: the sun %02d:%02d-%02d:%02d (by hand; eased over a minute, not stored)", rh, rm, sh, sm);
                else Serial.println("hours: refused -- HH:MM within the day, the rise before the set");
            } else if (argc == 3 && !strcmp(argv[2], "off")) { hours_sun(-1, -1, false); LOG("hours: the fixed hours (by hand; 06:30 / 18:30)"); }
            else Serial.println("hours sun HH:MM HH:MM | hours sun off");
        } else { char l[360]; hours_status(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "idle")) { char l[240]; idle_status(l, sizeof l); Serial.println(l); }
    else if (!strcmp(v, "cramp")) { char l[400]; cramp_status(l, sizeof l); Serial.println(l); }   /* THE SMALL HOUSE */
    else if (!strcmp(v, "tell")) { char l[400]; tell_status(l, sizeof l); Serial.println(l); }
    else if (!strcmp(v, "night")) { char l[400]; nightsum_status(l, sizeof l); Serial.println(l); }   /* THE NIGHT RING KNOWS HIS HEART (nightsum.c): the week's nights, the dip, the wander, the weather */
    else if (!strcmp(v, "grain")) { char l[480]; grain_status(l, sizeof l); Serial.println(l); }   /* THE GRAIN: the seed, regard, mood and every derived knob */
    else if (!strcmp(v, "sky")) {
        if (argc > 1) {                          /* a test aid: the words by hand, held the hour like the box's */
            char w[48]; snprintf(w, sizeof w, "%s%s%s", argv[1], argc > 2 ? " " : "", argc > 2 ? argv[2] : "");
            if (sky_set(w, "by hand")) LOG("sky: %s (by hand%s)", sky_known() ? w : "forgotten", sky_known() ? "; held an hour" : "");
            else Serial.println("sky: not a word the gel knows -- rain snow cold hot clear storm wind dark, or none");
        } else { char l[240]; sky_status(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "question")) { char l[200]; question_status(l, sizeof l, millis()); Serial.println(l); }   /* THE WORD'S QUESTION: the last question, its answer, the window */
    else if (!strcmp(v, "vigor")) { char l[300]; vigor_status(l, sizeof l); Serial.println(l);     /* VIGOR: the number, its source, what it moves */
                                    if (spent_line()) Serial.println("spent: the radio waits for the cable"); }   /* THE LAST DROP'S MANNERS (spent.h) */
    else if (!strcmp(v, "reboot")) { wrist_flush(); Serial.println("rebooting"); Serial.flush(); ESP.restart(); }   /* the minute in progress filed first (the OTA path always did; this never) */
    else if (!strcmp(v, "wifi")) {
        if (argc < 2) { Serial.println("wifi <ssid> [<psk>]"); return; }
        const char *psk = argc > 2 ? argv[2] : "";
        if (psk[0] && strlen(psk) < 8) { Serial.println("wifi: a WPA2 psk is 8 to 63 characters"); return; }
        bool ok = store_net_add(argv[1], psk[0] ? NET_PSK : NET_OPEN, "", "", psk);
        Serial.printf("wifi: %s %s (%s)\n", ok ? "remembered" : "FULL, not remembered", argv[1], psk[0] ? "psk" : "open");
        if (ok) net_rejoin();
    }
    else if (!strcmp(v, "wifi-eap")) {
        if (argc < 4) { Serial.println("wifi-eap <ssid> <identity> <password> [<anonymous identity>]"); return; }
        bool ok = store_net_add(argv[1], NET_EAP, argv[2], argc > 4 ? argv[4] : "", argv[3]);
        Serial.printf("wifi: %s %s (wpa2-enterprise peap, identity %s%s)\n", ok ? "remembered" : "FULL, not remembered",
                      argv[1], argv[2], argc > 4 ? ", anonymous identity given" : "");
        if (ok) net_rejoin();
    }
    else if (!strcmp(v, "wifi-list")) wifi_list();
    else if (!strcmp(v, "wifi-forget")) {
        if (argc < 2) { Serial.println("wifi-forget <ssid>"); return; }
        Serial.printf("wifi: %s %s\n", store_net_forget(argv[1]) ? "forgot" : "did not know", argv[1]);
    }
    else if (!strcmp(v, "wifi-scan")) net_scan();
    else if (!strcmp(v, "wifi-join")) net_rejoin();
    else if (!strcmp(v, "desk")) {
        if (argc < 2 || !strcmp(argv[1], "list")) {
            if (!store.n_doors) { Serial.println("desk: no doors (`desk add <host> <port> [tls]`)"); return; }
            if (argc < 2) { Serial.printf("desk %s %u%s (door 0 of %u; `desk list`)\n", store.doors[0].host, store.doors[0].port, store.doors[0].tls ? " tls" : "", store.n_doors); return; }
            Serial.printf("desk: %u of %d doors, tried in order\n", store.n_doors, STORE_DOORS);
            for (int i = 0; i < store.n_doors; i++) { char l[160]; link_door_line(i, l, sizeof l); Serial.println(l); }
            return;
        }
        if (!strcmp(argv[1], "add")) {
            if (argc < 4) { Serial.println("desk add <host> <port> [tls]"); return; }
            bool tls = argc > 4 && !strcmp(argv[4], "tls");
            uint16_t port = (uint16_t)atoi(argv[3]);
            bool ok = store_door_add(argv[2], port, tls);
            Serial.printf("desk: %s %s:%u %s%s\n", ok ? "added door" : "NOT added", argv[2], port, tls ? "tls" : "plain",
                          ok ? " (stored; takes effect at the next dial)" : store.n_doors >= STORE_DOORS ? " -- full (4)" : " -- a host and a port, please");
            return;
        }
        if (!strcmp(argv[1], "rm")) {
            if (argc < 3) { Serial.println("desk rm <n>"); return; }
            int n = atoi(argv[2]);
            Serial.printf("desk: door %d %s\n", n, store_door_rm(n) ? "removed" : "is not there");
            return;
        }
        /* `desk <host> <port>`: door 0, plain, as it always was */
        uint16_t port = argc > 2 ? (uint16_t)atoi(argv[2]) : (store.n_doors ? store.doors[0].port : 8891);
        if (!port) port = 8891;
        store_door_set0(argv[1], port);
        Serial.printf("desk: door 0 %s:%u plain (stored)\n", store.doors[0].host, store.doors[0].port);
    }
    else if (!strcmp(v, "key")) {
        if (argc < 2) { Serial.printf("key: %s\n", store.key[0] ? "set" : "unset"); return; }
        strncpy(store.key, argv[1], sizeof store.key - 1);
        store_save_key();
        Serial.printf("key: stored (%u chars)\n", (unsigned)strlen(store.key));
    }
    else if (!strcmp(v, "ota")) {
        if (argc < 2) { Serial.printf("ota: password %s\n", store.ota_pw[0] ? "set" : "the key's first 8"); return; }
        strncpy(store.ota_pw, argv[1], sizeof store.ota_pw - 1);
        store_save_ota();
        Serial.println("ota: password stored (takes effect at the next join)");
    }
    else if (!strcmp(v, "tz")) {
        if (argc < 2) { Serial.printf("tz %+d\n", (int)store.tz_min); return; }
        store.tz_min = atoi(argv[1]);
        store_save_tz();
        Serial.printf("tz: %+d min (stored)\n", (int)store.tz_min);
    }
    else if (!strcmp(v, "clock")) {
        if (argc < 2) { struct tm lt; uint32_t ep; clock_local(&lt, &ep); Serial.printf("clock: %s%04d-%02d-%02d %02d:%02d:%02d (epoch %lu)\n", clock_is_set() ? "" : "UNSET ", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, (unsigned long)ep); return; }
        clock_set_epoch((uint32_t)strtoul(argv[1], NULL, 10), "serial");
    }
    else if (!strcmp(v, "day")) wrist_cmd(argc, argv);
    else if (!strcmp(v, "come") || !strcmp(v, "release")) app_lease_toggle("serial");   /* THE LEASE, EASY: the toggle by the house, with the wrist's own feedback (a `release` sinks at once) */
    else if (!strcmp(v, "page")) {            /* THE WATCH PAGE: shows / hides it (or `page on|off`; `page week|notices|next` turns it); logged on the next tick */
        bool want = argc > 1 ? strcmp(argv[1], "off") != 0 : !page_up();
        if (!want) page_hide("serial");
        else if (argc > 1 && !strcmp(argv[1], "week")) page_show_which(PAGE_WEEK, "serial");
        else if (argc > 1 && !strcmp(argv[1], "notices")) page_show_which(PAGE_NOTICES_PAGE, "serial");
        else if (argc > 1 && !strcmp(argv[1], "settings")) page_show_which(PAGE_SETTINGS, "serial");   /* THE SETTINGS PAGE: 1/6 (a drag up or `page next` turns on: 2/6 .. 6/6, about) */
        else if (argc > 1 && !strcmp(argv[1], "about")) page_show_which(PAGE_ABOUT, "serial");         /* MORE SETTINGS: the last page */
        else if (argc > 1 && !strcmp(argv[1], "ble")) page_show_which(PAGE_BLE, "serial");             /* THE BLUETOOTH PAGE (T16): after the notices */
        else if (argc > 1 && !strcmp(argv[1], "next")) page_next("serial");
        else page_show("serial");
        if (want) app_keep_awake();
        char l[120]; page_status(l, sizeof l); Serial.println(l);
    }
    else if (!strcmp(v, "set")) {              /* THE SETTINGS PAGE's rows by hand: `set lease on`, `set alarm off`, `set link` (the dial now); bare, every row's state */
        if (argc < 2) {
            Serial.printf("set: slime %s (a tap = %s) | lease %s | quiet %s | face %s | lock %s | link (a dial now) | alarm %s | "
                          "wifi %s | network %s | address %s | bluetooth %s | bright %s | spell %s | lamp (an action) | bumps %s | clock %s | qfrom %02d | qto %02d | about | "
                          "pauto %s | dockhand %s | eargear %s | forget (a tap: `ble forget`) | pairing (a tap: the window) | ble (the page's words)\n",
                          house_holding() ? "HERE" : "AWAY", house_holding() ? "release" : "come",
                          store.lease_hand_off ? "off" : "on", wake_quiet_on() ? "on" : "off", face_on() ? "on" : "off",
                          gesture_locked() ? "on" : "off", clockwork_alarm() < 0 ? "off" : "on",
                          link_holding_radio() ? "HOLD" : "auto", net_ssid()[0] ? net_ssid() : (net_radio_is_on() ? "joining" : "off"), net_ip()[0] ? net_ip() : "--", ble_word(),
                          store.bright == 0 ? "LOW" : store.bright == 1 ? "MID" : "HIGH", store.spell_short ? "SHORT" : "LONG", store.bumps_off ? "off" : "on", store.h12 ? "12H" : "24H",
                          (int)store.quiet_from, (int)store.quiet_to,
                          store.ppg_auto_off ? "off" : "on", store.dockhand_off ? "off" : "on", store.ear_gear == 80 ? "LOW" : store.ear_gear == 160 ? "MID" : "FULL");   /* SETTINGS 6 (T15) */
            return;
        }
        int row = page_setting_by_name(argv[1]);
        /* bare `set <row>` = the tap (a toggle, a cycle, an action); `on` / `off` for a switch; a number for a step (bright 0-2) or an hour (qfrom / qto 0-23) */
        int want = -1;
        if (argc > 2) {
            if (!strcmp(argv[2], "on")) want = 1;
            else if (!strcmp(argv[2], "off")) want = 0;
            else if (!strcmp(argv[2], "pair")) want = 2;                /* BLUETOOTH's third state: THE PAIRING WINDOW */
            else if (argv[2][0] >= '0' && argv[2][0] <= '9') want = atoi(argv[2]);
            else want = -2;
        }
        if (row < 0 || want == -2) { Serial.println("set slime|lease|quiet|face|lock|link|alarm|wifi|network|address|bluetooth|bright|spell|lamp|bumps|clock|qfrom|qto|about|pauto|dockhand|eargear|forget|pairing|ble [on|off|<n>]   (bare = what a tap on the row does)"); return; }
        Serial.println(app_setting(row, want, "serial"));
    }
    else if (!strcmp(v, "sync")) { link_sync_now(); Serial.println("sync: asked"); }
    else if (!strcmp(v, "play")) { char l[400]; play_status(l, sizeof l); Serial.println(l); }   /* PLAY: THE BEAD, THE SPIN, THE DROP AND THE COURSE -- the state, one line */
    else if (!strcmp(v, "bumps")) {              /* THE BUMPS (bumps.h): the switch (the settings row's own road, NVS) or the tallies */
        if (argc > 1 && (!strcmp(argv[1], "on") || !strcmp(argv[1], "off"))) Serial.println(app_setting(SET_BUMPS, argv[1][1] == 'n' ? 1 : 0, "serial"));
        else if (argc > 1) Serial.println("bumps [on|off]");
        else { char l[480]; app_bumps_status(l, sizeof l); Serial.println(l); }   /* (480: THE HEARTBEAT HANDED BACK added the pulse's clause) */
    }
    else if (!strcmp(v, "beat-worn")) {          /* WORN ON THE BEAT (link.cpp): the flag, logged and stored */
        if (argc > 1 && (!strcmp(argv[1], "on") || !strcmp(argv[1], "off"))) link_beat_worn(argv[1][1] == 'n');
        else if (argc > 1) Serial.println("beat-worn [on|off]");
        else Serial.printf("beat-worn: %s (the house's worn test says %s now)\n", store.beat_worn ? "on" : "off", house_worn() ? "worn" : "still");
    }
    else if (!strcmp(v, "undock")) link_undock("undock");
    else if (!strcmp(v, "dock")) {               /* THE DOCK HANDS HIM OVER (dockhand.h): the switch (NVS `dockoff`) or its state */
        if (argc > 2 && !strcmp(argv[1], "hand") && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) link_dockhand_set(argv[2][1] == 'n');
        else if (argc > 1) Serial.println("dock [hand on|off]");
        else { char l[240]; link_dockhand_line(l, sizeof l); Serial.println(l); }
    }
    else if (!strcmp(v, "tls-mem")) {          /* THE TLS SESSION IN PSRAM (link.cpp): the escape hatch for the board */
        if (argc > 1) link_tls_mem(!strcmp(argv[1], "psram"));
        else Serial.printf("tls-mem: %s (tls-mem psram|internal)\n", link_tls_psram() ? "psram" : "internal");
    }
    else if (!strcmp(v, "lock") || !strcmp(v, "unlock")) gesture_set_lock(v[0] == 'l');   /* logged when it lands, next tick */
    else if (!strcmp(v, "clockwork")) { char l[240]; clockwork_status(l, sizeof l); Serial.println(l); }
    else if (!strcmp(v, "timer")) {           /* CLOCKWORK: a timer by hand, minutes (a fraction is fine: 0.5); logged on the next tick */
        if (argc < 2) { char l[240]; clockwork_status(l, sizeof l); Serial.println(l); return; }
        float min = !strcmp(argv[1], "off") ? 0 : (float)atof(argv[1]);
        if (min < 0 || min > 1440) { Serial.println("timer <minutes 0..1440> | off"); return; }
        clockwork_set_timer((uint32_t)(min * 60000.0f + 0.5f), millis(), "serial");
    }
    else if (!strcmp(v, "alarm")) {           /* CLOCKWORK: `alarm HH:MM` (local) / `alarm off`, kept in NVS */
        if (argc < 2) { int m = clockwork_alarm(); if (m < 0) Serial.println("alarm: off"); else Serial.printf("alarm: %02d:%02d\n", m / 60, m % 60); return; }
        int hh = -1, mm = -1;
        if (!strcmp(argv[1], "off")) store.alarm_min = -1;
        else if (sscanf(argv[1], "%d:%d", &hh, &mm) == 2 && hh >= 0 && hh < 24 && mm >= 0 && mm < 60) store.alarm_min = (int16_t)(hh * 60 + mm);
        else { Serial.println("alarm HH:MM | off"); return; }
        store_save_alarm();
        clockwork_set_alarm(store.alarm_min, "serial, stored");
    }
    else if (!strcmp(v, "wake") || !strcmp(v, "wake-cal") || !strcmp(v, "wake-set") || !strcmp(v, "wake-engine") || !strcmp(v, "wom-test")) wake_cmd(argc, argv);
    else if (!strcmp(v, "ble")) {             /* BLUETOOTH: the state (the gate's word, the stack, the tenants, the heap), the switch, THE PAIRING WINDOW, the bonds */
        if (argc > 1 && (!strcmp(argv[1], "on") || !strcmp(argv[1], "off"))) Serial.println(app_setting(SET_BT, argv[1][1] == 'n', "serial"));
        else if (argc > 1 && !strcmp(argv[1], "pair")) {
            int left = ble_pair_open(argc > 2 ? (uint32_t)atoi(argv[2]) : 0, "serial");
            if (left > 0) Serial.printf("ble: pairing open for %d s -- Settings > Bluetooth > slime-watch > Pair on the phone, or `slimectl watch ble pair` on the desk\n", left);
            else Serial.println("ble: the BLUETOOTH row is OFF -- `ble on` first");
        }
        else if (argc > 1 && !strcmp(argv[1], "forget")) ble_forget();
        else if (argc > 1) Serial.println("ble [on|off|pair [secs]|forget]");
        else { char a[600]; ble_status(a, sizeof a); Serial.println(a); }
    }
    else if (!strcmp(v, "ancs")) {            /* the iPhone's notices: the client's state, a wisp by the phone's path (with a message), the bonds dropped (= ble forget) */
        if (argc > 1 && !strcmp(argv[1], "fake")) ancs_fake(argc > 2 ? argv[2] : "", argc > 3 ? argv[3] : nullptr, argc > 4 ? argv[4] : nullptr);
        else if (argc > 1 && !strcmp(argv[1], "forget")) ble_forget();
        else if (argc > 1 && !strcmp(argv[1], "clear")) {   /* T17: the page's hold by hand -- the newest ledger line's app, or the one named; the lines say what came of it */
            const wisp_note_t *lg = nullptr; int n = wisp_ledger(&lg);
            const char *app = argc > 2 ? argv[2] : n ? lg[0].app : "";
            if (!app[0]) Serial.println("ancs: the ledger is empty -- nothing to clear");
            else if (!ancs_clear(app)) Serial.println("ancs: not cleared (the line above says why)");
        }
        else { char a[120]; ancs_status(a, sizeof a); Serial.println(a); }
    }
    else if (!strcmp(v, "ear")) ear_cmd(argc, argv);   /* THE EAR'S LEDGER AND ITS LOW GEAR: the line; the gear (NVS) */
    else if (!strcmp(v, "markers")) { bool on = argc > 1 && !strcmp(argv[1], "on"); app_set_markers(on); }
    else if (!strcmp(v, "wisp") && argc == 1) { char a[240]; wisp_status(a, sizeof a); Serial.println(a); }
    else if (!strcmp(v, "d") && argc == 1) app_set_markers(!app_markers());
    else if (strlen(v) == 1 && v[0] >= '0' && v[0] <= '7' && argc == 1) app_set_axis_map(v[0] - '0');
    else if (!bus_cmd(argc, argv)) Serial.printf("? %s (try help)\n", v);   /* bom, bus, haptic, gauge, rtc, ppg */
}

static void dispatch(char *s)
{
    /* the desk on the dock: its lines are ok / no / hi / an ask. Its
     * ok/no answers to our own log lines arrive whether or not a dock is
     * open, and are never a command (answering them would be a loop). */
    bool word_ok = !strncmp(s, "ok", 2) && (s[2] == 0 || s[2] == ' ');
    bool word_no = !strncmp(s, "no", 2) && (s[2] == 0 || s[2] == ' ');
    bool word_hi = !strncmp(s, "hi", 2) && (s[2] == 0 || s[2] == ' ');
    bool ask = !strncmp(s, "wifi add ", 9)           /* unquoted: the desk's; `wifi "add" ...` is yours */
            || !strncmp(s, "wisp ", 5)               /* `wisp <app>` by hand goes the desk's way too: a test aid, answered on the port */
            || !strncmp(s, "knock ", 6);             /* THE KNOCK over the dock: `knock lost|free [<stamp> <mac>]` (link.cpp), answered `ok <house>` / `no <why>` */
    if (link_docked()) {
        if (word_ok || word_no || word_hi || ask) { link_serial_line(s); return; }
    } else {
        if (!strncmp(s, "hi ", 3)) { link_serial_line(s); return; }
        if (ask) { link_serial_ask(s); return; }
        if (word_ok || word_no || word_hi) return;
    }
    char *argv[8];
    int argc = tokenize(s, argv, 8);
    if (argc) run(argc, argv);
}

void console_begin(void) { n = 0; }

void console_tick(void)
{
    while (Serial.available()) {
        int ch = Serial.read();
        if (ch < 0) break;
        if (ch == '\n' || ch == '\r') {
            if (n) { line[n] = 0; n = 0; app_keep_awake(); dispatch(line); }
            continue;
        }
        if (n < sizeof line - 1) line[n++] = (char)ch;
        else n = 0;                              /* too long: dropped whole */
    }
}
