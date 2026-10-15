/* THE LOOP'S PULSE (the operator night, 2026-10-12; T1): the IDF's
 * task watchdog on the creature's own loopTask, WDT_TIMEOUT_S (30) --
 * a hang reboots inside 30 s instead of a dead glass till the cell
 * dies; the SDK's own watch (IDLE0 at 5 s) was never on this thread.
 * Fed from the top of loop(), each pass of the nap's awake-ear loop,
 * each pass of a long serial aid (wom-test, the OTA's chunks) and at
 * the edge of a light sleep (the MWDT does not count through it: the
 * SDK's 5 s IDLE0 watch has survived 60 s naps for weeks). The dial
 * task and the NimBLE host task are not subscribed. The rule of the
 * feeding is pure in wdtfeed.h (tools/wdt_test.cpp). */
#pragma once
#include <stddef.h>
#include "wdtfeed.h"

void wdt_begin(void);             /* arm it on this task; once, at the end of setup */
void wdt_feed(int spell);         /* a feed when one is owed from this WDT_SPELL_* (wdtfeed.h decides) */
int  wdt_status(char *out, size_t cap);   /* the doctor's clause: `wdt 30 s, fed 12 ms ago` */
