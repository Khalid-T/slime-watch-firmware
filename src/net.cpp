#include "net.h"
#include "app.h"
#include "wdt.h"        /* THE LOOP'S PULSE: fed per OTA chunk (the upload blocks loop for longer than the 30 s) */
#include "board.h"      /* SCREEN_W/H for /screen.bin */
#include "bmp.h"        /* THE FACE AS AN IMAGE: /screen.bmp, the chunked 24-bit writer */
#include "sky.h"        /* the held sky word in status.json */
#include "store.h"
#include "link.h"
#include "house.h"      /* THE HOUSE in status.json */
#include "daylog.h"
#include "doctor.h"     /* DOCTOR: GET /doctor, the health report as text */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <esp_sntp.h>
#include <string.h>
#include <sys/stat.h>
#include "ble.h"        /* BLUETOOTH (ruling 58): BLE only while the WiFi radio is down -- one call at each radio edge below */
#include "artota.h"     /* ART OVER THE AIR: `POST /art` in chunks -- the state machine; wardrobe.cpp holds the flash */
#include "wardrobe.h"   /* the veil off, the mapping released, the remount: artota's hooks live there */
#include "httpgate.h"   /* THE HTTP DOOR'S KNOCK: which paths take the knock's signature (the page and the face stay open) */

static const char *HOSTNAME = "slime-watch";
static const uint32_t JOIN_MS = 20000;      /* a join that takes longer is not happening */
/* FAST JOIN (store.h fast_join_t): the boot log read "looking for 10
 * networks" and JOINED ~2 s later -- the scan of thirteen channels was
 * most of it. The last AP is joined directly (its BSSID and channel
 * handed to the driver: one channel probed, one AP); a fast join that
 * has not landed in FAST_JOIN_MS falls back to the scan as before. A
 * plain join's association + DHCP is ~0.5-1 s; the board says the rest. */
static const uint32_t FAST_JOIN_MS = 2500;
static const uint32_t RESCAN_MS = 30000;    /* none seen: look again after this */
static const uint32_t HTTP_READ_MS = 400;   /* a request must arrive within this */
/* ART OVER THE AIR: a chunk's 4 KB body gets its own window (the LAN
 * lands it in a few ms; the tailnet share in a few hundred), and the
 * push may not start under the heap floor + its one buffer */
static const uint32_t ART_READ_MS = 3000;
static const uint32_t ART_HEAP_FLOOR = 60 * 1024 + ARTOTA_CHUNK;

enum NetState { N_OFF, N_SCAN, N_JOIN, N_UP, N_WAIT };
static const char *N_NAMES[] = { "off", "scanning", "joining", "up", "waiting" };
static NetState ns = N_OFF;
static uint32_t ns_ms = 0;
static char cur_ssid[STORE_SSID_MAX + 1] = "";
static char cur_ip[20] = "";
static uint16_t tried_mask = 0;             /* networks that failed this round */
static volatile bool ev_got_ip = false, ev_lost = false, ev_ntp = false;
static volatile int ev_reason = 0;
static bool print_scan = false;
static bool services_up = false;
static bool fast_try = false;               /* FAST JOIN: this N_JOIN is the direct one (its own, shorter wall) */
static uint32_t radio_on_ms = 0;            /* when the radio came up: the JOINED line says the whole cost */
static uint32_t http_hits = 0;
static WiFiServer server(80);
static WiFiClient http;
static char http_buf[600];
static httpgate_t http_gate;            /* THE HTTP DOOR'S KNOCK: the bare board's `taken unsigned`, said once */
static size_t http_n = 0;
static uint32_t http_ms = 0;
static uint32_t ntp_epoch = 0;
/* ART OVER THE AIR: the one 4 KB body buffer, malloc'd at `begin`, freed
 * when the push ends (the heap's whole cost of a push); http_buf holds
 * the headers only, the body is read past them into this */
static uint8_t *art_buf = NULL;
static size_t art_n = 0, art_want = 0;
static bool art_reading = false;

/* --- events (the WiFi task: flags only) --------------------------------------- */

static void on_event(WiFiEvent_t ev, WiFiEventInfo_t info)
{
    if (ev == ARDUINO_EVENT_WIFI_STA_GOT_IP) ev_got_ip = true;
    else if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) { ev_lost = true; ev_reason = info.wifi_sta_disconnected.reason; }
}

static void on_ntp(struct timeval *tv) { ntp_epoch = (uint32_t)tv->tv_sec; ev_ntp = true; }

/* --- the services on top ------------------------------------------------------- */

static void ota_pw(char *out, int cap)
{
    if (store.ota_pw[0]) { strncpy(out, store.ota_pw, cap - 1); out[cap - 1] = 0; return; }
    strncpy(out, store.key, 8); out[8] = 0;
}

static void services_begin(void)
{
    if (services_up) return;
    char pw[33]; ota_pw(pw, sizeof pw);
    ArduinoOTA.setHostname(HOSTNAME);
    if (pw[0]) ArduinoOTA.setPassword(pw);
    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA.onStart([]() { LOG("ota: begins (%s)", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem"); wrist_flush(); });
    ArduinoOTA.onEnd([]() { LOG("ota: done; rebooting"); });
    ArduinoOTA.onError([](ota_error_t e) { LOG("ota: error %d", (int)e); });
    ArduinoOTA.onProgress([](unsigned p, unsigned t) {
        wdt_feed(WDT_SPELL_AID);            /* THE LOOP'S PULSE: the flash over the air is a long aid, not a hang */
        static unsigned last = 0; unsigned pct = t ? p * 100 / t : 0;
        if (pct / 25 != last / 25) { last = pct; LOG("ota: %u%%", pct); }
    });
    ArduinoOTA.begin();                     /* also MDNS.begin(HOSTNAME) */
    MDNS.addService("http", "tcp", 80);
    server.begin();
    server.setNoDelay(true);
    sntp_set_time_sync_notification_cb(on_ntp);
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    services_up = true;
    LOG("net: %s.local, OTA (pw %s), http :80, ntp asked", HOSTNAME, pw[0] ? "set" : "NONE: set a key or `ota <pw>`");
}

static void art_buf_drop(void)
{
    if (art_buf) { free(art_buf); art_buf = NULL; }
    art_reading = false; art_n = art_want = 0;
}

static void services_end(void)
{
    if (!services_up) return;
    if (http) http.stop();
    /* ART OVER THE AIR: the door going away mid-push drops the push (the
     * image is gone if a sector was erased; the log says; `wardrobe`
     * says "no image" until a whole one lands) */
    if (artota_active()) artota_abandon("the radio went away mid-push (the hold ended, the dark house, the network lost)");
    art_buf_drop();
    server.stop();
    ArduinoOTA.end();
    MDNS.end();
    services_up = false;
}

/* --- joining ------------------------------------------------------------------- */

static void set_ns(NetState s) { ns = s; ns_ms = millis(); }

static void start_scan(void)
{
    WiFi.scanDelete();
    int r = WiFi.scanNetworks(true, false);
    if (r != WIFI_SCAN_RUNNING) LOG("net: scan did not start (%d; wifi %s)", r, WiFi.getMode() == WIFI_OFF ? "off" : "on");
    set_ns(N_SCAN);
}

/* fast = the last AP's channel and BSSID handed to the driver (FAST JOIN); else the scan's pick */
static void join(const net_cred_t *c, const fast_join_t *fast)
{
    strncpy(cur_ssid, c->ssid, sizeof cur_ssid - 1);
    int32_t ch = fast ? fast->channel : 0;
    const uint8_t *bssid = fast ? fast->bssid : NULL;
    if (fast) LOG("net: fast join %s (%s; ch %d, bssid %02x:%02x:%02x:%02x:%02x:%02x -- no scan)", c->ssid,
                  c->kind == NET_EAP ? "wpa2-enterprise peap" : c->kind == NET_PSK ? "psk" : "open", (int)ch,
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    else LOG("net: joining %s (%s)", c->ssid, c->kind == NET_EAP ? "wpa2-enterprise peap" : c->kind == NET_PSK ? "psk" : "open");
    WiFi.setHostname(HOSTNAME);
    if (c->kind == NET_EAP)
        WiFi.begin(c->ssid, WPA2_AUTH_PEAP, c->anon[0] ? c->anon : c->id, c->id, c->pw, NULL, NULL, NULL, ch, bssid);
    else if (c->kind == NET_PSK)
        WiFi.begin(c->ssid, c->pw, ch, bssid);
    else
        WiFi.begin(c->ssid, (const char *)NULL, ch, bssid);
    fast_try = fast != NULL;
    set_ns(N_JOIN);
}

/* FAST JOIN: the last AP, if its network is still remembered; false = scan */
static bool join_fast(void)
{
    if (!store.fast.ssid[0] || !store.fast.channel) return false;
    const net_cred_t *c = store_net_find(store.fast.ssid);
    if (!c) return false;
    join(c, &store.fast);
    return true;
}

static void print_results(int found)
{
    print_scan = false;
    Serial.printf("wifi-scan: %d network%s\n", found, found == 1 ? "" : "s");
    for (int i = 0; i < found; i++) {
        wifi_auth_mode_t m = WiFi.encryptionType(i);
        Serial.printf("  %-32s %4d dBm ch %2d %s%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      m == WIFI_AUTH_OPEN ? "open" : m == WIFI_AUTH_WPA2_ENTERPRISE ? "802.1x" : "psk",
                      store_net_find(WiFi.SSID(i).c_str()) ? "  (known)" : "");
    }
}

static void pick_and_join(int found)
{
    int best = -1, best_rssi = -1000;
    for (int i = 0; i < found; i++) {
        String s = WiFi.SSID(i);
        for (int k = 0; k < store.n_nets; k++) {
            if (tried_mask & (1u << k)) continue;
            if (s == store.nets[k].ssid && WiFi.RSSI(i) > best_rssi) { best = k; best_rssi = WiFi.RSSI(i); }
        }
    }
    if (print_scan) print_results(found);
    WiFi.scanDelete();
    if (best < 0) {
        if (store.n_nets) LOG("net: none of the %u remembered network%s seen (%d in the air); again in %lu s",
                              store.n_nets, store.n_nets == 1 ? "" : "s", found, (unsigned long)(RESCAN_MS / 1000));
        tried_mask = 0;
        set_ns(N_WAIT);
        return;
    }
    tried_mask |= (1u << best);
    join(&store.nets[best], NULL);
}

/* --- the HTTP page ------------------------------------------------------------- */

static void http_send_head(const char *status, const char *type, size_t len)
{
    http.printf("HTTP/1.1 %s\r\nContent-Type: %s\r\nConnection: close\r\nCache-Control: no-store\r\n", status, type);
    if (len) http.printf("Content-Length: %u\r\n", (unsigned)len);
    http.print("\r\n");
}

static void status_json(char *out, int cap)
{
    daylog_sum_t s; bool have = clock_local_date() && wrist_sum(clock_local_date(), &s);
    struct tm lt; uint32_t ep = 0; clock_local(&lt, &ep);
    const link_grain_t *g = link_grain();
    snprintf(out, cap,
        "{\"body\":\"watch\",\"state\":\"%s\",\"wall\":%d,\"phi\":%.1f,\"fps\":%.1f,"
        "\"steps\":%lu,\"worn\":%s,\"minutes_logged\":%d,\"raises\":%d,\"taps\":%d,"
        "\"linked\":%s,\"link\":\"%s\",\"holding\":%s,\"house\":\"%s\",\"docked\":%s,\"sky\":\"%s%s%s\","
        "\"batt_mv\":%u,\"charging\":%s,\"uptime_s\":%lu,"
        "\"clock\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"clock_set\":%s,\"tz_min\":%d,"
        "\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
        "\"grain\":{\"have\":%s,\"seed\":%d,\"regard\":%.2f,\"night\":%d,\"quiet\":\"%s\",\"mood\":\"%s\"}}",
        APP_STATE_NAMES[app.state], app.wall, app.phi_deg, app.fps,
        (unsigned long)wrist_steps_today(), wrist_worn() ? "true" : "false",
        have ? s.minutes : 0, have ? s.raises : 0, have ? s.taps : 0,
        link_linked() ? "true" : "false", link_state_name(), link_holding() ? "true" : "false", house_state_name(),
        link_docked() ? "true" : "false", sky_word(0), sky_word(1)[0] ? " " : "", sky_word(1),
        batt_mv(), charging() ? "true" : "false", (unsigned long)(millis() / 1000),
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec,
        clock_is_set() ? "true" : "false", (int)store.tz_min,
        cur_ssid, cur_ip, WiFi.RSSI(),
        g->have ? "true" : "false", g->seed, g->regard, g->night, g->quiet, g->mood);
}

static void http_page(void)
{
    daylog_sum_t s; bool have = clock_local_date() && wrist_sum(clock_local_date(), &s);
    struct tm lt; uint32_t ep = 0; clock_local(&lt, &ep);
    static const char *WALLS[4] = { "bottom", "right", "top", "left" };
    /* every path RELATIVE (design/watch-link.md 10: the tailnet share
     * serves this page under /watch/ and strips the prefix; a browser
     * resolves `screen.bmp` against the URL it loaded) */
    char body[1800];
    snprintf(body, sizeof body,
        "<!doctype html><title>slime watch</title>"
        "<body style=\"font:15px/1.5 monospace;background:#101418;color:#cfe;max-width:32em;margin:2em auto;padding:0 1em\">"
        "<h2 style=\"color:#7fd\">slime watch</h2>"
        "<p>%s, on the <b>%s</b> wall, phi %.0f&deg;, %.0f fps</p>"
        "<p>steps today <b>%lu</b> (%d min logged, %s), raises %d, taps %d</p>"
        "<p>link: <b>%s</b>%s &middot; wifi %s %s (%d dBm)</p>"
        "<p>battery %u mV%s &middot; up %lu s &middot; clock %s%04d-%02d-%02d %02d:%02d</p>"
        "<p><img id=s src=screen.bmp width=240 height=240 alt=\"the face\" style=\"border-radius:50%%;background:#000;image-rendering:pixelated\"></p>"
        "<script>var s=document.getElementById('s');"
        "setInterval(function(){s.src='screen.bmp?t='+Date.now();},2000);</script>"
        "<form method=post action=pet><button style=\"font:inherit;padding:.4em 1.2em\">pet</button></form>"
        "<p><a href=status.json style=\"color:#7fd\">status.json</a> &middot; <a href=day/ style=\"color:#7fd\">day logs</a> &middot; <a href=screen.bmp style=\"color:#7fd\">screen.bmp</a> &middot; <a href=doctor style=\"color:#7fd\">doctor</a></p>",
        APP_STATE_NAMES[app.state], WALLS[app.wall & 3], app.phi_deg, app.fps,
        (unsigned long)wrist_steps_today(), have ? s.minutes : 0, wrist_worn() ? "worn" : "not worn",
        have ? s.raises : 0, have ? s.taps : 0,
        link_state_name(), link_holding() ? " (the slime is here)" : "", cur_ssid, cur_ip, WiFi.RSSI(),
        batt_mv(), charging() ? " (charging)" : "", (unsigned long)(millis() / 1000),
        clock_is_set() ? "" : "(unset) ", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    http_send_head("200 OK", "text/html; charset=utf-8", strlen(body));
    http.print(body);
}

static void http_day_list(void)
{
    File dir = LittleFS.open("/day");
    String out = "<!doctype html><title>day logs</title><body style=\"font:15px monospace;background:#101418;color:#cfe\"><h3>day logs</h3><ul>";
    if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
            if (strstr(f.name(), ".bin")) out += String("<li><a style=\"color:#7fd\" href=\"/day/") + f.name() + "\">" + f.name() + "</a> " + f.size() + " B</li>";
            f.close();
        }
        dir.close();
    }
    out += "</ul>";
    http_send_head("200 OK", "text/html; charset=utf-8", out.length());
    http.print(out);
}

static void http_day_file(const char *name)
{
    char p[40];
    snprintf(p, sizeof p, "/day/%s", name);
    struct stat st; char full[56]; snprintf(full, sizeof full, "/littlefs%s", p);
    if (strstr(name, "..") || stat(full, &st) != 0) { http_send_head("404 Not Found", "text/plain", 3); http.print("no\n"); return; }
    File f = LittleFS.open(p, "r");
    http_send_head("200 OK", "application/octet-stream", f.size());
    uint8_t b[512]; int n;
    while ((n = f.read(b, sizeof b)) > 0) http.write(b, n);
    f.close();
}

/* THE FACE AS AN IMAGE: the framebuffer as a 24-bit BMP, streamed in
 * bmp.h's 4 KB chunks straight into the socket (no encoder, no second
 * buffer; the header alone first) */
static void bmp_sink(const uint8_t *bytes, size_t n, void *ctx)
{
    (void)ctx;
    if (http.connected()) http.write(bytes, n);
}

static void http_screen_bmp(void)
{
    http_send_head("200 OK", "image/bmp", BMP_SIZE(SCREEN_W, SCREEN_H));
    if (!app_fb) return;
    bmp_write(app_fb, SCREEN_W, SCREEN_H, bmp_sink, NULL);
}

/* a case-blind header search (lwIP's libc has no strcasestr) */
static const char *strcasestr_len(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (const char *p = hay; *p; p++)
        if (!strncasecmp(p, needle, n)) return p;
    return NULL;
}

/* ART OVER THE AIR (artota.h has the protocol): `POST /art?begin=<total>`
 * / `?off=<n>&len=<n>&crc=<hex>` with the chunk as the body / `?end=<hex>`,
 * each signed as the knock is (`&stamp=<utc minute>&mac=<hex>`, mac =
 * HMAC(key, "<stamp> art begin <total>" / "<stamp> art chunk <off> <crc>"
 * / "<stamp> art end <crc>"); a bare board takes them unsigned, logged).
 * One line of text back: `ok ...` (200) or `no <word>` (403 the auth,
 * 409 `no wait`, 503 `no heap`, 400 the rest). The chunk's body was read
 * into art_buf by http_tick (art_n bytes). Every request keeps the glass
 * awake: a dim would end the hold's lit spell and the radio with it. */
static bool qget(const char *q, const char *key, char *out, int cap)
{
    size_t kl = strlen(key);
    for (const char *p = q; p && *p; ) {
        const char *amp = strchr(p, '&');
        size_t seg = amp ? (size_t)(amp - p) : strlen(p);
        if (seg > kl && !strncmp(p, key, kl) && p[kl] == '=') {
            size_t n = seg - kl - 1; if (n > (size_t)cap - 1) n = (size_t)cap - 1;
            memcpy(out, p + kl + 1, n); out[n] = 0;
            return true;
        }
        p = amp ? amp + 1 : NULL;
    }
    out[0] = 0;
    return false;
}

static void http_art_reply(int rc, const char *line)
{
    const char *status = rc == 0 ? "200 OK" : !strncmp(line, "no wait", 7) ? "409 Conflict" : !strncmp(line, "no heap", 7) ? "503 Service Unavailable"
                       : !strncmp(line, "no key", 6) || !strncmp(line, "stale", 5) ? "403 Forbidden" : "400 Bad Request";
    char l[128]; snprintf(l, sizeof l, "%s\n", line);
    http_send_head(status, "text/plain", strlen(l)); http.print(l);
}

static void http_art(const char *q)
{
    char total_s[16], off_s[16], len_s[8], crc_s[12], end_s[12], stamp_s[16], mac_s[40], what[48], why[24], out[128];
    bool begin = qget(q, "begin", total_s, sizeof total_s);
    bool end = qget(q, "end", end_s, sizeof end_s);
    bool chunk = qget(q, "off", off_s, sizeof off_s) && qget(q, "len", len_s, sizeof len_s) && qget(q, "crc", crc_s, sizeof crc_s);
    qget(q, "stamp", stamp_s, sizeof stamp_s); qget(q, "mac", mac_s, sizeof mac_s);
    app_keep_awake();
    if (!begin && !end && !chunk) { http_art_reply(1, "no ask (begin=<total> | off=&len=&crc= | end=<crc>)"); return; }
    uint32_t total = (uint32_t)strtoul(total_s, NULL, 10), off = (uint32_t)strtoul(off_s, NULL, 10), len = (uint32_t)strtoul(len_s, NULL, 10);
    uint32_t crc = (uint32_t)strtoul(begin ? "0" : chunk ? crc_s : end_s, NULL, 16);
    if (begin) snprintf(what, sizeof what, "art begin %lu", (unsigned long)total);
    else if (chunk) snprintf(what, sizeof what, "art chunk %lu %08lx", (unsigned long)off, (unsigned long)crc);
    else snprintf(what, sizeof what, "art end %08lx", (unsigned long)crc);
    if (link_sign_check(what, stamp_s, mac_s, why, sizeof why)) {
        LOG("http: /art %s from %s refused: %s", what, http.remoteIP().toString().c_str(), why);
        http_art_reply(1, !strcmp(why, "stale") ? "stale (the stamp is not this minute)" : "no key");
        return;
    }
    if (!strcmp(why, "unsigned")) LOG("http: /art %s taken unsigned (no key remembered here)", what);
    int rc;
    if (begin) {
        uint32_t heap = ESP.getFreeHeap();
        if (heap < ART_HEAP_FLOOR) { snprintf(out, sizeof out, "no heap free=%lu floor=%lu", (unsigned long)heap, (unsigned long)ART_HEAP_FLOOR); rc = 1; }
        else {
            if (!art_buf) art_buf = (uint8_t *)malloc(ARTOTA_CHUNK);
            if (!art_buf) { snprintf(out, sizeof out, "no heap (the 4 KB buffer)"); rc = 1; }
            else {
                rc = artota_begin(total, out, sizeof out);
                LOG("http: /art begin %lu B from %s -> %s (heap %lu free)", (unsigned long)total, http.remoteIP().toString().c_str(), out, (unsigned long)heap);
            }
        }
    } else if (chunk) {
        if (art_n != len) { snprintf(out, sizeof out, "no len body=%u len=%lu", (unsigned)art_n, (unsigned long)len); rc = 1; }
        else rc = artota_chunk(off, len, crc, art_buf, out, sizeof out);
        if (rc) LOG("http: /art off=%lu -> %s", (unsigned long)off, out);
        else if ((off / ARTOTA_CHUNK) % 32 == 0) LOG("http: /art %lu of %lu B (heap %lu free)", (unsigned long)artota_written(), (unsigned long)artota_total(), (unsigned long)ESP.getFreeHeap());
    } else {
        rc = artota_end(crc, out, sizeof out);
        LOG("http: /art end %08lx -> %s", (unsigned long)crc, out);
    }
    http_art_reply(rc, out);
    if (!artota_active()) art_buf_drop();
}

static void http_route(void)
{
    http_hits++;
    char *sp = strchr(http_buf, ' ');
    if (!sp) { http_send_head("400 Bad Request", "text/plain", 0); return; }
    *sp = 0;
    char *path = sp + 1;
    char *e = strchr(path, ' '); if (e) *e = 0;
    bool post = !strcmp(http_buf, "POST");
    char *q = httpgate_split(path);         /* THE HTTP DOOR'S KNOCK: the query off the path (every route sees it bare) */
    if (httpgate_gated(post, path)) {
        /* /doctor, /status.json, /day/.., POST /pet: the knock's signature (`?stamp=&mac=`) when a key is remembered */
        char what[HTTPGATE_WHAT_CAP], stamp_s[16], mac_s[40], why[24]; const char *status, *body; bool say;
        qget(q, "stamp", stamp_s, sizeof stamp_s); qget(q, "mac", mac_s, sizeof mac_s);
        httpgate_what(post, path, what, sizeof what);
        if (httpgate_verdict(&http_gate, link_sign_check(what, stamp_s, mac_s, why, sizeof why), why, &status, &body, &say)) {
            LOG("http: %s from %s refused: %s", what, http.remoteIP().toString().c_str(), why);
            http_send_head(status, "text/plain", strlen(body)); http.print(body);
            return;
        }
        if (say) LOG("http: %s taken unsigned (no key remembered here; said once)", what);
    }
    if (post && !strcmp(path, "/pet")) {
        app_pet();
        LOG("http: a pet from %s", http.remoteIP().toString().c_str());
        http_send_head("200 OK", "application/json", 11); http.print("{\"ok\":true}");
    } else if (post && !strcmp(path, "/lease")) {
        /* THE LAN KNOCK (link.cpp): the desk says `lost` / `free`; the
         * body follows the blank line (http_tick waited for it) */
        const char *body = strstr(e ? e + 1 : path, "\r\n\r\n");
        if (body) body += 4; else { body = strstr(e ? e + 1 : path, "\n\n"); if (body) body += 2; }
        char why[24]; int r = link_knock(body ? body : "", why, sizeof why);   /* the same verdict and act as the dock's `knock ...` line (link.cpp) */
        LOG("http: a knock from %s: %s -> %s", http.remoteIP().toString().c_str(), body ? body : "(no body)", r == 0 ? why : r == 1 ? "refused" : "not a knock");
        char j[64]; snprintf(j, sizeof j, "{\"ok\":%s,\"house\":\"%s\"}", r == 0 ? "true" : "false", r == 0 ? why : house_state_name());
        http_send_head(r == 0 ? "200 OK" : r == 1 ? "403 Forbidden" : "400 Bad Request", "application/json", strlen(j)); http.print(j);
    } else if (post && !strcmp(path, "/art")) {
        http_art(q);                            /* ART OVER THE AIR: begin / a chunk (the body in art_buf) / end (the query, split above) */
    } else if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
        http_page();
    } else if (!strcmp(path, "/screen.bin")) {
        /* the face as it is: 240x240 RGB565, the panel's byte order (swap on the laptop) */
        http_send_head("200 OK", "application/octet-stream", SCREEN_W * SCREEN_H * 2);
        if (app_fb) {
            const uint8_t *p = (const uint8_t *)app_fb;
            for (int off = 0; off < SCREEN_W * SCREEN_H * 2; off += 4096) {
                int n = SCREEN_W * SCREEN_H * 2 - off; if (n > 4096) n = 4096;
                http.write(p + off, n);
            }
        }
    } else if (!strncmp(path, "/screen.bmp", 11) && (path[11] == 0 || path[11] == '?')) {
        http_screen_bmp();                      /* the face as an image (the page's <img>, refreshed with ?t=) */
    } else if (!strcmp(path, "/doctor")) {
        /* DOCTOR: the same report `doctor` prints over serial, as text */
        char *d = (char *)malloc(DOCTOR_CAP);
        if (d) { doctor_text(d, DOCTOR_CAP); http_send_head("200 OK", "text/plain; charset=utf-8", strlen(d)); http.print(d); free(d); }
        else { http_send_head("503 Service Unavailable", "text/plain", 3); http.print("no\n"); }
    } else if (!strcmp(path, "/status.json")) {
        char j[900]; status_json(j, sizeof j);
        http_send_head("200 OK", "application/json", strlen(j)); http.print(j);
    } else if (!strcmp(path, "/day/") || !strcmp(path, "/day")) {
        http_day_list();
    } else if (!strncmp(path, "/day/", 5)) {
        http_day_file(path + 5);
    } else {
        http_send_head("404 Not Found", "text/plain", 3); http.print("no\n");
    }
}

static void http_tick(void)
{
    if (!services_up) return;
    if (!http) {
        http = server.available();
        if (!http) return;
        http_n = 0; http_ms = millis(); art_reading = false;
    }
    if (art_reading) {
        /* ART OVER THE AIR: a chunk's body, past the headers, straight
         * into the 4 KB buffer until Content-Length bytes are in (or the
         * window closes: the route then says `no len`) */
        while (art_n < art_want && http.available()) {
            int n = http.read(art_buf + art_n, art_want - art_n);
            if (n <= 0) break;
            art_n += (size_t)n;
        }
        if (art_n < art_want && http.connected() && millis() - http_ms < ART_READ_MS) return;
        http_route();
        art_reading = false; art_n = art_want = 0;
        http.stop();
        return;
    }
    while (http.available() && http_n < sizeof http_buf - 1) http_buf[http_n++] = (char)http.read();
    http_buf[http_n] = 0;
    const char *hdr_end = strstr(http_buf, "\r\n\r\n"); int sep = 4;
    if (!hdr_end) { hdr_end = strstr(http_buf, "\n\n"); sep = 2; }
    bool done = hdr_end != NULL;
    if (done) {
        /* a body (THE LAN KNOCK's `POST /lease`): wait for Content-Length
         * bytes of it, within the same HTTP_READ_MS -- the body may land
         * in the packet after the headers */
        const char *cl = strcasestr_len(http_buf, "content-length:");
        if (cl) {
            size_t want = (size_t)strtoul(cl + 15, NULL, 10);
            size_t hdr = (size_t)(hdr_end - http_buf) + (size_t)sep;
            size_t have = http_n - hdr;
            if (want && want <= ARTOTA_CHUNK && art_buf && !strncmp(http_buf, "POST /art", 9)) {
                /* ART OVER THE AIR: the chunk's body is not http_buf's --
                 * what came with the headers moves to the buffer, the rest
                 * is read on the next ticks (above); the headers alone
                 * stay here for the route */
                size_t take = have < want ? have : want;
                memcpy(art_buf, http_buf + hdr, take);
                art_n = take; art_want = want; art_reading = true;
                http_buf[hdr] = 0; http_n = hdr;
                http_ms = millis();
                return;
            }
            if (have < want && want < sizeof http_buf) done = false;
        }
    }
    if (!http.connected() && !done) { http.stop(); return; }
    if (!done && millis() - http_ms < HTTP_READ_MS && http_n < sizeof http_buf - 1) return;
    if (http_n) http_route();
    http.stop();
}

/* --- the public face ------------------------------------------------------------ */

void net_begin(void)
{
    WiFi.onEvent(on_event);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_OFF);
}

/* the wake: THE RADIO BUDGET -- the radio does not come up here. The
 * link's clocks start (link_wake) and, when a reason stands (a whistle,
 * a closed day, the claim due, the house never told), the link asks for
 * the radio (net_radio_on); a glance for the hour asks nothing. */
void net_on(void)
{
    ble_up("the wake");                     /* BLUETOOTH: the radio is off at a wake, so BLE comes up (the gate says) */
    link_wake();
}

void net_radio_on(void)
{
    if (ns != N_OFF) return;
    if (!store.n_nets) { LOG("net: no networks remembered (`wifi <ssid> <psk>`); the radio stays off"); return; }
    ble_down("the radio dials");            /* BLUETOOTH: down FIRST, before the WiFi radio comes up (never both) */
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    tried_mask = 0;
    radio_on_ms = millis();
    if (join_fast()) return;                /* FAST JOIN: the last AP straight away; the scan only if it fails */
    LOG("net: radio on; looking for %u network%s", store.n_nets, store.n_nets == 1 ? "" : "s");
    start_scan();
}

/* the dark house: the link's clocks stop, the radio goes away as ever */
void net_off(void)
{
    ble_dark();                             /* BLUETOOTH: down in the dark house (the nap is light sleep) -- unless THE DARK EAR keeps it (a peer bonded; ble.cpp) */
    link_dark();
    net_radio_off();
}

void net_radio_off(void)
{
    if (ns == N_OFF && WiFi.getMode() == WIFI_OFF) return;
    if (ns == N_UP) LOG("net: leaving %s (%s): radio off", cur_ssid, cur_ip);
    link_net_up(false);
    services_end();
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    cur_ip[0] = 0;
    ev_got_ip = ev_lost = false;
    set_ns(N_OFF);
    ble_up("the radio off");                /* BLUETOOTH: the WiFi radio is down again -- BLE back (the gate: not in the dark house, not OFF) */
}

void net_rejoin(void)
{
    /* `wifi-join`, or a network just remembered: he wants to see it join
     * -- the radio up and HELD for this lit spell (`link auto` releases) */
    if (ns == N_OFF) { link_hold(true); return; }
    if (ns == N_UP) return;
    tried_mask = 0;
    WiFi.disconnect(false, false);
    start_scan();
}

void net_scan(void)
{
    print_scan = true;
    if (ns == N_OFF) { ble_down("wifi-scan"); WiFi.mode(WIFI_STA); }   /* BLUETOOTH: a scan by hand is the radio up too */
    if (ns == N_UP) { WiFi.scanDelete(); WiFi.scanNetworks(true, false); return; }   /* prints, keeps the join */
    if (ns == N_JOIN) WiFi.disconnect(false, false);
    start_scan();
}

bool net_up(void) { return ns == N_UP; }
bool net_radio_is_on(void) { return ns != N_OFF; }
const char *net_state_name(void) { return N_NAMES[ns]; }
const char *net_ssid(void) { return cur_ssid; }
const char *net_ip(void) { return cur_ip; }
int net_rssi(void) { return ns == N_UP ? WiFi.RSSI() : 0; }
uint32_t net_http_hits(void) { return http_hits; }

void net_status(char *out, int cap)
{
    snprintf(out, cap, "wifi %s%s%s%s%s rssi %d nets %u http-hits %lu fast-join %s%s%d", N_NAMES[ns],
             cur_ssid[0] && ns >= N_JOIN ? " " : "", cur_ssid[0] && ns >= N_JOIN ? cur_ssid : "",
             cur_ip[0] ? " " : "", cur_ip, net_rssi(), store.n_nets, (unsigned long)http_hits,
             store.fast.ssid[0] ? store.fast.ssid : "-", store.fast.ssid[0] ? " ch " : "", store.fast.ssid[0] ? (int)store.fast.channel : 0);
}

void net_tick(void)
{
    uint32_t now = millis();
    if (artota_active()) artota_tick();      /* ART OVER THE AIR: the release asked again once the veil has taken the costume off */
    if (ev_ntp) {
        ev_ntp = false;
        clock_set_epoch(ntp_epoch, "ntp");
    }
    if (ev_got_ip) {
        ev_got_ip = false;
        strncpy(cur_ip, WiFi.localIP().toString().c_str(), sizeof cur_ip - 1);
        /* FAST JOIN: remember this AP for the next radio-on (only when it changed: NVS is not written per join) */
        const uint8_t *b = WiFi.BSSID(); int ch = WiFi.channel();
        bool changed = !b || strcmp(store.fast.ssid, cur_ssid) || memcmp(store.fast.bssid, b, 6) || store.fast.channel != (uint8_t)ch;
        if (b && changed && ch > 0 && ch < 256) {
            strncpy(store.fast.ssid, cur_ssid, sizeof store.fast.ssid - 1); store.fast.ssid[sizeof store.fast.ssid - 1] = 0;
            memcpy(store.fast.bssid, b, 6); store.fast.channel = (uint8_t)ch;
            store_save_fast();
        }
        LOG("net: JOINED %s, ip %s, rssi %d, in %lu ms (%s; %lu ms since the radio came up%s)", cur_ssid, cur_ip, WiFi.RSSI(),
            (unsigned long)(now - ns_ms), fast_try ? "the fast join" : "after a scan", (unsigned long)(now - radio_on_ms),
            changed ? "; this AP remembered for the next" : "");
        fast_try = false;
        set_ns(N_UP);
        tried_mask = 0;
        services_begin();
        link_net_up(true);
    }
    if (ev_lost) {
        ev_lost = false;
        if (ns == N_UP) {
            LOG("net: LEFT %s (reason %d)", cur_ssid, ev_reason);
            cur_ip[0] = 0;
            link_net_up(false);
            services_end();
            set_ns(N_WAIT);
            ns_ms = now - RESCAN_MS + 2000;          /* look again in 2 s */
        } else if (ns == N_JOIN) {
            LOG("net: %s refused (reason %d)%s", cur_ssid, ev_reason, fast_try ? " -- the fast join; scanning" : "");
            fast_try = false;
            WiFi.disconnect(false, false);
            start_scan();                            /* the next candidate */
        }
    }
    switch (ns) {
    case N_SCAN: {
        /* the core gives up on a scan at 6 s and answers FAILED, but the
         * radio's first scan after it comes up can take longer and the
         * results still land: keep asking until our own window closes */
        int r = WiFi.scanComplete();
        if (r >= 0) pick_and_join(r);
        else if ((int32_t)(millis() - ns_ms) > 15000) { LOG("net: scan failed (%d after %lu ms)", r, (unsigned long)(millis() - ns_ms)); print_scan = false; set_ns(N_WAIT); }
        break;
    }
    case N_JOIN:
        if (fast_try && (int32_t)(millis() - ns_ms) > (int32_t)FAST_JOIN_MS) {
            /* FAST JOIN missed (the AP moved channel, a new router, out of range): the scan as before */
            LOG("net: the fast join to %s did not land in %lu ms: scanning", cur_ssid, (unsigned long)FAST_JOIN_MS);
            fast_try = false;
            WiFi.disconnect(false, false);
            start_scan();
        } else if ((int32_t)(millis() - ns_ms) > (int32_t)JOIN_MS) {
            LOG("net: %s did not answer in %lu s", cur_ssid, (unsigned long)(JOIN_MS / 1000));
            WiFi.disconnect(false, false);
            start_scan();
        }
        break;
    case N_WAIT:
        if ((int32_t)(millis() - ns_ms) > (int32_t)RESCAN_MS) start_scan();
        break;
    case N_UP:
        if (print_scan) {
            int r = WiFi.scanComplete();
            if (r >= 0) { print_results(r); WiFi.scanDelete(); }
        }
        ArduinoOTA.handle();
        http_tick();
        break;
    default: break;
    }
}
