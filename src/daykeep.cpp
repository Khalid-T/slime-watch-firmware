/* DISK KEEPING: the decision; THE STATURE WEEK: the drift. See daykeep.h. */
#include "daykeep.h"
#include <string.h>

static void sort_by_date(daykeep_file_t *f, int n)
{
    /* insertion sort: at most a few hundred files, once a day */
    for (int i = 1; i < n; i++) {
        daykeep_file_t t = f[i];
        int j = i;
        while (j > 0 && f[j - 1].date > t.date) { f[j] = f[j - 1]; j--; }
        f[j] = t;
    }
}

static bool over(uint32_t used, uint32_t total)
{
    if (!total) return false;
    return (uint64_t)used * 100 > (uint64_t)total * DAY_PRUNE_PCT;
}

int daykeep_decide(daykeep_file_t *files, int n, uint32_t used, uint32_t total, uint32_t today,
                   uint8_t *verdict)
{
    memset(verdict, DAYKEEP_KEEP, (size_t)n);
    if (n <= 0) return 0;
    sort_by_date(files, n);
    int gone = 0;
    /* the keep: the newest DAY_KEEP files; everything before it is old */
    int old_n = n > DAY_KEEP ? n - DAY_KEEP : 0;
    /* 1. old and synced: the desk has it, oldest first */
    for (int i = 0; i < old_n; i++) {
        if (!files[i].synced || files[i].date >= today) continue;
        verdict[i] = DAYKEEP_OLD_SYNCED;
        used = used > files[i].size ? used - files[i].size : 0;
        gone++;
    }
    /* 2. the disk past DAY_PRUNE_PCT: the oldest goes regardless -- old
     * unsynced ones first (they are beyond the keep), then, if the disk is
     * still full, into the keep itself; never today's */
    while (over(used, total)) {
        int pick = -1;
        for (int i = 0; i < n; i++) {
            if (verdict[i] != DAYKEEP_KEEP || files[i].date >= today) continue;
            pick = i;
            break;
        }
        if (pick < 0) break;
        verdict[pick] = DAYKEEP_DISK_FULL;
        used = used > files[pick].size ? used - files[pick].size : 0;
        gone++;
    }
    return gone;
}

/* --- THE STATURE WEEK ------------------------------------------------------- */

/* days since 1970-01-01 for a civil date (Howard Hinnant's days_from_civil) */
static int32_t civil_days(uint32_t yyyymmdd)
{
    int32_t y = (int32_t)(yyyymmdd / 10000), m = (int32_t)(yyyymmdd / 100 % 100), d = (int32_t)(yyyymmdd % 100);
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int32_t daykeep_days_between(uint32_t from, uint32_t to)
{
    return civil_days(to) - civil_days(from);
}

/* face.cpp's norm, the same rule: the median of the STATURE_NORM_DAYS
 * newest totals from ``from`` on (newest first), shelf days left out;
 * 0 = no norm yet */
static uint32_t norm_from(const uint32_t *totals, int n, int from)
{
    uint32_t v[STATURE_NORM_DAYS];
    int m = 0;
    for (int i = from; i < n && m < STATURE_NORM_DAYS; i++)
        if (totals[i] >= STATURE_MIN_STEPS) v[m++] = totals[i];
    if (m < STATURE_NORM_MIN_DAYS) return 0;
    for (int i = 1; i < m; i++) {                      /* a sort of seven */
        uint32_t x = v[i]; int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    uint32_t med = (m & 1) ? v[m / 2] : (v[m / 2 - 1] + v[m / 2]) / 2;
    return med < STATURE_MIN_STEPS ? STATURE_MIN_STEPS : med;
}

/* is file i one of the week's (the calendar days 1..STATURE_DAYS
 * before today; the newest STATURE_DAYS files with no clock)? -1 = it
 * is today's or later (skip), 0 = older than the week, 1 = the week's */
static int in_week(const uint32_t *dates, int i, uint32_t today)
{
    if (!today) return i < STATURE_DAYS ? 1 : 0;
    int32_t ago = daykeep_days_between(dates[i], today);
    if (ago < 1) return -1;
    return ago <= STATURE_DAYS ? 1 : 0;
}

float daykeep_stature(const uint32_t *dates, const uint32_t *totals, int n, uint32_t today,
                      int *over, int *under)
{
    int up = 0, down = 0;
    /* the norm that stood before the week: the newest files older than it */
    int first_old = n;
    for (int i = 0; i < n; i++) if (in_week(dates, i, today) == 0) { first_old = i; break; }
    uint32_t norm = norm_from(totals, n, first_old);
    if (norm) {
        for (int i = 0; i < first_old; i++) {
            if (in_week(dates, i, today) != 1) continue;    /* today's own file, or later: not the week's */
            if (totals[i] < STATURE_MIN_STEPS) continue;    /* a shelf day: not his day, counts nothing */
            if (totals[i] >= norm) up++;
            else if ((float)totals[i] < STATURE_LAZY * (float)norm) down++;
        }
    }
    if (over) *over = up;
    if (under) *under = down;
    float px = (float)(up - down) * STATURE_STEP_PX;
    if (px > STATURE_PX) px = STATURE_PX;
    if (px < -STATURE_PX) px = -STATURE_PX;
    return px;
}
