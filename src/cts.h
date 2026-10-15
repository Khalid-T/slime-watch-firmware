/* THE PHONE'S HOUR (2026-10-12, the operator night, T9): the bonded
 * iPhone's Current Time Service (0x1805), read once per connection
 * after the Notification Center subscription, so a cold boot with no
 * WiFi and no desk still knows the hour -- the phone is in his pocket
 * before any router is.
 *
 * What the phone serves (the Bluetooth SIG's CTS 1.1):
 *   2A2B Current Time, 10 bytes: year LE16, month, day, hours, minutes,
 *        seconds, day-of-week (1 Mon .. 7 Sun, 0 unknown), fractions256,
 *        adjust reason -- the phone's LOCAL time, as it shows it;
 *   2A0F Local Time Information, 2 bytes: the time zone as a signed
 *        count of quarter-hours (-48..56; -128 = unknown), the DST
 *        offset as a code (0 standard, 2 half an hour, 4 daylight,
 *        8 double daylight, 255 unknown).
 * UTC = local - (zone + dst). iOS serves CTS only to a BONDED central:
 * the pairing window already makes the bond (blepair.h), so the read
 * rides the same connection the notices do and asks nothing new of him.
 *
 * This header is the pure half: the frame -> a local epoch (days from
 * the civil date by Howard Hinnant's days_from_civil, no mktime, no
 * tz database), the two tz bytes -> minutes east of UTC, the reject
 * rules (a frame short of 10 bytes, a year under CTS_YEAR_MIN -- 0 is
 * the spec's "unknown" and a frame of zeros reads as one -- a bad
 * month / day / hour / minute / second), and the decision whether the
 * watch's clock takes it (unset, or more than CTS_OFF_S off). ancs.cpp
 * does the read on the worker and hands the bytes to loop(), which
 * calls clock_set_epoch(ep, "the phone"); doctor's clock line says
 * `from the phone`. The tz goes to store.tz_min ONLY while the desk
 * never said one (store.tz_said: NVS has no "tz" key), RAM only -- the
 * desk's word, when it comes, is the one that is kept (ruling 138).
 * No Arduino: tools/cts_test.cpp walks it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CTS_UUID_SVC    0x1805      /* Current Time Service */
#define CTS_UUID_TIME   0x2A2B      /* Current Time (read, notify) */
#define CTS_UUID_LOCAL  0x2A0F      /* Local Time Information (read) */
#define CTS_FRAME_LEN   10          /* 2A2B's bytes; a shorter read is refused */
#define CTS_LOCAL_LEN   2           /* 2A0F's bytes */
#define CTS_YEAR_MIN    2024        /* under this the phone has no clock either (0 = the spec's unknown) */
#define CTS_YEAR_MAX    2099
#define CTS_OFF_S       2           /* the watch's clock is reset by the phone only when this far off */
#define CTS_TZ_UNKNOWN  (-128)      /* 2A0F's zone byte for "not known" */
#define CTS_DST_UNKNOWN 255

/* days since 1970-01-01 of a civil date (proleptic Gregorian); Howard
 * Hinnant's algorithm, exact for every year the watch will see */
static inline int64_t cts_days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                               /* [0, 399] */
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;          /* [0, 365] */
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                   /* [0, 146096] */
    return era * 146097 + (int64_t)doe - 719468;
}

static inline unsigned cts_days_in_month(int y, unsigned m)
{
    static const uint8_t len[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 0;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : len[m - 1];
}

/* 2A2B's frame -> the phone's LOCAL time as an epoch-shaped count
 * (seconds since 1970 as if the local clock were UTC). false = refused:
 * short, the year out of range (a frame of zeros is year 0), a month /
 * day / hour / minute / second that is not one. The day-of-week, the
 * fractions and the reason are not read (the second is enough; the
 * fraction would be a quarter of a second at most). */
static inline bool cts_local_epoch(const uint8_t *b, int n, uint32_t *out)
{
    if (!b || n < CTS_FRAME_LEN) return false;
    int y = (int)b[0] | ((int)b[1] << 8);
    unsigned mo = b[2], d = b[3], h = b[4], mi = b[5], s = b[6];
    if (y < CTS_YEAR_MIN || y > CTS_YEAR_MAX) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > cts_days_in_month(y, mo)) return false;
    if (h > 23 || mi > 59 || s > 59) return false;
    int64_t days = cts_days_from_civil(y, mo, d);
    *out = (uint32_t)(days * 86400 + (int64_t)h * 3600 + (int64_t)mi * 60 + s);
    return true;
}

/* 2A0F's two bytes -> minutes east of UTC (the zone in quarter-hours
 * plus the DST code's hours). false = the phone does not know (-128 /
 * 255, or a code that is not one, or a zone off the spec's range). */
static inline bool cts_offset_min(uint8_t zone_byte, uint8_t dst_byte, int *out_min)
{
    int8_t zone = (int8_t)zone_byte;
    if (zone == CTS_TZ_UNKNOWN || zone < -48 || zone > 56) return false;
    int dst;
    switch (dst_byte) {
        case 0: dst = 0; break;
        case 2: dst = 30; break;
        case 4: dst = 60; break;
        case 8: dst = 120; break;
        default: return false;                       /* 255 unknown, or not a code */
    }
    *out_min = (int)zone * 15 + dst;
    return true;
}

/* local -> UTC by the offset the phone gave, or the watch's own
 * store.tz_min when the phone's is unknown (the desk's word, or
 * Khalid's default: the same hour either way on his wrist) */
static inline uint32_t cts_utc(uint32_t local_epoch, int offset_min)
{
    return (uint32_t)((int64_t)local_epoch - (int64_t)offset_min * 60);
}

/* whether the watch's clock takes the phone's: unset, or more than
 * CTS_OFF_S apart either way */
static inline bool cts_take(bool clock_set, uint32_t watch_epoch, uint32_t phone_epoch)
{
    if (!clock_set) return true;
    int64_t d = (int64_t)phone_epoch - (int64_t)watch_epoch;
    return d > CTS_OFF_S || d < -CTS_OFF_S;
}

/* whether the phone's tz goes to store.tz_min: only while the desk
 * never said one (and only when it differs -- a log line, not a loop) */
static inline bool cts_take_tz(bool desk_said, bool have_offset, int offset_min, int store_tz_min)
{
    return !desk_said && have_offset && offset_min != store_tz_min;
}
