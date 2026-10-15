/* THE BLE STACK (2026-10-06): one NimBLE stack, one server, two tenants
 * -- the iPhone's Notification Center client (ancs.cpp) and THE AIR
 * DOOR, the desk's line protocol over a Nordic UART service
 * (bleuart.cpp). This module owns what both need and neither should:
 * NimBLE up and down (ble.cpp says when: ruling 58, only while the WiFi
 * radio is down), the server, the advertising (the ANCS solicitation
 * in the advertisement, the name + the NUS uuid in the scan response:
 * 31 bytes each, exactly), the security (bond, Just Works, secure
 * connections), THE PAIRING WINDOW (blepair.h: a stranger may bond only
 * inside it -- sent away at the connect otherwise), the peer table
 * (who is on: a phone, a desk, a stranger still pairing), the bonds.
 *
 * Two links at once (CONFIG_BT_NIMBLE_MAX_CONNECTIONS 2, platformio.ini):
 * the phone and the desk, each its own connection; advertising resumes
 * after the first connect so the second can find the watch. Threads as
 * before: the NimBLE host task runs the callbacks here and in the
 * tenants; loop() reads the peer table and the flags; the tenants' own
 * work rides their rings and the ANCS worker. README "BLUETOOTH". */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define BLESTACK_PEERS 2        /* = CONFIG_BT_NIMBLE_MAX_CONNECTIONS */

enum { PEER_NONE = 0, PEER_STRANGER, PEER_BONDED, PEER_PHONE, PEER_DESK };   /* what a connection turned out to be */

void blestack_begin(void);                 /* boot: nothing on the air; the tenants' own begin */
bool blestack_on(void);                    /* the stack up: init, the server with both services, advertising (false = NimBLE would not start) */
void blestack_off(void);                   /* the stack down, the heap back whole (the bonds stay in NVS) */
bool blestack_is_on(void);
void blestack_tick(void);                  /* loop(): the window's end sends unbonded strangers away; the tenants' ticks are their own */
/* THE PAIRING WINDOW */
void     blestack_pair_open(uint32_t secs, const char *how);   /* (re)open it: BLEPAIR_S by default; logged */
void     blestack_pair_close(const char *why);
uint32_t blestack_pair_left_s(void);       /* 0 = closed */
uint32_t blestack_take_bonded(void);       /* how many bonds landed since the last take (ble.cpp bumps once for any) */
/* the peers */
int      blestack_peer_kind(uint16_t conn_handle);
void     blestack_peer_is(uint16_t conn_handle, int kind);   /* a tenant's finding: PEER_PHONE (ANCS ready), PEER_DESK (the NUS subscribed) */
bool     blestack_has(int kind);           /* a peer of that kind is on */
int      blestack_peers(void);             /* connections now */
uint32_t blestack_refused(void);           /* strangers sent away since boot */
int      blestack_bonds(void);             /* bonds in NVS while the stack is up (-1 down) */
void     blestack_forget(void);            /* `ble forget`: every bond dropped (the phone must forget the watch too) */
const char *blestack_name(void);           /* "slime-watch" */
void     blestack_status(char *out, int cap);
