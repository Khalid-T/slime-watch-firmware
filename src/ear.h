/* THE EAR'S LEDGER AND ITS LOW GEAR (2026-10-09, the overhaul wave).
 * THE DARK EAR (ruling 78) keeps the BLE stack through the dark glass by
 * idling the nap AWAKE instead of light sleep; what that costs the cell
 * was owed to him unmeasured. This module makes it visible without a
 * bench (the ledger: drain.c, pure) and, opt-in, cheaper (the low gear:
 * the CPU at EAR_GEAR_MHZ while the ear naps, back at the wake).
 *
 * The ledger: main's nap hands every spell's kind and length here
 * (ear_napped); daylog.cpp's minute asks the minute's kind (ear_minute),
 * flags the row EAR (daylog.h bit4) and the ledger takes the minute's
 * cell with it. `doctor` and `ear` print the line.
 *
 * The gear: `ear gear 80|160|240` (NVS `eargear`, 240 = off, THE DEFAULT:
 * the board has not proven it). main's nap calls ear_gear_enter before
 * the first spell and each spell after the dark errand; ear_gear_leave
 * at the wake; link.cpp's night errand leaves it before dialing. Every
 * shift is one log line; a refusal says why (README "THE EAR'S LEDGER
 * AND ITS LOW GEAR" carries the research on what the clock change
 * touches on the S3). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define EAR_GEAR_MHZ       80    /* the low gear: the IDF's own floor under an active BLE controller and PSRAM (the PLL's 80 -- APB stays 80 MHz, so no peripheral retunes); 40 and under leave the PLL and the core's APB callbacks never fire on the S3 */
#define EAR_GEAR_MID_MHZ   160   /* a middle gear, offered because it costs nothing: the same PLL, the same 80 MHz APB */
#define EAR_GEAR_FULL_MHZ  240   /* the wake's gear: the build's default (CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ) */

void ear_begin(void);                                   /* boot: the ledger open, the gear's NVS word logged */
/* one nap spell done (wake_nap_once returned): its kind and its length */
void ear_napped(bool awake_ear, uint32_t ms);
/* the minute closing (daylog.cpp): the minute's dominant kind (drain.h
 * DRAIN_*), the ledger fed with the cell; the spell sums start over */
int  ear_minute(uint32_t date, uint16_t batt_mv, bool cable);
/* the gear */
bool ear_gear_enter(void);                              /* low gear now, if wanted (NVS 80) and the ear is on; idempotent, logged on a shift */
void ear_gear_leave(const char *why);                   /* back to 240 if low; logged */
bool ear_gear_low(void);
const char *ear_gear_set(int mhz, const char *how);     /* `ear gear 80|160|240`: NVS when changed; the line says what happened (a refusal names why) */
/* `ear` over serial, doctor's line: the kind now, the gear, the tallies, the slopes */
void ear_status(char *out, int cap);
void ear_cmd(int argc, char **argv);
