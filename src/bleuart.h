/* THE AIR DOOR (2026-10-06, Khalid's "some notifications sending data
 * to them, just full support"): the desk's line protocol over
 * Bluetooth. A Nordic UART service on the watch (blestack.cpp's server):
 * the desk writes lines into RX, the watch notifies lines on TX, and
 * between them runs exactly what the USB dock carries (link.cpp's fourth
 * transport, TR_BLE: `hi <nonce>` opens the session, the lease verbs,
 * the wisps, the words, the look, the day log home). It needs no WiFi
 * -- the case that blocks the watch's WiFi at the desk (reason 201)
 * cannot block a link to a laptop at arm's length -- and while it is
 * linked THE RADIO BUDGET has nothing to dial for (the dock's rule). It
 * exists only while the stack is up: lit glass, WiFi radio down (ruling
 * 58), so it is the desk's road exactly when no other is open.
 *
 * Both characteristics are ENCRYPTED-ONLY: the desk must be bonded
 * (THE PAIRING WINDOW, blepair.h); a stranger's write is refused by the
 * stack itself, and the watch notifies only the one bonded subscriber.
 * MTU 247 asked (blestack.cpp), so one LINK_LINE_MAX line is one
 * notify; smaller MTUs are chunked (the desk gathers to the newline,
 * as this side does: bleline.c).
 *
 * Threads: the host task's onWrite / onSubscribe push bytes and flags
 * into spinlocked rings; bleuart_tick (loop()) gathers the lines and
 * hands them to the link, and drains the TX ring into notifies. Only
 * loop() touches the link. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

class NimBLEServer;
class NimBLEUUID;

void bleuart_begin(void);                      /* boot: the rings */
void bleuart_attach(NimBLEServer *s);          /* blestack_on: the service and its two characteristics, before server->start() */
void bleuart_detach(void);                     /* blestack_off: the pointers go (deinit deletes the objects); the link undocked */
void bleuart_tick(void);                       /* loop(): RX bytes -> lines -> link_ble_line; TX ring -> notifies */
bool bleuart_write(const char *s, size_t n);   /* link_write for TR_BLE: queued (false = no desk subscribed, or no room) */
bool bleuart_linked(void);                     /* a bonded desk subscribed to TX */
uint16_t bleuart_mtu(void);                    /* the desk's MTU (0 = none) */
int  bleuart_desk_rssi(void);                  /* THE DESK NEXT DOOR (near.h): the desk's connection RSSI in dBm (one HCI ask), 0 = no desk / the host would not say */
void bleuart_peer_left(uint16_t conn_handle);  /* blestack: a connection ended */
void bleuart_peer_mtu(uint16_t conn_handle, uint16_t mtu);
const NimBLEUUID &bleuart_uuid(void);          /* the service uuid (the scan response) */
void bleuart_status(char *out, int cap);       /* one line for `ble` */
