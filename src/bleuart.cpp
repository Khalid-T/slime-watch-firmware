/* THE AIR DOOR -- bleuart.h says why. */
#include "bleuart.h"
#include "bleline.h"
#include "blestack.h"
#include "near.h"                /* THE DESK NEXT DOOR: the band's word on the status line */
#include "link.h"
#include "app.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

/* --- the tunables ------------------------------------------------------------ */

static NimBLEUUID UUID_NUS("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");   /* Nordic UART Service, as every BLE terminal knows it */
static NimBLEUUID UUID_RX ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");   /* the desk writes */
static NimBLEUUID UUID_TX ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");   /* the watch notifies */
#define RX_RING 1024            /* bytes from the desk waiting for loop(): five lines; a day chunk's `ok` is one short line */
#define TX_RING 1024            /* bytes for the desk: the day sync's `day <hex>` lines are 165 B each, sent one at a time */
static const int TX_PER_TICK = 4;   /* notifies per frame at most: the controller's buffers, not loop()'s time */

/* --- state ------------------------------------------------------------------- */

static NimBLEService *svc = nullptr;
static NimBLECharacteristic *ch_rx = nullptr, *ch_tx = nullptr;
static volatile uint16_t desk = BLE_HS_CONN_HANDLE_NONE;   /* the one bonded subscriber */
static volatile uint16_t desk_mtu = 0;
static volatile bool desk_gone = false;      /* the subscriber left / unsubscribed: loop() undocks the link */
static volatile bool desk_came = false;      /* ...subscribed: loop() logs it */
static uint8_t rx_ring[RX_RING]; static volatile int rx_head = 0, rx_tail = 0;
static uint8_t tx_ring[TX_RING]; static int tx_head = 0, tx_tail = 0;   /* loop() only */
static portMUX_TYPE rx_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t rx_dropped = 0, tx_dropped = 0, tx_notifies = 0, tx_refused = 0;
static bleline_t lines;

static int rx_avail(void) { return (rx_head - rx_tail + RX_RING) % RX_RING; }
static int tx_avail(void) { return (tx_head - tx_tail + TX_RING) % TX_RING; }

/* --- the host task's callbacks ----------------------------------------------- */

class RxCbs : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &ci) override
    {
        if (ci.getConnHandle() != desk) return;          /* only the subscribed desk speaks (the stack already refused the unencrypted) */
        NimBLEAttValue v = c->getValue();
        const uint8_t *d = v.data(); int n = (int)v.length();
        portENTER_CRITICAL(&rx_mux);
        for (int i = 0; i < n; i++) {
            int next = (rx_head + 1) % RX_RING;
            if (next == rx_tail) { rx_dropped += (uint32_t)(n - i); break; }
            rx_ring[rx_head] = d[i]; rx_head = next;
        }
        portEXIT_CRITICAL(&rx_mux);
    }
};
class TxCbs : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *c, NimBLEConnInfo &ci, uint16_t sub) override
    {
        (void)c;
        uint16_t h = ci.getConnHandle();
        if (sub & 1) {
            if (!ci.isEncrypted()) return;               /* a stranger's subscription is nothing: never notified */
            desk = h; desk_mtu = ci.getMTU(); desk_came = true;
            blestack_peer_is(h, PEER_DESK);
        } else if (h == desk) {
            desk = BLE_HS_CONN_HANDLE_NONE; desk_gone = true;
        }
    }
};
static RxCbs rx_cbs; static TxCbs tx_cbs;

/* --- the public face ---------------------------------------------------------- */

void bleuart_begin(void) { bleline_init(&lines); }

void bleuart_attach(NimBLEServer *s)
{
    svc = s->createService(UUID_NUS);
    /* encrypted-only, both ways: a bond is the only way in (THE PAIRING WINDOW) */
    ch_rx = svc->createCharacteristic(UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
    ch_tx = svc->createCharacteristic(UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC);
    ch_rx->setCallbacks(&rx_cbs);
    ch_tx->setCallbacks(&tx_cbs);
    svc->start();
}

void bleuart_detach(void)
{
    if (desk != BLE_HS_CONN_HANDLE_NONE) link_air_closed("the stack goes down");
    desk = BLE_HS_CONN_HANDLE_NONE; desk_mtu = 0; desk_gone = desk_came = false;
    svc = nullptr; ch_rx = ch_tx = nullptr;             /* deinit(true) deletes them */
    portENTER_CRITICAL(&rx_mux); rx_head = rx_tail = 0; portEXIT_CRITICAL(&rx_mux);
    tx_head = tx_tail = 0;
    bleline_reset(&lines);
}

static void take_line(const char *line, void *arg) { (void)arg; link_ble_line(line); }

void bleuart_tick(void)
{
    if (desk_came) { desk_came = false; LOG("ble: the desk subscribed to the air door (mtu %u): its `hi` opens the session", (unsigned)desk_mtu); }
    if (desk_gone) {
        desk_gone = false;
        link_air_closed("the desk left the air door");
        bleline_reset(&lines);
        tx_head = tx_tail = 0;
    }
    /* RX: the desk's bytes, gathered to lines, to the link (loop() only) */
    uint8_t buf[128];
    for (;;) {
        int n = 0;
        portENTER_CRITICAL(&rx_mux);
        while (rx_tail != rx_head && n < (int)sizeof buf) { buf[n++] = rx_ring[rx_tail]; rx_tail = (rx_tail + 1) % RX_RING; }
        portEXIT_CRITICAL(&rx_mux);
        if (!n) break;
        bleline_feed(&lines, buf, n, take_line, nullptr);
    }
    /* TX: the ring into notifies, a chunk of the desk's MTU - 3 each, a few a frame */
    if (desk == BLE_HS_CONN_HANDLE_NONE || !ch_tx) return;
    for (int k = 0; k < TX_PER_TICK && tx_avail(); k++) {
        int chunk = desk_mtu > 3 ? desk_mtu - 3 : 20;
        if (chunk > (int)sizeof buf) chunk = sizeof buf;
        int n = 0, t = tx_tail;
        while (t != tx_head && n < chunk) { buf[n++] = tx_ring[t]; t = (t + 1) % TX_RING; }
        if (!ch_tx->notify(buf, (size_t)n, desk)) { tx_refused++; break; }   /* the controller is full: the same bytes next frame */
        tx_tail = t; tx_notifies++;
    }
}

bool bleuart_write(const char *s, size_t n)
{
    if (desk == BLE_HS_CONN_HANDLE_NONE || !ch_tx) return false;
    if ((int)n >= TX_RING - tx_avail()) { tx_dropped++; return false; }   /* a line never half-queued */
    for (size_t i = 0; i < n; i++) { tx_ring[tx_head] = (uint8_t)s[i]; tx_head = (tx_head + 1) % TX_RING; }
    return true;
}

bool bleuart_linked(void) { return desk != BLE_HS_CONN_HANDLE_NONE; }
uint16_t bleuart_mtu(void) { return desk_mtu; }

/* THE DESK NEXT DOOR (near.h): the host's own word on the desk's
 * connection (the same call NimBLEClient::getRssi makes; the host
 * lock inside it, loop() may ask). 0 = no desk, or the host refused. */
int bleuart_desk_rssi(void)
{
    uint16_t h = desk;
    if (h == BLE_HS_CONN_HANDLE_NONE) return 0;
    int8_t rssi = 0;
    if (ble_gap_conn_rssi(h, &rssi) != 0) return 0;
    return rssi;
}

void bleuart_peer_left(uint16_t h)
{
    if (h == desk) { desk = BLE_HS_CONN_HANDLE_NONE; desk_gone = true; }
}

void bleuart_peer_mtu(uint16_t h, uint16_t mtu)
{
    if (h == desk) desk_mtu = mtu;
}

const NimBLEUUID &bleuart_uuid(void) { return UUID_NUS; }

void bleuart_status(char *out, int cap)
{
    snprintf(out, cap, "air door %s (mtu %u, the desk %s%s%d dBm): lines in %lu (dropped %lu, overlong %lu), notifies %lu (refused %lu, dropped %lu)",
             desk != BLE_HS_CONN_HANDLE_NONE ? "open" : "shut", (unsigned)desk_mtu,
             near_word(near_band()), near_dbm() ? " " : "", near_dbm(),   /* THE DESK NEXT DOOR: the band and its smoothed dBm (`-- 0` with no desk) */
             (unsigned long)lines.lines, (unsigned long)rx_dropped, (unsigned long)lines.dropped,
             (unsigned long)tx_notifies, (unsigned long)tx_refused, (unsigned long)tx_dropped);
}
