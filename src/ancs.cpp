/* ANCS: the iPhone's notifications on the wrist, and THE WISP.
 *
 * The phone side (Apple's ANCS spec): the watch is a BLE PERIPHERAL whose
 * advertisement solicits the Notification Center Service (blestack.cpp);
 * the iPhone, told to pair from Settings > Bluetooth, connects TO the
 * watch, and the watch then acts as the GATT CLIENT on that same
 * connection -- discovers the phone's ANCS, bonds (Just Works: a "Pair"
 * tap on the phone, no code; only inside THE PAIRING WINDOW, blepair.h),
 * subscribes to the Notification Source, and for each new notification
 * asks the Control Point for its attributes -- the app id, the title,
 * the message (ANCS_MSG_MAX) and the date -- which the Data Source
 * answers; once per app it asks the app's display name ("Messages" for
 * com.apple.MobileSMS) and keeps it in a small cache. Nothing else is
 * read, nothing is stored. Once bonded the phone reconnects on its own
 * whenever the watch advertises again.
 *
 * The stack is NimBLE (h2zero/NimBLE-Arduino 2.x on the core's BLE
 * controller): the core's own Bluedroid is the heavier stack by far,
 * and NimBLE 2.x has the one thing ANCS needs -- a client that rides a
 * connection the phone opened to our server (NimBLEServer::getClient).
 * The stack is UP ONLY WHILE THE WIFI RADIO IS DOWN (ruling 58; ble.cpp's
 * gate) and comes back whole each time (measured 2026-10-06: the TLS
 * session held reads 63.2 KB free before BLE ever rose and 63.2 KB after
 * each up/down cycle -- README "BLUETOOTH").
 *
 * THE GAP (2026-10-06): a notice that lands while the stack is down --
 * a dial (10-15 s), the dark house (minutes) -- replays at the next
 * subscription as PRE-EXISTING; iOS replays every notice still in the
 * Center that way. Those are asked for their date too, and the ones
 * younger than the gap itself (since the phone was last on, plus a
 * little; an hour at most; REPLAY_S at a boot) fly in as wisps -- what
 * he missed while the glass was dark meets him at the raise; the older
 * ones are counted, as before. Without a clock none fly.
 *
 * Threads: the NimBLE host task runs the callbacks (core 0); a small
 * WORKER task does the blocking GATT work (the security ask, the
 * discovery, the subscriptions, the attribute asks) so loop() never
 * waits on the air; the worker hands finished events to loop() through
 * a spinlocked ring, and only loop() touches the wisps, the link and
 * the frame.
 *
 * THE WISP is wisp.cpp's (the desk's doorbell sends one over the link
 * too, "THE WISP FROM THE DESK" in the README); this module hands it the
 * phone's notices through wisp_arrive, and takes one back (wisp_leave)
 * when a notice is cleared on the phone. A silent notice (the phone did
 * not announce it either) is logged, never a wisp. */
#include "ancs.h"
#include "app.h"
#include "link.h"
#include "wisp.h"
#include "notice.h"    /* THE NOTICE'S OWN REACTION: the category -> the bump, the halo */
#include "house.h"
#include "blestack.h"
#include "ble.h"        /* THE DARK EAR: a notice on a dark glass is felt now */
#include "cts.h"        /* THE PHONE'S HOUR: the bonded phone's Current Time Service, read once per connection after the subscription */
#include "store.h"      /* ... its zone into tz_min while the desk never said one */
#include "ancsact.h"    /* THE NOTICE CLEARED FROM THE WRIST (T17): the six-byte negative action, the one word back */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>
#include <time.h>

/* --- the tunables ------------------------------------------------------------ */

static const uint32_t PAIR_MS = 60000;        /* the human taps "Pair" on the phone within this */
static const uint32_t RESP_MS = 3000;         /* the phone answers an attribute ask within this */
static const uint32_t STRANGER_WAIT_MS = 1500;/* a stranger in the window gets this long to start the pairing itself (the desk does) before the watch asks */
static const uint16_t TITLE_MAX = 32;         /* asked of the phone; the wisp keeps no more */
static const uint32_t REPLAY_S = 120;         /* THE DIAL GAP: a pre-existing notice younger than this flies in -- at a boot, when the gap is not known */
static const uint32_t REPLAY_SLACK_S = 30;    /* ...otherwise younger than the gap itself (since the phone was last on) plus this */
static const uint32_t REPLAY_MAX_S = 3600;    /* ...an hour at most: older notices are the Center's, not the wrist's */

/* --- ANCS on the wire -------------------------------------------------------- */

static NimBLEUUID UUID_SVC("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static NimBLEUUID UUID_NS ("9FBF120D-6301-42D9-8C58-25E699A21DBD");   /* Notification Source: notify */
static NimBLEUUID UUID_CP ("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");   /* Control Point: write */
static NimBLEUUID UUID_DS ("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");   /* Data Source: notify */

enum { EV_ADDED = 0, EV_MODIFIED = 1, EV_REMOVED = 2 };
enum { FLAG_SILENT = 1, FLAG_IMPORTANT = 2, FLAG_PREEXISTING = 4 };
enum { CMD_GET_ATTRS = 0, CMD_GET_APP_ATTRS = 1 };     /* the third, PerformNotificationAction 2, is ancsact.h's (T17) */
enum { ATTR_APP_ID = 0, ATTR_TITLE = 1, ATTR_MESSAGE = 3, ATTR_DATE = 5 };
enum { APP_ATTR_NAME = 0 };
static const char *const CATEGORY[12] = {
    "other", "call", "missed call", "voicemail", "social", "schedule",
    "email", "news", "health", "business", "location", "entertainment"
};

/* --- state ------------------------------------------------------------------- */

enum AncsState { A_OFF, A_ADV, A_CONNECTED, A_PAIRING, A_READY };
static const char *const A_NAMES[] = { "off", "advertising", "connected", "pairing", "ready" };
static volatile AncsState st = A_OFF;
static bool on = false;
static NimBLEServer *server = nullptr;
static NimBLEClient *client = nullptr;                    /* the phone's (the worker's; nulled at a leave) */
static NimBLERemoteCharacteristic *ch_ns = nullptr, *ch_cp = nullptr, *ch_ds = nullptr;
static volatile uint16_t phone = BLE_HS_CONN_HANDLE_NONE; /* the connection the Notification Center was found on */
static volatile bool authed = false, auth_failed = false, ready = false;
static volatile bool worker_busy = false;
static volatile uint32_t gen = 0;             /* bumped on every leave/off: stale work stops */
static char peer[20] = "";
static uint32_t notes = 0;
static uint32_t cleared = 0;                  /* T17: negative actions the phone took */
static volatile uint32_t replayed = 0, replayed_old = 0;   /* pre-existing notices: seen; too old to fly */
static uint32_t replayed_seen = 0, replayed_ms = 0;
static uint32_t phone_off_ms = 0;             /* when the phone was last on (0 = never since boot): the gap to replay over */
static uint32_t replay_s = REPLAY_S;          /* this subscription's window */

/* the phone's events, from the worker to loop() */
typedef struct {
    uint8_t  kind, flags, cat;
    uint32_t uid;
    uint32_t age_s;                           /* a pre-existing notice's age by its date (0 = fresh / unknown) */
    char     app[WISP_APP_MAX];
    char     title[WISP_TITLE_MAX];
    char     msg[ANCS_MSG_MAX];
} ancs_ev_t;
#define EV_RING 6
static ancs_ev_t ev_ring[EV_RING];
static volatile uint8_t ev_head = 0, ev_tail = 0;
static portMUX_TYPE ev_mux = portMUX_INITIALIZER_UNLOCKED;

/* the worker's jobs: a new peer to look at, a notice to ask about */
enum { JOB_PEER = 0, JOB_NOTE = 1, JOB_CLEAR = 2 };   /* JOB_CLEAR (T17): the negative action for uid, off the frame as the asks are */
typedef struct { uint8_t kind; uint16_t handle; bool bonded; uint32_t uid; uint8_t flags, cat; } job_t;
static QueueHandle_t job_q = nullptr;
static TaskHandle_t worker_task = nullptr;

/* the attribute answer being assembled (Data Source, host task only) */
#define RESP_CAP 320
static uint8_t resp_buf[RESP_CAP];
static volatile int  resp_n = 0;
static volatile bool resp_active = false, resp_done = false;
static volatile uint8_t resp_cmd = 0;
static volatile uint32_t resp_uid = 0;
static char resp_app[WISP_APP_MAX], resp_title[WISP_TITLE_MAX], resp_msg[ANCS_MSG_MAX], resp_date[20], resp_name[ANCS_NAME_MAX];

/* THE PHONE'S HOUR (cts.h): what the worker read off the phone's
 * Current Time Service, for loop() to set the clock by (only loop()
 * touches the clock and the store). `pending` is the handshake. */
static struct {
    volatile bool pending;
    uint32_t local_epoch;                     /* the phone's wall clock as an epoch-shaped count */
    uint32_t at_ms;                           /* millis() at the read: the apply adds what passed since */
    bool     have_off; int off_min;           /* 2A0F's zone + DST in minutes, when the phone knows */
} hour;
static uint32_t hour_reads = 0, hour_takes = 0;   /* the status line's tally: reads that parsed, clocks set by them */

/* the app names, once per app id */
#define NAMES 8
static struct { char app[WISP_APP_MAX]; char name[ANCS_NAME_MAX]; } names[NAMES];
static int names_at = 0;

static void ev_push(const ancs_ev_t *e)
{
    portENTER_CRITICAL(&ev_mux);
    uint8_t next = (uint8_t)((ev_head + 1) % EV_RING);
    if (next != ev_tail) { ev_ring[ev_head] = *e; ev_head = next; }
    portEXIT_CRITICAL(&ev_mux);
}

static bool ev_pop(ancs_ev_t *e)
{
    bool got = false;
    portENTER_CRITICAL(&ev_mux);
    if (ev_tail != ev_head) { *e = ev_ring[ev_tail]; ev_tail = (uint8_t)((ev_tail + 1) % EV_RING); got = true; }
    portEXIT_CRITICAL(&ev_mux);
    return got;
}

/* --- the host task's callbacks ----------------------------------------------- */

/* Notification Source: 8 bytes -- EventID, EventFlags, CategoryID,
 * CategoryCount, NotificationUID (LE). An added notice goes to the
 * worker for its attributes (a pre-existing one too, for its date: THE
 * DIAL GAP); a removed one straight to loop(). */
static void on_ns(NimBLERemoteCharacteristic *c, uint8_t *d, size_t n, bool is_notify)
{
    (void)c; (void)is_notify;
    if (n < 8) return;
    uint32_t uid = (uint32_t)d[4] | ((uint32_t)d[5] << 8) | ((uint32_t)d[6] << 16) | ((uint32_t)d[7] << 24);
    if (d[0] == EV_ADDED) {
        if (d[1] & FLAG_PREEXISTING) replayed++;
        job_t j = { JOB_NOTE, phone, true, uid, d[1], d[2] };
        if (job_q) xQueueSend(job_q, &j, 0);
    } else if (d[0] == EV_REMOVED) {
        ancs_ev_t e; memset(&e, 0, sizeof e);
        e.kind = EV_REMOVED; e.uid = uid; e.flags = d[1]; e.cat = d[2];
        ev_push(&e);
    }
}

/* Data Source: the answer to GetNotificationAttributes (CommandID 0,
 * UID LE, then (AttributeID, Length LE16, bytes) per attribute asked)
 * or GetAppAttributes (CommandID 1, the app id NUL-ended, then the
 * same), possibly over several notifies. */
static void on_ds(NimBLERemoteCharacteristic *c, uint8_t *d, size_t n, bool is_notify)
{
    (void)c; (void)is_notify;
    if (!resp_active) return;
    int room = RESP_CAP - resp_n;
    if ((int)n > room) n = (size_t)room;
    memcpy(resp_buf + resp_n, d, n);
    resp_n += (int)n;
    if (resp_n < 2 || resp_buf[0] != resp_cmd) { if (resp_n >= 2 && resp_buf[0] != resp_cmd) resp_n = 0; return; }
    int pos, want;
    if (resp_cmd == CMD_GET_ATTRS) {
        if (resp_n < 5) return;
        uint32_t uid = (uint32_t)resp_buf[1] | ((uint32_t)resp_buf[2] << 8) | ((uint32_t)resp_buf[3] << 16) | ((uint32_t)resp_buf[4] << 24);
        if (uid != resp_uid) { resp_n = 0; return; }
        pos = 5; want = 4;
    } else {
        int k = 1;
        while (k < resp_n && resp_buf[k]) k++;
        if (k >= resp_n) return;                          /* the app id's NUL not here yet */
        pos = k + 1; want = 1;
    }
    int parsed = 0;
    while (pos + 3 <= resp_n) {
        uint8_t id = resp_buf[pos];
        int len = (int)resp_buf[pos + 1] | ((int)resp_buf[pos + 2] << 8);
        if (pos + 3 + len > resp_n) break;
        char *dst = nullptr; int cap = 0;
        if (resp_cmd == CMD_GET_ATTRS) {
            if (id == ATTR_APP_ID)       { dst = resp_app;   cap = WISP_APP_MAX; }
            else if (id == ATTR_TITLE)   { dst = resp_title; cap = WISP_TITLE_MAX; }
            else if (id == ATTR_MESSAGE) { dst = resp_msg;   cap = ANCS_MSG_MAX; }
            else if (id == ATTR_DATE)    { dst = resp_date;  cap = sizeof resp_date; }
        } else if (id == APP_ATTR_NAME)  { dst = resp_name;  cap = ANCS_NAME_MAX; }
        if (dst) {
            /* copied without iOS's invisible direction marks (U+200E / U+200F, three bytes each:
             * the phone prefixes every name with one; the glass would draw it as a stray glyph) */
            int k = 0; const uint8_t *src = resp_buf + pos + 3;
            for (int i = 0; i < len && k < cap - 1; i++) {
                if (i + 2 < len && src[i] == 0xE2 && src[i + 1] == 0x80 && (src[i + 2] == 0x8E || src[i + 2] == 0x8F)) { i += 2; continue; }
                dst[k++] = (char)src[i];
            }
            dst[k] = 0;
        }
        pos += 3 + len;
        parsed++;
    }
    if (parsed >= want) { resp_done = true; resp_active = false; }
}

/* --- the worker: the blocking GATT work, off the frame ------------------------- */

static bool ask(uint8_t cmd, const uint8_t *req, int n, uint32_t uid)
{
    NimBLERemoteCharacteristic *cp = ch_cp;
    if (!cp) return false;
    uint32_t g = gen;
    resp_app[0] = resp_title[0] = resp_msg[0] = resp_date[0] = resp_name[0] = 0;
    resp_cmd = cmd; resp_uid = uid; resp_n = 0; resp_done = false; resp_active = true;
    if (!cp->writeValue(req, (size_t)n, true)) { resp_active = false; return false; }
    uint32_t t0 = millis();
    while (!resp_done && g == gen && millis() - t0 < RESP_MS) vTaskDelay(pdMS_TO_TICKS(10));
    resp_active = false;
    return resp_done && g == gen;
}

/* the date the phone gives: yyyyMMdd'T'HHmmSS, the phone's local time; the watch's clock is local too */
static uint32_t age_from_date(const char *date)
{
    if (!date[0] || !clock_is_set()) return 0;
    struct tm t; memset(&t, 0, sizeof t);
    if (sscanf(date, "%4d%2d%2dT%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) return 0;
    t.tm_year -= 1900; t.tm_mon -= 1;
    struct tm now; uint32_t ep; clock_local(&now, &ep);
    now.tm_isdst = 0; t.tm_isdst = 0;
    time_t a = mktime(&t), b = mktime(&now);              /* both local, the same offset: the difference is the age */
    if (a == (time_t)-1 || b == (time_t)-1 || b < a) return 0;
    return (uint32_t)(b - a);
}

static void name_remember(const char *app, const char *name)
{
    for (int i = 0; i < NAMES; i++) if (!strcmp(names[i].app, app)) { strncpy(names[i].name, name, ANCS_NAME_MAX - 1); names[i].name[ANCS_NAME_MAX - 1] = 0; return; }
    strncpy(names[names_at].app, app, WISP_APP_MAX - 1); names[names_at].app[WISP_APP_MAX - 1] = 0;
    strncpy(names[names_at].name, name, ANCS_NAME_MAX - 1); names[names_at].name[ANCS_NAME_MAX - 1] = 0;
    names_at = (names_at + 1) % NAMES;
}

const char *ancs_app_name(const char *app)
{
    if (!app) return "";
    for (int i = 0; i < NAMES; i++) if (names[i].app[0] && !strcmp(names[i].app, app)) return names[i].name;
    return "";
}

/* THE PHONE'S HOUR: once per connection, after the subscription (the
 * same bonded link; iOS serves CTS to a bonded central only). Two reads,
 * ~0.3 KB transient on the worker (the remote service and its two
 * characteristics, freed with the client at the leave); the bytes go
 * to loop() through `hour`. A phone without the service, or a frame the
 * rules refuse, is one log line and nothing else. */
static void read_phone_hour(NimBLEClient *cl, uint32_t g)
{
    NimBLERemoteService *ts = cl->getService(NimBLEUUID((uint16_t)CTS_UUID_SVC));
    if (g != gen || !cl->isConnected()) return;
    if (!ts) { LOG("cts: %s serves no Current Time (iOS serves it to a bonded phone only)", peer); return; }
    NimBLERemoteCharacteristic *ct = ts->getCharacteristic(NimBLEUUID((uint16_t)CTS_UUID_TIME));
    if (g != gen || !cl->isConnected()) return;
    if (!ct) { LOG("cts: %s's Current Time has no 2A2B", peer); return; }
    NimBLEAttValue v = ct->readValue();
    uint32_t at = millis();
    if (g != gen || !cl->isConnected()) return;
    uint32_t local;
    if (!cts_local_epoch(v.data(), (int)v.size(), &local)) { LOG("cts: %s's hour refused (%u bytes, year %u)", peer, (unsigned)v.size(), v.size() >= 2 ? (unsigned)(v.data()[0] | (v.data()[1] << 8)) : 0u); return; }
    bool have_off = false; int off_min = 0;
    NimBLERemoteCharacteristic *lt = ts->getCharacteristic(NimBLEUUID((uint16_t)CTS_UUID_LOCAL));
    if (g != gen || !cl->isConnected()) return;
    if (lt) {
        NimBLEAttValue w = lt->readValue();
        if (g != gen) return;
        if (w.size() >= CTS_LOCAL_LEN) have_off = cts_offset_min(w.data()[0], w.data()[1], &off_min);
    }
    hour_reads++;
    hour.local_epoch = local; hour.at_ms = at; hour.have_off = have_off; hour.off_min = off_min;
    hour.pending = true;                       /* loop() applies (ancs_tick) */
}

static void look_at_peer(const job_t *j)
{
    uint32_t g = gen;
    if (!server || phone != BLE_HS_CONN_HANDLE_NONE) return;   /* a phone is on already: the second peer is the desk's */
    NimBLEClient *cl = server->getClient(j->handle);
    if (!cl || !cl->isConnected()) return;
    client = cl;
    strncpy(peer, cl->getPeerAddress().toString().c_str(), sizeof peer - 1);
    st = A_CONNECTED;
    NimBLEConnInfo ci = cl->getConnInfo();
    if (!ci.isEncrypted()) {
        /* a moment first: the desk subscribes to the air door within a second of connecting and encrypts
         * itself when it writes (bonded, or pairing from the laptop inside THE PAIRING WINDOW) -- it is never
         * asked (10-06 15:02: the ask parked the worker 60 s on a laptop that would not answer, and a phone
         * connecting meanwhile would have waited behind it); the phone waits to be asked, so after the
         * moment the ask goes out */
        { uint32_t t0 = millis(); while (!authed && !auth_failed && g == gen && millis() - t0 < STRANGER_WAIT_MS
                                         && blestack_peer_kind(j->handle) != PEER_DESK) vTaskDelay(pdMS_TO_TICKS(50)); }
        if (g != gen || !cl->isConnected()) { client = nullptr; return; }
        if (blestack_peer_kind(j->handle) == PEER_DESK) { st = A_ADV; client = nullptr; return; }   /* the desk: the air door's, not a phone */
        if (!authed && !auth_failed) {
            st = A_PAIRING;
            if (!cl->secureConnection(true)) LOG("ancs: security ask refused; waiting on the other side");
            uint32_t t0 = millis();
            while (!authed && !auth_failed && g == gen && cl->isConnected() && millis() - t0 < PAIR_MS) vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (g != gen || !cl->isConnected()) { client = nullptr; return; }
        if (!authed) { LOG("ancs: not paired with %s (%s)", peer, auth_failed ? "refused" : "no answer in 60 s"); st = A_CONNECTED; client = nullptr; return; }
    }
    NimBLERemoteService *svc = cl->getService(UUID_SVC);
    if (g != gen || !cl->isConnected()) { client = nullptr; return; }
    if (!svc) { LOG("ancs: %s has no Notification Center (the desk, or not an iPhone)", peer); st = A_ADV; client = nullptr; return; }   /* not the phone: the client is back to waiting for one */
    NimBLERemoteCharacteristic *ns = svc->getCharacteristic(UUID_NS);
    NimBLERemoteCharacteristic *cp = svc->getCharacteristic(UUID_CP);
    NimBLERemoteCharacteristic *ds = svc->getCharacteristic(UUID_DS);
    if (g != gen || !cl->isConnected()) { client = nullptr; return; }
    if (!ns || !cp || !ds) { LOG("ancs: the service is missing a characteristic (ns %d cp %d ds %d)", !!ns, !!cp, !!ds); client = nullptr; return; }
    if (!ds->subscribe(true, on_ds) || g != gen) { LOG("ancs: the data source refused the subscription"); client = nullptr; return; }
    if (!ns->subscribe(true, on_ns) || g != gen) { LOG("ancs: the notification source refused the subscription"); client = nullptr; return; }
    ch_ns = ns; ch_cp = cp; ch_ds = ds;
    phone = j->handle;
    blestack_peer_is(j->handle, PEER_PHONE);
    /* THE GAP: what landed while the stack was down (a dial, the dark house) flies in now -- as old as the gap, an hour at most */
    replay_s = phone_off_ms ? (uint32_t)(millis() - phone_off_ms) / 1000u + REPLAY_SLACK_S : REPLAY_S;
    if (replay_s > REPLAY_MAX_S) replay_s = REPLAY_MAX_S;
    ready = true;
    st = A_READY;
    LOG("ancs: READY -- %s's notices come to the wrist (the Center's existing ones replay now: the ones under %lu s old fly in)", peer, (unsigned long)replay_s);
    read_phone_hour(cl, g);                   /* THE PHONE'S HOUR: the clock off the same bonded link, once */
}

static void fetch(const job_t *j)
{
    if (!ready || !client || !ch_cp || !client->isConnected()) return;
    uint8_t cmd[16] = {
        CMD_GET_ATTRS,
        (uint8_t)j->uid, (uint8_t)(j->uid >> 8), (uint8_t)(j->uid >> 16), (uint8_t)(j->uid >> 24),
        ATTR_APP_ID,
        ATTR_TITLE,   (uint8_t)TITLE_MAX, (uint8_t)(TITLE_MAX >> 8),
        ATTR_MESSAGE, (uint8_t)(ANCS_MSG_MAX - 1), (uint8_t)((ANCS_MSG_MAX - 1) >> 8),
        ATTR_DATE
    };
    if (!ask(CMD_GET_ATTRS, cmd, 13, j->uid)) { if (gen) LOG("ancs: no answer about uid %lu", (unsigned long)j->uid); return; }
    ancs_ev_t e; memset(&e, 0, sizeof e);
    e.kind = EV_ADDED; e.uid = j->uid; e.flags = j->flags; e.cat = j->cat;
    strncpy(e.app, resp_app, sizeof e.app - 1);
    strncpy(e.title, resp_title, sizeof e.title - 1);
    strncpy(e.msg, resp_msg, sizeof e.msg - 1);
    if (j->flags & FLAG_PREEXISTING) e.age_s = age_from_date(resp_date);
    /* the app's display name, once per app: a second ask, cached */
    if (e.app[0] && !ancs_app_name(e.app)[0]) {
        uint8_t req[WISP_APP_MAX + 3]; int n = 0;
        req[n++] = CMD_GET_APP_ATTRS;
        int k = (int)strlen(e.app); memcpy(req + n, e.app, (size_t)k); n += k; req[n++] = 0;
        req[n++] = APP_ATTR_NAME;
        if (ask(CMD_GET_APP_ATTRS, req, n, 0) && resp_name[0]) name_remember(e.app, resp_name);
        else name_remember(e.app, "");                    /* asked once; not asked again this stack-up */
    }
    ev_push(&e);
}

/* THE NOTICE CLEARED FROM THE WRIST (T17): one write to the Control
 * Point -- PerformNotificationAction, the NEGATIVE action (ancsact.h
 * builds nothing else) for the notice's uid; the phone takes it off its
 * shade (a ringing call: declined) and says so back as an EV_REMOVED,
 * which takes the wisp as any clearing on the phone does. A phone that
 * no longer has the notice refuses the write (ATT 0xA2): a line, nothing
 * else. With response, as the asks are, so a refusal is heard. */
static void clear_on_phone(const job_t *j)
{
    NimBLERemoteCharacteristic *cp = ch_cp;
    if (!ready || !client || !cp || !client->isConnected()) { LOG("ancs: clear uid %lu: the phone is not on", (unsigned long)j->uid); return; }
    uint8_t f[ANCSACT_LEN]; int n = ancsact_frame(f, j->uid, ANCSACT_NEGATIVE);
    if (!n) return;
    if (cp->writeValue(f, (size_t)n, true)) { cleared++; LOG("ancs: uid %lu cleared on the phone (the negative action: dismissed; a call, declined)", (unsigned long)j->uid); }
    else LOG("ancs: the phone refused the clear of uid %lu (gone from the Center already?)", (unsigned long)j->uid);
}

static void worker(void *arg)
{
    (void)arg;
    job_t j;
    for (;;) {
        if (xQueueReceive(job_q, &j, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        if (!on) continue;
        worker_busy = true;
        if (j.kind == JOB_PEER) look_at_peer(&j);
        else if (j.kind == JOB_CLEAR) clear_on_phone(&j);
        else if (j.handle == phone) fetch(&j);
        worker_busy = false;
    }
}

/* --- the wisp ------------------------------------------------------------------- */

/* the mote itself lives in wisp.cpp; this side keeps only which app each
 * of the phone's notices belonged to, so a notice cleared on the phone
 * (EV_REMOVED, by uid) can take its wisp with it */
#define NOTE_MAP 8
static struct { uint32_t uid; char app[WISP_APP_MAX]; } note_map[NOTE_MAP];
static int note_map_at = 0;

static void note_remember(uint32_t uid, const char *app)
{
    note_map[note_map_at].uid = uid;
    strncpy(note_map[note_map_at].app, app, WISP_APP_MAX - 1);
    note_map[note_map_at].app[WISP_APP_MAX - 1] = 0;
    note_map_at = (note_map_at + 1) % NOTE_MAP;
}

static void note_cleared(uint32_t uid)
{
    for (int i = 0; i < NOTE_MAP; i++)
        if (note_map[i].uid == uid && note_map[i].app[0]) { wisp_leave(note_map[i].app); note_map[i].app[0] = 0; return; }
}

/* THE NOTICE CLEARED FROM THE WRIST (T17): the page's row -> the uid.
 * The ledger is per app (the same app again takes the top line), so the
 * newest uid still remembered for that app is the notice the card shows;
 * one the phone cleared already (note_cleared) is gone from the map. */
static uint32_t note_uid_newest(const char *app)
{
    for (int k = 1; k <= NOTE_MAP; k++) {
        int i = (note_map_at - k + NOTE_MAP) % NOTE_MAP;
        if (note_map[i].app[0] && !strcmp(note_map[i].app, app)) return note_map[i].uid;
    }
    return 0;
}

bool ancs_clear(const char *app)
{
    if (!app || !app[0]) return false;
    uint32_t uid = note_uid_newest(app);
    if (!ancsact_uid_real(uid)) { LOG("ancs: nothing to clear for %s on the phone (%s)", app, uid ? "the wrist's own fake" : "the desk's, or cleared on the phone already"); return false; }
    if (!ready) { LOG("ancs: cannot clear %s -- no phone on (%s)", app, A_NAMES[st]); return false; }
    job_t j = { JOB_CLEAR, phone, true, uid, 0, 0 };
    if (!job_q || xQueueSend(job_q, &j, 0) != pdTRUE) { LOG("ancs: cannot clear %s -- the worker's queue is full", app); return false; }
    LOG("ancs: clearing %s (uid %lu) on the phone -- the negative action, never the positive", app, (unsigned long)uid);
    return true;
}

/* THE WISP's arrival, with THE NOTICE'S OWN REACTION (notice.h: the
 * category and the flags -> the haptic, its repeats, the halo) and the
 * message for the ledger; the app's display name, when the cache has
 * it, goes on the ledger line too */
static int deliver(const ancs_ev_t *e)
{
    notice_react_t r; notice_react(e->cat, e->flags & ~FLAG_PREEXISTING, &r);   /* a replayed-young notice is as fresh to him */
    if (app.state == 2) {
        /* THE DARK EAR: felt now, on the dark glass; the wisp waits for the light with no second bump at its landing */
        ble_dark_notice(r.haptic, r.repeats);
        r.haptic = -1; r.repeats = 0;
    }
    int rc = wisp_arrive_from(WISP_SRC_PHONE, e->app, e->title, e->msg[0] ? e->msg : NULL, &r, house_holding());   /* THE WISP'S INK (E42): the phone's wear the shade */
    if (rc == WISP_NO_HOUSE) wisp_ledger_keep(e->app, e->title, e->msg[0] ? e->msg : NULL);   /* THE PHONE'S NOTICE IS HIS: on the page whether or not the slime is home (since E43 the EMPTY house keeps it at the door itself; this is the belt-and-braces) */
    const char *name = ancs_app_name(e->app);
    if (name[0]) wisp_ledger_name(e->app, name);
    return rc;
}

/* --- the public face ---------------------------------------------------------- */

void ancs_begin(void)
{
    job_q = xQueueCreate(8, sizeof(job_t));
    xTaskCreatePinnedToCore(worker, "ancs", 4096, nullptr, 1, &worker_task, 1);
    memset(names, 0, sizeof names);
    LOG("ancs: the iPhone's notices as wisps (the stack is blestack.cpp's to raise); worker task up, heap %u free", (unsigned)ESP.getFreeHeap());
}

void ancs_attach(NimBLEServer *s)
{
    server = s;
    on = true;
    st = A_ADV;
    phone = BLE_HS_CONN_HANDLE_NONE;
    ready = authed = auth_failed = false;
    ch_ns = ch_cp = ch_ds = nullptr; client = nullptr;
}

void ancs_detach(void)
{
    on = false;
    if (ready) phone_off_ms = millis();           /* the stack goes down with the phone on: the gap starts now */
    ready = false;
    ch_ns = ch_cp = ch_ds = nullptr;
    gen++;
}

void ancs_quiesce(void)
{
    uint32_t t0 = millis();
    while (worker_busy && millis() - t0 < 600) delay(10);
    if (job_q) xQueueReset(job_q);
    client = nullptr; server = nullptr;
    phone = BLE_HS_CONN_HANDLE_NONE;
    st = A_OFF;
}

void ancs_peer_connected(NimBLEConnInfo &ci, bool bonded)
{
    authed = false; auth_failed = false;
    gen++;
    job_t j = { JOB_PEER, ci.getConnHandle(), bonded, 0, 0, 0 };
    if (job_q) xQueueSend(job_q, &j, 0);
}

void ancs_peer_authed(uint16_t h, bool ok)
{
    (void)h;
    if (ok) authed = true; else auth_failed = true;
}

void ancs_peer_left(uint16_t h)
{
    if (h == phone) {
        phone_off_ms = millis();
        ready = false; ch_ns = ch_cp = ch_ds = nullptr; client = nullptr;
        phone = BLE_HS_CONN_HANDLE_NONE;
        st = on ? A_ADV : A_OFF;
    }
    gen++;
}

void ancs_tick(float dt)
{
    (void)dt;
    if (hour.pending) {
        /* THE PHONE'S HOUR, applied on loop(): the zone first (while the desk never said one; RAM only),
         * then UTC by the phone's offset -- or the watch's own when the phone does not know -- plus the
         * seconds since the read; the clock reset only when unset or more than CTS_OFF_S off; nothing lit */
        hour.pending = false;
        if (cts_take_tz(store.tz_said, hour.have_off, hour.off_min, (int)store.tz_min)) {
            store.tz_min = hour.off_min;
            LOG("cts: tz %+d min from the phone (the desk never said one; not stored)", hour.off_min);
        }
        uint32_t utc = cts_utc(hour.local_epoch, hour.have_off ? hour.off_min : (int)store.tz_min) + (millis() - hour.at_ms) / 1000u;
        uint32_t now = (uint32_t)time(NULL);
        if (cts_take(clock_is_set(), now, utc)) { hour_takes++; clock_set_epoch(utc, "the phone"); }
        else LOG("cts: the phone's hour agrees (%+ld s; tz %+d min %s)", (long)((int64_t)utc - (int64_t)now), hour.have_off ? hour.off_min : (int)store.tz_min, hour.have_off ? "the phone's" : "the watch's own");
    }
    ancs_ev_t e;
    while (ev_pop(&e)) {
        if (e.kind == EV_REMOVED) { note_cleared(e.uid); continue; }
        const char *cat = e.cat < 12 ? CATEGORY[e.cat] : "?";
        bool pre = (e.flags & FLAG_PREEXISTING) != 0;
        bool quiet = (e.flags & FLAG_SILENT) != 0;
        /* THE DIAL GAP: a replayed notice flies only while young; the rest are counted */
        if (pre && (!e.age_s || e.age_s > replay_s)) { replayed_old++; continue; }
        notes++;
        LOG("ancs: %s%s%s -- \"%s\" (%s%s%s%s)%s%s%s", e.app[0] ? e.app : "?", ancs_app_name(e.app)[0] ? " = " : "", ancs_app_name(e.app), e.title, cat,
            pre ? ", replayed " : "", pre ? "young" : "", quiet ? ", silent" : "", e.msg[0] ? ": \"" : "", e.msg, e.msg[0] ? "\"" : "");
        if (quiet || !e.app[0]) continue;                 /* the phone did not announce it: no wisp */
        /* THE WISP (wisp.cpp): while he is home; on a dark glass it waits for the light */
        if (deliver(&e) == WISP_OK) note_remember(e.uid, e.app);
        else LOG("ancs: no wisp for %s -- the house is %s (the notice kept on the page)", e.app, house_state_name());
        link_sense("wrist:doorbell", e.app);              /* one a kind per 30 s: the link's rule */
    }
    if (replayed != replayed_seen) { replayed_seen = replayed; replayed_ms = millis(); }
    else if (replayed_seen && millis() - replayed_ms > 2500) {
        LOG("ancs: %lu notice%s already in the Center replayed (%lu too old to fly)", (unsigned long)replayed_seen, replayed_seen == 1 ? "" : "s", (unsigned long)replayed_old);
        replayed = replayed_seen = 0; replayed_old = 0;
    }
}

void ancs_fake(const char *appid, const char *title, const char *msg)
{
    ancs_ev_t e; memset(&e, 0, sizeof e);
    e.kind = EV_ADDED; e.uid = ANCSACT_FAKE_BASE + (wisp_sent() & 0xFFFF); e.cat = 0;   /* the wrist's own: never cleared on the phone (ancsact.h) */
    strncpy(e.app, appid && appid[0] ? appid : "com.slime.fake", sizeof e.app - 1);
    strncpy(e.title, title ? title : "a wisp with no phone", sizeof e.title - 1);
    if (msg) strncpy(e.msg, msg, sizeof e.msg - 1);
    ev_push(&e);
}

bool ancs_connected(void) { return st == A_READY; }

void ancs_status(char *out, int cap)
{
    snprintf(out, cap, "ancs %s%s%s notes %lu wisps %d hour %lu/%lu cleared %lu", A_NAMES[st],
             st >= A_CONNECTED && peer[0] ? " " : "", st >= A_CONNECTED ? peer : "",
             (unsigned long)notes, wisp_count(), (unsigned long)hour_takes, (unsigned long)hour_reads,   /* THE PHONE'S HOUR: clocks set / hours read */
             (unsigned long)cleared);   /* THE NOTICE CLEARED FROM THE WRIST */
}
