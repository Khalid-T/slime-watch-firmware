#include "rtc.h"
#include "bus.h"
#include "app.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ADDR          0x51
#define REG_CONTROL1  0x00   /* bit7 EXT_TEST, bit5 STOP, bit4 SR (0x58 = soft reset), bit0 CAP_SEL (0 = 7 pF, 1 = 12.5 pF) */
#define REG_CONTROL2  0x01   /* bit7 AIE, bit6 AF, bit5 MI, bit4 HMI, bit3 TF, [2:0] COF (111 = CLKOUT off) */
#define REG_SECONDS   0x04   /* bit7 OS: the oscillator stopped since the last set -- the time is not to be trusted */
#define REG_YEARS     0x0A

/* the crystal load: the common breakouts carry a 12.5 pF crystal --
 * FIRST-DAY: read the crystal's marking; a wrong CAP_SEL is minutes a
 * month. Offset (0x02) trims what is left, later. */
#define CAP_SEL_12P5  1

#define JUMP_S        2      /* the clock moved this much between ticks = someone set it: write it back */
#define DRIFT_CHECK_MS (10u * 60u * 1000u)

static bool s_present = false;
static int64_t s_last_off = 0;       /* epoch - uptime, the last tick's */
static uint32_t s_drift_at = 0;
static uint32_t s_writes = 0, s_takes = 0;

static uint8_t bcd2(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0F)); }
static uint8_t tobcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

/* days from civil (Howard Hinnant), for an epoch without the libc's TZ */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static int64_t clock_off(void) { return (int64_t)time(NULL) - (int64_t)(millis() / 1000u); }

void rtc_begin(void)
{
    s_present = false;
    uint8_t c[2];
    if (!bus2_rd(ADDR, REG_CONTROL1, c, 2)) { LOG("rtc: PCF85063 absent at 0x%02X (on the BOM)", ADDR); return; }
    /* no id register: an ACK, the unused bits of Control_1 reading 0, and the seconds in BCD range */
    uint8_t sec = bus2_rd1(ADDR, REG_SECONDS);
    if ((c[0] & 0x48) || bcd2(sec & 0x7F) > 59) { LOG("rtc: 0x%02X answers but does not look like a PCF85063 (ctl 0x%02X sec 0x%02X)", ADDR, c[0], sec); return; }
    bus2_wr1(ADDR, REG_CONTROL1, (uint8_t)((c[0] & ~0x01) | CAP_SEL_12P5));
    bus2_wr1(ADDR, REG_CONTROL2, 0x07);                    /* CLKOUT off, alarms off (the INT pin reserved for later) */
    s_present = true;
    s_last_off = clock_off();
    s_drift_at = millis();
    uint32_t ep;
    if (rtc_get(&ep)) {
        if (!clock_is_set()) { clock_set_epoch(ep, "the rtc"); s_last_off = clock_off(); s_takes++; }
        else LOG("rtc: PCF85063 found at 0x%02X, keeps %lu (the clock already set, %+ld s apart)", ADDR, (unsigned long)ep, (long)((int64_t)ep - (int64_t)time(NULL)));
    } else LOG("rtc: PCF85063 found at 0x%02X, NO valid time (never set, or its power was lost) -- written when the clock is known", ADDR);
}

void rtc_end(void) { s_present = false; }
bool rtc_present(void) { return s_present; }

bool rtc_get(uint32_t *epoch)
{
    if (!s_present) return false;
    uint8_t t[7];
    if (!bus2_rd(ADDR, REG_SECONDS, t, 7)) return false;
    if (t[0] & 0x80) return false;                          /* OS: the oscillator stopped */
    unsigned s = bcd2(t[0] & 0x7F), mi = bcd2(t[1] & 0x7F), h = bcd2(t[2] & 0x3F);
    unsigned d = bcd2(t[3] & 0x3F), mo = bcd2(t[5] & 0x1F), y = 2000 + bcd2(t[6]);
    if (s > 59 || mi > 59 || h > 23 || d < 1 || d > 31 || mo < 1 || mo > 12) return false;
    int64_t days = days_from_civil((int)y, mo, d);
    int64_t ep = days * 86400 + h * 3600 + mi * 60 + s;
    if (ep < 1600000000LL) return false;                    /* before 2020: never set */
    *epoch = (uint32_t)ep;
    return true;
}

bool rtc_set(uint32_t epoch)
{
    if (!s_present) return false;
    time_t tt = (time_t)epoch;
    struct tm g;
    gmtime_r(&tt, &g);
    uint8_t t[7] = { tobcd((uint8_t)g.tm_sec), tobcd((uint8_t)g.tm_min), tobcd((uint8_t)g.tm_hour), tobcd((uint8_t)g.tm_mday),
                     (uint8_t)g.tm_wday, tobcd((uint8_t)(g.tm_mon + 1)), tobcd((uint8_t)(g.tm_year % 100)) };
    /* seconds written with OS clear: one 7-byte transfer, the chip latches the whole set together */
    bool ok = bus2_wr(ADDR, REG_SECONDS, t, 7);
    if (ok) s_writes++;
    return ok;
}

void rtc_tick(void)
{
    if (!s_present) return;
    int64_t off = clock_off();
    if (clock_is_set() && llabs(off - s_last_off) > JUMP_S) {
        /* someone set the clock (NTP, the desk, `clock`): the RTC follows */
        if (rtc_set((uint32_t)time(NULL))) LOG("rtc: written from the clock (it moved %+ld s)", (long)(off - s_last_off));
        s_last_off = off;
        s_drift_at = millis();
        return;
    }
    s_last_off = off;
    if (millis() - s_drift_at < DRIFT_CHECK_MS) return;
    s_drift_at = millis();
    uint32_t ep;
    if (!clock_is_set() || !rtc_get(&ep)) return;
    int64_t diff = (int64_t)ep - (int64_t)time(NULL);
    if (llabs(diff) > JUMP_S) {
        clock_set_epoch(ep, "the rtc (drift)");
        s_last_off = clock_off();
        s_takes++;
    }
}

void rtc_status(char *out, int cap)
{
    if (!s_present) { snprintf(out, cap, "rtc: absent (bom %s)", bus_on_bom(BOM_RTC) ? "on" : "off"); return; }
    uint32_t ep;
    bool ok = rtc_get(&ep);
    if (!ok) { snprintf(out, cap, "rtc: PCF85063 at 0x%02X, NO valid time; clock %s (`rtc sync` writes it)", ADDR, clock_is_set() ? "set" : "unset"); return; }
    time_t tt = (time_t)ep; struct tm g; gmtime_r(&tt, &g);
    snprintf(out, cap, "rtc: PCF85063 at 0x%02X keeps %04d-%02d-%02d %02d:%02d:%02d UTC (%+ld s from the clock%s), INT GPIO %d %s, %lu written %lu taken",
             ADDR, g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec,
             (long)((int64_t)ep - (int64_t)time(NULL)), clock_is_set() ? "" : ", clock UNSET", PIN_INT_RTC,
             digitalRead(PIN_INT_RTC) ? "high" : "LOW", (unsigned long)s_writes, (unsigned long)s_takes);
}
