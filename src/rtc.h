/* PCF85063 RTC (0x51 on the second bus): the hour through a power cycle.
 * A DARK DRIVER: compiled in, inert until `bom rtc on`. At boot, when
 * the chip keeps a valid time (its oscillator-stop flag clear) and
 * nobody has told the hour yet, the clock takes it; whenever NTP, the
 * desk or `clock` set the clock, it is written back (rtc_tick watches
 * for the jump); every 10 min a drift beyond 2 s is corrected FROM the
 * RTC (the better crystal). Raw registers, BCD, no library. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void rtc_begin(void);
void rtc_end(void);
void rtc_tick(void);
bool rtc_present(void);
bool rtc_get(uint32_t *epoch);      /* false = absent, or no valid time (never set, or the cell died) */
bool rtc_set(uint32_t epoch);       /* UTC in; clears the oscillator-stop flag */
void rtc_status(char *out, int cap);
