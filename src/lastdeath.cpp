#include "lastdeath.h"
#include "app.h"
#include "link.h"               /* link_state_name: the crumb's link word */
#include "doctor.h"             /* doctor_build_id: the header's crc (build_id.h is doctor.cpp's alone) */
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_attr.h>

/* THE BREADCRUMB: RTC slow memory, untouched by the loader at any reset but a power-on */
static RTC_NOINIT_ATTR death_crumb_t rtc_crumb;

static int            s_code = DEATH_UNKNOWN;
static death_crumb_t  s_last;               /* the previous life's crumb, copied out before the new one starts */
static bool           s_have_last = false;
static uint32_t       s_boots = 0, s_crashes = 0;
static bool           s_claimed = false;    /* the day header's boot stamp, once per boot */
static uint32_t       s_tick_s = 0xFFFFFFFFu;

static int reason_code(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return DEATH_POWERON;
    case ESP_RST_SW:        return DEATH_SOFTWARE;
    case ESP_RST_TASK_WDT:  return DEATH_TASK_WDT;
    case ESP_RST_INT_WDT:   return DEATH_INT_WDT;
    case ESP_RST_WDT:       return DEATH_OTHER_WDT;
    case ESP_RST_BROWNOUT:  return DEATH_BROWNOUT;
    case ESP_RST_PANIC:     return DEATH_PANIC;
    case ESP_RST_DEEPSLEEP: return DEATH_DEEPSLEEP;
    default:                return DEATH_UNKNOWN;   /* EXT, SDIO, UNKNOWN */
    }
}

void death_begin(void)
{
    s_code = reason_code(esp_reset_reason());
    s_have_last = s_code != DEATH_POWERON && death_crumb_valid(&rtc_crumb);
    if (s_have_last) s_last = rtc_crumb;
    Preferences p;
    if (p.begin("watch", false)) {
        s_boots = p.getUInt("boots", 0);
        s_crashes = p.getUInt("crashes", 0);
        death_count(s_code, &s_boots, &s_crashes);
        p.putUInt("boots", s_boots);
        if (death_is_crash(s_code)) p.putUInt("crashes", s_crashes);
        p.end();
    }
    death_crumb_clear(&rtc_crumb);
    char l[200]; death_status(l, sizeof l);
    LOG("%s", l);
}

void death_tick(uint32_t now_ms)
{
    uint32_t s = now_ms / 1000u;
    if (s == s_tick_s) return;
    s_tick_s = s;
    death_crumb_link(&rtc_crumb, link_state_name());
    death_crumb_tick(&rtc_crumb, s);
}

void death_verb(const char *verb) { death_crumb_verb(&rtc_crumb, verb); }
void death_nap(int kind) { death_crumb_nap(&rtc_crumb, kind); }
int  death_code(void) { return s_code; }
const death_crumb_t *death_last(void) { return s_have_last ? &s_last : NULL; }
uint32_t death_boots(void) { return s_boots; }
uint32_t death_crashes(void) { return s_crashes; }

void death_hdr_stamp(uint8_t *reserved)
{
    bool boot = !s_claimed;
    s_claimed = true;
    death_hdr_mark(reserved, boot, boot && death_is_crash(s_code), death_crc16(doctor_build_id()));
}

int death_status(char *out, size_t cap) { return death_line(out, cap, s_code, death_last(), s_boots, s_crashes); }
int death_verdict_line(char *out, size_t cap, uint32_t up_s) { return death_verdict(out, cap, s_code, up_s, death_last()); }
