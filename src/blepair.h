/* THE PAIRING WINDOW (Khalid 2026-10-06: "on the watch I can turn on
 * bluetooth, then turn on pairing, then connect from the other device
 * -- or via slime too"). A bond is the only way in: the phone (ANCS)
 * and the desk (the air door, bleuart.cpp) both need one, and the NUS
 * characteristics are encrypted-only. So WHEN a stranger may bond is
 * the rule here: only inside a window he opened -- the BLUETOOTH row's
 * third state, `ble pair` on the console, the desk's `ble pair` ask over
 * any open door -- BLEPAIR_S long, closed early by the bond that lands
 * or a tap. Outside it an unbonded peer is sent away at the connect
 * (blestack.cpp), and the watch never asks a stranger for security. A
 * peer already bonded comes and goes as it pleases, window or not.
 *
 * Pure (no Arduino): the clock is the caller's millis; tools/blepair_test.cpp
 * walks the admit table and the window's edges on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BLEPAIR_S 60u      /* the window: long enough to find Settings > Bluetooth on the phone, short enough to be a moment, not a mode */

typedef struct {
    uint32_t open_ms;      /* when it opened (0 = never / closed) */
    uint32_t secs;         /* its length */
    bool     open;
} blepair_t;

/* the verdict on a peer at the connect (and again when its bond lands) */
enum { BLEPAIR_BONDED = 0,   /* known: in, window or not */
       BLEPAIR_WINDOW = 1,   /* a stranger inside the window: may bond now */
       BLEPAIR_REFUSED = 2 };/* a stranger, no window: sent away */

void     blepair_init(blepair_t *p);
void     blepair_open(blepair_t *p, uint32_t now_ms, uint32_t secs);   /* (re)open: the clock restarts */
void     blepair_close(blepair_t *p);                                  /* a bond landed, a tap, the switch off */
bool     blepair_is_open(const blepair_t *p, uint32_t now_ms);        /* still inside the window (closes itself at the end) */
uint32_t blepair_left_s(const blepair_t *p, uint32_t now_ms);         /* seconds left, 0 when closed (1 for the last fraction: the row never shows 0 S) */
int      blepair_admit(const blepair_t *p, uint32_t now_ms, bool bonded);   /* BLEPAIR_* */
const char *blepair_why(int verdict);                                  /* the log's word */
#ifdef __cplusplus
}
#endif
