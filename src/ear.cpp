/* THE EAR'S LEDGER AND ITS LOW GEAR (ear.h). The ledger's arithmetic is
 * drain.c's (pure, tools/drain_test.cpp); this file is the Arduino
 * side: the spell sums, NVS, the clock change, the log lines. */
#include "ear.h"
#include "drain.h"
#include "app.h"
#include "store.h"
#include "ble.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static drain_t ledger;
static uint32_t cur_deep_ms, cur_ear_ms;     /* the minute in progress: ms napped by kind */
static bool     last_spell_ear;              /* the last spell's kind (the status line's "now" while dark) */
static bool     low;                         /* the CPU is in a low gear now */
static uint32_t shifts, refusals;            /* the gear's tallies since boot */
static uint32_t low_since_ms, low_total_ms;

static bool gear_ok(int mhz) { return mhz == EAR_GEAR_MHZ || mhz == EAR_GEAR_MID_MHZ || mhz == EAR_GEAR_FULL_MHZ; }

void ear_begin(void)
{
    drain_begin(&ledger);
    if (!gear_ok(store.ear_gear)) store.ear_gear = EAR_GEAR_FULL_MHZ;   /* a torn NVS word: off */
    if (store.ear_gear < EAR_GEAR_FULL_MHZ)
        LOG("ear: the ledger open (window %d min, %d to speak); LOW GEAR ON: %u MHz while the ear naps (NVS eargear; unproven on the board -- `ear gear 240` is off)",
            DRAIN_WINDOW_MIN, DRAIN_MIN_MIN, (unsigned)store.ear_gear);
    else
        LOG("ear: the ledger open (window %d min, %d to speak); low gear off: 240 MHz through every nap (`ear gear 80|160` to try it)",
            DRAIN_WINDOW_MIN, DRAIN_MIN_MIN);
}

void ear_napped(bool awake_ear, uint32_t ms)
{
    if (awake_ear) cur_ear_ms += ms; else cur_deep_ms += ms;
    last_spell_ear = awake_ear;
}

int ear_minute(uint32_t date, uint16_t batt_mv, bool cable)
{
    int kind = drain_kind_of(cur_deep_ms, cur_ear_ms);
    cur_deep_ms = cur_ear_ms = 0;
    drain_minute(&ledger, date, batt_mv, cable, kind);
    return kind;
}

/* --- the gear --------------------------------------------------------------- */

bool ear_gear_enter(void)
{
    if (store.ear_gear >= EAR_GEAR_FULL_MHZ) return false;   /* off (the default) */
    if (!ble_keeps_dark()) return false;                     /* the deep nap: light sleep keeps its own clocks; nothing to gain */
    if (low) return true;
    if (!setCpuFrequencyMhz(store.ear_gear)) {
        refusals++;
        LOG("ear: the low gear REFUSED by the core (setCpuFrequencyMhz %u said no; %lu refusals) -- the awake nap stays at %lu MHz",
            (unsigned)store.ear_gear, (unsigned long)refusals, (unsigned long)getCpuFrequencyMhz());
        return false;
    }
    low = true; shifts++; low_since_ms = millis();
    LOG("ear: low gear %lu MHz for the awake nap (APB %lu MHz, unchanged: the UART, LEDC and I2C clocks are the crystal's; shift %lu)",
        (unsigned long)getCpuFrequencyMhz(), (unsigned long)(getApbFrequency() / 1000000u), (unsigned long)shifts);
    return true;
}

void ear_gear_leave(const char *why)
{
    if (!low) return;
    setCpuFrequencyMhz(EAR_GEAR_FULL_MHZ);
    low = false;
    uint32_t span = millis() - low_since_ms;
    low_total_ms += span;
    LOG("ear: back to %lu MHz (%s) after %lu s in low gear", (unsigned long)getCpuFrequencyMhz(), why, (unsigned long)(span / 1000u));
}

bool ear_gear_low(void) { return low; }

const char *ear_gear_set(int mhz, const char *how)
{
    static char line[200];
    if (!gear_ok(mhz)) {
        snprintf(line, sizeof line, "ear: gear %d refused (%s) -- %d or %d (the low gears) or %d (off): under 80 the S3 leaves the PLL for the crystal and the core never retunes APB's tenants (the SPI, the hw timers)",
                 mhz, how, EAR_GEAR_MHZ, EAR_GEAR_MID_MHZ, EAR_GEAR_FULL_MHZ);
        LOG("%s", line);
        return line;
    }
    if (low) ear_gear_leave("the switch");                   /* a word on the port woke it already; belt and braces */
    bool changed = store.ear_gear != (uint8_t)mhz;
    store.ear_gear = (uint8_t)mhz;
    if (changed) store_save_ear_gear();
    if (mhz < EAR_GEAR_FULL_MHZ)
        snprintf(line, sizeof line, "ear: low gear ON at %d MHz (%s%s) -- the awake nap runs there from the next dark spell; unproven on the board: watch the phone's notices and `ear`",
                 mhz, how, changed ? ", NVS" : ", unchanged");
    else
        snprintf(line, sizeof line, "ear: low gear off (%s%s) -- the awake nap runs at 240 MHz", how, changed ? ", NVS" : ", unchanged");
    LOG("%s", line);
    return line;
}

/* --- the line ------------------------------------------------------------------ */

void ear_status(char *out, int cap)
{
    char led[200];
    drain_text(&ledger, led, sizeof led);
    const char *now = app.state != 2 ? "the glass lit" : last_spell_ear ? "the awake ear" : "the deep nap";
    char gear[24];
    if (store.ear_gear < EAR_GEAR_FULL_MHZ) snprintf(gear, sizeof gear, "ON at %u", (unsigned)store.ear_gear); else strcpy(gear, "off");
    snprintf(out, (size_t)cap, "%s | now %s | gear %lu MHz (low gear %s, %lu shift%s, %lu refusal%s, %lu s low) | ear %s",
             led, now, (unsigned long)getCpuFrequencyMhz(), gear, (unsigned long)shifts, shifts == 1 ? "" : "s",
             (unsigned long)refusals, refusals == 1 ? "" : "s", (unsigned long)((low_total_ms + (low ? millis() - low_since_ms : 0)) / 1000u),
             ble_keeps_dark() ? "on (a peer bonded, the switch on)" : "off (no bond or the switch off: the deep nap)");
}

void ear_cmd(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "gear")) { Serial.println(ear_gear_set(atoi(argv[2]), "serial")); return; }
    if (argc > 1) { Serial.println("ear | ear gear 80|160|240"); return; }
    char l[400]; ear_status(l, sizeof l); Serial.println(l);
}
