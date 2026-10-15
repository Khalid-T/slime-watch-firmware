/* THE PHONE'S HOUR (src/cts.h) on the laptop: the 10-byte Current Time
 * frame -> an epoch, the two Local Time bytes -> minutes, the reject
 * rules, the take rules.
 * Build: c++ -std=gnu++17 -I src -o cts_test tools/cts_test.cpp
 * Run:   ./cts_test
 * days_from_civil is walked against a naive day count over every day
 * of 1970..2100 and three anchors (y2k, the 2024 leap day, the 2038
 * second); a real phone frame lands on the right epoch; the zone's
 * quarter-hours and the DST codes; a frame of zeros, a 2023, a 13th
 * month, a 30th of February, a 24th hour, a short read are refused;
 * the clock takes the phone when unset or more than 2 s off, never at
 * 2; the tz goes in only while the desk never said. */
#include "cts.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* the naive reference: count the days from 1970-01-01, one at a time */
static int64_t ref_days(int y, unsigned m, unsigned d)
{
    int64_t n = 0;
    for (int yy = 1970; yy < y; yy++) n += ((yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0) ? 366 : 365;
    for (unsigned mm = 1; mm < m; mm++) n += cts_days_in_month(y, mm);
    return n + d - 1;
}

static void frame(uint8_t *b, int y, int mo, int d, int h, int mi, int s)
{
    memset(b, 0, CTS_FRAME_LEN);
    b[0] = (uint8_t)y; b[1] = (uint8_t)(y >> 8); b[2] = (uint8_t)mo; b[3] = (uint8_t)d; b[4] = (uint8_t)h; b[5] = (uint8_t)mi; b[6] = (uint8_t)s;
    b[7] = 1; b[8] = 128; b[9] = 1;                   /* a Monday, half a second, a manual adjust: not read */
}

int main(void)
{
    /* the civil arithmetic */
    check("1970-01-01 is day 0", cts_days_from_civil(1970, 1, 1) == 0);
    check("2000-01-01 is day 10957", cts_days_from_civil(2000, 1, 1) == 10957);
    check("2024-02-29 is day 19782", cts_days_from_civil(2024, 2, 29) == 19782);
    check("2038-01-19 is day 24855 (the 2038 second lands at 03:14:07)", cts_days_from_civil(2038, 1, 19) == 24855
          && cts_days_from_civil(2038, 1, 19) * 86400 + 3 * 3600 + 14 * 60 + 7 == 2147483647LL);
    {
        bool all = true; long walked = 0;
        for (int y = 1970; y <= 2100 && all; y++)
            for (unsigned m = 1; m <= 12 && all; m++)
                for (unsigned d = 1; d <= cts_days_in_month(y, m) && all; d++, walked++)
                    if (cts_days_from_civil(y, m, d) != ref_days(y, m, d)) { printf("  differs at %d-%u-%u\n", y, m, d); all = false; }
        check("every day of 1970..2100 agrees with the naive count", all && walked == 47847);
    }
    check("February: 28, 29 in 2024, 28 in 2100, 29 in 2000", cts_days_in_month(2025, 2) == 28 && cts_days_in_month(2024, 2) == 29
          && cts_days_in_month(2100, 2) == 28 && cts_days_in_month(2000, 2) == 29);
    check("month 0 and 13 have no days", cts_days_in_month(2026, 0) == 0 && cts_days_in_month(2026, 13) == 0);

    /* a real frame */
    uint8_t b[CTS_FRAME_LEN]; uint32_t ep = 0;
    frame(b, 2026, 10, 12, 23, 40, 15);
    check("2026-10-12 23:40:15 parses", cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    check("... to its local epoch", ep == (uint32_t)(cts_days_from_civil(2026, 10, 12) * 86400 + 23 * 3600 + 40 * 60 + 15));
    check("... the year is little-endian (0xEA 0x07)", b[0] == 0xEA && b[1] == 0x07);
    check("a longer read (notify with trailing bytes) still parses", cts_local_epoch(b, 12, &ep));

    /* the refusals */
    uint8_t z[CTS_FRAME_LEN]; memset(z, 0, sizeof z);
    check("a frame of zeros is refused (year 0 = the phone's unknown)", !cts_local_epoch(z, CTS_FRAME_LEN, &ep));
    frame(b, 2023, 12, 31, 23, 59, 59); check("2023 is refused (under CTS_YEAR_MIN)", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2024, 1, 1, 0, 0, 0);      check("2024-01-01 00:00:00 is the first accepted", cts_local_epoch(b, CTS_FRAME_LEN, &ep) && ep == 19723u * 86400u);
    frame(b, 2100, 1, 1, 0, 0, 0);      check("2100 is refused (over CTS_YEAR_MAX)", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 13, 1, 0, 0, 0);     check("a 13th month is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 0, 1, 0, 0, 0);      check("month 0 is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 2, 30, 0, 0, 0);     check("the 30th of February is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 2, 29, 0, 0, 0);     check("the 29th of February 2026 is refused (not a leap year)", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2028, 2, 29, 0, 0, 0);     check("the 29th of February 2028 is a day", cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 10, 0, 0, 0, 0);     check("day 0 is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 10, 12, 24, 0, 0);   check("a 24th hour is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 10, 12, 23, 60, 0);  check("a 60th minute is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 10, 12, 23, 59, 60); check("a 60th second is refused", !cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    frame(b, 2026, 10, 12, 23, 59, 59); check("23:59:59 is the last second", cts_local_epoch(b, CTS_FRAME_LEN, &ep));
    check("a 9-byte read is refused", !cts_local_epoch(b, 9, &ep));
    check("an empty read is refused", !cts_local_epoch(b, 0, &ep));
    check("a null read is refused", !cts_local_epoch(nullptr, CTS_FRAME_LEN, &ep));

    /* the local time information */
    int off = 0;
    check("zone -16 (quarter-hours), standard: -240 min (Khalid's winter)", cts_offset_min((uint8_t)-16, 0, &off) && off == -240);
    check("zone -20, daylight: -240 min (the same wall clock in summer)", cts_offset_min((uint8_t)-20, 4, &off) && off == -240);
    check("zone 0, standard: 0", cts_offset_min(0, 0, &off) && off == 0);
    check("zone 22 (+5:30), standard: 330", cts_offset_min(22, 0, &off) && off == 330);
    check("zone 38 (+9:30), half-hour DST: 600", cts_offset_min(38, 2, &off) && off == 600);
    check("zone 48 (+12), double daylight: 840", cts_offset_min(48, 8, &off) && off == 840);
    check("zone 56 (+14) is the top", cts_offset_min(56, 0, &off) && off == 840);
    check("zone 57 is refused", !cts_offset_min(57, 0, &off));
    check("zone -48 (-12) is the bottom", cts_offset_min((uint8_t)-48, 0, &off) && off == -720);
    check("zone -49 is refused", !cts_offset_min((uint8_t)-49, 0, &off));
    check("zone -128 (unknown) is refused", !cts_offset_min((uint8_t)CTS_TZ_UNKNOWN, 0, &off));
    check("DST 255 (unknown) is refused", !cts_offset_min((uint8_t)-16, CTS_DST_UNKNOWN, &off));
    check("DST 3 (not a code) is refused", !cts_offset_min((uint8_t)-16, 3, &off));

    /* local -> UTC */
    frame(b, 2026, 10, 12, 19, 40, 15);
    cts_local_epoch(b, CTS_FRAME_LEN, &ep);
    check("19:40:15 at -240 is 23:40:15 UTC", cts_utc(ep, -240) == (uint32_t)(cts_days_from_civil(2026, 10, 12) * 86400 + 23 * 3600 + 40 * 60 + 15));
    frame(b, 2026, 10, 13, 1, 10, 0);
    cts_local_epoch(b, CTS_FRAME_LEN, &ep);
    check("01:10 on the 13th at +330 is 19:40 UTC on the 12th", cts_utc(ep, 330) == (uint32_t)(cts_days_from_civil(2026, 10, 12) * 86400 + 19 * 3600 + 40 * 60));
    check("at offset 0 local is UTC", cts_utc(ep, 0) == ep);

    /* the take rules */
    check("an unset clock takes the phone's", cts_take(false, 0, 1000));
    check("an unset clock takes it even when they agree", cts_take(false, 1000, 1000));
    check("a set clock 3 s behind takes it", cts_take(true, 1000, 1003));
    check("a set clock 3 s ahead takes it", cts_take(true, 1003, 1000));
    check("a set clock 2 s off keeps its own (the phone's second is coarse)", !cts_take(true, 1000, 1002) && !cts_take(true, 1002, 1000));
    check("a set clock that agrees keeps its own", !cts_take(true, 1000, 1000));
    check("the tz goes in while the desk never said and the phone knows and it differs", cts_take_tz(false, true, -300, -240));
    check("... not when the desk said", !cts_take_tz(true, true, -300, -240));
    check("... not when the phone does not know", !cts_take_tz(false, false, 0, -240));
    check("... not when it is the same already", !cts_take_tz(false, true, -240, -240));
    check("the numbers: 10 bytes, 2 bytes, 2024, 2 s", CTS_FRAME_LEN == 10 && CTS_LOCAL_LEN == 2 && CTS_YEAR_MIN == 2024 && CTS_OFF_S == 2);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
