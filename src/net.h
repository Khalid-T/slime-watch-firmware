/* The radio and what rides on it: WiFi (PSK, open, WPA2-Enterprise
 * PEAP), auto-join of any remembered network seen, mDNS
 * slime-watch.local, NTP, ArduinoOTA, and the little HTTP status page
 * on :80 that the tailnet share exposes. Off in the dark house. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void net_begin(void);
/* THE RADIO BUDGET (radio.h, link.cpp): the wake and the dark house are
 * the LINK's hooks; the radio itself is switched by the link, for a
 * reason -- a wake no longer joins WiFi on its own */
void net_on(void);                      /* the wake: BLE up (ble.cpp: the radio is off); the link told (link_wake) */
void net_off(void);                     /* the dark house: BLE down, the link told (link_dark), the radio goes away */
void net_radio_on(void);                /* the link's ask: BLE down first, the radio comes up, joins */
void net_radio_off(void);               /* the link is done: the radio goes away, BLE back (ruling 58) */
bool net_radio_is_on(void);             /* the radio is up in any state (scanning, joining, joined) */
void net_tick(void);                    /* from loop() */
bool net_up(void);
const char *net_state_name(void);
const char *net_ssid(void);
const char *net_ip(void);               /* "" when down */
int  net_rssi(void);
void net_scan(void);                    /* `wifi-scan`: prints what it sees when done */
void net_rejoin(void);                  /* after `wifi ...`: try again now */
void net_status(char *out, int cap);
uint32_t net_http_hits(void);
