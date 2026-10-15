#include "gauge.h"
#include "bus.h"
#include "app.h"
#include <Arduino.h>
#include <stdio.h>

#define ADDR         0x36
#define REG_VCELL    0x02   /* 78.125 uV / LSB */
#define REG_SOC      0x04   /* high byte = %, low byte = 1/256 % */
#define REG_MODE     0x06
#define REG_VERSION  0x08   /* 0x0011 / 0x0012 */
#define REG_CONFIG   0x0C   /* high RCOMP (0x97), low: bit7 SLEEP, bit6 ALSC, bit5 ALRT (sticky), [4:0] ATHD = 32 - % */
#define REG_STATUS   0x1A
#define REG_CMD      0xFE   /* 0x5400 = power-on reset */

#define ALERT_PCT    10     /* ALRT pulls PIN_INT_GAUGE low under this */
#define CACHE_MS     1000

static bool s_present = false;
static uint16_t s_ver = 0;
static uint16_t s_mv = 0;
static uint8_t s_pct = 0;
static uint32_t s_at = 0;

static bool rd16(uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    if (!bus2_rd(ADDR, reg, b, 2)) return false;
    *v = (uint16_t)((b[0] << 8) | b[1]);
    return true;
}

static bool wr16(uint8_t reg, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    return bus2_wr(ADDR, reg, b, 2);
}

void gauge_begin(void)
{
    s_present = false;
    if (!rd16(REG_VERSION, &s_ver)) { LOG("gauge: MAX17048 absent at 0x%02X (on the BOM)", ADDR); return; }
    if ((s_ver & 0xFFF0) != 0x0010) { LOG("gauge: 0x%02X answers but VERSION is 0x%04X, not a MAX17048", ADDR, s_ver); return; }
    /* the alert threshold; RCOMP at the default (a 20 C cell); ALRT cleared */
    wr16(REG_CONFIG, (uint16_t)(0x9700 | (32 - ALERT_PCT)));
    s_present = true;
    s_at = 0;
    uint16_t mv; uint8_t pct;
    if (gauge_read(&mv, &pct)) LOG("gauge: MAX17048 v0x%04X found at 0x%02X: %u mV, %u %%, alert under %d %%", s_ver, ADDR, mv, pct, ALERT_PCT);
    else LOG("gauge: MAX17048 found at 0x%02X but the first read failed", ADDR);
}

void gauge_end(void) { s_present = false; }
bool gauge_present(void) { return s_present; }

bool gauge_read(uint16_t *mv, uint8_t *pct)
{
    if (!s_present) return false;
    uint32_t now = millis();
    if (!s_at || now - s_at >= CACHE_MS) {
        uint16_t v, s;
        if (!rd16(REG_VCELL, &v) || !rd16(REG_SOC, &s)) return false;
        s_mv = (uint16_t)(((uint32_t)v * 5u) / 64u);   /* 78.125 uV = 5/64 mV */
        s_pct = (uint8_t)(s >> 8 > 100 ? 100 : s >> 8);
        s_at = now;
    }
    if (mv) *mv = s_mv;
    if (pct) *pct = s_pct;
    return true;
}

uint16_t gauge_prefer(uint16_t adc_mv)
{
    uint16_t mv;
    if (s_present && gauge_read(&mv, NULL) && mv > 2500) return mv;
    return adc_mv;
}

bool gauge_alert(void) { return s_present && digitalRead(PIN_INT_GAUGE) == LOW; }

void gauge_status(char *out, int cap)
{
    if (!s_present) { snprintf(out, cap, "gauge: absent (bom %s); the ADC says %u mV", bus_on_bom(BOM_GAUGE) ? "on" : "off", batt_mv()); return; }
    uint16_t mv = 0; uint8_t pct = 0;
    bool ok = gauge_read(&mv, &pct);
    snprintf(out, cap, "gauge: MAX17048 v0x%04X at 0x%02X: %s%u mV %u %% (the ADC says %u mV), alert %s (under %d %%)",
             s_ver, ADDR, ok ? "" : "READ FAILED ", mv, pct, batt_mv(), gauge_alert() ? "LOW" : "clear", ALERT_PCT);
}
