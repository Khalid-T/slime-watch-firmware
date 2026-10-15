/* THE NIGHT'S SUM -- nightsum.h says what; this is how. One pass over
 * rows in time order; the state is the run in hand, the reference
 * ring, the day's histogram and the week's small tables. */
#include "nightsum.h"
#include <string.h>
#include <stdio.h>

/* daylog.h's flag bits, repeated so this file has no header but its own */
#define F_WORN      0x01
#define F_CHARGING  0x04
#define F_UP        0x08

typedef struct {
    bool     on;
    uint32_t end_abs, mid_abs;
    int      minutes;
    int      dip;
    int      ref_hr, mid_hr, ref_n, mid_n;
} night_t;

static struct {
    uint32_t today;             /* the walk's date, 0 = none */
    uint32_t base_ord;          /* the ordinal of today - 7: day slot 0 */
    bool     have_last;
    uint32_t last_abs;          /* the last row fed */
    int      cur_di;            /* the open day's slot (-1 = none yet) */
    /* the reference: the last DIP_REF_MIN rows' hr, by minute (0 = none) */
    uint8_t  ref[DIP_REF_MIN];
    /* the run in hand */
    bool     run_on;
    uint32_t run_start, run_last_still;
    int      run_rows, toss;
    bool     run_worn_late, run_cable, run_quiet;
    int      run_ref_hr, run_ref_n;             /* the reference median, taken as the run began */
    uint8_t  bins[NS_RUN_BINS];                 /* the run's hr, one per NS_BIN_MIN (0 = none) */
    /* the day: its awake-rest samples */
    uint16_t day_hist[NS_HR_MAX];
    int      day_n;
    /* the week */
    uint8_t  day_hr[NS_DAYS];                   /* each closed day's resting median (0 = none / too few) */
    night_t  nights[NS_DAYS];                   /* the longest night that ended on each day */
    int      nights_found, runs_seen;
} N;

/* --- the calendar ------------------------------------------------------------ */
uint32_t nightsum_ordinal(uint32_t yyyymmdd)
{
    int32_t y = (int32_t)(yyyymmdd / 10000), m = (int32_t)(yyyymmdd / 100 % 100), d = (int32_t)(yyyymmdd % 100);
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (uint32_t)(era * 146097 + doe - 719468);
}

/* --- the medians ------------------------------------------------------------ */
/* the median of a histogram of ``n`` samples over NS_HR_MAX bins (the
 * lower middle for an even count: a bpm, never a half) */
static int median_hist(const uint16_t *h, int n)
{
    if (n <= 0) return 0;
    int at = (n - 1) / 2, seen = 0;
    for (int v = 0; v < NS_HR_MAX; v++) {
        seen += h[v];
        if (seen > at) return v;
    }
    return 0;
}

/* the median of the non-zero bytes in v[0..n) through a counting pass */
static int median_bytes(const uint8_t *v, int n, int *count)
{
    uint16_t h[NS_HR_MAX];
    memset(h, 0, sizeof h);
    int m = 0;
    for (int i = 0; i < n; i++) if (v[i]) { h[v[i] >= NS_HR_MAX ? NS_HR_MAX - 1 : v[i]]++; m++; }
    if (count) *count = m;
    return median_hist(h, m);
}

/* --- the day ---------------------------------------------------------------- */
static void day_sample(uint8_t hr)
{
    if (!hr) return;
    N.day_hist[hr >= NS_HR_MAX ? NS_HR_MAX - 1 : hr]++;
    N.day_n++;
}

static void day_close(void)
{
    if (N.cur_di >= 0 && N.cur_di < NS_DAYS)
        N.day_hr[N.cur_di] = (uint8_t)(N.day_n >= WEATHER_MIN_SAMPLES ? median_hist(N.day_hist, N.day_n) : 0);
    memset(N.day_hist, 0, sizeof N.day_hist);
    N.day_n = 0;
}

/* --- the run ---------------------------------------------------------------- */
static void run_begin(uint32_t abs, uint8_t flags, bool quiet)
{
    N.run_on = true;
    N.run_start = N.run_last_still = abs;
    N.run_rows = 0; N.toss = 0;
    N.run_worn_late = false;
    N.run_cable = (flags & F_CHARGING) != 0;
    N.run_quiet = quiet;
    N.run_ref_hr = median_bytes(N.ref, DIP_REF_MIN, &N.run_ref_n);
    memset(N.bins, 0, sizeof N.bins);
}

static void run_end(void)
{
    N.run_on = false;
    N.runs_seen++;
    int minutes = (int)(N.run_last_still - N.run_start) + 1;
    bool night = minutes >= NS_NIGHT_MIN && N.run_quiet && N.run_worn_late && !N.run_cable;
    if (!night) {
        /* his rest by day, or the shelf: the spots were resting ones either way -- the day's */
        for (int i = 0; i < NS_RUN_BINS; i++) day_sample(N.bins[i]);
        return;
    }
    night_t n;
    memset(&n, 0, sizeof n);
    n.on = true;
    n.end_abs = N.run_last_still;
    n.mid_abs = N.run_start + (uint32_t)(minutes / 2);
    n.minutes = minutes;
    /* the dip: the middle third's median against the evening's */
    int nb = (minutes + NS_BIN_MIN - 1) / NS_BIN_MIN;
    if (nb > NS_RUN_BINS) nb = NS_RUN_BINS;
    int b0 = nb / 3, b1 = 2 * nb / 3;
    n.mid_hr = median_bytes(N.bins + b0, b1 - b0, &n.mid_n);
    n.ref_hr = N.run_ref_hr; n.ref_n = N.run_ref_n;
    if (n.mid_n >= DIP_MIN_SAMPLES && n.ref_n >= DIP_MIN_SAMPLES)
        n.dip = (n.ref_hr - n.mid_hr >= DIP_BPM) ? NS_DEEP : NS_FLAT;
    else n.dip = NS_UNKNOWN;
    /* the day it ended on keeps its longest */
    int32_t di = (int32_t)(n.end_abs / 1440u) - (int32_t)N.base_ord;
    if (di >= 0 && di < NS_DAYS && (!N.nights[di].on || N.nights[di].minutes < n.minutes)) {
        if (!N.nights[di].on) N.nights_found++;
        N.nights[di] = n;
    }
}

/* --- the feed ---------------------------------------------------------------- */
void nightsum_begin(uint32_t today)
{
    memset(&N, 0, sizeof N);
    N.today = today;
    N.base_ord = today ? nightsum_ordinal(today) - (NS_WALK_DAYS - 1) : 0;
    N.cur_di = -1;
}

void nightsum_row(uint32_t date, int minute, uint16_t steps, uint16_t energy_mg, uint8_t flags,
                  uint8_t raises, uint8_t taps, uint8_t hr, bool quiet)
{
    if (!N.today || !date || minute < 0) return;
    uint32_t abs = nightsum_ordinal(date) * 1440u + (uint32_t)(minute % 1440);
    if (N.have_last && abs <= N.last_abs) return;             /* fed twice (the walk's last minute, then the live one) */
    if (N.have_last && abs - N.last_abs > NS_GAP_MIN) {        /* the watch was off: the run ends, the evening is forgotten */
        if (N.run_on) run_end();
        memset(N.ref, 0, sizeof N.ref);
    }
    int32_t di = (int32_t)(abs / 1440u) - (int32_t)N.base_ord;
    if (di != N.cur_di) { if (N.cur_di >= 0) day_close(); N.cur_di = di < 0 ? -1 : di; }
    bool still = energy_mg < NS_STILL_MG && steps == 0;
    if (!N.run_on) {
        if (still) run_begin(abs, flags, quiet);
        else day_sample(hr);
    }
    if (N.run_on) {
        N.run_rows++;
        if (still) { N.run_last_still = abs; N.toss = 0; }
        else N.toss++;
        if (N.run_rows > NS_WORN_AFTER && (flags & F_WORN)) N.run_worn_late = true;
        if (quiet) N.run_quiet = true;
        bool up = (flags & F_UP) || steps >= NS_STEPS_UP || (!quiet && (raises || taps)) || N.toss >= NS_TOSS_MAX_MIN;
        if (up) {
            run_end();
            if (!still) day_sample(hr);                       /* the row he got up in is the day's, not the night's */
        } else {
            uint32_t bin = (abs - N.run_start) / NS_BIN_MIN;
            if (hr && bin < NS_RUN_BINS) N.bins[bin] = hr;
        }
    }
    N.ref[abs % DIP_REF_MIN] = hr;
    N.last_abs = abs; N.have_last = true;
}

/* --- the readers -------------------------------------------------------------- */
static const night_t *newest(void)
{
    const night_t *b = NULL;
    for (int i = 0; i < NS_DAYS; i++)
        if (N.nights[i].on && (!b || N.nights[i].end_abs > b->end_abs)) b = &N.nights[i];
    return b;
}

bool nightsum_last_night(nightsum_night_t *out)
{
    const night_t *b = newest();
    if (!b) return false;
    if (out) {
        out->end_abs = b->end_abs; out->mid_abs = b->mid_abs; out->minutes = b->minutes; out->dip = b->dip;
        out->ref_hr = b->ref_hr; out->mid_hr = b->mid_hr; out->ref_n = b->ref_n; out->mid_n = b->mid_n;
    }
    return true;
}

bool nightsum_deep(uint32_t night_id)
{
    const night_t *b = newest();
    if (!b || !night_id || b->dip != NS_DEEP) return false;
    uint32_t d = b->end_abs > night_id ? b->end_abs - night_id : night_id - b->end_abs;
    return d <= NS_MATCH_MIN;
}

/* the newest STEADY_NIGHTS nights' midpoints, as minutes of the day
 * folded around the first's; the range */
bool nightsum_wander(int *range_min, int *nights)
{
    int32_t off[STEADY_NIGHTS]; int n = 0;
    uint32_t first = 0;
    for (int i = NS_DAYS - 1; i >= 0 && n < STEADY_NIGHTS; i--) {
        if (!N.nights[i].on) continue;
        uint32_t m = N.nights[i].mid_abs % 1440u;
        if (!n) first = m;
        int32_t d = (int32_t)m - (int32_t)first;
        if (d > 720) d -= 1440;
        if (d < -720) d += 1440;
        off[n++] = d;
    }
    int32_t lo = 0, hi = 0;
    for (int i = 0; i < n; i++) { if (off[i] < lo) lo = off[i]; if (off[i] > hi) hi = off[i]; }
    int range = n ? (int)(hi - lo) : 0;
    if (range_min) *range_min = range;
    if (nights) *nights = n;
    return n >= STEADY_MIN_NIGHTS && range > STEADY_WANDER_MIN;
}

/* the week's norm: the median of the previous WEATHER_DAYS days' medians */
static int week_norm(int *days)
{
    uint8_t v[WEATHER_DAYS]; int n = 0;
    for (int back = 1; back <= WEATHER_DAYS; back++) {
        int di = N.cur_di - back;
        if (di < 0 || di >= NS_DAYS) continue;
        if (N.day_hr[di]) v[n++] = N.day_hr[di];
    }
    if (days) *days = n;
    if (n < WEATHER_MIN_DAYS) return 0;
    return median_bytes(v, n, NULL);
}

bool nightsum_weather(int *today_hr, int *norm_hr)
{
    int today = N.cur_di >= 0 && N.day_n >= WEATHER_MIN_SAMPLES ? median_hist(N.day_hist, N.day_n) : 0;
    int norm = week_norm(NULL);
    if (today_hr) *today_hr = today;
    if (norm_hr) *norm_hr = norm;
    return today && norm && today - norm >= WEATHER_BPM;
}

int nightsum_day_hr(int days_ago)
{
    if (days_ago == 0) return N.day_n >= WEATHER_MIN_SAMPLES ? median_hist(N.day_hist, N.day_n) : 0;
    int di = N.cur_di - days_ago;
    return di >= 0 && di < NS_DAYS ? N.day_hr[di] : 0;
}
int nightsum_today_samples(void) { return N.day_n; }

void nightsum_status(char *o, int cap)
{
    if (!N.today) { snprintf(o, cap, "night: no clock yet, nothing read"); return; }
    int range, nn; bool wander = nightsum_wander(&range, &nn);
    int th, nh, days; bool weather = nightsum_weather(&th, &nh); week_norm(&days);
    nightsum_night_t ln; bool have = nightsum_last_night(&ln);
    char last[96];
    if (have) {
        uint32_t m = ln.mid_abs % 1440u;
        if (ln.dip == NS_UNKNOWN) snprintf(last, sizeof last, "last %.1f h, midpoint %02lu:%02lu, no dip known (%d + %d spots)",
                                           ln.minutes / 60.0f, (unsigned long)(m / 60), (unsigned long)(m % 60), ln.ref_n, ln.mid_n);
        else snprintf(last, sizeof last, "last %.1f h, midpoint %02lu:%02lu, %s (%d by evening, %d asleep)", ln.minutes / 60.0f,
                      (unsigned long)(m / 60), (unsigned long)(m % 60), ln.dip == NS_DEEP ? "DEEP: the ring in the shine" : "flat",
                      ln.ref_hr, ln.mid_hr);
    } else snprintf(last, sizeof last, "no night in the files");
    snprintf(o, cap, "night: %d night%s of %d run%s | %s | midpoints wander %d min over %d night%s: %s | resting today %s%d (%d spots) vs the week's %d (%d day%s): %s",
             N.nights_found, N.nights_found == 1 ? "" : "s", N.runs_seen, N.runs_seen == 1 ? "" : "s", last,
             range, nn, nn == 1 ? "" : "s", wander ? "WANDER: the tip breathes" : nn >= STEADY_MIN_NIGHTS ? "steady" : "too few, steady",
             th ? "" : "none yet, ", th, N.day_n, nh, days, days == 1 ? "" : "s",
             weather ? "UNDER THE WEATHER: the droopy first face" : (th && nh) ? "fine" : "no verdict (the hr bytes are 0 until the PPG writes them)");
}
