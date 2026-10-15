/* BLUETOOTH (2026-09-10 21:50, ruling 58: "do BLE only when WiFi is
 * down"; 2026-10-06 THE PAIRING WINDOW and THE AIR DOOR). The BLE
 * stack's two edges, the switch, the row's word, the heap line. The
 * rule is blegate.h's (pure, tested); this file applies it to the live
 * radio; blestack.cpp is the stack itself (the server, the advertising,
 * the bonds, the window) with its two tenants -- ancs.cpp (the iPhone's
 * notices as wisps) and bleuart.cpp (the desk's lines over the air).
 *
 * ble_up / ble_down are called from THE RADIO's edges ONLY -- net.cpp's
 * net_on (the wake), net_radio_on (a dial: down FIRST, before
 * WiFi.begin), net_radio_off (the linger's end: back up), net_off (the
 * dark house: down), net_scan (a scan by hand turns the radio on: down)
 * -- and from the switch. Never both stacks: ble_up refuses with a log
 * line while the WiFi radio is up (blegate_both), and never crashes.
 * README "BLUETOOTH". */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void ble_begin(void);                    /* boot: the switch as NVS has it, the tenants' own begin; nothing on the air yet */
void ble_up(const char *why);            /* a radio edge: BLE may come up now -- through the gate (the switch, the dark house, the WiFi radio) */
void ble_down(const char *why);          /* a radio edge: BLE goes down now (WiFi about to come up): unconditional, the heap back whole */
/* THE DARK EAR (2026-10-06, ruling 78 his way: "how would I get a notification when I turn the watch on?"):
 * with a peer bonded the dark house keeps the stack (the nap idles awake, wake.cpp's ear); without one it is
 * ble_down as before. A phone's notice in the dark bumps the wrist at once (the wisp waits for the light). */
void ble_dark(void);                     /* the dark house's edge (net_off): keep or down */
bool ble_keeps_dark(void);               /* the ear is on: the switch ON and a peer bonded (as of the last rise) */
bool ble_phone_ready(void);              /* the stack up and the phone on, bonded and subscribed (ancs_connected): the page's PHONE NEAR, the row's PHONE */
bool ble_desk_open(void);                /* the stack up and a bonded desk subscribed over the air (bleuart_linked): the page's AIR DOOR OPEN */
int  ble_desk_rssi(void);                /* THE DESK NEXT DOOR (near.h): the desk's connection RSSI in dBm while the stack is up and the air door open, else 0 (main.cpp needs no bleuart header) */
bool ble_bonded(void);                   /* a bond in NVS: the stack's count while up, the last rise's while down (THE BLUETOOTH PAGE's PHONE, SETTINGS 6's FORGET) */
void ble_ear_tick(void);                 /* from the nap: the tenants' work while the glass is dark */
void ble_dark_notice(int haptic_kind, int repeats);   /* ancs: a notice landed on a dark glass -- felt now (the quiet hours and the switch permitting) */
bool ble_is_up(void);
bool ble_enabled(void);                  /* the BLUETOOTH row's switch (NVS `bleoff` absent = on) */
void ble_set_enabled(bool on, const char *how);   /* the row / `set bluetooth on|off` / `ble on|off`: NVS when changed; applied at once through the gate */
/* THE PAIRING WINDOW: the row's third state, `ble pair`, the desk's `ble pair` ask.
 * Returns the seconds it is open for; -1 = the switch is OFF (nothing opened) */
int  ble_pair_open(uint32_t secs, const char *how);
uint32_t ble_pair_left_s(void);          /* seconds left in the window, 0 = closed (the row counts it down; main holds the page and the light) */
void ble_pair_close(const char *why);
void ble_forget(void);                   /* `ble forget` / `ancs forget`: every bond dropped */
const char *ble_word(void);              /* the row's value: "OFF" / "ON --" (on, nobody) / "PHONE" / "DESK" / "BOTH" (the shine) / "42 S" (the window's count) */
void ble_tick(float dt);                 /* loop(): the tenants' events, the window's end, a bond's bump; the 30 s budget line while up */
void ble_status(char *out, int cap);     /* `ble` over serial, doctor's line */
