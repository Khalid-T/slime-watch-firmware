/* ANCS: the iPhone's notifications over BLE (Apple Notification Center
 * Service -- no app, no MFi: the watch is a bonded BLE peripheral and
 * reads the phone's own notification service). In the default build
 * since 2026-09-10 21:50 (ruling 58: BLE only when WiFi is down); since
 * 2026-10-06 one tenant of blestack.cpp's server (the other is THE AIR
 * DOOR, bleuart.cpp) -- the stack, the advertising, the security and
 * the pairing window are the stack's; this module is the CLIENT: on
 * each new peer it looks for the Notification Center, and on the phone
 * that has one it subscribes and reads, per new notice, the app id, the
 * title, the MESSAGE and the date, and once per app its display name.
 * What comes of it: THE WISP (wisp.h), a `sense wrist:doorbell <app>`
 * up the link, a log line. README "BLUETOOTH". */
#pragma once
#include <stdint.h>
#include <stdbool.h>

class NimBLEServer;
class NimBLEConnInfo;

#define ANCS_MSG_MAX  96        /* the message asked of the phone (the ledger keeps as much) */
#define ANCS_NAME_MAX 24        /* an app's display name ("Messages"), cached per app id */

void ancs_begin(void);                  /* boot: the worker; nothing on the air yet */
void ancs_attach(NimBLEServer *s);      /* blestack_on: nothing to add to the server (the client rides the phone's connection); the state reset */
void ancs_detach(void);                 /* blestack_off: the pointers go before deinit */
void ancs_quiesce(void);                /* blestack_off: the worker off the air (bounded) */
void ancs_peer_connected(NimBLEConnInfo &ci, bool bonded);   /* blestack (host task): a new peer -- the worker looks for its Notification Center */
void ancs_peer_authed(uint16_t conn_handle, bool ok);        /* blestack (host task): the pairing's verdict */
void ancs_peer_left(uint16_t conn_handle);
void ancs_tick(float dt);               /* from loop(): the phone's events -> wisps (wisp.cpp draws them) */
void ancs_fake(const char *app, const char *title, const char *msg);   /* `ancs fake`: a wisp with no phone */
void ancs_status(char *out, int cap);   /* one line for `status` / `ancs` */
bool ancs_connected(void);              /* a phone connected, bonded and subscribed (READY): the row's PHONE */
const char *ancs_app_name(const char *app);   /* the cached display name for an app id ("" = not known yet) */
/* THE NOTICE CLEARED FROM THE WRIST (T17, ancsact.h): the NEGATIVE action for the newest notice of that app still on the
 * phone (dismissed; a ringing call declined) -- the worker writes the Control Point once. true = the ask went to the
 * worker; false = nothing to send (the desk's notice, a fake, cleared on the phone already, no phone ready): the line says why */
bool ancs_clear(const char *app);
