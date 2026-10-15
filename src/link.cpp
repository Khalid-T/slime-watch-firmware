#include "link.h"
#include "ear.h"                  /* THE EAR'S LOW GEAR: left before the night's errand dials */
#include "grain.h"          /* THE GRAIN: the line's numbers become the wrist's knobs */
#include "app.h"
#include "store.h"
#include "daylog.h"
#include "word.h"
#include "sky.h"        /* THE SKY: the weather word, fetched once per link after the word */
#include "hours.h"      /* THE REAL SUN: `ok time ... sunrise=HH:MM sunset=HH:MM` -> the hours' light */
#include "house.h"      /* THE HOUSE: the lease answers land there; it says whether the wrist holds */
#include "wisp.h"       /* THE WISP FROM THE DESK: the desk's doorbell, passed on as one ask */
#include "wardrobe.h"   /* THE WRIST WARDROBE: `form=` / `pal=` on the handover's `ok held`, the grain line, and a pushed `look` */
#include "net.h"        /* THE RADIO BUDGET: the link switches the radio (net_radio_on / off) */
#include "bleuart.h"    /* THE AIR DOOR: the fourth transport -- the desk's lines over BLE (TR_BLE), the dock without the cable */
#include "ble.h"        /* ...and the desk's `ble pair` ask: THE PAIRING WINDOW opened over any open door */
#include "alarmask.h"   /* THE ALARM FROM THE DESK (T13): the `alarm HH:MM|off` ask's grammar, pure */
#include "holdask.h"    /* THE FLASH FROM THE DESK (T14): the `hold <s>` ask's grammar and its clock, pure */
#include "clockwork.h"  /* ...and the alarm as it stands, for the answer */
#include "radio.h"      /* ...for a reason, decided Arduino-free (tools/radio_test.cpp) */
#include "rest.h"       /* THE NIGHT RELEASE: the night's onset and the rising, taken from rest.cpp's verdict */
#include "vigor.h"      /* BATTERY MANNERS: under 10 % the claim comes half as often (radio_in_t.lean) */
#include "dockhand.h"   /* THE DOCK HANDS HIM OVER (C24, 2026-10-09): the cable's edges while holding / worn -> the release / the claim, pure */
#include "gel.h"        /* THE TRACE ANSWERED (C26): GEL_WALL_NAMES on the release line */
#include "refusal.h"    /* THE REFUSAL SAID ONCE (T5, 2026-10-12): a door's repeated word said once, then counted (`link`, doctor), pure */
#include "proof.h"
#include "wdt.h"        /* the errand feeds the watchdog */      /* THE DOOR'S OWN PROOF (T10, 2026-10-13): the wrist's nonce on the hello; a door that cannot HMAC it back gets a read-only session, pure */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <mbedtls/md.h>
#include <mbedtls/platform.h>   /* mbedtls_platform_set_calloc_free: THE TLS SESSION IN PSRAM (below) */
#include <esp_heap_caps.h>
#include <string.h>
#include <ctype.h>      /* isxdigit / tolower: link_sign_check (the door's auth, lent to `POST /art`) */
#include <errno.h>
#include <fcntl.h>

/* --- the tunables ------------------------------------------------------------ */
/* the lease verb's cadence over a session is radio.cpp's now (THE LIT
 * GLASS POLLS: LINK_BEAT_S 60 with the glass dim, CLAIM_LIT_S / BEAT_LIT_S
 * 5 while lit -- was BEAT_MS 60000 here) */
static const uint32_t REPLY_MS       = 6000;    /* everything answers at once... */
static const uint32_t LEASE_REPLY_MS = 15000;   /* ...but the lease verbs wait for the box */
static const uint32_t CONNECT_MS     = 8000;
static const uint32_t TLS_HS_S       = 15;      /* the handshake over the funnel's relay: 1-2 s seen, this is the wall */
static const uint32_t HI_WAIT_MS     = 4000;    /* the desk speaks first; else we ask */
static const uint32_t BACKOFF_MIN_MS = 5000, BACKOFF_MAX_MS = 60000;
static const uint32_t DOOR_SKIP_MIN_MS = 10000, DOOR_SKIP_MAX_MS = 600000;   /* a door that failed is skipped this long, doubling */
static const uint32_t DAY_HOLD_MS    = 3600000; /* a day the desk REFUSED (no big / no short / no day) waits this long */
static const uint32_t SENSE_GAP_MS   = 30000;   /* one sense a kind per this */
static const uint32_t SENSE_STALE_S  = 600;     /* THE RADIO BUDGET: a ride-along sense (raised, shaken, up, the lock) older than this at the dial is not sent -- "raised to look" half an hour ago is no longer news */
static const int      DAY_CHUNK      = 80;      /* raw bytes a `day <hex>` line carries (165 B, under LINK_LINE_MAX) */
static const uint32_t HEAP_FLOOR     = 60000;   /* under this with TLS up, the session must not outlive the wake */
static const uint32_t TLS_DIAL_HEAP  = 50000;   /* a TLS dial needs this much free internal heap (the session's buffers, when they are internal)... */
static const uint32_t TLS_DIAL_BLOCK = 20000;   /* ...and one block this big (the 16 KB record buffers) -- in PSRAM when the session lives there */
static const uint32_t TLS_DIAL_HEAP_PSRAM = 30000;   /* with the session in PSRAM the internal need is the dial task's 10 KB stack, the socket and lwIP's pbufs */
/* THE NIGHT RELEASE's dark errand (below): the radio up from the nap for
 * the one verb, bounded -- a dial that finds no link gives up at
 * LINK_TRY_S (45 s) on its own; this is the wall past that for the
 * handshake, the verb's reply (LEASE_REPLY_MS) and the linger's bye */
static const uint32_t ERRAND_MAX_MS  = 75000;
#define LINK_LINE_MAX 180

/* THE TLS SESSION IN PSRAM (the optimization pass, 2026-09-10). mbedtls
 * allocates a session's two record buffers (16 KB in, 4 KB out), its
 * contexts and the certificate chain through ONE calloc it lets us set
 * at run time (MBEDTLS_PLATFORM_MEMORY in the core's esp_config.h; the
 * core's own default, esp_mbedtls_mem_calloc, is internal RAM only --
 * CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC, not a build flag we can turn). So
 * link_begin points it at a calloc that asks the 2 MB PSRAM first and
 * falls back to internal: the ~45 KB the box door cost the internal heap
 * while held becomes a few KB (the WiFiClientSecure object, the socket,
 * the pbufs), and the 60 KB floor stops being the door's problem. The
 * CPU is the only reader of those buffers (lwIP copies into them, the
 * AES/SHA drivers bounce anything not DMA-capable through their own
 * internal scratch -- the very `esp-sha: Failed to allocate buf memory`
 * the PSRAM cell fixed came from that bounce), and the crypto's PSRAM
 * penalty is a handful of ms on a 1-2 s handshake. It is process-wide:
 * the HMAC's 32 bytes and the campus PEAP's inner TLS go the same way
 * (a help: the join no longer squeezes the heap). `tls-mem internal`
 * over serial puts it back for the next dial (the escape hatch for the
 * board), `-DLINK_TLS_PSRAM=0` builds it off. WiFiClientSecure has no
 * setBufferSizes (that is the ESP8266's BearSSL); the sdkconfig's
 * CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN / DYNAMIC_BUFFER are baked into the
 * core's prebuilt libmbedtls and not settable from platformio.ini -- so
 * moving the buffers is the lever, not shrinking them. */
#ifndef LINK_TLS_PSRAM
#define LINK_TLS_PSRAM 1
#endif
static bool tls_psram = LINK_TLS_PSRAM != 0;
static uint32_t tls_psram_bytes = 0, tls_internal_bytes = 0;   /* what each calloc handed out since boot (the log's proof) */
static void *tls_calloc(size_t n, size_t size)
{
    void *p = NULL;
    if (tls_psram) {
        p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) { tls_psram_bytes += n * size; return p; }
    }
    p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   /* the core's own choice */
    if (p) tls_internal_bytes += n * size;
    return p;
}
static void tls_free(void *p) { heap_caps_free(p); }        /* one free serves both heaps */

void link_tls_mem(bool psram)
{
    tls_psram = psram;
    LOG("tls-mem: mbedtls's sessions in %s from the next dial (so far %lu B psram, %lu B internal)",
        psram ? "PSRAM" : "internal RAM", (unsigned long)tls_psram_bytes, (unsigned long)tls_internal_bytes);
}
bool link_tls_psram(void) { return tls_psram; }

/* Let's Encrypt's roots. The funnel's chain tonight: leaf <- YE1 <- Root YE
 * <- ISRG Root X2 <- ISRG Root X1 (the cross-signs are served), so either
 * root closes it; both are pinned (X1 to 2035, X2 to 2040). Never
 * setInsecure. */
static const char ISRG_ROOTS[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
"-----BEGIN CERTIFICATE-----\n"
"MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
"CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
"R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
"MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
"ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
"EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
"+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
"ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
"AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
"zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
"tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
"/q4AaOeMSQ+2b1tbFfLn\n"
"-----END CERTIFICATE-----\n";

/* --- the engine -------------------------------------------------------------- */
enum Tr { TR_NONE, TR_TCP, TR_TLS, TR_SERIAL, TR_BLE };   /* TR_BLE: THE AIR DOOR (bleuart.cpp), 2026-10-06 */
enum St { S_IDLE, S_DIAL, S_CONNECT, S_WAIT_HI, S_HELLO, S_LINKED, S_BACKOFF };
static const char *ST_NAMES[] = { "idle", "dialing", "connecting", "waiting-hi", "hello", "linked", "backoff" };
enum Req { R_NONE, R_HELLO, R_TIME, R_GRAIN, R_BEAT, R_CLAIM, R_COME, R_RELEASE, R_SENSE,
           R_DAY_BEGIN, R_DAY_CHUNK, R_DAY_END, R_WORD, R_SKY, R_BYE, R_LOOK, R_STATUS };
static const char *REQ_NAMES[] = { "-", "hello", "time", "grain", "beat", "claim", "come", "release",
                                   "sense", "day begin", "day", "day end", "word", "sky", "bye", "look", "status" };
static bool need_look = false;             /* THE WRIST WARDROBE: `look` owed -- after the link's first `ok held` (the handover's own moment) */
static bool looked_this_link = false;      /* ... asked once per link; the desk pushes every later change */

static Tr  tr = TR_NONE;
static St  st = S_IDLE;
static bool wifi_up = false;
static int  sock = -1;                     /* the plain door's socket */
static WiFiClientSecure *tls = NULL;       /* the TLS door's session (alive only between dial and drop) */
static uint32_t st_ms = 0, backoff_ms = BACKOFF_MIN_MS, hi_asked = 0;
static char rxline[LINK_LINE_MAX]; static size_t rxn = 0;   /* one line, never more */
static Req  inflight = R_NONE;
static uint32_t sent_ms = 0, sent_timeout = 0;
static bool need_time = false, need_grain = false, need_claim = false, need_word = false, need_sky = false, sync_asked = true;
static bool need_house = false;            /* the HONEST claim at link-up, after the day sync (house.h): never a come */
static uint32_t sync_check_at = 0;         /* the day directory is walked at most once a minute (it is a LittleFS walk: never per frame) */
static int  whistle_req = 0;               /* 0 none, 1 come, 2 release, 3 the night's `release asleep`, 4 the rising's claim (THE NIGHT RELEASE), 5 the dock's `release docked` (THE DOCK HANDS HIM OVER) */
static const char *const WHISTLE_NAMES[] = { "-", "come", "release", "the night's release", "the rising's claim", "the dock's release" };
static dockhand_t dockhand;                /* THE DOCK HANDS HIM OVER: the cable's last edge and whether this module released him there */
static uint32_t grain_live_ms = 0;         /* THE OTHER HOUSE'S DAY: when the last LIVE grain line landed (0 = never) */
static const uint32_t GRAIN_DAY_FRESH_MS = 30u * 60000u;   /* sat= / pets= are the desk's NOW: worn on an arrival within the half hour, else nothing (the line is re-asked at every hello) */
static bool night_released = false;        /* THE NIGHT RELEASE went out (or was meant to): the rising claims back */
static bool reaching = false;              /* THE REACHING CUE: a come / a claim by hand is out (set with the ask, cleared by the answer or the give-up) */
static bool errand = false;                /* the dark errand is running (nap): one verb, nothing rides along */
/* whether the lease is held here is THE HOUSE's to know (house_holding()); the link only carries the words */
static uint32_t lease_ms = 0, last_ok_ms = 0;   /* lease_ms: when the last lease verb went out (the cadence counts from it) */
static uint32_t desk_ok_ms = 0;               /* THE SMALL HOUSE: the last ok over a door that is the desk itself (not the box's TLS door), or a knock taken */
static bool     lease_now = false;         /* a beat to resend at once (the worded one declined) */
/* THE LIT GLASS POLLS (radio.h): the glass is lit = main's AWAKE state
 * (app.state 0), not the dim; heap_low = this TLS session left less than
 * HEAP_FLOOR free at hello, so it is not held past the linger */
static bool     heap_low = false;
/* THE PAIRING WINDOW (ble.h) wants the WiFi radio OFF -- the stack it
 * opens for comes up only then (ruling 58) -- but the window keeps the
 * glass lit (the count on the row), and a lit glass holds the session;
 * so for the radio the glass is not lit while the window is open: the
 * linger's bye ends the session within LINK_LINGER_S and BLE rises */
static bool glass_lit(void) { return app.state == 0 && ble_pair_left_s() == 0; }
/* THE CABLE NEVER CLAIMS (radio.h says why): on the cable = the charger's
 * float on VBAT (main's charging(): the board has no VBUS pin -- THE
 * SHELF RULE's own evidence, the day log's CHARGING flag) or the desk on
 * the serial port (the dock IS the cable). cable_said: the last edge
 * logged, so each edge is one line */
static bool on_cable(void) { return charging() || tr == TR_SERIAL; }
/* THE AIR DOOR is a door like the dock -- no radio dial while it is open
 * (wired(): the two transports that are not the radio's) -- but it is on
 * the WRIST, not the cable: the honest claim goes over it (on_cable above
 * says TR_SERIAL alone, on purpose) */
static bool wired(void) { return tr == TR_SERIAL || tr == TR_BLE; }
static bool     cable_said = false;
/* THE SHELF NEVER CLAIMS (radio.h): the honest claim goes only WORN --
 * the house's own test (motion on the arm within WORN_WINDOW_MIN 5),
 * the same word the beat carries; UNKNOWN asks its one question anyway */
static bool may_claim(void) { return radio_may_claim(on_cable(), house_state(), house_worn()); }
static bool poll_verb(void) { return radio_lease_verb(on_cable(), house_state(), house_worn()) != LEASE_NONE; }   /* a verb to hold a lit session for */
static bool still_said = false;            /* the shelf's edge, logged once each way (as the cable's) */
/* WORN ON THE BEAT (SAGE-WATCH-LINK-SLIME-ACK section 8: PRESENCE BY
 * EVIDENCE -- "a lived-at desk has him, else a worn wrist, else he
 * stays"), behind store.beat_worn -- DEFAULT ON since 2026-09-17
 * (SAGE-WEARABLE-CLAW-REPLY section 1: the box accepts `beat worn|still`
 * / `claim worn|still` and arbitrates on it; a wrist that set it off
 * keeps off, the NVS key is the same). On, the minute's verb carries
 * the house's worn test:
 *
 *   > beat worn          < ok held          (the lease refreshed, as today)
 *   > beat still         < ok held | no desk | no lapsed
 *   > claim worn         < ok held          (the desk's beat older than 60 s, the arm moving: the handover
 *                                            WITHOUT the whistle -- logged `lease: taken from the desk (worn)`)
 *   > claim worn         < no desk          (a lived-at desk has him; the box says `no desk` for every refusal)
 *   > claim still        never sent while EMPTY (THE SHELF NEVER CLAIMS); UNKNOWN's one question may carry it
 *   > beat worn          < no verb          the box does not know the word yet:
 *   > beat               < ok held          logged once, bare for the rest of this link
 *
 * The replies are handled exactly as the bare verbs' (lease_reply);
 * only `no verb` to a WORDED beat / claim is special: the word is
 * turned off for this link (not in NVS) and the bare verb goes out at
 * once. `come` / `release` never carry it. The box's own numbers for
 * the wrist: the interval 600 s, the lapse 1800 s (3x); a desk is
 * lived-at while its beat is within 60 s. */
static bool word_declined = false;         /* this link's box answered `no verb` to the word */
static bool sent_worded = false;           /* the verb in flight carried the word */
static bool sent_worn = false;             /* ... and the word was `worn` (the handover's log line) */
/* THE BOX'S STATUS (SAGE-WEARABLE-CLAW-REPLY section 4): `status` ->
 * `ok status holder=<body|none> last_day=<YYYY-MM-DD|none> words=<n>
 * sky=<w[,w]|none>`, keyed (any order), under 120 bytes, from the
 * box's files only -- asked ONCE per link after the hello and the day
 * sync, next to `grain` / `look`; `no verb` = an old box, tolerated.
 * Kept for the console's `link` line, the doctor and the watch page's
 * ABOUT row; `words > 0` with no word fetched this link asks `word`
 * (the box's serve order applies: one bubble, as today) */
typedef struct {
    bool have;              /* the box answered this link */
    bool declined;          /* `no verb`: an old box */
    char holder[25];        /* the live lease's body, "none" */
    char last_day[11];      /* the newest synced day on the box, "none" */
    int  words;             /* lines waiting for this body (desk + mind spools) */
    char sky[20];           /* the box's own sky, "none" (the `sky` verb is what the gel wears; this is what the box would say) */
} link_box_t;
static link_box_t box;
static bool need_status = false;
static bool word_fetched = false;          /* an `ok word <text>` came this link */
static int  days_synced = 0;
static uint32_t sync_done_seq = 0;         /* THE DOCK FACE: bumped on each `ok day` end -- the ring's one blink */
static link_grain_t grain;
static char desk_name[24] = "";
static char no_link_said = 0;

/* the doors (store.doors), as the link sees them */
static struct { uint8_t fails; uint32_t next_ms; char last[20]; } door_st[STORE_DOORS];
static int cur_door = -1;                  /* the one being dialled / held */
static uint32_t dial_ms = 0;               /* when the dial began */
static uint32_t heap_before = 0;           /* free heap before a TLS dial */

/* the senses waiting to go: "sense <kind> <words>\n" -- a kind is under
 * 16 characters and rest.cpp's words cap at 48, so a line is under 80
 * (was 128 a slot: the optimization pass, pass 2) */
#define SENSE_Q 6
#define SENSE_LINE 80
/* THE RADIO BUDGET: the queue holds senses while the radio rests; a
 * kind WORTH THE RADIO (the night's `slept` / `napped`, a gesture's
 * `pet` / `water` / `meal` / `stroked`: things done to him, or his
 * night) is a reason to dial and is always sent; the rest (`raised`,
 * `shaken`, `up`, the lock) ride along on the next dial and are dropped
 * at SENSE_STALE_S */
static struct { char line[SENSE_LINE]; uint32_t ms; bool worth; } sense_q[SENSE_Q]; static int sense_n = 0;
static struct { char kind[24]; uint32_t ms; } sense_gap[8]; static int sense_gap_n = 0;
static const char *const SENSE_RIDE_ALONG[] = { "wrist:raised", "wrist:shaken", "wrist:up", "wrist:locked", "wrist:unlocked", "wrist:belly", NULL };   /* wrist:belly: THE BELLY TRAVELS -- the meal it follows is the dial's reason */

/* THE RADIO BUDGET's state (radio.h has the rule) */
static bool     awake = false;            /* between net_on and net_off */
static uint32_t awake_tick_ms = 0;        /* the last tick's millis while awake: the awake clock's step */
static uint32_t awake_since_ok_ms = 0;    /* awake ms since the last ok (the EVERY clock; kept across naps) */
static bool     hold = false;             /* `link hold`: the radio kept up this lit spell */
static uint32_t hold_until_ms = 0;        /* THE FLASH FROM THE DESK: the desk's timed hold ends here (0 = the console's, untimed) */
static bool     force_once = false;       /* `link now`: one dial, whatever the clocks */
static bool     day_pending = false;      /* a closed day waits to go home (checked at the wake) */
static bool     radio_asked = false;      /* the link turned the radio on and is waiting for its link */
static uint32_t try_ms = 0;               /* when */
static bool     retry_wait = false;       /* a due dial found no link: not again until retry_at_ms */
static uint32_t retry_at_ms = 0;
static uint32_t quiet_ms = 0;             /* linked with nothing to say since (the linger's clock) */
static bool     bye_said = false;         /* the linger's `bye` is out */
static int      reason = RADIO_REST;      /* the last tick's verdict */
static bool     wake_said = false;        /* the one resting line per wake */

/* the day in flight; a day that failed waits: until 0 = this session, else a clock */
static struct { bool active; uint32_t date; size_t len, off; File f;
                struct { uint32_t date, until; } failed[4]; int nfail; } dsync;

/* the dial: a short-lived task, because both halves block -- a hostname
 * lookup, and WiFiClientSecure's connect (the TCP connect plus the
 * handshake, 1-2 s through the funnel's relay). The loop starts it,
 * polls dial_state, and takes the result; the frame never waits. */
static volatile int dial_state = 0;        /* 0 idle 1 running 2 done 3 failed */
static uint32_t dial_ip = 0;
static char dial_host[64];
static uint16_t dial_port = 0;
static bool dial_tls = false;
static char dial_err[80] = "";
static uint32_t heap_after = 0;

static void dial_task(void *arg)
{
    if (dial_tls) {
        /* the session is the task's argument, not the global: the loop
         * may abandon the dial and the pointer while this runs */
        WiFiClientSecure *c = (WiFiClientSecure *)arg;
        /* SNI = the host (connect(host, ...) resolves and sets it), the roots pinned */
        c->setCACert(ISRG_ROOTS);
        c->setHandshakeTimeout(TLS_HS_S);
        int ok = c->connect(dial_host, dial_port, (int32_t)CONNECT_MS);
        heap_after = ESP.getFreeHeap();
        if (ok) dial_state = 2;
        else {
            dial_err[0] = 0;
            int e = c->lastError(dial_err, sizeof dial_err);
            if (!e) strncpy(dial_err, "no route, no answer, or the name does not resolve", sizeof dial_err - 1);
            dial_err[sizeof dial_err - 1] = 0;
            dial_state = 3;
        }
        vTaskDelete(NULL);
        return;
    }
    IPAddress ip;
    size_t n = strlen(dial_host);
    if (n > 6 && !strcmp(dial_host + n - 6, ".local")) {
        /* a .local name is mDNS (the laptop with avahi on), not the resolver */
        char host[64]; strncpy(host, dial_host, sizeof host - 1); host[sizeof host - 1] = 0;
        host[n - 6] = 0;
        ip = MDNS.queryHost(host, 3000);
        if ((uint32_t)ip) { dial_ip = (uint32_t)ip; dial_state = 2; } else dial_state = 3;
    } else if (WiFi.hostByName(dial_host, ip)) { dial_ip = (uint32_t)ip; dial_state = 2; }
    else dial_state = 3;
    vTaskDelete(NULL);
}

/* --- the doors ----------------------------------------------------------------- */

static const door_t *door(int i) { return (i >= 0 && i < store.n_doors) ? &store.doors[i] : NULL; }

static void door_failed(int i, const char *why)
{
    if (i < 0 || i >= STORE_DOORS) return;
    if (door_st[i].fails < 30) door_st[i].fails++;
    uint32_t skip = DOOR_SKIP_MIN_MS;
    for (int k = 1; k < door_st[i].fails && skip < DOOR_SKIP_MAX_MS; k++) skip *= 2;
    if (skip > DOOR_SKIP_MAX_MS) skip = DOOR_SKIP_MAX_MS;
    door_st[i].next_ms = millis() + skip;
    strncpy(door_st[i].last, why, sizeof door_st[i].last - 1); door_st[i].last[sizeof door_st[i].last - 1] = 0;
    const door_t *d = door(i);
    LOG("link: door %d %s:%u%s failed (%s); skipped %lu s", i, d ? d->host : "?", d ? d->port : 0,
        d && d->tls ? " tls" : "", why, (unsigned long)(skip / 1000));
}

static void door_worked(int i)
{
    if (i < 0 || i >= STORE_DOORS) return;
    door_st[i].fails = 0;
    door_st[i].next_ms = millis();
    strncpy(door_st[i].last, "ok hello", sizeof door_st[i].last);
    /* THE NEAR DOOR FIRST: a PLAIN door that answered on this network is
     * the one to try first here next time (the laptop's LAN door: the
     * claim proxied over ssh in ~1 s, no TLS handshake) */
    const door_t *d = door(i);
    if (d && !d->tls && net_ssid()[0] && store_near_door(net_ssid()) != i) {
        store_near_set(net_ssid(), i);
        LOG("link: door %d %s:%u is the near door on %s (remembered: tried first here)", i, d->host, d->port, net_ssid());
    }
}

/* THE NEAR DOOR FIRST: the plain door remembered for the joined network,
 * if it is still a plain door and not skipped; -1 = none */
static int door_near(void)
{
    int i = store_near_door(net_ssid());
    if (i < 0 || i >= store.n_doors) return -1;
    const door_t *d = door(i);
    if (!d || d->tls) return -1;
    if ((int32_t)(millis() - door_st[i].next_ms) < 0) return -1;
    return i;
}

/* the near door for this network first, then the list in order -- the
 * first whose skip has passed; -1 = none yet */
static int door_pick(void)
{
    int near = door_near();
    if (near >= 0) return near;
    uint32_t now = millis();
    for (int i = 0; i < store.n_doors; i++)
        if ((int32_t)(now - door_st[i].next_ms) >= 0) return i;
    return -1;
}

/* --- bytes out --------------------------------------------------------------- */

static bool link_write(const char *s)
{
    size_t n = strlen(s);
    if (tr == TR_SERIAL) { Serial.write((const uint8_t *)s, n); return true; }
    if (tr == TR_BLE) return bleuart_write(s, n);         /* THE AIR DOOR: queued for the notifies (bleuart_tick) */
    if (tr == TR_TCP && sock >= 0) {
        int r = send(sock, s, n, 0);
        return r == (int)n;
    }
    if (tr == TR_TLS && tls) return tls->write((const uint8_t *)s, n) == n;
    return false;
}

static void set_state(St s) { st = s; st_ms = millis(); }
static void drop(const char *why);

static bool request(Req r, const char *line, uint32_t timeout)
{
    if (!link_write(line)) { drop("write failed"); return false; }
    inflight = r;
    sent_ms = millis();
    sent_timeout = timeout;
    if (r == R_BEAT || r == R_CLAIM || r == R_COME || r == R_RELEASE) lease_ms = sent_ms;   /* the cadence counts from the last lease verb */
    if (r != R_DAY_CHUNK) {
        char shown[64];
        strncpy(shown, line, sizeof shown - 1); shown[sizeof shown - 1] = 0;
        char *nl = strchr(shown, '\n'); if (nl) *nl = 0;
        if (r == R_HELLO) strcpy(shown, "hello watch <mac>");
        LOG("link> %s", shown);
    }
    return true;
}

/* the one primitive both doors share: HMAC-SHA256(the watch key, msg), the first 16 bytes as 32 hex */
static void hmac_msg(const char *msg, char *out33)
{
    unsigned char d[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                    (const unsigned char *)store.key, strlen(store.key),
                    (const unsigned char *)msg, strlen(msg), d);
    for (int i = 0; i < 16; i++) sprintf(out33 + 2 * i, "%02x", d[i]);
    out33[32] = 0;
}

static void hmac_hex(const char *nonce, char *out33)
{
    char msg[80];
    snprintf(msg, sizeof msg, "%s watch", nonce);
    hmac_msg(msg, out33);
}

/* THE LAN KNOCK (radio.h says the line): the desk's `POST /lease`. The
 * knock never changes the lease by itself -- it makes the wrist ASK the
 * box: `lost` sinks the house on the wrist's own belief and the next
 * claim confirms (or `ok held` stands him up again); `free` claims at
 * once while EMPTY and the glass is lit (a pending whistle: the radio
 * comes up for it if it is not). With a key remembered the knock must be
 * signed -- mac = HMAC(key, "<stamp> lease <what>"), the stamp the UTC
 * minute, within KNOCK_STALE_MIN of the wrist's clock when it has one
 * (unset: the mac is checked, the stamp not; a replay within the minute
 * can only make the wrist ask) -- else refused; with no key (a bare
 * board) a bare knock is taken, logged as such. */
int link_knock(const char *body, char *why, int cap)
{
    knock_t k;
    if (!radio_knock_parse(body, &k)) { snprintf(why, cap, "knock"); return 2; }
    /* the verdict is radio.cpp's (pure, tested); this only computes the mac and reads the clock */
    char msg[48], mac[33]; const char *ours = NULL;
    if (store.key[0] && k.signed_) { snprintf(msg, sizeof msg, "%lu lease %s", (unsigned long)k.stamp, k.what); hmac_msg(msg, mac); ours = mac; }
    uint32_t now_min = 0; bool clock = clock_is_set();
    if (clock) { uint32_t ep = 0; struct tm lt; clock_local(&lt, &ep); now_min = ep / 60u; }
    if (radio_knock_judge(&k, store.key[0] != 0, ours, clock, now_min, why, cap)) {
        LOG("knock: %s refused: %s", k.what, !strcmp(why, "stale") ? "stale" : !k.signed_ ? "unsigned, and a key is remembered" : "the mac is not the desk's");
        if (!strcmp(why, "stale")) LOG("knock: stamp %lu, now %lu", (unsigned long)k.stamp, (unsigned long)now_min);
        return 1;
    }
    if (!strcmp(why, "unsigned")) LOG("knock: %s taken unsigned (no key remembered here)", k.what);
    desk_ok_ms = millis() ? millis() : 1;                 /* THE SMALL HOUSE: a knock of his is the desk at the door */
    bool lit = app.state != 2;
    if (!strcmp(k.what, "lost")) {
        if (house_holding()) {
            LOG("knock: lost -- the desk has him: the sink on the wrist's belief; the next claim confirms");
            house_lease("knock", "no desk");                 /* the goodbye, then the sink, as for the box's own word */
            need_claim = true;                                /* over the session, if one is up; else the next dial's honest claim */
        } else LOG("knock: lost -- the house is already %s", house_state_name());
    } else {
        if (house_holding()) LOG("knock: free -- he is here already");
        else if (whistle_req) LOG("knock: free -- %s is already on its way", WHISTLE_NAMES[whistle_req]);
        else {
            whistle_req = 4;                                  /* the rising's shape: the honest claim, a reason to dial */
            reaching = true;
            LOG("knock: free -- the claim%s%s", st == S_LINKED ? "" : " (the radio comes up for it)", lit ? "" : " (the glass is dark: on the next wake)");
        }
    }
    snprintf(why, cap, "%s", house_state_name());
    return 0;
}

/* THE DOOR'S AUTH, lent (link.h): the knock's judge over any message --
 * `POST /art`'s `begin <total>` / `chunk <off> <crc>` / `end <crc>` */
int link_sign_check(const char *what, const char *stamp_s, const char *mac_s, char *why, int cap)
{
    knock_t k; memset(&k, 0, sizeof k);
    if (stamp_s && stamp_s[0] && mac_s && mac_s[0]) {
        k.stamp = (uint32_t)strtoul(stamp_s, NULL, 10);
        int n = 0;
        while (n < KNOCK_MAC_HEX && isxdigit((unsigned char)mac_s[n])) { k.mac[n] = (char)tolower((unsigned char)mac_s[n]); n++; }
        k.mac[n] = 0;
        k.signed_ = n == KNOCK_MAC_HEX && mac_s[n] == 0;
    }
    char msg[96], mac[33]; const char *ours = NULL;
    if (store.key[0] && k.signed_) { snprintf(msg, sizeof msg, "%lu %s", (unsigned long)k.stamp, what); hmac_msg(msg, mac); ours = mac; }
    uint32_t now_min = 0; bool clock = clock_is_set();
    if (clock) { uint32_t ep = 0; struct tm lt; clock_local(&lt, &ep); now_min = ep / 60u; }
    return radio_knock_judge(&k, store.key[0] != 0, ours, clock, now_min, why, cap);
}

/* --- the day sync ------------------------------------------------------------- */

/* hold_ms 0: the day is skipped for this session (a transport failure);
 * else for that long (the desk refused it: no big / no short / no day) */
static void dsync_abort(const char *why, uint32_t hold_ms)
{
    if (!dsync.active) return;
    LOG("link: day %08lu not synced (%s); %s", (unsigned long)dsync.date, why,
        hold_ms ? "not again for an hour" : "next session");
    if (dsync.f) dsync.f.close();
    int slot = dsync.nfail < 4 ? dsync.nfail++ : 3;
    dsync.failed[slot].date = dsync.date;
    dsync.failed[slot].until = hold_ms ? millis() + hold_ms : 0;
    dsync.active = false;
}

static void dsync_refuse(const char *line)
{
    /* the box caps a day at 256 KB (`no big`); a count that disagrees is
     * `no short`; a chunk with no begin is `no day` -- all the desk's
     * refusals, none of which a retry now would change */
    dsync_abort(line, DAY_HOLD_MS);
}

static bool dsync_failed(uint32_t d)
{
    uint32_t now = millis();
    for (int i = 0; i < dsync.nfail; i++)
        if (dsync.failed[i].date == d && (!dsync.failed[i].until || (int32_t)(dsync.failed[i].until - now) > 0))
            return true;
    return false;
}

/* a new session forgets the transport failures, keeps the desk's refusals */
static void dsync_forget(bool all)
{
    int w = 0;
    for (int i = 0; i < dsync.nfail; i++)
        if (!all && dsync.failed[i].until) dsync.failed[w++] = dsync.failed[i];
    dsync.nfail = w;
}

static void dsync_send_chunk(void)
{
    static char line[LINK_LINE_MAX + 8];
    uint8_t raw[DAY_CHUNK];
    dsync.f.seek(dsync.off);
    int n = dsync.f.read(raw, DAY_CHUNK);
    if (n <= 0) { dsync_abort("read", 0); return; }
    size_t k = 0;
    memcpy(line, "day ", 4); k = 4;
    for (int i = 0; i < n; i++) { sprintf(line + k, "%02x", raw[i]); k += 2; }
    line[k++] = '\n'; line[k] = 0;
    dsync.off += n;
    request(R_DAY_CHUNK, line, REPLY_MS);
}

static bool dsync_start(void)
{
    uint32_t d;
    /* a day that failed this session is skipped until the next one */
    if (!wrist_next_unsynced(&d) || dsync_failed(d)) return false;
    char p[32];
    wrist_file_path(d, p, sizeof p);
    dsync.f = LittleFS.open(p, "r");
    if (!dsync.f) return false;
    dsync.active = true;
    dsync.date = d;
    dsync.len = dsync.f.size();
    dsync.off = 0;
    char line[48];
    snprintf(line, sizeof line, "day begin %04lu-%02lu-%02lu\n", (unsigned long)(d / 10000),
             (unsigned long)(d / 100 % 100), (unsigned long)(d % 100));
    LOG("link: syncing day %08lu, %u B", (unsigned long)d, (unsigned)dsync.len);
    return request(R_DAY_BEGIN, line, REPLY_MS);
}

/* --- replies ------------------------------------------------------------------- */

/* live = the line just came down the wire (a cached line from NVS is
 * re-read at boot: its belly= is an hour stale and is not worn) */
static void parse_grain(const char *s, bool live)
{
    /* seed=... regard=... night=... quiet=... mood=... [belly=...] [sat=...] [pets=...] */
    link_grain_t g; memset(&g, 0, sizeof g);
    char buf[160]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;   /* store.grain is 128 since the day's two fields */
    char *save = NULL;
    for (char *tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        const char *v = eq + 1;
        if (!strcmp(tok, "seed")) g.seed = atoi(v);
        else if (!strcmp(tok, "regard")) g.regard = atof(v);
        else if (!strcmp(tok, "night")) g.night = atoi(v);
        else if (!strcmp(tok, "quiet")) { strncpy(g.quiet, v, sizeof g.quiet - 1); }
        else if (!strcmp(tok, "mood")) { strncpy(g.mood, v, sizeof g.mood - 1); }
        else if (!strcmp(tok, "belly")) { g.have_belly = true; g.belly = atof(v); }   /* THE BELLY TRAVELS: the desk's stomach */
        else if (live && !strcmp(tok, "sat")) { g.have_day = true; g.sat_min = atoi(v); }     /* THE OTHER HOUSE'S DAY (C21): the desk's sit, a live line only (the cached one is hours stale) */
        else if (live && !strcmp(tok, "pets")) { g.have_day = true; g.pets = atoi(v); }
    }
    g.have = true;
    if (live) grain_live_ms = millis() ? millis() : 1;
    grain = g;
    grain_update(g.seed, g.regard, g.night, g.mood);   /* THE GRAIN: the knobs, recomputed when the line changed */
    if (live && g.have_belly) grain_belly(g.belly);    /* THE BELLY TRAVELS: worn from a live line only */
    strncpy(store.grain, s, sizeof store.grain - 1);
    store.grain[sizeof store.grain - 1] = 0;
    store_save_grain();
    char day[40] = "";
    if (g.have_day) snprintf(day, sizeof day, " sat %d min pets %d", g.sat_min, g.pets);   /* the arrival's first face reads these (tell.cpp) */
    if (g.have_belly) LOG("link: grain seed %d regard %.2f night %d quiet %s mood %s belly %.2f%s%s (cached)",
                          g.seed, g.regard, g.night, g.quiet, g.mood, g.belly, live ? "" : " (stale: not worn)", day);
    else LOG("link: grain seed %d regard %.2f night %d quiet %s mood %s%s (cached)",
             g.seed, g.regard, g.night, g.quiet, g.mood, day);
}

bool link_grain_day(int *sat_min, int *pets)
{
    bool fresh = grain.have_day && grain_live_ms && millis() - grain_live_ms < GRAIN_DAY_FRESH_MS;
    if (sat_min) *sat_min = fresh ? grain.sat_min : 0;
    if (pets) *pets = fresh ? grain.pets : 0;
    return fresh;
}

/* THE BELLY TRAVELS, the other way: the wrist's own lump back to the
 * desk once per change over BELLY_STEP (the feed's 1.0 at the meal,
 * ~0.25 at 30 s, 0 at 60 s: three lines a meal at most, the first
 * riding the meal's own dial) */
static const float BELLY_STEP = 0.10f;
static float belly_sent = 0; static uint32_t belly_ms = 0;
void link_belly(float lump)
{
    if (lump < 0) lump = 0;
    if (lump > 1) lump = 1;
    if (fabsf(lump - belly_sent) <= BELLY_STEP) return;
    uint32_t now = millis();
    if (belly_ms && now - belly_ms < SENSE_GAP_MS) return;   /* the senses' own pace: one a kind per 30 s */
    belly_sent = lump; belly_ms = now ? now : 1;
    char w[8]; snprintf(w, sizeof w, "%.2f", lump);
    link_sense("wrist:belly", w);
}

static refusal_t refusals;                 /* THE REFUSAL SAID ONCE (T5): the doors' repeated words, one row a verb (~150 B); reset at every `ok hello` */
static proof_t   proof;                    /* THE DOOR'S OWN PROOF (T10): this session's nonce and the door's standing (proven / unproven = read-only); ~52 B, set at every `hi` */
static void lease_reply(Req r, const char *line, bool worded, bool worn)
{
    if (r == R_COME || r == R_CLAIM) reaching = false;       /* THE REACHING CUE: answered, whatever the answer */
    if (!strcmp(line, "ok held")) {
        LOG("link: the lease is HELD here (the slime is on the wrist)");
        /* THE WRIST TAKES HIM WORN (SAGE-WEARABLE-CLAW-REPLY section 1): a
         * `claim worn` that landed while the house was not holding = the
         * handover without the whistle -- the desk's beat was older than
         * 60 s (or nobody held him); the box's journal says which */
        if (r == R_CLAIM && !house_holding())
            LOG("lease: taken from the desk (%s)", worded ? (worn ? "worn" : "still") : "bare");
        /* THE WRIST WARDROBE: the handover's own moment -- `look` next (the
         * costume, the colour), once per link (the first `ok held`, the
         * arrival's); every later change the desk pushes as `look ...` */
        if (!looked_this_link) { looked_this_link = true; need_look = true; }
    }
    else if (!strcmp(line, "ok released")) LOG("link: released");
    else if (!strcmp(line, "no desk")) LOG("link: a desk has him: dormant");
    else if (!strcmp(line, "no lapsed")) LOG("link: lapsed");
    else {  /* THE REFUSAL SAID ONCE (2026-10-12 night, T5): the lit glass beats every 5 s; a box that answers `no gate` for an evening wrote this line 12 times a minute -- now once per verb's word, again when the word changes, counted between (refusal.h; `link` / doctor carry the count) */
        if (refusal_note(&refusals, REQ_NAMES[r], line, millis()))
            LOG("link: lease says '%s' (kept believing what was believed; said once until the word changes, counted meanwhile: `link`)", line);
    }
    /* THE HOUSE learns the state (and may ask for one more claim: a beat's `no lapsed` while holding) */
    if (house_lease(REQ_NAMES[r], line)) need_claim = true;
}

static const char *tr_name(void) { return tr == TR_TCP ? "tcp" : tr == TR_TLS ? "tls" : tr == TR_BLE ? "ble" : "serial"; }

/* THE BOX'S STATUS: the keyed line after `ok status ` -- key=value
 * pairs split on spaces, in any order; a key not known is skipped, a
 * key missing leaves its field "none" / 0 */
static void parse_status(const char *s)
{
    memset(&box, 0, sizeof box);
    box.have = true;
    strcpy(box.holder, "none"); strcpy(box.last_day, "none"); strcpy(box.sky, "none");
    char buf[LINK_LINE_MAX]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq || !eq[1]) continue;
        *eq = 0; const char *v = eq + 1;
        if (!strcmp(tok, "holder")) { strncpy(box.holder, v, sizeof box.holder - 1); box.holder[sizeof box.holder - 1] = 0; }
        else if (!strcmp(tok, "last_day")) { strncpy(box.last_day, v, sizeof box.last_day - 1); box.last_day[sizeof box.last_day - 1] = 0; }
        else if (!strcmp(tok, "words")) { int n = atoi(v); box.words = n < 0 ? 0 : n > 999 ? 999 : n; }
        else if (!strcmp(tok, "sky")) { strncpy(box.sky, v, sizeof box.sky - 1); box.sky[sizeof box.sky - 1] = 0; }
    }
}

static void handle_reply(const char *line)
{
    bool ok = !strncmp(line, "ok", 2) && (line[2] == 0 || line[2] == ' ');
    Req r = inflight;
    inflight = R_NONE;
    bool worded = sent_worded; sent_worded = false;
    bool worn = sent_worn; sent_worn = false;
    if (ok) { last_ok_ms = millis(); awake_since_ok_ms = 0; }   /* THE RADIO BUDGET: both clocks from the last ok */
    if (ok && tr != TR_TLS) desk_ok_ms = last_ok_ms ? last_ok_ms : 1;   /* THE SMALL HOUSE: the desk's own door answered (the LAN door, the dock, the air door) */
    if (r != R_DAY_CHUNK) LOG("link< %s", line);
    switch (r) {
    case R_HELLO:
        if (ok) {
            /* ok hello <desk> <kind> <epoch> */
            char kind[16] = ""; unsigned long epoch = 0;
            sscanf(line, "ok hello %23s %15s %lu", desk_name, kind, &epoch);
            /* THE DOOR'S OWN PROOF (T10): the sixth word is HMAC(key, "<nonce2> desk") from a desk that has the key; a bare
             * answer (an old watchlink) or a wrong one makes this session READ-ONLY (handle_ask's gate, R_TIME below) */
            if (proof_judge(&proof, line, hmac_msg) == PROOF_PROVEN) LOG("link: the door proved itself");
            else LOG("link: the door did not prove itself (%s) -- this session is read-only: wifi add / look / knock / alarm refused `no proof`, `time` not taken while the clock is set; the sync and the lease flow",
                     proof.wrong ? "its proof is NOT the key's" : "a bare answer: an old desk");
            set_state(S_LINKED);
            backoff_ms = BACKOFF_MIN_MS;
            need_time = need_grain = true;
            need_word = true;                   /* once per link, after the day sync */
            need_sky = true;                    /* THE SKY: once per link, after the word */
            need_house = true;                  /* THE HOUSE: the honest claim, after the day sync, before the word */
            word_declined = false;              /* WORN ON THE BEAT: a new link's box may know the word */
            need_status = true; word_fetched = false;   /* THE BOX'S STATUS: once per link, after the day sync */
            memset(&box, 0, sizeof box);
            looked_this_link = false; need_look = false;   /* THE WRIST WARDROBE: this link's first `ok held` asks `look` */
            lease_ms = millis(); lease_now = false;   /* the cadence from here (the honest claim below is the first verb anyway) */
            heap_low = false;
            refusal_reset(&refusals);           /* THE REFUSAL SAID ONCE: a new session's door owes its own refusals */
            dsync_forget(false);
            sync_asked = true;                  /* walk the day directory once, now */
            quiet_ms = millis(); bye_said = false; force_once = false; radio_asked = false;   /* THE RADIO BUDGET: served; the linger's clock starts */
            if (epoch > 1600000000UL && !clock_is_set()) clock_set_epoch((uint32_t)epoch, "the desk's hello");
            if (!wired()) door_worked(cur_door);
            LOG("link: LINKED to %s (%s) over %s%s", desk_name, kind, tr_name(),
                tr == TR_TLS ? " (the box door)" : "");
            if (tr == TR_TLS) {
                uint32_t free_now = ESP.getFreeHeap();
                heap_low = free_now < HEAP_FLOOR;   /* THE LIT GLASS POLLS: under the floor the session is not held past the linger */
                LOG("link: heap %lu free with the TLS session held (largest block %lu; mbedtls has %lu B in psram, %lu B internal since boot)%s",
                    (unsigned long)free_now, (unsigned long)ESP.getMaxAllocHeap(),
                    (unsigned long)tls_psram_bytes, (unsigned long)tls_internal_bytes,
                    heap_low ? " -- under the 60 KB floor: not held while lit; the linger ends it, redialled for a reason" : "");
            }
        } else {
            LOG("link: the door said '%s'%s", line, !strcmp(line, "no key") ? " -- the key here is not the desk's WATCH_KEY" : "");
            backoff_ms = BACKOFF_MAX_MS;
            if (!wired()) door_failed(cur_door, line);
            drop("refused");
        }
        break;
    case R_TIME:
        if (ok && !proof_takes_time(&proof, clock_is_set())) LOG("link: the door's time not taken (no proof; the clock is already set)");   /* THE DOOR'S OWN PROOF (T10): asked once per link, so said once */
        else if (ok) {
            unsigned long epoch = 0; long off = 0;
            if (sscanf(line, "ok time %lu %ld", &epoch, &off) == 2) {
                clock_set_epoch((uint32_t)epoch, "the desk");
                int32_t tz = (int32_t)(off / 60);
                if (tz != store.tz_min) { store.tz_min = tz; store_save_tz(); LOG("link: tz %+d min (stored)", (int)tz); }
                /* THE REAL SUN (watch-ideas E39; design/watch-link.md section 2):
                 * the door may append today's `sunrise=HH:MM sunset=HH:MM`,
                 * local -- both words or nothing; the pair goes to the hours'
                 * light (eased over a minute) and to NVS when it changed. An
                 * old door that sends none leaves the last known pair as it was. */
                const char *sr = strstr(line, " sunrise="), *ss = strstr(line, " sunset=");
                int rh = 0, rm = 0, sh = 0, sm = 0;
                if (sr && ss && sscanf(sr + 9, "%d:%d", &rh, &rm) == 2 && sscanf(ss + 8, "%d:%d", &sh, &sm) == 2) {
                    int r = rh * 60 + rm, st = sh * 60 + sm;
                    if (hours_sun(r, st, false)) {
                        if (r != store.sun_rise || st != store.sun_set) {
                            store.sun_rise = (int16_t)r; store.sun_set = (int16_t)st; store_save_sun();
                            LOG("link: the sun %02d:%02d-%02d:%02d today (from the door; stored)", rh, rm, sh, sm);
                        }
                    } else LOG("link: the door's sun refused (%s) -- the fixed hours keep", sr + 1);
                }
            }
        }
        break;
    case R_GRAIN:
        if (ok && !strncmp(line, "ok grain ", 9)) parse_grain(line + 9, true);
        break;
    case R_BEAT: case R_CLAIM:
        /* WORN ON THE BEAT: `no verb` to the worded verb = the box does not
         * know the word yet -- bare for this link, the same verb again now */
        if (worded && !strcmp(line, "no verb")) {
            word_declined = true;
            LOG("link: the box does not know the word on the %s yet (no verb): bare for the rest of this link", REQ_NAMES[r]);
            if (r == R_BEAT) lease_now = true; else need_claim = true;
            break;
        }
        lease_reply(r, line, worded, worn);
        break;
    case R_COME: case R_RELEASE:
        lease_reply(r, line, false, false);
        break;
    case R_SENSE:
        break;
    case R_DAY_BEGIN:
        if (ok) dsync_send_chunk(); else dsync_refuse(line);
        break;
    case R_DAY_CHUNK:
        if (!ok) { dsync_refuse(line); break; }      /* `no big`: the box's 256 KB cap; `no day` */
        if (dsync.off < dsync.len) dsync_send_chunk();
        else {
            char l[32];
            snprintf(l, sizeof l, "day end %u\n", (unsigned)dsync.len);
            request(R_DAY_END, l, REPLY_MS);
        }
        break;
    case R_DAY_END:
        if (ok) {
            wrist_mark_synced(dsync.date);
            days_synced++;
            sync_done_seq++;                    /* THE DOCK FACE: the ring blinks once */
            sync_asked = true;                  /* there may be another day waiting */
            LOG("link: day %08lu synced (%u B)", (unsigned long)dsync.date, (unsigned)dsync.len);
            if (dsync.f) dsync.f.close();
            dsync.active = false;
        } else dsync_refuse(line);              /* `no short` */
        break;
    case R_WORD:
        /* ok word <text> | ok none; a laptop door says ok none, an older one no verb */
        if (ok && !strncmp(line, "ok word ", 8) && line[8]) { word_set(line + 8); word_fetched = true; }
        else if (ok && refusal_note(&refusals, "word", "none", millis())) LOG("link: no word (said once per link; counted: `link`)");   /* THE REFUSAL SAID ONCE */
        break;
    case R_STATUS:
        /* THE BOX'S STATUS: ok status holder=<body|none> last_day=<date|none>
         * words=<n> sky=<w[,w]|none> -- keyed, any order, anything unknown
         * skipped; `no verb` = an old box (nothing changes) */
        if (ok && !strncmp(line, "ok status ", 10)) {
            parse_status(line + 10);
            LOG("link: the box: holder=%s day=%s words=%d sky=%s", box.holder, box.last_day, box.words, box.sky);
            if (box.words > 0 && !word_fetched && !need_word) {
                need_word = true;               /* a line waits and none came this link: ask (the box's serve order; one bubble) */
                LOG("link: %d word%s waiting on the box and none fetched this link: asking", box.words, box.words == 1 ? "" : "s");
            }
        } else if (!ok) { box.declined = true; if (refusal_note(&refusals, "status", line, millis())) LOG("link: the door does not know `status` ('%s'): an old box", line); }   /* THE REFUSAL SAID ONCE */
        break;
    case R_LOOK:
        /* THE WRIST WARDROBE: ok look form=<form>[/<variant>] colour=<palette>
         * (watch-wardrobe.md rule 3) -- the veil and the theme; a door
         * that does not know the verb says `no verb` (nothing changes) */
        if (ok && !strncmp(line, "ok look ", 8) && line[8]) {
            int r = wardrobe_look(line + 8, "the look", false);
            if (r == 1) LOG("link: the look names '%s', not baked here: as it was", wardrobe_look_missing());
        } else if (!ok && refusal_note(&refusals, "look", line, millis())) LOG("link: the door does not know `look` ('%s'): the wrist keeps its wear", line);   /* THE REFUSAL SAID ONCE */
        break;
    case R_SKY:
        /* ok sky <word> [<word>] | ok none; a desk that does not know the verb says `no verb` (nothing changes) */
        if (ok && !strncmp(line, "ok sky ", 7) && line[7]) {
            const char *who = tr == TR_TLS ? "the box" : "the desk";
            if (sky_set(line + 7, who)) LOG("sky: %s (from %s; held an hour)", line + 7, who);
            else LOG("sky: '%s' is not a word the gel knows (rain snow cold hot clear storm wind dark): ignored", line + 7);
        } else if (ok && refusal_note(&refusals, "sky", "none", millis())) LOG("link: no sky (said once per link; counted: `link`)");   /* THE REFUSAL SAID ONCE */
        break;
    case R_BYE:
        drop("bye");
        if (!wired()) { radio_asked = false; net_radio_off(); }   /* THE RADIO BUDGET: the session's end is the radio's */
        break;
    default:
        LOG("link: a stray reply '%s'", line);
    }
}

/* the desk asks two things: wifi add <ssid> <psk> | wifi add <ssid> eap <id> <pw>
 * (a network handed over), and wisp <app> [<title>] -- THE WISP FROM THE
 * DESK: its doorbell rang; `ok` = it flew in or waits for the light
 * (wisp.cpp), `ok door` = nobody home: it waits at the air door for him
 * (THE WISP AT THE DOOR, E43; `no house` is retired -- the desk's done()
 * takes any `ok ...`). `wisp jarvas <title>` is the box's own voice: the
 * mote in the body's ink (THE WISP'S INK, E42) */
static void handle_ask(const char *line)
{
    LOG("link< %s (an ask)", !strncmp(line, "wifi add ", 9) ? "wifi add ..." : line);
    if (proof_read_only(&proof) && proof_gated_ask(line)) {   /* THE DOOR'S OWN PROOF (T10): an unproven door's hands (wifi add / look / knock / alarm) are refused; wisp and ble pair pass (ruling 149) */
        link_write("no proof\n");
        LOG("link> no proof (the door did not prove itself: %s refused)", !strncmp(line, "wifi add ", 9) ? "wifi add" : line);
        return;
    }
    /* THE KNOCK (LAN or the dock), 2026-09-10 night: `knock <what> [<stamp>
     * <mac>]` -- the desk's `POST /lease` body with one word in front,
     * written into the dock's session (the same verify, the same act:
     * link_knock). `free` here claims at once, cable or not -- the desk's
     * hand outranks the cable rule as the hold does (the whistle path).
     * From Khalid's 21:16-21:29: `slimectl go` on the laptop, the watch on
     * the campus dock, the LAN knock lost to client isolation, the docked
     * watch never claiming on its own -- the slime was nowhere. */
    if (!strncmp(line, "knock ", 6)) {
        bool held = house_holding();
        char why[24]; int r = link_knock(line + 6, why, sizeof why);
        char l[64];
        if (r == 0 && held && !house_holding()) {     /* THE TRACE ANSWERED (C26): `lost` took him -- the posture he left in rides the answer */
            int w; float lean; app_trace(&w, &lean);
            snprintf(l, sizeof l, "ok %s wall=%s lean=%.2f\n", why, GEL_WALL_NAMES[w & 3], lean);
        } else snprintf(l, sizeof l, "%s %s\n", r == 0 ? "ok" : "no", why);
        link_write(l);
        LOG("link> %s %s (the desk's knock%s)", r == 0 ? "ok" : "no", why, tr == TR_SERIAL ? " over the dock" : tr == TR_BLE ? " over the air door" : "");
        return;
    }
    if (!strncmp(line, "wisp ", 5) && line[5]) {
        char buf[LINK_LINE_MAX + 1]; strncpy(buf, line + 5, sizeof buf - 1); buf[sizeof buf - 1] = 0;
        char *save = NULL;
        char *appid = strtok_r(buf, " ", &save);
        char *title = strtok_r(NULL, "", &save);      /* the rest of the line, spaces and all */
        bool home = house_holding();
        int r = wisp_arrive(appid, title, home);
        const char *ans = r != WISP_OK ? "no house" : !home ? "ok door" : "ok";
        char l[16]; snprintf(l, sizeof l, "%s\n", ans); link_write(l);
        LOG("link> %s (wisp %s [%s]%s)", ans, appid ? appid : "?", wisp_source_name(wisp_source_of(appid)),
            r != WISP_OK ? "" : !home ? ": waits at the air door for him" : wisp_waiting() ? ": waits for the light" : ": flies in");
        return;
    }
    if (!strncmp(line, "look ", 5) && line[5]) {
        /* THE WRIST WARDROBE, pushed (watch-wardrobe.md rule 3): after
         * every `ok held`, and whenever the desk morphed, shed or
         * recoloured while the wrist holds -- `look form=<form>[/<variant>]
         * colour=<palette>`: `ok` = the veil and the change; `no art
         * <form>` = a form not baked here (the colour taken, the body as
         * it was; the desk logs it, nothing else) */
        int r = wardrobe_look(line + 5, "the desk's look", false);
        char l[48];
        if (r == 1) snprintf(l, sizeof l, "no art %s\n", wardrobe_look_missing());
        else snprintf(l, sizeof l, "%s\n", r == 0 ? "ok" : "no words");
        link_write(l);
        LOG("link> %s (look)", l);
        return;
    }
    if (!strcmp(line, "ble pair") || !strncmp(line, "ble pair ", 9)) {
        /* THE PAIRING WINDOW opened from the desk (`slimectl watch ble pair`), over whichever door is open:
         * `ok pair <secs>` = open for that long; `no ble off` = the BLUETOOTH row is OFF here (his switch: tap it first) */
        int secs = atoi(line + 8);
        int left = ble_pair_open(secs > 0 ? (uint32_t)secs : 0, "the desk");
        char l[32];
        if (left > 0) snprintf(l, sizeof l, "ok pair %d\n", left); else strcpy(l, "no ble off\n");
        link_write(l);
        LOG("link> %s (the desk's ble pair)", left > 0 ? l : "no ble off");
        return;
    }
    if (!strcmp(line, "alarm") || !strncmp(line, "alarm ", 6)) {
        /* THE ALARM FROM THE DESK (T13, `slimectl watch alarm 07:30|off|status`): the ALARM row's own road
         * (app_alarm_set: NVS, the hour remembered on off); `ok alarm HH:MM` / `ok alarm off` / `no clock`
         * (a set on a wrist nobody told the day) / `no alarm` (a bad word). Nothing lit before its minute. */
        int minute = -1, kind = alarmask_parse(line[5] ? line + 6 : "", &minute);
        bool clock = clock_is_set();
        if (kind == ALARMASK_SET && clock) app_alarm_set(minute, "the desk");
        else if (kind == ALARMASK_OFF) app_alarm_set(-1, "the desk");
        char ans[24]; alarmask_answer(kind, clockwork_alarm(), clock, ans, sizeof ans);
        char l[28]; snprintf(l, sizeof l, "%s\n", ans); link_write(l);
        LOG("link> %s (the desk's alarm)", ans);
        return;
    }
    if (!strcmp(line, "hold") || !strncmp(line, "hold ", 5)) {
        /* THE FLASH FROM THE DESK (T14, `slimectl watch flash`): `hold <s>` keeps the radio up for s seconds
         * (capped 300, ruling 151) as the console's `link hold` keeps it this lit spell -- the desk's espota
         * rides it; `hold off` / `hold` release / look. `ok hold <s>` / `ok hold off` / `no hold` (a bad word)
         * / `no network` (nothing to bring up) / `no spent` (THE LAST DROP'S MANNERS refuse the dial). */
        uint32_t secs = 0; int kind = holdask_parse(line[4] ? line + 5 : "", &secs);
        bool nets = store.n_nets > 0, spent = spent_line() && !charging();
        if (kind == HOLDASK_SET && nets && !spent) { link_hold_for(secs); app_keep_awake(); }   /* the ask is a word: the glass stays lit for it, as over serial */
        else if (kind == HOLDASK_OFF && hold) link_hold_for(0);
        char ans[24]; holdask_answer(kind, hold, link_hold_left_s(), nets, spent, ans, sizeof ans);
        char l[28]; snprintf(l, sizeof l, "%s\n", ans); link_write(l);
        LOG("link> %s (the desk's hold)", ans);
        return;
    }
    if (!strncmp(line, "wifi add ", 9)) {
        char buf[LINK_LINE_MAX + 1]; strncpy(buf, line + 9, sizeof buf - 1); buf[sizeof buf - 1] = 0;
        char *save = NULL;
        char *ssid = strtok_r(buf, " ", &save);
        char *a = strtok_r(NULL, " ", &save);
        bool ok = false;
        if (ssid && a && !strcmp(a, "eap")) {
            char *id = strtok_r(NULL, " ", &save), *pw = strtok_r(NULL, " ", &save);
            if (id && pw) ok = store_net_add(ssid, NET_EAP, id, "", pw);
        } else if (ssid) {
            ok = store_net_add(ssid, a ? NET_PSK : NET_OPEN, "", "", a ? a : "");
        }
        link_write(ok ? "ok\n" : "no full\n");
        LOG("link> %s", ok ? "ok" : "no full");
        return;
    }
    link_write("no verb\n");
}

static void handle_line(const char *line)
{
    if (!line[0]) return;
    quiet_ms = millis();                        /* THE RADIO BUDGET: a line either way is something to say */
    if (!strncmp(line, "hi ", 3) || !strcmp(line, "hi")) {
        if (!line[3]) { LOG("link< hi (no nonce)"); return; }
        const char *nonce = line + 3;
        if (!store.key[0]) { LOG("link: no key here (`key <hex>`)"); drop("no key"); return; }
        char mac[33];
        hmac_hex(nonce, mac);
        char l[64];
        proof_ask(&proof, mac, esp_random(), esp_random());   /* THE DOOR'S OWN PROOF (T10): `hello watch <mac> <nonce2>` -- the door must HMAC the nonce back */
        proof_hello_line(&proof, l, sizeof l);
        set_state(S_HELLO);
        LOG("link< hi %s", nonce);
        request(R_HELLO, l, REPLY_MS);
        return;
    }
    bool ans = (!strncmp(line, "ok", 2) && (line[2] == 0 || line[2] == ' '))
            || (!strncmp(line, "no", 2) && (line[2] == 0 || line[2] == ' '));
    if (ans) {
        if (inflight == R_HELLO && proof_refused(&proof, line)) {   /* THE DOOR'S OWN PROOF (T10): an old watchlink counts the hello's words and says `no hello` without hanging up -- the bare hello of old goes out once on the same door nonce; its bare `ok hello` lands as unproven (read-only) */
            char l[64]; proof_hello_line(&proof, l, sizeof l);
            LOG("link: `no hello` to the nonce'd hello: an old desk -- the bare hello goes out (read-only once linked)");
            request(R_HELLO, l, REPLY_MS);
            return;
        }
        /* over the dock the desk also answers our own log lines: those are
         * `no verb` / `no long`, never a reply to a verb we speak */
        bool worded_lease = sent_worded && (inflight == R_BEAT || inflight == R_CLAIM);
        if (tr == TR_SERIAL && !(worded_lease && !strcmp(line, "no verb"))
            && (!strcmp(line, "no verb") || !strcmp(line, "no long") || !strcmp(line, "no hello"))) return;
        if (inflight == R_NONE) { LOG("link: unasked '%s'", line); return; }
        handle_reply(line);
        return;
    }
    handle_ask(line);
}

static void feed(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = (char)buf[i];
        if (c == '\n') { rxline[rxn] = 0; rxn = 0; handle_line(rxline); continue; }
        if (c == '\r') continue;
        if (rxn < sizeof rxline - 1) rxline[rxn++] = c; else rxn = 0;
    }
}

/* --- the transports ------------------------------------------------------------ */

static void close_sock(void)
{
    if (sock >= 0) { close(sock); sock = -1; }
}

static void close_tls(const char *why)
{
    if (!tls) return;
    if (dial_state == 1) {
        /* the dial task still owns it: the reaper in link_tick frees it
         * when the task is done (its connect fails on its own with the
         * radio gone, or times out) */
        LOG("link: the tls dial is still running (%s); freed when it ends", why);
        return;
    }
    uint32_t before = ESP.getFreeHeap();
    tls->stop();
    delete tls;
    tls = NULL;
    LOG("link: tls session dropped (%s): heap %lu -> %lu", why, (unsigned long)before, (unsigned long)ESP.getFreeHeap());
}

static void drop(const char *why)
{
    bool was = st == S_LINKED;
    if (dsync.active) dsync_abort("dropped", 0);
    if (st != S_IDLE && st != S_BACKOFF) LOG("link: %s (%s)", was ? "hung up" : "not linked", why);
    close_sock();
    if (tls) close_tls(why);
    inflight = R_NONE;
    sent_worded = false;
    lease_now = false;
    heap_low = false;
    rxn = 0;
    /* the lease held is THE HOUSE's belief, sticky until a door says (house.cpp) */
    if (wired()) { tr = TR_NONE; set_state(wifi_up ? S_BACKOFF : S_IDLE); return; }
    tr = TR_NONE;
    set_state(wifi_up ? S_BACKOFF : S_IDLE);
}

static bool tcp_start(uint32_t ip, uint16_t port)
{
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return false;
    int fl = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, fl | O_NONBLOCK);
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    struct sockaddr_in sa; memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = ip;
    int r = connect(sock, (struct sockaddr *)&sa, sizeof sa);
    if (r < 0 && errno != EINPROGRESS) { close_sock(); return false; }
    tr = TR_TCP;
    return true;
}

static int tcp_connected(void)     /* 1 yes, 0 not yet, -1 failed */
{
    fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
    struct timeval tv = { 0, 0 };
    int r = select(sock + 1, NULL, &wf, NULL, &tv);
    if (r <= 0) return 0;
    int err = 0; socklen_t el = sizeof err;
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &el);
    return err == 0 ? 1 : -1;
}

static void tcp_read(void)
{
    uint8_t buf[128];
    for (int k = 0; k < 4; k++) {
        int n = recv(sock, buf, sizeof buf, MSG_DONTWAIT);
        if (n > 0) { feed(buf, n);
        if (sock < 0 || tr != TR_TCP) return;   /* the line may have dropped the session */ continue; }
        if (n == 0) { drop("the desk hung up"); return; }
        if (errno == EWOULDBLOCK || errno == EAGAIN) return;
        drop("recv error");
        return;
    }
}

/* the TLS session's socket stays non-blocking after the handshake (the
 * core sets O_NONBLOCK and never clears it): available() pulls what the
 * socket holds and returns the decrypted bytes waiting, or 0, or < 0 for
 * a peer gone -- never a wait */
static void tls_read(void)
{
    uint8_t buf[128];
    for (int k = 0; k < 4; k++) {
        int n = tls->available();
        if (n < 0) { drop("the box hung up"); return; }
        if (n == 0) {
            /* a peer gone closes the session inside available(); after
             * that it answers 0 for ever -- connected() is what says so */
            if (!tls->connected()) drop("the box hung up");
            return;
        }
        if (n > (int)sizeof buf) n = sizeof buf;
        n = tls->read(buf, n);
        if (n <= 0) { drop("tls read error"); return; }
        feed(buf, n);
        /* the line just fed may have ended the session (`ok bye` -> drop
         * -> the client deleted): never touch tls again this loop -- this
         * null read was the boot loop of the first radio-budget night
         * (LoadProhibited in tls_read, 105 resets in two hours) */
        if (!tls || tr != TR_TLS) return;
    }
}

/* start dialling door i: a task for the lookup (plain) or the whole
 * connect (tls); an IP literal on a plain door needs neither */
static void dial(int i)
{
    const door_t *d = door(i);
    if (!d) { set_state(S_BACKOFF); return; }
    cur_door = i;
    dial_ms = millis();
    strncpy(dial_host, d->host, sizeof dial_host - 1); dial_host[sizeof dial_host - 1] = 0;
    dial_port = d->port;
    dial_tls = d->tls != 0;
    if (dial_tls) {
        /* the room the session needs: with mbedtls in PSRAM (the default)
         * the big block is PSRAM's and the internal need is the dial's
         * stack, the socket and the pbufs; internal, the old rule */
        uint32_t free_now = ESP.getFreeHeap(), block = ESP.getMaxAllocHeap();
        uint32_t need_heap = tls_psram ? TLS_DIAL_HEAP_PSRAM : TLS_DIAL_HEAP;
        uint32_t big_block = tls_psram ? (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) : block;
        if (free_now < need_heap || big_block < TLS_DIAL_BLOCK) {
            LOG("link: heap %lu free, largest %s block %lu: too little for a TLS session (%lu / %lu needed)",
                (unsigned long)free_now, tls_psram ? "psram" : "internal", (unsigned long)big_block,
                (unsigned long)need_heap, (unsigned long)TLS_DIAL_BLOCK);
            door_failed(i, "low heap");
            set_state(S_BACKOFF);
            return;
        }
        heap_before = free_now;
        tls = new WiFiClientSecure();
        if (!tls) { door_failed(i, "no memory"); set_state(S_BACKOFF); return; }
        tr = TR_TLS;
        dial_state = 1;
        LOG("link: dialing door %d %s:%u tls (heap %lu free; the session's buffers in %s)", i, d->host, d->port,
            (unsigned long)free_now, tls_psram ? "psram" : "internal ram");
        /* the handshake wants stack: 10 KB, from the heap, for the dial's seconds only */
        if (xTaskCreatePinnedToCore(dial_task, "dial", 10240, tls, 1, NULL, 0) != pdPASS) {
            dial_state = 0;
            door_failed(i, "no task");
            drop("no task for the dial");
            return;
        }
        set_state(S_DIAL);
        return;
    }
    struct in_addr ia;
    if (inet_aton(d->host, &ia)) {
        if (tcp_start(ia.s_addr, d->port)) { set_state(S_CONNECT); LOG("link: calling door %d %s:%u", i, d->host, d->port); }
        else { door_failed(i, "socket"); set_state(S_BACKOFF); }
        return;
    }
    dial_state = 1;
    if (xTaskCreatePinnedToCore(dial_task, "dial", 4096, NULL, 1, NULL, 0) != pdPASS) {
        dial_state = 0; door_failed(i, "no task"); set_state(S_BACKOFF); return;
    }
    set_state(S_DIAL);
}

/* --- the public face ------------------------------------------------------------ */

void link_begin(void)
{
    /* THE TLS SESSION IN PSRAM: mbedtls's calloc is ours from here (the
     * HMAC below goes through it too: a few bytes, either heap) */
    mbedtls_platform_set_calloc_free(tls_calloc, tls_free);
    memset(&grain, 0, sizeof grain);
    memset(door_st, 0, sizeof door_st);
    for (int i = 0; i < STORE_DOORS; i++) strcpy(door_st[i].last, "-");
    if (store.grain[0]) { parse_grain(store.grain, false); }   /* the cached line: the knobs, not the belly, not the day */
    dockhand_begin(&dockhand, on_cable());                     /* THE DOCK HANDS HIM OVER: a boot on the cable is no edge */
}

void link_net_up(bool up)
{
    wifi_up = up;
    if (tr == TR_SERIAL) return;
    if (!up) { drop("wifi down"); set_state(S_IDLE); return; }
    if (!store.key[0] || !store.n_doors) {
        if (!no_link_said) { LOG("link: no desk to call (`desk add <host> <port> [tls]` and `key <hex>`)"); no_link_said = 1; }
        return;
    }
    backoff_ms = BACKOFF_MIN_MS;
    set_state(S_BACKOFF);
    st_ms = millis() - backoff_ms;             /* now */
}

bool link_serial_line(const char *line)
{
    if (tr != TR_SERIAL) {
        if (strncmp(line, "hi ", 3)) return false;
        if (tr == TR_TCP || tr == TR_TLS) drop("the dock takes over");
        tr = TR_SERIAL;
        set_state(S_WAIT_HI);
        LOG("link: DOCKED (the desk on the serial port)");
    }
    handle_line(line);
    return true;
}

bool link_docked(void) { return tr == TR_SERIAL; }

/* THE DOCK FACE: the day being carried -- true while a sync is in
 * flight over any door, with the bytes sent so far and the day's size */
bool link_sync_progress(uint32_t *sent, uint32_t *total)
{
    if (sent) *sent = dsync.active ? (uint32_t)dsync.off : 0;
    if (total) *total = dsync.active ? (uint32_t)dsync.len : 0;
    return dsync.active;
}
uint32_t link_sync_done_seq(void) { return sync_done_seq; }

/* the desk's one ask arriving on the serial port outside a dock session
 * (slimectl watch wifi add ... --serial, with no hi first) */
void link_serial_ask(const char *line)
{
    Tr was = tr;
    if (tr == TR_NONE) tr = TR_SERIAL;
    handle_ask(line);
    tr = was;
}

void link_undock(const char *why)
{
    if (tr != TR_SERIAL) return;
    drop(why);
    LOG("link: undocked");
}

/* THE AIR DOOR (bleuart.cpp, 2026-10-06): the desk's lines over BLE,
 * handled as the dock's are -- a `hi <nonce>` opens the session (and
 * takes over a radio session: the air door is the nearer desk), every
 * later line is the session's. The central leaving closes it. */
bool link_ble_line(const char *line)
{
    if (tr != TR_BLE) {
        if (strncmp(line, "hi ", 3)) return false;
        if (tr == TR_TCP || tr == TR_TLS) drop("the air door takes over");
        if (tr == TR_SERIAL) return false;                  /* the cable is nearer still: the air door waits */
        tr = TR_BLE;
        set_state(S_WAIT_HI);
        LOG("link: AIR-DOCKED (the desk over bluetooth)");
    }
    handle_line(line);
    return true;
}

void link_air_closed(const char *why)
{
    if (tr != TR_BLE) return;
    drop(why);
    LOG("link: the air door closed");
}

bool link_air(void) { return tr == TR_BLE; }

void link_sense(const char *kind, const char *words)
{
    /* the body raised to look is the word's glance, linked or not -- and the sky's raise (a cold shiver, a hot droop) */
    if (!strcmp(kind, "wrist:raised")) { word_glance(); sky_raise(); }
    /* THE HOUSE: while EMPTY only his own day goes up (raised, shaken, slept) */
    if (!house_sense_ok(kind)) return;
    /* THE RADIO BUDGET: queued whether or not a link is up -- a kind
     * worth the radio is a reason to dial, the rest ride along */
    bool worth = true;
    for (int i = 0; SENSE_RIDE_ALONG[i]; i++) if (!strcmp(kind, SENSE_RIDE_ALONG[i])) { worth = false; break; }
    uint32_t now = millis();
    for (int i = 0; i < sense_gap_n; i++)
        if (!strcmp(sense_gap[i].kind, kind)) {
            if (now - sense_gap[i].ms < SENSE_GAP_MS) return;
            sense_gap[i].ms = now;
            goto queue;
        }
    if (sense_gap_n < 8) { strncpy(sense_gap[sense_gap_n].kind, kind, 23); sense_gap[sense_gap_n].ms = now; sense_gap_n++; }
queue:
    if (sense_n >= SENSE_Q) return;
    snprintf(sense_q[sense_n].line, sizeof sense_q[0].line, "sense %s %s\n", kind, words);
    sense_q[sense_n].ms = now;
    sense_q[sense_n].worth = worth;
    sense_n++;
    if (worth && st != S_LINKED) LOG("link: sense %s queued (worth the radio: a dial is due)", kind);
}

/* the next sense to send, the stale ride-alongs dropped; false = none */
static bool sense_pop(char *out, int cap)
{
    uint32_t now = millis();
    while (sense_n) {
        bool stale = !sense_q[0].worth && (now - sense_q[0].ms) >= SENSE_STALE_S * 1000u;
        if (!stale) { strncpy(out, sense_q[0].line, cap - 1); out[cap - 1] = 0; }
        for (int i = 1; i < sense_n; i++) sense_q[i - 1] = sense_q[i];
        sense_n--;
        if (!stale) return true;
    }
    return false;
}

static bool sense_worth_waiting(void)
{
    for (int i = 0; i < sense_n; i++) if (sense_q[i].worth) return true;
    return false;
}

void link_whistle(void)
{
    bool held = house_holding();               /* THE HOUSE says whether this is the come or the release */
    whistle_req = held ? 2 : 1;
    reaching = !held;                          /* THE REACHING CUE: a come is out; a release sinks him on the wrist's word, nothing to wait for */
    if (st != S_LINKED) LOG("whistle: %s (no link yet: the radio comes up for it)", held ? "release" : "come");
    else LOG("whistle: %s", held ? "release" : "come");
}

void link_sync_now(void) { sync_asked = true; dsync_forget(true); }
bool link_linked(void) { return st == S_LINKED; }
bool link_holding(void) { return house_holding(); }   /* THE HOUSE's word */
const char *link_state_name(void) { return ST_NAMES[st]; }
const link_grain_t *link_grain(void) { return &grain; }
uint32_t link_last_ok_ms(void) { return last_ok_ms; }
uint32_t link_desk_ok_ms(void) { return desk_ok_ms; }
int link_days_synced(void) { return days_synced; }

void link_door_line(int i, char *out, int cap)
{
    const door_t *d = door(i);
    if (!d) { snprintf(out, cap, "  %d  -", i); return; }
    uint32_t now = millis();
    bool skipped = (int32_t)(now - door_st[i].next_ms) < 0;
    char note[64] = "";
    if (i == cur_door && st == S_LINKED) strcpy(note, "  (held)");
    else if (skipped) snprintf(note, sizeof note, "  skipped %lu s more", (unsigned long)((door_st[i].next_ms - now) / 1000));
    if (net_ssid()[0] && store_near_door(net_ssid()) == i && !d->tls) {   /* THE NEAR DOOR FIRST */
        size_t k = strlen(note);
        snprintf(note + k, sizeof note - k, "  near door on %s: tried first", net_ssid());
    }
    snprintf(out, cap, "  %d  %-40s %5u  %-5s  %s%s", i, d->host, d->port, d->tls ? "tls" : "plain",
             door_st[i].last, note);
}

void link_dockhand_set(bool on)
{
    bool changed = on == store.dockhand_off;
    store.dockhand_off = !on;
    if (changed) store_save_dockhand();
    LOG("link: dock hand %s%s -- %s", on ? "on" : "off", changed ? " (stored)" : "",
        on ? "docking while holding releases him to the desk (`release docked`); undocking worn claims him back" : "the cable is a charger: a held slime stays through a dock");
}

void link_dockhand_line(char *out, int cap)
{
    snprintf(out, cap, "dock: hand %s; %s%s; house %s%s; the undock's claim %s (moved %lu s ago, the minute is the line)",
             store.dockhand_off ? "off" : "on", on_cable() ? "on the cable" : "off the cable", tr == TR_SERIAL ? " (the dock's session is open)" : "",
             house_state_name(), dockhand.handed ? ", handed over at this dock" : "",
             dockhand.handed ? (house_still_ms() < DOCKHAND_WORN_MS ? "would go" : "would wait for a move") : "not owed",
             (unsigned long)(house_still_ms() / 1000u));
}

void link_beat_worn(bool on)
{
    store.beat_worn = on;
    store_save_beat_worn();
    word_declined = false;
    LOG("link: beat-worn %s (stored) -- the minute's %s %s", on ? "on" : "off", house_holding() ? "beat" : "claim",
        on ? (house_worn() ? "carries `worn`" : "carries `still`") : "goes bare");
}

void link_status(char *out, int cap)
{
    const door_t *d = door(cur_door);
    char via[96];
    if (tr == TR_SERIAL) strcpy(via, "dock");
    else if (tr == TR_BLE) snprintf(via, sizeof via, "air door (mtu %u)", (unsigned)bleuart_mtu());
    else if (d && st != S_IDLE && st != S_BACKOFF)
        snprintf(via, sizeof via, "door %d %s:%u %s", cur_door, d->host, d->port, d->tls ? "tls" : "plain");
    else snprintf(via, sizeof via, "%u door%s", store.n_doors, store.n_doors == 1 ? "" : "s");
    char sky[24];
    if (sky_known()) snprintf(sky, sizeof sky, "%s%s%s", sky_word(0), sky_word(1)[0] ? " " : "", sky_word(1));
    else strcpy(sky, "-");
    char radio[160]; link_radio_line(radio, sizeof radio);
    char bx[96]; link_box_line(bx, sizeof bx);
    char rf[200]; link_refusal_line(rf, sizeof rf);   /* THE REFUSAL SAID ONCE: "" until a door repeats itself */
    snprintf(out, cap, "link %s%s%s%s via %s holding %s beat-worn %s%s (%s) synced %d last-ok %lus ago grain %s word %s sky %s%s | %s | radio %s%s%s",
             ST_NAMES[st], desk_name[0] && st == S_LINKED ? " to " : "",
             desk_name[0] && st == S_LINKED ? desk_name : "",
             st == S_LINKED && proof_read_only(&proof) ? " (unproven: read-only)" : "",   /* THE DOOR'S OWN PROOF (T10) */
             via,
             house_holding() ? "yes" : "no",
             store.beat_worn ? "on" : "off", store.beat_worn && word_declined ? " (no verb: bare this link)" : "",
             house_worn() ? "worn" : "still",
             days_synced, last_ok_ms ? (unsigned long)((millis() - last_ok_ms) / 1000) : 0UL,
             grain.have ? store.grain : "-",
             word_showing() ? "showing" : word_held() ? "held" : "-", sky,
             dsync.active ? " (a day in flight)" : "", bx, radio, rf[0] ? " | " : "", rf);
}

/* THE REFUSAL SAID ONCE (T5): the doors' repeated words since this session's
 * hello -- "refusals: no gate x 212 since 09:12 (beat)"; "" while no door
 * has refused anything (the line is carried only when there is one) */
void link_refusal_line(char *out, int cap)
{
    if (!refusal_rows(&refusals) && !refusals.unkept) { if (cap > 0) out[0] = 0; return; }
    refusal_status(&refusals, out, cap, millis(), clock_local_minute());
}

/* THE BOX'S STATUS for the console (`link`, `status`, the doctor):
 * `box: holder=kaptop day=2026-09-11 words=2 sky=clear` -- or what
 * stands in its way */
void link_box_line(char *out, int cap)
{
    if (box.have) snprintf(out, cap, "box: holder=%s day=%s words=%d sky=%s", box.holder, box.last_day, box.words, box.sky);
    else if (box.declined) snprintf(out, cap, "box: no verb (an old box: no status)");
    else snprintf(out, cap, "box: not asked yet");
}

/* ... and for the watch page's ABOUT row, short: `KAPTOP 2 CLEAR` (the
 * holder, the words waiting, the box's sky); "" = nothing to show */
const char *link_box_short(void)
{
    static char s[32];
    if (!box.have) return "";
    snprintf(s, sizeof s, "%s %d %s", box.holder, box.words, box.sky);
    for (char *p = s; *p; p++) { if (*p >= 'a' && *p <= 'z') *p -= 32; if (*p == ',') *p = ' '; }
    return s;
}

/* THE TRACE ANSWERED (design/watch-ideas.md C26, 2026-10-09): the release
 * carries the posture he rests in -- `release [docked] wall=<bottom|right|
 * top|left> lean=<-1.00..1.00>` (app_trace: THE WALL and the residual's
 * eased lean as a fraction of LEAN_CAP, + = his right along the wall) --
 * so the desk's arrival can lean the way he left (watch-link.md section
 * 2; this desk's door reads the words, the box's is asked to shrug at
 * them as at `release asleep`). The night's release carries none: a dark
 * glass has no posture worth answering. */
static const char *release_line(const char *word)
{
    static char l[56];
    int w; float lean; app_trace(&w, &lean);
    snprintf(l, sizeof l, "release%s%s wall=%s lean=%.2f\n", word ? " " : "", word ? word : "", GEL_WALL_NAMES[w & 3], lean);
    return l;
}

/* the minute's verb with its word, when the flag is on and this link's box has not declined it */
static const char *lease_verb(Req r)
{
    static char l[16];
    bool worded = store.beat_worn && !word_declined;
    bool worn = house_worn();
    snprintf(l, sizeof l, "%s%s\n", r == R_BEAT ? "beat" : "claim",
             worded ? (worn ? " worn" : " still") : "");
    sent_worded = worded;
    sent_worn = worded && worn;
    return l;
}

/* the next verb, in order; true = something was sent or is in flight
 * (a day), false = nothing to say (THE RADIO BUDGET's linger counts) */
static bool schedule(void)
{
    uint32_t now = millis();
    if (whistle_req) {
        int w = whistle_req; whistle_req = 0;
        /* THE NIGHT RELEASE: `release asleep` -- the bare verb with one
         * word the box may ignore (the desk's door does: watchlink.py
         * reads the first word; the box's daemon is ASSUMED to, as the
         * ACK asks of `beat`'s trailing word); the rising's claim is the
         * honest claim, worded as the minute's */
        if (w == 3) request(R_RELEASE, "release asleep\n", LEASE_REPLY_MS);
        else if (w == 4) request(R_CLAIM, lease_verb(R_CLAIM), LEASE_REPLY_MS);
        else if (w == 5) request(R_RELEASE, release_line("docked"), LEASE_REPLY_MS);   /* THE DOCK HANDS HIM OVER: the desk claims at once on the word */
        else if (w == 2) request(R_RELEASE, release_line(NULL), LEASE_REPLY_MS);       /* the hold's release, THE TRACE on it */
        else request(R_COME, "come\n", LEASE_REPLY_MS);
        return true;
    }
    if (need_claim) {
        need_claim = false;
        /* THE CABLE NEVER CLAIMS: the re-claim a beat's `no lapsed` or a
         * knock's `lost` asked for is not sent on the cable -- the lapse
         * is let go on the next beat, the knock taken at its word */
        if (may_claim()) { request(R_CLAIM, lease_verb(R_CLAIM), LEASE_REPLY_MS); return true; }
        LOG("link: the claim owed is not sent %s (%s)", on_cable() ? "on the cable" : "still (the shelf)", house_state_name());
    }
    {
        char l[SENSE_LINE];
        if (sense_pop(l, sizeof l)) { request(R_SENSE, l, REPLY_MS); return true; }
    }
    if (need_time) { need_time = false; request(R_TIME, "time\n", REPLY_MS); return true; }
    if (need_grain) { need_grain = false; request(R_GRAIN, "grain\n", REPLY_MS); return true; }
    if (need_look) { need_look = false; request(R_LOOK, "look\n", REPLY_MS); return true; }   /* THE WRIST WARDROBE: the costume and the colour, after the `ok held` */
    if (errand) { need_word = need_sky = need_status = false; sync_asked = false; return false; }   /* the dark errand carries its verb and nothing else: no day, no word, no sky, no status */
    if (!dsync.active && (sync_asked || (int32_t)(now - sync_check_at) >= 0)) {
        if (sync_asked) { sync_asked = false; }
        sync_check_at = now + 60000;
        if (dsync_start()) return true;
        day_pending = false;                    /* the walk found nothing to carry (or nothing it may) */
    }
    if (dsync.active) return true;
    /* THE HOUSE's claim: once per link, HONEST (never a come), after
     * the day sync has had its walk -- `ok held` = the slime comes to
     * the wrist (the desk fell silent); `no desk` = the desk has him */
    if (need_house && !dsync.active && !sync_asked) {
        need_house = false;
        /* THE CABLE NEVER CLAIMS: the link-up's honest claim stays unsent
         * on the cable (UNKNOWN still asks its one first question); a
         * HOLDING house is refreshed by the cadence's beat instead */
        if (may_claim()) { request(R_CLAIM, lease_verb(R_CLAIM), LEASE_REPLY_MS); return true; }
        LOG("link: the link-up's claim is not sent %s (%s)", on_cable() ? "on the cable" : "still (the shelf)", house_state_name());
    }
    /* THE BOX'S STATUS: once per link, after the day sync (the box's
     * last_day then names the day just carried), before the word */
    if (need_status && !dsync.active && !sync_asked && !need_house) {
        need_status = false;
        request(R_STATUS, "status\n", REPLY_MS);
        return true;
    }
    /* the word: once, after the day sync has had its walk (never a stream) */
    if (need_word && !dsync.active && !sync_asked) {
        need_word = false;
        request(R_WORD, "word\n", REPLY_MS);
        return true;
    }
    /* THE SKY: once, after the word (fetch-only; `ok none` is today's usual answer) */
    if (need_sky && !dsync.active && !sync_asked && !need_word) {
        need_sky = false;
        request(R_SKY, "sky\n", REPLY_MS);
        return true;
    }
    if (lease_now || (uint32_t)(now - lease_ms) >= radio_lease_every_s(glass_lit(), house_state()) * 1000u) {
        lease_now = false;
        /* the cadence: a `beat` while THE HOUSE holds (refreshing it);
         * not holding, an honest `claim` instead -- watch-link.md 6:
         * "when he walks away the desk falls silent, the lease lapses
         * on the box's clock, and the wrist's next claim lands".
         * THE LIT GLASS POLLS (radio.cpp): while the glass is lit the
         * session is held and the verb goes every CLAIM_LIT_S / BEAT_LIT_S
         * (5 s: a `go` or a `come` on the desk is felt within it); with
         * the glass dim it is the minute, which over the radio only the
         * dock or a hold ever reaches (the linger hangs up first) */
        int v = radio_lease_verb(on_cable(), house_state(), house_worn());   /* THE SHELF NEVER CLAIMS: EMPTY and still has no verb either */
        if (v != LEASE_NONE) {                  /* THE CABLE NEVER CLAIMS: EMPTY on the cable has no verb -- nothing to say, the linger counts */
            Req r = v == LEASE_BEAT ? R_BEAT : R_CLAIM;
            request(r, lease_verb(r), LEASE_REPLY_MS);
            return true;
        }
    }
    return sync_asked;                          /* a walk still owed: not quiet yet */
}

/* --- THE RADIO BUDGET ------------------------------------------------------------ */

static bool can_dial(void) { return store.n_nets && store.n_doors && store.key[0]; }

static void radio_in(radio_in_t *in)
{
    memset(in, 0, sizeof *in);
    in->now_ms = millis();
    in->have_ok = last_ok_ms != 0;
    in->last_ok_ms = last_ok_ms;
    in->awake_since_ok_s = awake_since_ok_ms / 1000u;
    in->pending = whistle_req != 0 || day_pending || sense_worth_waiting();
    in->house_state = house_state();
    in->docked = wired();                                 /* the dock or THE AIR DOOR: no radio needed */
    in->can = can_dial();
    in->forced = (hold || force_once) && store.n_nets;   /* his hand, but never without a network: nothing to bring up */
    in->lean = vigor_lean();                              /* BATTERY MANNERS: the cell under 10 % -- the EVERY clock doubled */
    in->spent = spent_line();                             /* THE LAST DROP'S MANNERS: under the spent line no WiFi dial... */
    in->cable = charging();                               /* ... unless on the cable (the rail is fed) */
}

static void ago(char *out, int cap, uint32_t s)
{
    if (s < 60) snprintf(out, cap, "%lu s", (unsigned long)s);
    else if (s < 3600) snprintf(out, cap, "%lu min", (unsigned long)(s / 60));
    else snprintf(out, cap, "%lu h %lu min", (unsigned long)(s / 3600), (unsigned long)(s % 3600 / 60));
}

void link_radio_line(char *out, int cap)
{
    radio_in_t in; radio_in(&in);
    int r = radio_reason(&in);
    uint32_t now = in.now_ms;
    char last[24] = "never", next[24];
    if (in.have_ok) ago(last, sizeof last, (now - last_ok_ms) / 1000u);
    ago(next, sizeof next, radio_next_due_s(&in));
    bool up = net_radio_is_on();
    bool linked_radio = st == S_LINKED && !wired();
    if (r == RADIO_DOCKED) snprintf(out, cap, "the dock: the cable is the link, no radio needed%s", up ? " (radio still up)" : "");
    else if (hold && hold_until_ms) snprintf(out, cap, "held up for the desk, %lu s left (`link auto` releases); last link %s ago", (unsigned long)holdask_left_s(hold_until_ms, now), last);   /* THE FLASH FROM THE DESK */
    else if (hold) snprintf(out, cap, "held up this lit spell (`link auto` releases); last link %s ago", last);
    else if (r == RADIO_NOWAY) snprintf(out, cap, "off: nothing to dial with (%s%s%s)", store.n_nets ? "" : "no network ",
                                        store.n_doors ? "" : "no door ", store.key[0] ? "" : "no key");
    else if (r == RADIO_SPENT) snprintf(out, cap, "off: %s (%u mV; the day's sync and the claim too; the ear stays)", radio_reason_name(r), (unsigned)batt_mv());   /* THE LAST DROP'S MANNERS */
    else if (r == RADIO_REST) {
        if (up && linked_radio && radio_hold_session(glass_lit(), heap_low, poll_verb()))
            snprintf(out, cap, "up, served: linked and held while the glass is lit (%s every %lu s); hangs up %lu s after it dims (last ok %s ago)",
                     house_holding() ? "beat" : "claim", (unsigned long)radio_lease_every_s(true, house_state()), (unsigned long)LINK_LINGER_S, last);
        else if (up && linked_radio) snprintf(out, cap, "up, served: linked; hangs up after %lu s of nothing to say (last ok %s ago)%s", (unsigned long)LINK_LINGER_S, last,
                                              !poll_verb() ? (on_cable() ? " -- on the cable: the honest claim is off, nothing to poll with"
                                                                         : " -- still (the shelf): the honest claim is off, nothing to poll with") : "");
        else snprintf(out, cap, "resting: last link %s ago, next due in %s (awake %lu s of %lu%s; the courtesy at %lu min)", last, next,
                      (unsigned long)(awake_since_ok_ms / 1000u), (unsigned long)radio_every_s(&in), in.lean ? " -- the cell under 10 %: doubled" : "",   /* BATTERY MANNERS: the lean cell's doubled claim */
                      (unsigned long)(LINK_STALE_S / 60));
    }
    else if (up && linked_radio) snprintf(out, cap, "up: %s -- linked, at work", radio_reason_name(r));
    else if (up) snprintf(out, cap, "up: %s -- trying (%lu of %lu s; wifi %s)", radio_reason_name(r),
                          (unsigned long)((now - try_ms) / 1000u), (unsigned long)LINK_TRY_S, net_state_name());
    else if (retry_wait) snprintf(out, cap, "off: %s, but no link was found; again in %lu s", radio_reason_name(r),
                                  (unsigned long)((int32_t)(retry_at_ms - now) > 0 ? (retry_at_ms - now) / 1000u : 0));
    else if (!awake) snprintf(out, cap, "off: the dark house (%s waits for the wake)", radio_reason_name(r));
    else snprintf(out, cap, "coming up: %s", radio_reason_name(r));
}

/* the wake: the awake clock steps from here; a closed day is looked for
 * once (a LittleFS walk: never per frame); the retry's wait ends (a new
 * wake may be on a new network) */
void link_wake(void)
{
    awake = true;
    awake_tick_ms = millis();
    retry_wait = false;
    wake_said = false;
    uint32_t d;
    day_pending = wrist_next_unsynced(&d) && !dsync_failed(d);
}

void link_dark(void)
{
    awake = false;
    hold = false;                              /* a hold is for the lit spell */
    hold_until_ms = 0;                         /* ...the desk's timed one too */
    force_once = false;
    radio_asked = false;
    bye_said = false;
}

void link_now(const char *why)
{
    force_once = true;
    retry_wait = false;
    LOG("link: a dial now (%s)", why);
}

void link_hold(bool on)
{
    hold = on;
    hold_until_ms = 0;                         /* the console's hold is untimed */
    retry_wait = false;
    if (on) LOG("link: the radio held up this lit spell (OTA, the page, provisioning); `link auto` releases");
    else LOG("link: the radio back on the budget");
}

bool link_holding_radio(void) { return hold; }

/* THE FLASH FROM THE DESK (T14): the desk's `hold <s>` -- the radio kept
 * up for s seconds (capped by holdask.h), released at the end, at the
 * dark house, or by `hold off` / `link auto`; 0 releases */
void link_hold_for(uint32_t s)
{
    hold = s > 0;
    hold_until_ms = holdask_until(millis(), s);
    retry_wait = false;
    if (hold) LOG("link: the radio held up for the desk, %lu s (a flash, the page); the dark house or `link auto` ends it sooner", (unsigned long)s);
    else LOG("link: the desk let the hold go -- the radio back on the budget");
}
uint32_t link_hold_left_s(void) { return hold ? holdask_left_s(hold_until_ms, millis()) : 0; }
bool link_reaching(void) { return reaching; }
bool link_resting(void) { return reason == RADIO_REST; }
bool link_radio_wanted(void) { return radio_wanted(reason); }

/* the rule applied, once a tick while awake: a reason brings the radio
 * up; a due dial with no link after LINK_TRY_S gives it up for
 * LINK_RETRY_S; no reason and no session over it = off */
static void radio_tick(uint32_t now)
{
    radio_in_t in; radio_in(&in);
    reason = radio_reason(&in);
    if (!awake) return;
    if (hold && holdask_expired(hold_until_ms, now)) {   /* THE FLASH FROM THE DESK: the desk's hold ran out */
        hold = false; hold_until_ms = 0;
        LOG("link: the desk's hold ran out -- the radio back on the budget");
    }
    bool up = net_radio_is_on();
    bool linked_radio = st == S_LINKED && !wired();
    if (!wake_said) {
        wake_said = true;
        char l[160]; link_radio_line(l, sizeof l);
        if (!radio_wanted(reason) || reason == RADIO_NOWAY) LOG("link: %s", l);
    }
    if (!radio_wanted(reason) || reason == RADIO_NOWAY) {
        /* nothing to dial for: a radio up with no session over it goes
         * away (a session's end is its own: the linger, the dark house);
         * a dial task still running is left to the reaper */
        if (up && !hold && !linked_radio && (st == S_IDLE || st == S_BACKOFF || wired()) && dial_state != 1) {
            LOG("link: radio off (%s)", radio_reason_name(reason));
            radio_asked = false;
            net_radio_off();
        }
        return;
    }
    if (linked_radio) { radio_asked = false; return; }   /* being served */
    if (!up) {
        if (retry_wait && (int32_t)(now - retry_at_ms) < 0) return;
        retry_wait = false;
        radio_asked = true; try_ms = now;
        char what[80] = "";
        if (reason == RADIO_PENDING) snprintf(what, sizeof what, " (%s%s%s%s)", whistle_req ? WHISTLE_NAMES[whistle_req] : "", whistle_req ? " " : "",
                                              day_pending ? "a closed day " : "", sense_worth_waiting() ? "a sense" : "");
        else if (reason == RADIO_EVERY && last_ok_ms) snprintf(what, sizeof what, " (awake %lu s since the last ok)", (unsigned long)(awake_since_ok_ms / 1000u));
        else if (reason == RADIO_STALE) snprintf(what, sizeof what, " (last ok %lu min ago)", (unsigned long)((now - last_ok_ms) / 60000u));
        LOG("link: radio on -- %s%s", radio_reason_name(reason), what);
        net_radio_on();
        return;
    }
    if (!radio_asked) { radio_asked = true; try_ms = now; }   /* up by another's hand (a hold released): the try's wall from here */
    if (!hold && (int32_t)(now - try_ms) > (int32_t)(LINK_TRY_S * 1000u)) {
        LOG("link: no link in %lu s (%s; wifi %s, link %s): radio off, again in %lu s", (unsigned long)LINK_TRY_S,
            radio_reason_name(reason), net_state_name(), ST_NAMES[st], (unsigned long)LINK_RETRY_S);
        if (whistle_req) { LOG("whistle: no door found: dropped"); whistle_req = 0; reaching = false; }
        force_once = false;
        radio_asked = false;
        retry_wait = true; retry_at_ms = now + LINK_RETRY_S * 1000u;
        net_radio_off();
    }
}

/* --- THE NIGHT RELEASE (ruling 52) ---------------------------------------------- */

/* rest.cpp's two moments, as the lease's: the night began -> `release
 * asleep` once, the house EMPTY without theatre; a night span ended ->
 * the honest claim at once. Not the wrist's lease at the night's onset:
 * nothing is sent, and the rising claims nothing either (the ordinary
 * rules stand). */
static void night_event(int ev)
{
    if (ev == REST_EV_NIGHT) {
        if (!house_holding()) { LOG("link: the night: the lease is not the wrist's (%s) -- nothing to send", house_state_name()); night_released = false; return; }
        if (!house_night_release()) return;
        whistle_req = 3;
        night_released = true;
        LOG("link: the night: `release asleep`%s", st == S_LINKED ? "" : " (the radio comes up for it)");
    } else if (ev == REST_EV_RISING) {
        if (!night_released) return;
        night_released = false;
        whistle_req = 4;
        reaching = true;                                      /* THE REACHING CUE: the morning's claim is on its way */
        LOG("link: the rising: the claim%s", st == S_LINKED ? "" : " (the radio comes up for it)");
    }
}

/* the nap's minute (main's nap(), between light sleeps): the night's
 * onset lands here with the loop stopped and the radio off, so the verb
 * goes on a DARK ERRAND -- the radio up with the glass dark, the dial,
 * `release asleep`, the linger's bye, the radio off; bounded by
 * ERRAND_MAX_MS. The rising's claim, when a span ends in the dark (30
 * steps in a minute, 15 restless minutes), waits for the wake that is
 * coming anyway: the raise for the hour dials for it (a pending whistle
 * is a reason). One dial a night, one a morning. */
void link_dark_minute(void)
{
    int ev = rest_take_lease_event();
    if (!ev) return;
    night_event(ev);
    if (ev != REST_EV_NIGHT || whistle_req != 3) return;
    if (wired()) return;                                      /* the dock or the air door: the wire carries it on the next tick */
    if (spent_line() && !charging()) { whistle_req = 0; house_release_unanswered(); LOG("link: the night's errand skipped -- spent, off the cable: the radio would be refused (the wrist's word stands)"); return; }   /* THE BROWNOUT LOOP review (2026-10-14): the errand spun its 75 s with the dial refused and no watchdog feed -- a bite every night onset while spent */
    ear_gear_leave("the night's errand");                     /* THE EAR'S LOW GEAR: the dial and the TLS handshake at 240 MHz, never at 80 */
    errand = true;
    awake = true; awake_tick_ms = millis(); retry_wait = false; wake_said = false;
    uint32_t t0 = millis();
    LOG("link: the night's errand: the radio up with the glass dark, `release asleep`, then off (at most %lu s)", (unsigned long)(ERRAND_MAX_MS / 1000));
    while ((int32_t)(millis() - t0) < (int32_t)ERRAND_MAX_MS) {
        net_tick();
        link_tick();
        bool session = st != S_IDLE && st != S_BACKOFF;
        if (whistle_req == 0 && inflight == R_NONE && !session && !net_radio_is_on()) break;   /* answered (or dropped) and hung up */
        if (retry_wait && !net_radio_is_on()) break;                                             /* no link found: given up */
        wdt_feed(WDT_SPELL_AID);                                                                 /* THE LOOP'S PULSE: the errand is a long aid (up to 75 s) */
        delay(10);
    }
    uint32_t took = (millis() - t0) / 1000u;
    if (whistle_req == 3) { whistle_req = 0; house_release_unanswered(); LOG("link: the errand found no door in %lu s: the wrist's word stands (%s)", (unsigned long)took, house_state_name()); }
    if (st != S_IDLE || tls || sock >= 0) drop("the errand's end");
    errand = false;
    link_dark();
    net_radio_off();
    LOG("link: the errand done in %lu s: house %s, radio off", (unsigned long)took, house_state_name());
}

void link_tick(void)
{
    uint32_t now = millis();
    /* THE RADIO BUDGET: the awake clock (the loop does not run in a nap,
     * so this only ever steps lit time) and the rule */
    if (awake) { awake_since_ok_ms += now - awake_tick_ms; awake_tick_ms = now; }
    if (!errand) { int ev = rest_take_lease_event(); if (ev) night_event(ev); }   /* THE NIGHT RELEASE: the lit path (a span ending on a raise, a night begun under a held glass) */
    /* THE CABLE NEVER CLAIMS: each edge logged once (at boot on the cable, the first line) */
    if (on_cable() != cable_said) {
        cable_said = !cable_said;
        if (cable_said) LOG("link: on the cable -- the honest claim is off (the hold still claims)%s", tr == TR_SERIAL ? " [the dock]" : "");
        else LOG("link: off the cable -- the honest claim is back");
    }
    {   /* THE DOCK HANDS HIM OVER (dockhand.h, C24): the same settled edge, read once a tick; a boot on the cable is no edge */
        dockhand_in_t din; din.on = !store.dockhand_off; din.cable = on_cable(); din.holding = house_holding(); din.still_ms = house_still_ms();
        char why[96]; int d = dockhand_tick(&dockhand, &din, why, sizeof why);
        if (d == DOCKHAND_RELEASE) {
            /* the hold's own road: the goodbye and the sink on the wrist's word, `release docked` on the wire (the desk claims at once on it) */
            LOG("link: dock hand: %s%s", why, st == S_LINKED ? "" : " (no link yet: the radio comes up for it)");
            whistle_req = 5;
            reaching = false;
            house_release_now();
        } else if (d == DOCKHAND_CLAIM) {
            if (house_holding()) LOG("link: dock hand: undocked worn, but he is here already -- nothing to claim");
            else if (whistle_req) LOG("link: dock hand: undocked worn -- %s is already on its way", WHISTLE_NAMES[whistle_req]);
            else { whistle_req = 4; reaching = true; LOG("link: dock hand: %s%s", why, st == S_LINKED ? "" : " (the radio comes up for it: a lease verb is a reason)"); }
        } else if (why[0]) LOG("link: dock hand: %s", why);   /* nothing fails quietly: an edge kept back says why */
    }
    /* THE SHELF NEVER CLAIMS: the worn edge logged once each way too (off the cable only: on it the line above already says) */
    if (!on_cable() && !house_worn() != still_said) {
        still_said = !still_said;
        if (still_said) LOG("link: still 5 min -- the honest claim is off (the shelf; the beat says `still`, the hold still claims)");
        else LOG("link: worn again -- the honest claim is back (`claim worn`)");
    }
    radio_tick(now);
    /* a dial that outlived its state (the radio went, the dock took over):
     * the task is done, the session is ours to free */
    if (tls && tr != TR_TLS && dial_state != 1) {
        dial_state = 0;
        close_tls("the dial outlived its state");
    }
    if (tr == TR_TCP && sock >= 0 && st >= S_WAIT_HI) tcp_read();
    if (tr == TR_TLS && tls && st >= S_WAIT_HI) tls_read();
    if (inflight != R_NONE && (int32_t)(millis() - sent_ms) > (int32_t)sent_timeout) {
        LOG("link: no answer to %s in %lu ms", REQ_NAMES[inflight], (unsigned long)sent_timeout);
        if (inflight == R_RELEASE) house_release_unanswered();   /* THE LEASE, EASY: the wrist's word stands (house.cpp logs it) */
        drop("timeout");
        return;
    }
    switch (st) {
    case S_IDLE:
        break;
    case S_BACKOFF: {
        if (wired()) { set_state(S_IDLE); break; }
        if (!wifi_up) { set_state(S_IDLE); break; }
        if (now - st_ms < backoff_ms) break;
        if (dial_state == 1) { st_ms = now; break; }   /* an abandoned dial's task still runs: not two at once */
        int i = door_pick();
        if (i < 0) { st_ms = now; break; }             /* every door is skipped: look again after the backoff */
        backoff_ms = backoff_ms * 2 > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : backoff_ms * 2;
        dial(i);
        break;
    }
    case S_DIAL:
        if (dial_state == 2) {
            dial_state = 0;
            if (dial_tls) {
                LOG("link: tls up to door %d %s:%u in %lu ms: heap %lu -> %lu (the session ~%lu B); waiting for the box's hi",
                    cur_door, dial_host, dial_port, (unsigned long)(now - dial_ms), (unsigned long)heap_before,
                    (unsigned long)heap_after, (unsigned long)(heap_before > heap_after ? heap_before - heap_after : 0));
                set_state(S_WAIT_HI); hi_asked = 0;
            } else if (tcp_start(dial_ip, dial_port)) {
                set_state(S_CONNECT);
                LOG("link: calling door %d %s (%s):%u", cur_door, dial_host, IPAddress(dial_ip).toString().c_str(), dial_port);
            } else { door_failed(cur_door, "socket"); set_state(S_BACKOFF); }
        } else if (dial_state == 3 || now - st_ms > (dial_tls ? CONNECT_MS + TLS_HS_S * 1000 + 5000 : 10000)) {
            bool timed = dial_state != 3;
            if (dial_state == 3) dial_state = 0;
            if (dial_tls) {
                LOG("link: tls to %s:%u failed after %lu ms: %s", dial_host, dial_port, (unsigned long)(now - dial_ms),
                    timed ? "the dial task did not end" : dial_err);
                door_failed(cur_door, "tls failed");
                drop("tls failed");
            } else {
                LOG("link: %s does not resolve", dial_host);
                door_failed(cur_door, "no resolve");
                set_state(S_BACKOFF);
            }
        }
        break;
    case S_CONNECT: {
        int c = tcp_connected();
        if (c > 0) { set_state(S_WAIT_HI); hi_asked = 0; LOG("link: connected; waiting for the desk's hi"); }
        else if (c < 0 || now - st_ms > CONNECT_MS) {
            door_failed(cur_door, c < 0 ? "refused" : "connect timeout");
            drop(c < 0 ? "refused" : "connect timeout");
        }
        break;
    }
    case S_WAIT_HI:
        if (now - st_ms > HI_WAIT_MS) {
            if (!hi_asked) { hi_asked = 1; link_write("hi\n"); LOG("link> hi"); st_ms = now; }
            else { if (!wired()) door_failed(cur_door, "no hi"); drop("no hi from the desk"); }
        }
        break;
    case S_HELLO:
        break;
    case S_LINKED:
        if (inflight != R_NONE) break;
        if (schedule()) { quiet_ms = now; break; }
        /* THE RADIO BUDGET's linger: over the radio, nothing to say for
         * LINK_LINGER_S -> `bye` (the desk answers `ok bye` and hangs up;
         * the reply drops the session), the radio off with it. The dock
         * and a hold keep the session as ever (the minute's beat).
         * THE LIT GLASS POLLS: a lit glass holds the session too (the
         * verb every 5 s is the linger's own reset); the dim starts the
         * linger's clock -- `bye` within LINK_LINGER_S of the dim. */
        if (!wired() && !hold && !bye_said && !radio_hold_session(glass_lit(), heap_low, poll_verb())   /* THE CABLE NEVER CLAIMS: no verb to poll with, no hold */
            && (int32_t)(now - quiet_ms) >= (int32_t)(LINK_LINGER_S * 1000u)) {
            bye_said = true;
            char l[160]; link_radio_line(l, sizeof l);
            LOG("link: nothing to say for %lu s: bye, radio off -- %s", (unsigned long)LINK_LINGER_S, l);
            request(R_BYE, "bye\n", REPLY_MS);
        }
        break;
    }
}
