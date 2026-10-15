#include "store.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

store_t store;
static Preferences prefs;

static void cp(char *dst, size_t cap, const char *src)
{
    if (!src) src = "";
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

void store_begin(void)
{
    memset(&store, 0, sizeof store);
    store.tz_min = -240;                   /* Khalid's default until the desk says */
    store.markers = false;
    prefs.begin("watch", false);
    size_t n = prefs.isKey("nets") ? prefs.getBytesLength("nets") : 0;
    if (n == sizeof store.nets) {
        prefs.getBytes("nets", store.nets, sizeof store.nets);
        for (int i = 0; i < STORE_NETS; i++) {
            store.nets[i].ssid[STORE_SSID_MAX] = 0;
            if (store.nets[i].ssid[0]) store.n_nets = i + 1;
        }
        /* compact: no holes */
        int w = 0;
        for (int i = 0; i < STORE_NETS; i++)
            if (store.nets[i].ssid[0]) { if (w != i) store.nets[w] = store.nets[i]; w++; }
        for (int i = w; i < STORE_NETS; i++) memset(&store.nets[i], 0, sizeof store.nets[i]);
        store.n_nets = w;
    } else if (n) {
        Serial.printf("[store] nets blob %u B is not the shape known (%u): ignored\n",
                      (unsigned)n, (unsigned)sizeof store.nets);
    }
    /* the doors; the old single desk migrates into door 0 once */
    n = prefs.isKey("doors") ? prefs.getBytesLength("doors") : 0;
    if (n == sizeof store.doors) {
        prefs.getBytes("doors", store.doors, sizeof store.doors);
        int w = 0;
        for (int i = 0; i < STORE_DOORS; i++) {
            store.doors[i].host[sizeof store.doors[i].host - 1] = 0;
            if (store.doors[i].host[0]) { if (w != i) store.doors[w] = store.doors[i]; w++; }
        }
        for (int i = w; i < STORE_DOORS; i++) memset(&store.doors[i], 0, sizeof store.doors[i]);
        store.n_doors = w;
    } else {
        if (n) Serial.printf("[store] doors blob %u B is not the shape known (%u): ignored\n",
                             (unsigned)n, (unsigned)sizeof store.doors);
        char host[64] = "";
        if (prefs.isKey("host")) prefs.getString("host", host, sizeof host);
        if (host[0]) {
            store_door_set0(host, prefs.getUShort("port", 8891));
            Serial.printf("[store] the single desk %s:%u became door 0 (plain)\n", host, store.doors[0].port);
        }
    }
    if (prefs.isKey("key")) prefs.getString("key", store.key, sizeof store.key);
    store.tz_min = prefs.getInt("tz", -240);
    store.tz_said = prefs.isKey("tz");      /* THE PHONE'S HOUR: the desk (or his `tz`) said one; the phone's zone fills in only until then */
    if (prefs.isKey("ota")) prefs.getString("ota", store.ota_pw, sizeof store.ota_pw);
    store.markers = prefs.getBool("mark", false);
    /* THE LOCK IS NEVER A TRAP (2026-09-10 night): the water lock has no key
     * here any more (gesture.cpp: RAM only, a reboot unlocks); a `wlock` a
     * board wrote before is erased so nothing ever reads it again */
    if (prefs.isKey("wlock")) { prefs.remove("wlock"); Serial.println("[store] the water lock's old key erased: the lock never persists"); }
    if (prefs.isKey("grain")) prefs.getString("grain", store.grain, sizeof store.grain);
    if (prefs.isKey("worn")) prefs.getString("worn", store.worn, sizeof store.worn);     /* THE WRIST WARDROBE: the worn form, the theme */
    if (prefs.isKey("theme")) prefs.getString("theme", store.theme, sizeof store.theme);
    store.house = prefs.getUChar("house", 0);        /* THE HOUSE: sticky across power (house.cpp logs it) */
    store.face_off = prefs.getBool("faceoff", false); /* THE WATCH FACE: `face off` survives a reboot */
    store.bom = prefs.getUChar("bom", 0);            /* HARDWARE ROOM: the add-ons soldered (bus.cpp logs them) */
    if (prefs.isKey("hcal") && prefs.getBytesLength("hcal") == sizeof store.haptic_cal)
        prefs.getBytes("hcal", store.haptic_cal, sizeof store.haptic_cal);
    store.ppg_led = prefs.getUChar("ppgled", 1);     /* THE PPG's LED: IR by default (the MAX30102 on the bench 2026-09-12) */
    store.ppg_rail = prefs.getUChar("ppgrail", 0);   /* THE PPG's RAIL: a GPIO as its VIN, 0 = none (ppg.cpp drives it) */
    store.ppg_auto_off = prefs.getBool("ppgauto", false);   /* THE PULSE IN THE DAY: on unless he turned it off (ppg.cpp logs it when the part is found) */
    store.alarm_min = prefs.getShort("alarm", -1);    /* CLOCKWORK: the alarm survives a reboot (clockwork.cpp logs it) */
    store.beat_worn = prefs.getBool("bworn", true);   /* WORN ON THE BEAT: ON since 2026-09-17 (the box arbitrates on the word); the same key, so a wrist that set it off keeps off */
    store.told_night = prefs.getUInt("toldn", 0);     /* THE NIGHT AFTER A REBOOT: the last night told (daylog.cpp restores it after the replay) */
    /* THE NEAR DOOR FIRST: per network, the plain door that answered; its own key, another shape ignored */
    if (prefs.isKey("near") && prefs.getBytesLength("near") == sizeof store.near) {
        prefs.getBytes("near", store.near, sizeof store.near);
        for (int i = 0; i < STORE_NETS; i++) store.near[i].ssid[STORE_SSID_MAX] = 0;
    }
    /* THE SETTINGS PAGE: his switches (each its own key; absent = the default: the lease by hand on, quiet time on, no hour remembered) */
    store.lease_hand_off = prefs.getBool("leaseoff", false);
    store.quiet_off = prefs.getBool("quietoff", false);
    store.alarm_last = prefs.getShort("alarmlst", -1);
    /* MORE SETTINGS (09-10 21:12): SETTINGS 3-5's rows -- absent = the defaults (the radio on the budget, MID, the LONG spell, 24H, 23-7) */
    store.wifi_hold = prefs.getBool("wifihold", false);
    store.bright = prefs.getUChar("bright", 1);
    if (store.bright > 2) store.bright = 1;
    store.spell_short = prefs.getBool("spellsht", false);
    store.h12 = prefs.getBool("h12", false);
    store.quiet_from = (int8_t)prefs.getChar("qfrom", 23);
    store.quiet_to = (int8_t)prefs.getChar("qto", 7);
    if (store.quiet_from < 0 || store.quiet_from > 23) store.quiet_from = 23;
    if (store.quiet_to < 0 || store.quiet_to > 23) store.quiet_to = 7;
    store.ble_off = prefs.getBool("bleoff", false);   /* BLUETOOTH: on unless he turned the row off (ble.cpp logs it) */
    store.bumps_off = prefs.getBool("bumpsoff", false); /* THE BUMPS: on unless he turned them off (main.cpp logs it at boot) */
    store.ear_gear = prefs.getUChar("eargear", 240);    /* THE EAR'S LOW GEAR: off (240) unless he set 80 (ear.cpp logs it at boot) */
    store.dockhand_off = prefs.getBool("dockoff", false); /* THE DOCK HANDS HIM OVER: on unless he turned it off (link.cpp logs each edge) */
    store.sun_rise = prefs.getShort("sunrise", -1);   /* THE REAL SUN: the last pair the wire gave, -1 = the fixed hours */
    store.sun_set = prefs.getShort("sunset", -1);
    if (store.sun_rise < 0 || store.sun_rise > 1439 || store.sun_set < 0 || store.sun_set > 1439 || store.sun_rise >= store.sun_set) store.sun_rise = store.sun_set = -1;
    store.fs_formatted = prefs.getBool("fsfmt", false);   /* THE DISK ASKS BEFORE IT FORMATS: absent = never formatted by this firmware (daylog.cpp's wrist_begin sets it once) */
    /* FAST JOIN: the last AP, its own key; a blob of another shape is ignored (a scan, as before) */
    if (prefs.isKey("fast") && prefs.getBytesLength("fast") == sizeof store.fast) {
        prefs.getBytes("fast", &store.fast, sizeof store.fast);
        store.fast.ssid[STORE_SSID_MAX] = 0;
    }
    Serial.printf("[store] %u network%s, %u door%s (0: %s:%u%s), key %s, tz %+d min, ota pw %s, markers %s\n",
                  store.n_nets, store.n_nets == 1 ? "" : "s", store.n_doors, store.n_doors == 1 ? "" : "s",
                  store.n_doors ? store.doors[0].host : "(unset)", store.doors[0].port,
                  store.doors[0].tls ? " tls" : "",
                  store.key[0] ? "set" : "UNSET", (int)store.tz_min,
                  store.ota_pw[0] ? "set" : "(the key's first 8)", store.markers ? "on" : "off");   /* no water lock here: THE LOCK IS NEVER A TRAP (gesture.cpp) */
}

const net_cred_t *store_net_find(const char *ssid)
{
    for (int i = 0; i < store.n_nets; i++)
        if (!strcmp(store.nets[i].ssid, ssid)) return &store.nets[i];
    return NULL;
}

bool store_net_add(const char *ssid, uint8_t kind, const char *id, const char *anon, const char *pw)
{
    if (!ssid || !ssid[0] || strlen(ssid) > STORE_SSID_MAX) return false;
    net_cred_t *slot = (net_cred_t *)store_net_find(ssid);
    if (!slot) {
        if (store.n_nets >= STORE_NETS) return false;
        slot = &store.nets[store.n_nets++];
    }
    memset(slot, 0, sizeof *slot);
    cp(slot->ssid, sizeof slot->ssid, ssid);
    slot->kind = kind;
    cp(slot->id, sizeof slot->id, id);
    cp(slot->anon, sizeof slot->anon, anon);
    cp(slot->pw, sizeof slot->pw, pw);
    store_save_nets();
    return true;
}

bool store_net_forget(const char *ssid)
{
    for (int i = 0; i < store.n_nets; i++) {
        if (strcmp(store.nets[i].ssid, ssid)) continue;
        for (int j = i; j + 1 < store.n_nets; j++) store.nets[j] = store.nets[j + 1];
        store.n_nets--;
        memset(&store.nets[store.n_nets], 0, sizeof store.nets[0]);
        store_save_nets();
        return true;
    }
    return false;
}

/* --- the doors ------------------------------------------------------------------ */

static bool door_ok(const char *host, uint16_t port)
{
    return host && host[0] && strlen(host) < sizeof store.doors[0].host && port;
}

bool store_door_add(const char *host, uint16_t port, bool tls)
{
    if (!door_ok(host, port) || store.n_doors >= STORE_DOORS) return false;
    door_t *d = &store.doors[store.n_doors++];
    memset(d, 0, sizeof *d);
    cp(d->host, sizeof d->host, host);
    d->port = port;
    d->tls = tls ? 1 : 0;
    store_save_doors();
    return true;
}

bool store_door_rm(int n)
{
    if (n < 0 || n >= store.n_doors) return false;
    for (int j = n; j + 1 < store.n_doors; j++) store.doors[j] = store.doors[j + 1];
    store.n_doors--;
    memset(&store.doors[store.n_doors], 0, sizeof store.doors[0]);
    store_save_doors();
    return true;
}

void store_door_set0(const char *host, uint16_t port)
{
    if (!door_ok(host, port)) return;
    door_t *d = &store.doors[0];
    memset(d, 0, sizeof *d);
    cp(d->host, sizeof d->host, host);
    d->port = port;
    d->tls = 0;
    if (!store.n_doors) store.n_doors = 1;
    store_save_doors();
}

void store_save_doors(void)
{
    prefs.putBytes("doors", store.doors, sizeof store.doors);
    /* door 0 mirrored into the old pair, for a firmware that only knows those */
    prefs.putString("host", store.doors[0].host);
    prefs.putUShort("port", store.doors[0].port);
}

void store_save_nets(void)    { prefs.putBytes("nets", store.nets, sizeof store.nets); }
void store_save_key(void)     { prefs.putString("key", store.key); }
void store_save_tz(void)      { prefs.putInt("tz", store.tz_min); }
void store_save_ota(void)     { prefs.putString("ota", store.ota_pw); }
void store_save_markers(void) { prefs.putBool("mark", store.markers); }
void store_save_grain(void)   { prefs.putString("grain", store.grain); }
void store_save_worn(void)    { prefs.putString("worn", store.worn); }      /* THE WRIST WARDROBE */
void store_save_theme(void)   { prefs.putString("theme", store.theme); }
void store_save_house(void)   { prefs.putUChar("house", store.house); }
void store_save_face(void)    { prefs.putBool("faceoff", store.face_off); }
void store_save_bom(void)     { prefs.putUChar("bom", store.bom); }
void store_save_haptic_cal(void) { prefs.putBytes("hcal", store.haptic_cal, sizeof store.haptic_cal); }
void store_save_ppg_led(void) { prefs.putUChar("ppgled", store.ppg_led); }
void store_save_ppg_rail(void) { prefs.putUChar("ppgrail", store.ppg_rail); }
void store_save_ppg_auto(void) { prefs.putBool("ppgauto", store.ppg_auto_off); }
void store_save_alarm(void)   { prefs.putShort("alarm", store.alarm_min); }
void store_save_beat_worn(void) { prefs.putBool("bworn", store.beat_worn); }
void store_save_fast(void)    { prefs.putBytes("fast", &store.fast, sizeof store.fast); }

int store_near_door(const char *ssid)
{
    if (!ssid || !ssid[0]) return -1;
    for (int i = 0; i < STORE_NETS; i++)
        if (store.near[i].ssid[0] && !strcmp(store.near[i].ssid, ssid)) return store.near[i].door;
    return -1;
}

void store_near_set(const char *ssid, int door)
{
    if (!ssid || !ssid[0] || strlen(ssid) > STORE_SSID_MAX) return;
    int slot = -1, empty = -1;
    for (int i = 0; i < STORE_NETS; i++) {
        if (store.near[i].ssid[0] && !strcmp(store.near[i].ssid, ssid)) { slot = i; break; }
        if (!store.near[i].ssid[0] && empty < 0) empty = i;
    }
    if (slot >= 0 && store.near[slot].door == door) return;      /* known already: NVS untouched */
    if (slot < 0) slot = empty >= 0 ? empty : 0;                  /* full: the first slot is reused */
    memset(&store.near[slot], 0, sizeof store.near[slot]);
    cp(store.near[slot].ssid, sizeof store.near[slot].ssid, ssid);
    store.near[slot].door = (int8_t)door;
    prefs.putBytes("near", store.near, sizeof store.near);
}
void store_save_told_night(void) { prefs.putUInt("toldn", store.told_night); }
void store_save_lease_hand(void) { prefs.putBool("leaseoff", store.lease_hand_off); }
void store_save_quiet(void)      { prefs.putBool("quietoff", store.quiet_off); }
void store_save_alarm_last(void) { prefs.putShort("alarmlst", store.alarm_last); }
void store_save_wifi_hold(void)  { prefs.putBool("wifihold", store.wifi_hold); }
void store_save_bright(void)     { prefs.putUChar("bright", store.bright); }
void store_save_spell(void)      { prefs.putBool("spellsht", store.spell_short); }
void store_save_h12(void)        { prefs.putBool("h12", store.h12); }
void store_save_quiet_hours(void) { prefs.putChar("qfrom", store.quiet_from); prefs.putChar("qto", store.quiet_to); }
void store_save_sun(void) { prefs.putShort("sunrise", store.sun_rise); prefs.putShort("sunset", store.sun_set); }   /* THE REAL SUN */
void store_save_ble(void)        { prefs.putBool("bleoff", store.ble_off); }
void store_save_bumps(void)      { prefs.putBool("bumpsoff", store.bumps_off); }
void store_save_ear_gear(void)   { prefs.putUChar("eargear", store.ear_gear); }
void store_save_dockhand(void)   { prefs.putBool("dockoff", store.dockhand_off); }
void store_save_fs_formatted(void) { prefs.putBool("fsfmt", store.fs_formatted); }   /* THE DISK ASKS BEFORE IT FORMATS */
void store_save_clock(uint32_t epoch) { prefs.putUInt("clock", epoch); }
uint32_t store_load_clock(void) { return prefs.getUInt("clock", 0); }
