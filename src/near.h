/* THE DESK NEXT DOOR (design/watch-night-2026-10-12.md C9, THE
 * OPERATOR NIGHT): how far the desk is, read off the air door's own
 * signal. While the desk is on the air door (bleuart.cpp: the one
 * bonded subscriber) the host can say the connection's RSSI
 * (`ble_gap_conn_rssi`, one HCI ask, no buffer); asked every
 * NEAR_POLL_MS on the lit glass, smoothed, and banded FAR / NEAR /
 * AT HAND with a hysteresis of NEAR_HYST_DBM so a signal wobbling on a
 * line does not flap. Two things read it: THE SMALL HOUSE's nudge --
 * "the bigger house is reachable" gains "and it is near" (a laptop
 * across the flat is reachable but not a bowl to go to: the fidget
 * only, no bump) -- and one glance toward the air door (the wisp's
 * rim point opposite the floor, `wisp_door_point`) for NEAR_GLANCE_S
 * when the band RISES into AT HAND from a held NEAR or FAR: he sat
 * down at the desk, and the creature looks at the door it would go
 * through. A band that is simply KNOWN for the first time (the desk
 * connecting at arm's length, the stack back after a dial, the glass
 * lit again) is no edge: the arrival is the air door's own moment.
 * Never a bump, never a mark, nothing on the glass, nothing of the
 * rack.
 *
 * THE HONEST LIMIT (ruling 58): the BLE stack is up only while the
 * WiFi radio is down, so at home on WiFi there is no air door and no
 * RSSI -- the band is NONE and nothing here plays. This is for the
 * air-door days: the case that blocks the WiFi at the desk, the
 * laptop at arm's length over Bluetooth.
 *
 * The dark keeps no band: with no sample for NEAR_STALE_MS (the dark
 * glass asks none) the band falls to NONE, so the first lit sample
 * sets it fresh and cannot be the edge.
 *
 * Pure C, no Arduino. tools/near_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/near_test tools/near_test.cpp -x c src/near.c && /tmp/near_test */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEAR_POLL_MS     2000     /* the desk's RSSI asked this often while lit (one HCI command; never in the dark) */
#define NEAR_DBM         -70      /* smoothed RSSI at or over this = NEAR (a room away); the ruling's number, guessed */
#define ATHAND_DBM       -55      /* ...at or over this = AT HAND (the laptop at arm's length); guessed */
#define NEAR_HYST_DBM    5        /* a band lets go only this many dBm under its line (NEAR -> FAR under -75, AT HAND -> NEAR under -60) */
#define NEAR_SMOOTH_K    0.4f     /* the smoothing: s += k (sample - s) per poll; a step settles in ~3 polls (6 s); one spike moves it 40 % */
#define NEAR_STALE_MS    10000    /* no sample for this long (the dark glass) = the band is NONE again */
#define NEAR_GLANCE_S    1.5f     /* the glance toward the air door on the rise into AT HAND (the far tap's own second and a half) */

enum { NEAR_NONE = 0, NEAR_FAR, NEAR_NEAR, NEAR_ATHAND };      /* NONE = no desk on the air door (or no sample yet) */
enum { NEAR_EV_NONE = 0,
       NEAR_EV_BAND,        /* the band changed (main logs its word) */
       NEAR_EV_ATHAND };    /* ...and it ROSE into AT HAND from a held NEAR or FAR: the glance toward the door */

bool        near_due(uint32_t now_ms, bool lit);   /* one call a frame: true when a sample is wanted (every NEAR_POLL_MS while lit; at once on the lit edge); the dark ages the band */
int         near_feed(int rssi_dbm);               /* the sample asked (0 = no desk on the air door): the smoothing, the band, the event */
int         near_band(void);                       /* NEAR_* now */
bool        near_is_near(void);                    /* NEAR or AT HAND: the bigger house is near (the cramp's word) */
int         near_dbm(void);                        /* the smoothed RSSI, 0 = none */
const char *near_word(int band);                   /* "--" / "FAR" / "NEAR" / "AT HAND" */
int         near_glances(void);                    /* rises into AT HAND since boot (edges, whether main glanced or not) */

#ifdef __cplusplus
}
#endif
