/* Host test of DISK KEEPING's decision (src/daykeep.cpp): made-up
 * disks -- a list of day files (date, size, synced) and the partition's
 * used / total -- and what the keeper says goes. And THE STATURE WEEK
 * (2026-09-23): made-up fortnights of step totals -> the drift in art
 * px (the median -> the drift).
 * Build: c++ -std=gnu++17 -I src -o daykeep_test tools/daykeep_test.cpp src/daykeep.cpp
 * Run:   ./daykeep_test
 */
#include "../src/daykeep.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const uint32_t TOTAL = 3456u * 1024u;   /* the 3.4 MB LittleFS */
static const uint32_t DAY_B = 23u * 1024u;     /* ~23 KB a day */
static const uint32_t TODAY = 20260910;

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* a run of ``n`` days ending today, YYYYMMDD by a plain day count
 * (September has 30: the dates step back a month at 1) */
static uint32_t date_back(int days_ago)
{
    int y = 2026, m = 9, d = 10;
    static const int ML[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    while (days_ago-- > 0) {
        if (--d < 1) { if (--m < 1) { m = 12; y--; } d = ML[m]; }
    }
    return (uint32_t)(y * 10000 + m * 100 + d);
}

static daykeep_file_t files[300];
static uint8_t verdict[300];

static int fill(int n, uint32_t size)
{
    for (int i = 0; i < n; i++) { files[i].date = date_back(n - 1 - i); files[i].size = size; files[i].synced = true; }
    return n;
}
static uint32_t sum_sizes(int n) { uint32_t s = 0; for (int i = 0; i < n; i++) s += files[i].size; return s; }
static int count_verdict(int n, int v) { int c = 0; for (int i = 0; i < n; i++) if (verdict[i] == v) c++; return c; }
static uint32_t oldest_kept(int n) { for (int i = 0; i < n; i++) if (verdict[i] == DAYKEEP_KEEP) return files[i].date; return 0; }

int main(void)
{
    /* a young watch: 3 files, 36 KB -- nothing goes */
    int n = fill(3, 12 * 1024);
    int gone = daykeep_decide(files, n, sum_sizes(n), TOTAL, TODAY, verdict);
    check("3 files, 36 KB: nothing goes", gone == 0);

    /* 120 files exactly, all synced: the keep is whole */
    n = fill(DAY_KEEP, DAY_B);
    gone = daykeep_decide(files, n, sum_sizes(n), TOTAL, TODAY, verdict);
    check("120 files, all synced, 2.7 MB (78 %): nothing goes", gone == 0);

    /* 130 files, all synced: the oldest 10 go, oldest first */
    n = fill(130, DAY_B);
    gone = daykeep_decide(files, n, sum_sizes(n), TOTAL, TODAY, verdict);
    check("130 synced: 10 go", gone == 10 && count_verdict(n, DAYKEEP_OLD_SYNCED) == 10);
    check("... the oldest 10", verdict[0] == DAYKEEP_OLD_SYNCED && verdict[9] == DAYKEEP_OLD_SYNCED && verdict[10] == DAYKEEP_KEEP);
    check("... as 'older than the keep', not 'the disk was full'", count_verdict(n, DAYKEEP_DISK_FULL) == 0);
    check("... the oldest kept is the 120th newest", oldest_kept(n) == date_back(119));

    /* the list arrives in any order: the verdicts follow the dates */
    n = fill(130, DAY_B);
    for (int i = 0; i < 65; i++) { daykeep_file_t t = files[i]; files[i] = files[129 - i]; files[129 - i] = t; }
    gone = daykeep_decide(files, n, sum_sizes(n), TOTAL, TODAY, verdict);
    check("reversed input: sorted, the same 10 oldest go", gone == 10 && files[0].date == date_back(129) && verdict[0] == DAYKEEP_OLD_SYNCED && verdict[10] == DAYKEEP_KEEP);

    /* 130 files, the oldest 10 unsynced, the disk at 50 %: they wait for the desk */
    n = fill(130, DAY_B);
    for (int i = 0; i < 10; i++) files[i].synced = false;
    gone = daykeep_decide(files, n, (uint32_t)(TOTAL * 0.50), TOTAL, TODAY, verdict);
    check("10 old unsynced at 50 %: kept (the only copy)", gone == 0);

    /* the same, the disk at 90 %: the oldest go regardless, only as many as bring it under 85 % */
    n = fill(130, DAY_B);
    for (int i = 0; i < 10; i++) files[i].synced = false;
    uint32_t used = (uint32_t)((uint64_t)TOTAL * 90 / 100);
    gone = daykeep_decide(files, n, used, TOTAL, TODAY, verdict);
    /* 90 % - 85 % = 5 % of 3456 KB = 172.8 KB -> 8 files of 23 KB */
    check("10 old unsynced at 90 %: 8 go, marked 'the disk was full'", gone == 8 && count_verdict(n, DAYKEEP_DISK_FULL) == 8);
    check("... the oldest 8, the two newer of them kept", verdict[0] == DAYKEEP_DISK_FULL && verdict[7] == DAYKEEP_DISK_FULL && verdict[8] == DAYKEEP_KEEP && verdict[9] == DAYKEEP_KEEP);

    /* mixed old files at 90 %: the synced ones go first (as old), then the unsynced for the disk */
    n = fill(130, DAY_B);
    for (int i = 0; i < 10; i++) files[i].synced = (i % 2) == 1;   /* 5 unsynced (even), 5 synced (odd) */
    gone = daykeep_decide(files, n, used, TOTAL, TODAY, verdict);
    check("5 synced + 5 unsynced old at 90 %: the 5 synced as old, 3 unsynced for the disk", gone == 8 && count_verdict(n, DAYKEEP_OLD_SYNCED) == 5 && count_verdict(n, DAYKEEP_DISK_FULL) == 3);
    check("... the unsynced dropped are the oldest three (0, 2, 4), 6 and 8 kept", verdict[0] == DAYKEEP_DISK_FULL && verdict[2] == DAYKEEP_DISK_FULL && verdict[4] == DAYKEEP_DISK_FULL && verdict[6] == DAYKEEP_KEEP && verdict[8] == DAYKEEP_KEEP);

    /* the keep itself is eaten only when the disk is full and nothing older is left */
    n = fill(100, DAY_B);
    for (int i = 0; i < n; i++) files[i].synced = false;
    used = (uint32_t)((uint64_t)TOTAL * 95 / 100);
    gone = daykeep_decide(files, n, used, TOTAL, TODAY, verdict);
    /* 10 % of 3456 KB = 345.6 KB -> 16 files */
    check("100 unsynced (inside the keep) at 95 %: 16 of the oldest go for the disk", gone == 16 && count_verdict(n, DAYKEEP_DISK_FULL) == 16 && verdict[0] == DAYKEEP_DISK_FULL && verdict[15] == DAYKEEP_DISK_FULL && verdict[16] == DAYKEEP_KEEP);

    /* today's file never goes */
    n = fill(1, DAY_B);
    gone = daykeep_decide(files, n, (uint32_t)((uint64_t)TOTAL * 99 / 100), TOTAL, TODAY, verdict);
    check("one file, today's, the disk full of something else: kept", gone == 0);
    n = fill(130, DAY_B);
    gone = daykeep_decide(files, n, TOTAL, TOTAL, TODAY, verdict);
    /* 10 old synced (230 KB) bring 100 % to 93.3 %; then 13 more of 23 KB bring it under 85 % */
    check("130 synced with the disk brim full: 10 as old, 13 more for the disk, then it stops", gone == 23 && count_verdict(n, DAYKEEP_OLD_SYNCED) == 10 && count_verdict(n, DAYKEEP_DISK_FULL) == 13);
    n = fill(130, 1);                               /* tiny files: removing them never brings the disk under */
    gone = daykeep_decide(files, n, TOTAL, TOTAL, TODAY, verdict);
    check("the disk brim full of something else: everything but today goes, today stays", verdict[129] == DAYKEEP_KEEP && files[129].date == TODAY && gone == 129);

    /* no files, no partition size: nothing, no division by zero */
    gone = daykeep_decide(files, 0, 0, 0, TODAY, verdict);
    check("nothing on disk: nothing", gone == 0);
    n = fill(130, DAY_B);
    gone = daykeep_decide(files, n, sum_sizes(n), 0, TODAY, verdict);
    check("a total of 0 (no filesystem numbers): only the keep rule runs", gone == 10 && count_verdict(n, DAYKEEP_DISK_FULL) == 0);

    /* --- THE STATURE WEEK: the median -> the drift ------------------------- */
    puts("-- the stature week");
    {
        uint32_t dd[16], tt[16];
        int over = 0, under = 0;
        const float STEP = STATURE_STEP_PX;
        /* a fortnight, newest first: the week judged (days 1-7) and the
         * week its norms come from (8-14) */
        #define WEEKS(a1,a2,a3,a4,a5,a6,a7, b1,b2,b3,b4,b5,b6,b7) do { \
            const uint32_t v[14] = { a1,a2,a3,a4,a5,a6,a7, b1,b2,b3,b4,b5,b6,b7 }; \
            for (int i = 0; i < 14; i++) { dd[i] = date_back(i + 1); tt[i] = v[i]; } } while (0)
        check("the civil calendar: 2026-09-10 is 9 days after 2026-09-01", daykeep_days_between(20260901, 20260910) == 9);
        check("... and 10 days after 2026-08-31 (across the month)", daykeep_days_between(20260831, 20260910) == 10);
        check("... 2026-01-01 is 1 day after 2025-12-31 (across the year)", daykeep_days_between(20251231, 20260101) == 1);
        check("... backwards is negative", daykeep_days_between(20260910, 20260901) == -9);
        check("... 2024-03-01 is 1 day after 2024-02-29 (the leap)", daykeep_days_between(20240229, 20240301) == 1);

        /* no files: no drift */
        check("no files: 0 px", daykeep_stature(dd, tt, 0, TODAY, &over, &under) == 0.0f && over == 0 && under == 0);

        /* a steady week at 6000 on a steady week at 6000: every day AT the
         * norm counts over (the desk's >= 1.0) -> seven steps = the cap;
         * the norm is the one that stood BEFORE the week (the median of
         * the seven files older than it), every day judged against it */
        WEEKS(6000,6000,6000,6000,6000,6000,6000, 6000,6000,6000,6000,6000,6000,6000);
        float px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a steady week at the norm: 7 over, 0 under, +0.25 px (the cap)", over == 7 && under == 0 && fabsf(px - STATURE_PX) < 1e-6f);
        check("... STATURE_STEP_PX x 7 IS the quarter pixel", fabsf(7 * STEP - STATURE_PX) < 1e-6f);

        /* a week of walks on a lazy week before it: taller, capped */
        WEEKS(9000,9500,8000,10000,9000,8500,9000, 4000,4200,3800,4100,4000,3900,4000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a week of walks over a lazy week: +0.25 px, 7 over", fabsf(px - STATURE_PX) < 1e-6f && over == 7);

        /* a lazy week (under half) on a week of walks: rounder, capped */
        WEEKS(1000,1200,900,1100,1000,800,1000, 9000,9500,8000,10000,9000,8500,9000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a lazy week under a week of walks: -0.25 px, 7 under", fabsf(px + STATURE_PX) < 1e-6f && under == 7 && over == 0);
        check("... the drift is symmetric (one STATURE_STEP_PX both ways)", fabsf(px) == fabsf(STATURE_PX));

        /* the middle band: 60-99 % of the norm moves nothing */
        WEEKS(5000,5500,4500,5900,5000,4000,5000, 6000,6000,6000,6000,6000,6000,6000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a week between lazy and the norm (60-99 %): nothing moves", px == 0.0f && over == 0 && under == 0);

        /* mixed: the days over and under against the one norm, the sum in steps */
        WEEKS(7000,2000,6000,6000,6000,6000,6000, 6000,6000,6000,6000,6000,6000,6000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        /* the norm 6000; day 1 7000 over, day 2 2000 under (< 3000), days 3-7 at it: over -> 6 over, 1 under = 5 steps */
        check("a mixed week: 6 over, 1 under -> 5 steps", over == 6 && under == 1 && fabsf(px - 5 * STEP) < 1e-6f);
        /* the week's own walks never raise the norm they are judged by:
         * a week climbing out of a slump is a week over it, every day */
        WEEKS(9000,8000,7000,6000,5000,4000,3000, 2500,2500,2500,2500,2500,2500,2500);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a week climbing out of a slump (3000 -> 9000 over a 2500 norm): 7 over", over == 7 && under == 0);
        /* the norm is a median: one wild day before the week does not move it */
        WEEKS(6000,6000,6000,6000,6000,6000,6000, 6000,6000,6000,40000,6000,6000,6000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a 40000-step day in the norm's week: the median still 6000, 7 over", over == 7);

        /* a shelf day (under 200 steps) counts nothing, either way */
        WEEKS(0,150,6000,6000,6000,6000,6000, 6000,6000,6000,6000,6000,6000,6000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("two shelf days in the week: not lazy, not counted (5 over, 0 under)", over == 5 && under == 0 && fabsf(px - 5 * STEP) < 1e-6f);
        /* ... and shelf days are left out of a norm too (face.cpp's rule) */
        WEEKS(6000,6000,6000,6000,6000,6000,6000, 0,0,0,0,6000,6000,6000);
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("shelf days in the norm's week are left out: the norm 6000 from the 3 real days, 7 over", over == 7);

        /* a young watch: no norm before the week until 3 real days stand there */
        for (int i = 0; i < 16; i++) { dd[i] = date_back(i + 1); tt[i] = 8000; }
        px = daykeep_stature(dd, tt, 7, TODAY, &over, &under);
        check("7 files, all the week's: no norm before it -> nothing", over == 0 && under == 0 && px == 0.0f);
        px = daykeep_stature(dd, tt, 9, TODAY, &over, &under);
        check("9 files: 2 before the week -> not a norm yet, nothing", over == 0 && px == 0.0f);
        px = daykeep_stature(dd, tt, 10, TODAY, &over, &under);
        check("10 files at 8000: 3 before the week are its norm -> 7 over, the cap", over == 7 && fabsf(px - STATURE_PX) < 1e-6f);

        /* only the calendar week before today is judged: older files are norm, not week */
        for (int i = 0; i < 16; i++) { dd[i] = date_back(i + 1); tt[i] = 8000; }
        for (int i = 7; i < 16; i++) tt[i] = 2000;        /* days 8-16: lazy; days 1-7: walks */
        px = daykeep_stature(dd, tt, 16, TODAY, &over, &under);
        check("16 files, the older 9 lazy: only days 1-7 are judged (7 over), the lazy ones are their norm", over == 7 && under == 0);
        /* a gap: files missing inside the week count nothing, the days that exist are judged */
        int m = 0;
        for (int back = 1; back <= 14; back++) { if (back == 2 || back == 5) continue; dd[m] = date_back(back); tt[m] = 8000; m++; }
        px = daykeep_stature(dd, tt, m, TODAY, &over, &under);
        check("two days missing in the week: 5 judged, all over -> 5 steps", over == 5 && under == 0 && fabsf(px - 5 * STEP) < 1e-6f);
        /* today's own file never comes (wrist_recent_days excludes it); a file dated today or later is skipped anyway */
        dd[0] = TODAY; tt[0] = 20000;
        for (int i = 1; i < 14; i++) { dd[i] = date_back(i); tt[i] = 6000; }
        px = daykeep_stature(dd, tt, 14, TODAY, &over, &under);
        check("a file dated today is not the week's", over == 7 && under == 0);

        /* no clock (today 0): the newest 7 files stand for the week */
        WEEKS(9000,9000,9000,9000,9000,9000,9000, 4000,4000,4000,4000,4000,4000,4000);
        px = daykeep_stature(dd, tt, 14, 0, &over, &under);
        check("no clock: the newest 7 files are the week -> 7 over", over == 7 && fabsf(px - STATURE_PX) < 1e-6f);

        /* the counts may be left unasked */
        check("NULL for the counts is fine", fabsf(daykeep_stature(dd, tt, 14, 0, NULL, NULL) - STATURE_PX) < 1e-6f);
        #undef WEEKS
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
