/* THE LAST DEATH (the operator night, 2026-10-12; T2): how the body
 * last died, for doctor's first line. The SoC's reset reason read once
 * at boot and worded (death.h); THE BREADCRUMB in RTC slow memory
 * (RTC_NOINIT_ATTR, 36 B: kept across every reset but a power-on),
 * ticked once a second from loop with the uptime, the last console
 * verb, the last nap's kind and the link's state, read back at the
 * next boot when its magic and sum hold; the NVS counters `boots` /
 * `crashes` (namespace "watch"; a crash by ruling 112); the day
 * header's reserved bytes stamped once per boot (daylog.cpp's
 * open_day). Nothing on the glass. The words and the rules are pure
 * in death.h (tools/death_test.cpp). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "death.h"

void death_begin(void);                    /* at boot, after store_begin: the reason, the old crumb, the counters, a fresh crumb; one log line */
void death_tick(uint32_t now_ms);          /* from loop: the crumb's uptime and link state, once a second */
void death_verb(const char *verb);         /* console.cpp: the verb about to run */
void death_nap(int kind);                  /* main's nap tally: DEATH_NAP_DEEP / DEATH_NAP_EAR */
int  death_code(void);                     /* DEATH_* of the last death */
const death_crumb_t *death_last(void);     /* the previous life's crumb, NULL when none survived */
uint32_t death_boots(void);                /* NVS `boots` after this boot */
uint32_t death_crashes(void);              /* NVS `crashes` after this boot */
/* the day header's reserved bytes: this boot counted once (the first file it opens), the build's crc every time */
void death_hdr_stamp(uint8_t *reserved);
int  death_status(char *out, size_t cap);              /* doctor's first line */
int  death_verdict_line(char *out, size_t cap, uint32_t up_s);   /* the verdict's words for a crash inside the hour; 0 = none */
