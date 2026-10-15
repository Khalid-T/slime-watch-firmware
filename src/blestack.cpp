/* THE BLE STACK -- blestack.h says what. The stack, the server, the
 * advertising, the security, the pairing window and the peer table;
 * ancs.cpp and bleuart.cpp attach their services here and are told of
 * every connect, leave and bond. */
#include "blestack.h"
#include "blepair.h"
#include "ancs.h"
#include "bleuart.h"
#include "app.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

/* --- the tunables ------------------------------------------------------------ */

static const char *NAME = "slime-watch";   /* the mDNS name too: one name for the watch on both airs */
static const uint16_t ADV_ITVL_MIN = 0x0152;  /* x 0.625 ms: 211.25 ms -- one of Apple's listed intervals (the Accessory Design Guidelines: 152.5, 211.25, 318.75 ...), */
static const uint16_t ADV_ITVL_MAX = 0x0152;  /* ...the one the proven ANCS accessories use; iOS listed nothing at 200-260 ms (10-06 15:05, the air textbook in btmon) */
static const int8_t   TX_DBM = 3;             /* the phone is in the same pocket, the desk across it */
static const uint16_t MTU_WANT = 247;         /* a LINK_LINE_MAX line in one notify (the desk's side negotiates down if it must) */
static const uint32_t LEAVE_MS = 150;         /* the clean leave at the stack-down: wait this long for the disconnects, no more (was a blind 80 ms delay) */

static NimBLEUUID UUID_ANCS("7905F431-B5CE-4E99-A40F-4B1E122D00D0");   /* solicited in the advertisement */

/* --- state ------------------------------------------------------------------- */

static bool on = false;
static NimBLEServer *server = nullptr;
static uint32_t on_ms = 0;
static blepair_t pair;
static volatile uint32_t bonded_landed = 0;    /* bonds that landed (ble.cpp's bump) */
static uint32_t refused = 0;
static char pair_how[16] = "";

typedef struct { uint16_t handle; int kind; char addr[20]; bool encrypted; } peer_t;
static peer_t peers[BLESTACK_PEERS];
static portMUX_TYPE peer_mux = portMUX_INITIALIZER_UNLOCKED;

static peer_t *peer_slot(uint16_t h)
{
    for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind != PEER_NONE && peers[i].handle == h) return &peers[i];
    return nullptr;
}
static peer_t *peer_free(void)
{
    for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind == PEER_NONE) return &peers[i];
    return nullptr;
}
static int peers_on(void)
{
    int n = 0;
    for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind != PEER_NONE) n++;
    return n;
}
static bool known(const NimBLEConnInfo &ci)
{
    /* the phone comes on a resolvable private address; the bond is on its identity address */
    return NimBLEDevice::isBonded(ci.getIdAddress()) || NimBLEDevice::isBonded(ci.getAddress());
}

/* --- the host task's callbacks ----------------------------------------------- */

class ServerCbs : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, NimBLEConnInfo &ci) override
    {
        bool bonded = known(ci);
        int verdict = blepair_admit(&pair, millis(), bonded);
        const char *addr = ci.getAddress().toString().c_str();
        if (verdict == BLEPAIR_REFUSED) {
            /* THE PAIRING WINDOW: a stranger with no window is sent away at the door; the log says what would let it in */
            refused++;
            LOG("ble: %s sent away -- %s", addr, blepair_why(verdict));
            s->disconnect(ci.getConnHandle());
            return;
        }
        portENTER_CRITICAL(&peer_mux);
        peer_t *p = peer_free();
        if (p) { p->handle = ci.getConnHandle(); p->kind = bonded ? PEER_BONDED : PEER_STRANGER; p->encrypted = ci.isEncrypted(); strncpy(p->addr, addr, sizeof p->addr - 1); p->addr[sizeof p->addr - 1] = 0; }
        portEXIT_CRITICAL(&peer_mux);
        LOG("ble: %s connected (%s; %d of %d on)%s", addr, blepair_why(verdict), peers_on(), BLESTACK_PEERS,
            bonded ? "" : ": expect a Pair ask on the other side");
        /* two links: the air door stays open for the second peer */
        if (s->getConnectedCount() < BLESTACK_PEERS) NimBLEDevice::startAdvertising();
        ancs_peer_connected(ci, bonded);
    }
    void onDisconnect(NimBLEServer *s, NimBLEConnInfo &ci, int reason) override
    {
        uint16_t h = ci.getConnHandle();
        char addr[20] = "?"; int kind = PEER_NONE;
        portENTER_CRITICAL(&peer_mux);
        peer_t *p = peer_slot(h);
        if (p) { strncpy(addr, p->addr, sizeof addr - 1); kind = p->kind; p->kind = PEER_NONE; p->handle = 0; p->addr[0] = 0; }
        portEXIT_CRITICAL(&peer_mux);
        if (kind != PEER_NONE) LOG("ble: %s left (reason %d%s)%s", addr, reason, kind == PEER_PHONE ? ", the phone" : kind == PEER_DESK ? ", the desk" : "", on ? "; advertising" : "");
        ancs_peer_left(h);
        bleuart_peer_left(h);
        (void)s;
    }
    void onAuthenticationComplete(NimBLEConnInfo &ci) override
    {
        uint16_t h = ci.getConnHandle();
        if (ci.isEncrypted()) {
            bool bonded = ci.isBonded() || known(ci);
            bool fresh = false;                            /* a stranger became bonded now -- not a known peer re-encrypting (that is every reconnect: no bump, no window closed) */
            portENTER_CRITICAL(&peer_mux);
            peer_t *p = peer_slot(h);
            if (p) { p->encrypted = true; if (bonded && p->kind == PEER_STRANGER) { p->kind = PEER_BONDED; fresh = true; } }
            portEXIT_CRITICAL(&peer_mux);
            if (fresh) LOG("ble: paired with %s (%s)", ci.getAddress().toString().c_str(), bonded ? "bonded" : "not bonded");
            if (fresh && blepair_is_open(&pair, millis())) { blepair_close(&pair); bonded_landed++; LOG("ble: the pairing window closes -- the bond landed"); }
            else if (fresh) bonded_landed++;
            ancs_peer_authed(h, true);
        } else {
            LOG("ble: pairing with %s failed", ci.getAddress().toString().c_str());
            ancs_peer_authed(h, false);
        }
    }
    void onMTUChange(uint16_t mtu, NimBLEConnInfo &ci) override
    {
        bleuart_peer_mtu(ci.getConnHandle(), mtu);
    }
};
static ServerCbs server_cbs;   /* static: setCallbacks(..., false) -- the library must never delete it (README "THE ASSERT") */

/* --- the public face ---------------------------------------------------------- */

void blestack_begin(void)
{
    blepair_init(&pair);
    memset(peers, 0, sizeof peers);
    ancs_begin();
    bleuart_begin();
}

bool blestack_on(void)
{
    if (on) return true;
    if (!NimBLEDevice::init(NAME)) { LOG("ble: the BLE stack would not start"); return false; }
    NimBLEDevice::setSecurityAuth(true, false, true);     /* bond, no MITM (Just Works: a Pair tap), secure connections */
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setPower(TX_DBM);
    NimBLEDevice::setMTU(MTU_WANT);
    server = NimBLEDevice::createServer();
    server->setCallbacks(&server_cbs, false);             /* THE ASSERT's fix: the library's default would `delete` the static */
    server->advertiseOnDisconnect(true);
    ancs_attach(server);
    bleuart_attach(server);
    server->start();
    /* the advertisement: flags + the SOLICITATION of Apple's service (AD
     * type 0x15, the 128-bit UUID little-endian) -- what makes the phone
     * connect on its own, now and every time it sees the watch again;
     * the scan response carries the name and THE AIR DOOR's uuid (13 +
     * 18 = 31 bytes: exactly the room), so the desk can find the watch
     * by either */
    NimBLEAdvertisementData ad;
    ad.setFlags(0x06);                                    /* general discoverable, BR/EDR unsupported */
    { uint8_t txp[3] = { 2, 0x0A, (uint8_t)TX_DBM }; ad.addData(txp, sizeof txp); }   /* TX power: in Apple's guideline list of fields; 3 + 3 + 18 = 24 */
    uint8_t sol[18];
    sol[0] = 17; sol[1] = 0x15;
    memcpy(sol + 2, UUID_ANCS.getValue(), 16);
    ad.addData(sol, sizeof sol);
    /* the scan response: the name, and Apple's uuid once more as a plain
     * service (AD type 0x07) -- the recipe the proven ESP32 ANCS projects
     * advertise with; iOS lists under Other Devices only an accessory it
     * has a reason to pair, and 10-06 15:00 it listed nothing with the
     * solicitation alone while the laptop heard the watch at -41 dBm.
     * The air door's uuid leaves the air (13 + 18 = 31 is the room): the
     * desk finds the watch by its name. */
    NimBLEAdvertisementData sr;
    sr.setName(NAME);
    if (!sr.addServiceUUID(UUID_ANCS)) LOG("ble: the scan response has no room for Apple's uuid (the name alone)");
    NimBLEAdvertising *adv = server->getAdvertising();
    adv->setAdvertisementData(ad);
    adv->setScanResponseData(sr);
    adv->setMinInterval(ADV_ITVL_MIN);
    adv->setMaxInterval(ADV_ITVL_MAX);
    adv->start();
    on = true;
    on_ms = millis();
    return true;                                          /* ble.cpp logs the rise with the heap's numbers */
}

void blestack_off(void)
{
    if (!on) return;
    on = false;
    ancs_detach();
    bleuart_detach();
    if (server && server->getConnectedCount()) {
        /* a clean leave: each peer told, not timed out; bounded */
        for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind != PEER_NONE) server->disconnect(peers[i].handle);
        uint32_t t0 = millis();
        while (server->getConnectedCount() && millis() - t0 < LEAVE_MS) delay(10);
    }
    ancs_quiesce();                                       /* the worker off the air (bounded) */
    NimBLEDevice::deinit(true);                           /* the server, its clients, the advertising: once each; the bonds stay in NVS */
    server = nullptr;
    portENTER_CRITICAL(&peer_mux);
    memset(peers, 0, sizeof peers);
    portEXIT_CRITICAL(&peer_mux);
}

bool blestack_is_on(void) { return on; }

void blestack_tick(void)
{
    /* THE PAIRING WINDOW's end: a stranger still unbonded is sent away (it had its minute) */
    static bool was_open = false;
    bool open = blepair_is_open(&pair, millis());
    if (was_open && !open) {
        if (pair.open) { blepair_close(&pair); LOG("ble: the pairing window closed (%s): no bond landed", pair_how); }
        if (on && server) {
            for (int i = 0; i < BLESTACK_PEERS; i++)
                if (peers[i].kind == PEER_STRANGER) { LOG("ble: %s sent away -- the window closed unpaired", peers[i].addr); server->disconnect(peers[i].handle); }
        }
    }
    was_open = open;
}

void blestack_pair_open(uint32_t secs, const char *how)
{
    blepair_open(&pair, millis(), secs);
    strncpy(pair_how, how ? how : "?", sizeof pair_how - 1); pair_how[sizeof pair_how - 1] = 0;
    LOG("ble: the pairing window is open for %lu s (%s) -- pair the phone (Settings > Bluetooth > %s > Pair) or the desk (`slimectl watch ble pair`) now%s",
        (unsigned long)pair.secs, how, NAME, on ? "" : " (the stack is down: it advertises once the WiFi radio is off)");
}

void blestack_pair_close(const char *why)
{
    if (!blepair_is_open(&pair, millis())) return;
    blepair_close(&pair);
    LOG("ble: the pairing window closed (%s)", why);
}

uint32_t blestack_pair_left_s(void) { return blepair_left_s(&pair, millis()); }

uint32_t blestack_take_bonded(void)
{
    uint32_t n = bonded_landed;
    if (n) bonded_landed = 0;
    return n;
}

int blestack_peer_kind(uint16_t h)
{
    int k = PEER_NONE;
    portENTER_CRITICAL(&peer_mux);
    peer_t *p = peer_slot(h);
    if (p) k = p->kind;
    portEXIT_CRITICAL(&peer_mux);
    return k;
}

void blestack_peer_is(uint16_t h, int kind)
{
    portENTER_CRITICAL(&peer_mux);
    peer_t *p = peer_slot(h);
    if (p) p->kind = kind;
    portEXIT_CRITICAL(&peer_mux);
}

bool blestack_has(int kind)
{
    for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind == kind) return true;
    return false;
}

int blestack_peers(void) { return peers_on(); }
uint32_t blestack_refused(void) { return refused; }
int blestack_bonds(void) { return on ? NimBLEDevice::getNumBonds() : -1; }
const char *blestack_name(void) { return NAME; }

void blestack_forget(void)
{
    if (!on) { LOG("ble: the stack is down (it is up while the WiFi radio is down: `ble`); the bonds go when it is up"); return; }
    if (server) for (int i = 0; i < BLESTACK_PEERS; i++) if (peers[i].kind != PEER_NONE) server->disconnect(peers[i].handle);
    NimBLEDevice::deleteAllBonds();
    LOG("ble: bonds dropped; forget \"%s\" on the phone too (Settings > Bluetooth > i > Forget) and on the desk (`slimectl watch ble forget`), then pair again", NAME);
}

void blestack_status(char *out, int cap)
{
    int n = snprintf(out, cap, "%s %lu s, %d bond%s, %d on", on ? "up" : "down", (unsigned long)(on ? (millis() - on_ms) / 1000 : 0),
                     blestack_bonds(), blestack_bonds() == 1 ? "" : "s", peers_on());
    for (int i = 0; i < BLESTACK_PEERS && n < cap; i++) if (peers[i].kind != PEER_NONE)
        n += snprintf(out + n, cap - n, " [%s %s%s]", peers[i].addr,
                      peers[i].kind == PEER_PHONE ? "phone" : peers[i].kind == PEER_DESK ? "desk" : peers[i].kind == PEER_BONDED ? "bonded" : "stranger",
                      peers[i].encrypted ? "" : " unencrypted");
    uint32_t left = blestack_pair_left_s();
    if (n < cap) snprintf(out + n, cap - n, ", pairing %s%lu%s, %lu sent away", left ? "open " : "closed", (unsigned long)left, left ? " s" : "", (unsigned long)refused);
}
