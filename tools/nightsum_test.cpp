/* THE NIGHT'S SUM on the laptop (no board, no Arduino: nightsum.c is
 * rows in, a few verdicts out -- the pattern of daykeep_test):
 *   c++ -std=gnu++17 -I src -o /tmp/nightsum_test tools/nightsum_test.cpp -x c src/nightsum.c && /tmp/nightsum_test
 * Synthetic weeks of day-log rows (one a minute, the hr byte a spot
 * every 5 min): a regular week dipping 70 -> 58 at night = DEEP and the
 * ring's night matched by its end; a flat week not; no hr bytes (today's
 * files) = unknown and silent; too few spots = unknown; the midpoint's
 * arithmetic; a steady week vs bed times wandering two hours; two
 * nights = too few; a nap by day, a shelf night, a night on the cable
 * not nights; the toss forgiven under 15 min and the run cut by a
 * longer one; a gap (the watch off) cutting the run; the weather: a
 * week at 63 then a day at 71 = under it, 66 not, three spots not, two
 * days of norm not; a row fed twice dropped; the live day after the
 * walk; the status line. */
#include "../src/nightsum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define WORN 0x01
#define CHARGING 0x04
#define UP 0x08

static const uint32_t TODAY = 20260915;             /* the walk's base is 20260908 */
static uint32_t day(int d) { return 20260900u + (uint32_t)d; }   /* September: 8..15 is the week, 16 the live day after */
static bool quiet_at(int minute) { return minute >= 23 * 60 || minute < 7 * 60; }   /* rest's default 23-7 */

/* one day's rows, every minute, as a profile says:
 *   asleep before ``wake`` and from ``bed`` (1440 = no bed tonight);
 *   hr spots every 5 min: ``hr_day`` awake, ``hr_night`` asleep (0 = no byte);
 *   ``flags`` the row's (WORN usually); a nap [nap0, nap1) still by day */
typedef struct {
    int wake, bed;
    int hr_day, hr_night;
    uint8_t flags;
    int nap0, nap1;
    int toss_at, toss_len;      /* a toss (energy 100, no steps) inside the night */
    int gap0, gap1;             /* rows missing: the watch off */
    int spots_only;             /* > 0: only this many night spots in the whole night (then none) */
    int until;                  /* rows fed: 1440 = the whole day */
} profile_t;
static int g_night_spots = 0;   /* spots_only's count, across the days of a night */

static profile_t plain(int wake, int bed, int hr_day, int hr_night)
{
    profile_t p; memset(&p, 0, sizeof p);
    p.wake = wake; p.bed = bed; p.hr_day = hr_day; p.hr_night = hr_night; p.flags = WORN;
    p.nap0 = p.nap1 = -1; p.toss_at = -1; p.gap0 = p.gap1 = -1; p.until = 1440;
    return p;
}

static void feed_day(uint32_t date, const profile_t *p)
{
    for (int m = 0; m < p->until; m++) {
        if (p->gap0 >= 0 && m >= p->gap0 && m < p->gap1) continue;
        bool asleep = m < p->wake || m >= p->bed;
        bool nap = p->nap0 >= 0 && m >= p->nap0 && m < p->nap1;
        bool toss = p->toss_at >= 0 && m >= p->toss_at && m < p->toss_at + p->toss_len;
        bool still = (asleep || nap) && !toss;
        uint16_t energy = still ? 5 : toss ? 100 : 200;
        uint16_t steps = still || toss ? 0 : 40;
        uint8_t hr = 0;
        if (m % 5 == 0) {
            if (asleep) {
                hr = (uint8_t)p->hr_night;
                if (p->spots_only > 0 && g_night_spots++ >= p->spots_only) hr = 0;
            } else hr = (uint8_t)p->hr_day;
        }
        nightsum_row(date, m, steps, energy, p->flags, 0, 0, hr, quiet_at(m));
    }
}

/* a week of the same profile, days 8..15 (today's rows up to ``today_until``) */
static void feed_week(const profile_t *p, int today_until)
{
    nightsum_begin(TODAY);
    for (int d = 8; d <= 15; d++) {
        if (d == 14) g_night_spots = 0;                 /* the last night's spots counted from its evening */
        if (d < 15) { feed_day(day(d), p); continue; }
        profile_t t = *p; t.bed = 1440; t.until = today_until;
        feed_day(day(d), &t);
    }
}

int main(void)
{
    char st[400];
    nightsum_night_t ln;

    printf("-- the calendar\n");
    CHECK(nightsum_ordinal(20260915) - nightsum_ordinal(20260908) == 7, "a week is 7 days");
    CHECK(nightsum_ordinal(20261001) - nightsum_ordinal(20260930) == 1, "September to October");
    CHECK(nightsum_ordinal(20270301) - nightsum_ordinal(20270228) == 1 && nightsum_ordinal(20280301) - nightsum_ordinal(20280228) == 2, "the leap day");

    printf("-- a regular week: bed 23:30, up 07:00, the heart 70 by evening and 58 asleep\n");
    profile_t p = plain(7 * 60, 23 * 60 + 30, 70, 58);
    feed_week(&p, 12 * 60);
    nightsum_status(st, sizeof st); printf("       %s\n", st);
    CHECK(nightsum_last_night(&ln), "a night found");
    CHECK(ln.minutes == 450, "the last night 7.5 h (%d min)", ln.minutes);
    CHECK(ln.end_abs % 1440 == 7 * 60 - 1 && ln.end_abs / 1440 == nightsum_ordinal(TODAY), "it ended at 06:59 today");
    CHECK(ln.mid_abs % 1440 == 3 * 60 + 15, "the midpoint 03:15 (%02lu:%02lu)", (unsigned long)(ln.mid_abs % 1440 / 60), (unsigned long)(ln.mid_abs % 60));
    CHECK(ln.dip == NS_DEEP && ln.ref_hr == 70 && ln.mid_hr == 58, "DEEP: %d by evening, %d in the middle third (%d + %d spots)", ln.ref_hr, ln.mid_hr, ln.ref_n, ln.mid_n);
    CHECK(nightsum_deep(ln.end_abs), "the ring's night (its wake minute) matches: the shine");
    CHECK(nightsum_deep(ln.end_abs + 60), "...a wake an hour later still matches (the toss, the wake's minute)");
    CHECK(!nightsum_deep(ln.end_abs + 300), "...a night five hours off is another night: not this one");
    CHECK(!nightsum_deep(0), "no night id (no clock when it ended): nothing");
    int range, nn;
    CHECK(!nightsum_wander(&range, &nn) && nn == 7 && range == 0, "seven nights, the midpoints steady (range %d min over %d nights)", range, nn);
    int th, nh;
    CHECK(!nightsum_weather(&th, &nh) && th == 70 && nh == 70, "resting today %d vs the week's %d: fine", th, nh);
    CHECK(strstr(st, "DEEP") && strstr(st, "steady") && strstr(st, "fine"), "`night` says so");

    printf("-- a flat week: 70 by evening, 66 asleep (a dip of 4)\n");
    p = plain(7 * 60, 23 * 60 + 30, 70, 66);
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.dip == NS_FLAT, "flat (%d vs %d)", ln.ref_hr, ln.mid_hr);
    CHECK(!nightsum_deep(ln.end_abs), "no shine");
    p = plain(7 * 60, 23 * 60 + 30, 70, 62);
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.dip == NS_DEEP, "a dip of exactly DIP_BPM (8) is deep");

    printf("-- no hr bytes at all (the files today): nothing known, nothing worn\n");
    p = plain(7 * 60, 23 * 60 + 30, 0, 0);
    feed_week(&p, 12 * 60);
    nightsum_status(st, sizeof st); printf("       %s\n", st);
    CHECK(nightsum_last_night(&ln) && ln.dip == NS_UNKNOWN && ln.ref_n == 0 && ln.mid_n == 0, "the night found, its dip unknown");
    CHECK(!nightsum_deep(ln.end_abs), "no shine");
    CHECK(!nightsum_weather(&th, &nh) && th == 0 && nh == 0, "no weather verdict");
    CHECK(strstr(st, "no dip known") && strstr(st, "no verdict"), "`night` says the bytes are 0");
    CHECK(!nightsum_wander(&range, &nn) && nn == 7, "...but the steadiness needs no heart: 7 nights judged, steady");

    printf("-- too few spots in the night (two): unknown\n");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.spots_only = 2;
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.dip == NS_UNKNOWN && ln.mid_n < DIP_MIN_SAMPLES, "two spots asleep: no verdict (%d in the middle third)", ln.mid_n);

    printf("-- the nights wander: bed 22:00 / 00:30 / 23:00 / 01:30 ...\n");
    {
        nightsum_begin(TODAY);
        static const int bed_min[7]  = { 22 * 60, 23 * 60 + 30, 23 * 60, 21 * 60 + 30, 22 * 60 + 30, 23 * 60 + 30, 22 * 60 };
        static const int wake_min[7] = { 6 * 60, 8 * 60, 7 * 60, 6 * 60, 7 * 60 + 30, 8 * 60 + 30, 6 * 60 };
        for (int d = 8; d <= 15; d++) {
            profile_t q = plain(d == 8 ? 0 : wake_min[d - 9], d == 15 ? 1440 : bed_min[d - 8], 70, 58);
            if (d == 8) q.wake = 0;
            feed_day(day(d), &q);
        }
        nightsum_status(st, sizeof st); printf("       %s\n", st);
        bool w = nightsum_wander(&range, &nn);
        CHECK(w && nn == 7 && range > STEADY_WANDER_MIN, "the midpoints range %d min over %d nights: WANDER", range, nn);
        CHECK(strstr(st, "WANDER") != NULL, "`night` says so");
    }
    {
        nightsum_begin(TODAY);
        static const int bed_min[7]  = { 23 * 60, 23 * 60 + 20, 23 * 60 + 40, 23 * 60, 23 * 60 + 30, 23 * 60 + 10, 23 * 60 };
        static const int wake_min[7] = { 7 * 60, 7 * 60 + 10, 7 * 60 + 20, 7 * 60, 7 * 60 + 30, 7 * 60 + 10, 7 * 60 };
        for (int d = 8; d <= 15; d++) {
            profile_t q = plain(d == 8 ? 0 : wake_min[d - 9], d == 15 ? 1440 : bed_min[d - 8], 70, 58);
            feed_day(day(d), &q);
        }
        bool w = nightsum_wander(&range, &nn);
        CHECK(!w && nn == 7 && range <= STEADY_WANDER_MIN && range > 0, "half an hour's drift (range %d min): steady", range);
    }
    {
        nightsum_begin(TODAY);
        profile_t q = plain(7 * 60, 23 * 60 + 30, 70, 58);
        for (int d = 13; d <= 15; d++) { if (d == 13) q.wake = 0; else q.wake = 7 * 60; if (d == 15) q.bed = 1440; else q.bed = 23 * 60 + 30; feed_day(day(d), &q); }
        /* two nights only (ending on 14 and 15), the first file's morning cut short by the walk's base */
        nightsum_wander(&range, &nn);
        CHECK(nn == 2 && !nightsum_wander(NULL, NULL), "two nights: too few, steady (%d)", nn);
    }

    printf("-- a nap by day, the shelf, the cable: not nights\n");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.nap0 = 12 * 60; p.nap1 = 15 * 60 + 30;
    feed_week(&p, 12 * 60);
    nightsum_wander(&range, &nn);
    CHECK(nn == 7 && nightsum_last_night(&ln) && ln.minutes == 450, "a 3.5 h nap at noon every day: still 7 nights, the last the real one");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.flags = 0;
    feed_week(&p, 12 * 60);
    CHECK(!nightsum_last_night(&ln), "never WORN: the shelf, no night");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.flags = WORN | CHARGING;
    feed_week(&p, 12 * 60);
    CHECK(!nightsum_last_night(&ln), "on the cable all night: the dock, no night");

    printf("-- the toss and the gap\n");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.toss_at = 3 * 60; p.toss_len = 10;
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.minutes == 450, "a 10 min toss at 03:00 forgiven: the night whole (%d min)", ln.minutes);
    p.toss_len = 20;
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.minutes == 220, "a 20 min toss cuts it at 03:00: the longer piece (03:20 to 07:00) kept (%d min)", ln.minutes);
    p = plain(7 * 60, 23 * 60 + 30, 70, 58); p.gap0 = 3 * 60; p.gap1 = 3 * 60 + 30;
    feed_week(&p, 12 * 60);
    CHECK(nightsum_last_night(&ln) && ln.minutes == 210, "the watch off 03:00-03:30: the run cut in two of 3.5 h, one kept (%d min)", ln.minutes);

    printf("-- UNDER THE WEATHER: a week resting at 63, today at 71\n");
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 23 * 60 + 30, 63, 55); feed_day(day(d), &q); }
    { profile_t q = plain(7 * 60, 1440, 71, 55); for (int m = 0; m < 12 * 60; m++) { bool asleep = m < q.wake; nightsum_row(day(15), m, asleep ? 0 : 40, asleep ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : asleep ? 55 : 71), quiet_at(m)); } }
    nightsum_status(st, sizeof st); printf("       %s\n", st);
    CHECK(nightsum_weather(&th, &nh) && th == 71 && nh == 63, "resting %d vs the week's %d: under the weather", th, nh);
    CHECK(nightsum_day_hr(1) == 63 && nightsum_day_hr(7) == 63 && nightsum_day_hr(8) == 0, "the day medians (yesterday %d, a week ago %d, before the walk %d)", nightsum_day_hr(1), nightsum_day_hr(7), nightsum_day_hr(8));
    CHECK(strstr(st, "UNDER THE WEATHER") != NULL, "`night` says so");
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 23 * 60 + 30, 63, 55); feed_day(day(d), &q); }
    { for (int m = 0; m < 12 * 60; m++) { bool asleep = m < 7 * 60; nightsum_row(day(15), m, asleep ? 0 : 40, asleep ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : asleep ? 55 : 66), quiet_at(m)); } }
    CHECK(!nightsum_weather(&th, &nh) && th == 66 && nh == 63, "resting %d vs %d: under the margin, fine", th, nh);
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 23 * 60 + 30, 63, 55); feed_day(day(d), &q); }
    { for (int m = 0; m < 7 * 60 + 15; m++) { bool asleep = m < 7 * 60; nightsum_row(day(15), m, asleep ? 0 : 40, asleep ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : asleep ? 55 : 80), quiet_at(m)); } }
    CHECK(!nightsum_weather(&th, &nh) && th == 0 && nightsum_today_samples() == 3, "three spots today (%d): no median yet, fine", nightsum_today_samples());
    CHECK(nightsum_day_hr(0) == 0, "today's median 0 under WEATHER_MIN_SAMPLES");
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 23 * 60 + 30, d >= 13 ? 63 : 0, 0); feed_day(day(d), &q); }
    { for (int m = 0; m < 12 * 60; m++) { bool asleep = m < 7 * 60; nightsum_row(day(15), m, asleep ? 0 : 40, asleep ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : asleep ? 0 : 80), quiet_at(m)); } }
    CHECK(!nightsum_weather(&th, &nh) && th == 80 && nh == 0, "two days of norm: no norm (needs %d), fine", WEATHER_MIN_DAYS);
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 23 * 60 + 30, d >= 12 ? 63 : 0, 0); feed_day(day(d), &q); }
    { for (int m = 0; m < 12 * 60; m++) { bool asleep = m < 7 * 60; nightsum_row(day(15), m, asleep ? 0 : 40, asleep ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : asleep ? 0 : 80), quiet_at(m)); } }
    CHECK(nightsum_weather(&th, &nh) && nh == 63, "three days of norm: the norm stands (%d)", nh);
    printf("-- the night's spots stay out of the day's median (an evening at 90 on the couch, the night at 55)\n");
    nightsum_begin(TODAY);
    for (int d = 8; d <= 14; d++) { profile_t q = plain(7 * 60, 20 * 60, 90, 55); feed_day(day(d), &q); }   /* bed at 20:00: eleven hours still, seven of them before the quiet hours */
    CHECK(nightsum_day_hr(1) == 90 && nightsum_last_night(&ln) && ln.minutes == 660, "yesterday's resting median %d (the night's 55 kept out), the night 11 h", nightsum_day_hr(1));

    printf("-- a row fed twice, and the live day after the walk\n");
    p = plain(7 * 60, 23 * 60 + 30, 70, 58);
    feed_week(&p, 12 * 60);
    nightsum_status(st, sizeof st);
    char before[400]; strcpy(before, st);
    nightsum_row(day(15), 11 * 60 + 59, 40, 200, WORN, 0, 0, 0, false);     /* the walk's last minute, again */
    nightsum_row(day(15), 11 * 60, 40, 200, WORN, 0, 0, 0, false);          /* an older one */
    nightsum_status(st, sizeof st);
    CHECK(!strcmp(before, st), "the same line: nothing changed");
    for (int m = 12 * 60; m < 1440; m++) nightsum_row(day(15), m, m >= 23 * 60 + 30 ? 0 : 40, m >= 23 * 60 + 30 ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : m >= 23 * 60 + 30 ? 58 : 70), quiet_at(m));
    for (int m = 0; m < 9 * 60; m++) nightsum_row(day(16), m, m < 7 * 60 ? 0 : 40, m < 7 * 60 ? 5 : 200, WORN, 0, 0, (uint8_t)(m % 5 ? 0 : m < 7 * 60 ? 58 : 70), quiet_at(m));
    CHECK(nightsum_last_night(&ln) && ln.end_abs / 1440 == nightsum_ordinal(day(16)) && ln.dip == NS_DEEP, "the night lived past the walk is the newest, deep");
    nightsum_wander(&range, &nn);
    CHECK(nn == 7, "still seven judged (the newest seven)");
    CHECK(nightsum_weather(&th, &nh) == false && nightsum_day_hr(1) == 70, "the day closed at midnight: yesterday's median %d", nightsum_day_hr(1));

    printf("-- no clock: nothing read\n");
    nightsum_begin(0);
    nightsum_row(day(15), 100, 0, 5, WORN, 0, 0, 60, false);
    nightsum_status(st, sizeof st); printf("       %s\n", st);
    CHECK(!nightsum_last_night(&ln) && strstr(st, "no clock"), "nothing");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
