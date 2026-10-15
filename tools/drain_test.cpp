/* THE EAR'S LEDGER on the laptop (drain.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/drain_test tools/drain_test.cpp -x c src/drain.c && /tmp/drain_test
 * Synthetic minutes: a deep hour at a known slope and an awake-ear hour
 * at a steeper one come back as those slopes (least squares, mV/h);
 * the cable's minutes are left out of every line; under DRAIN_MIN_MIN
 * minutes of a kind the slope is `--`; the window forgets the hour
 * before; the ADC's wobble does not move the line far; the kind of a
 * minute follows the ms it spent (half the minute or more in a nap
 * kind, else lit); the day's tally turns with the date, the boot's
 * does not; the text line's shape. */
#include "drain.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

/* ``n`` minutes of ``kind`` from ``mv0`` at ``mv_per_h`` (negative drains), a wobble pattern on top */
static void feed(drain_t *d, uint32_t date, int kind, int n, double mv0, double mv_per_h, bool cable, int wobble_mv)
{
    for (int i = 0; i < n; i++) {
        double mv = mv0 + mv_per_h * i / 60.0;
        int w = wobble_mv ? ((i % 3) - 1) * wobble_mv : 0;   /* -w, 0, +w, ... : mean zero */
        drain_minute(d, date, (uint16_t)(mv + w + 0.5), cable, kind);
    }
}

int main(void)
{
    drain_t d;
    float s; int n;

    /* 1. the kind of a minute */
    CHECK(drain_kind_of(0, 0) == DRAIN_LIT);
    CHECK(drain_kind_of(60000, 0) == DRAIN_DEEP);
    CHECK(drain_kind_of(0, 60000) == DRAIN_EAR);
    CHECK(drain_kind_of(29999, 0) == DRAIN_LIT);         /* under half the minute asleep: the glass had it */
    CHECK(drain_kind_of(30000, 0) == DRAIN_DEEP);
    CHECK(drain_kind_of(0, 30000) == DRAIN_EAR);
    CHECK(drain_kind_of(40000, 20000) == DRAIN_DEEP);    /* the larger share names it */
    CHECK(drain_kind_of(20000, 40000) == DRAIN_EAR);
    CHECK(drain_kind_of(30000, 30000) == DRAIN_EAR);     /* a tie goes to the ear (the costlier kind is the one to see) */
    CHECK(!strcmp(drain_kind_name(DRAIN_DEEP), "deep") && !strcmp(drain_kind_name(DRAIN_EAR), "awake") && !strcmp(drain_kind_name(DRAIN_LIT), "lit"));

    /* 2. empty: nothing speaks */
    drain_begin(&d);
    CHECK(!drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 0 && s == 0);
    CHECK(!drain_slope(&d, DRAIN_EAR, &s, &n) && n == 0);
    char line[200];
    drain_text(&d, line, sizeof line);
    CHECK(!strcmp(line, "ear: deep 0 min / awake 0 min today (since boot 0 / 0, lit 0); drain deep -- (0 of 20 min), awake -- (0 of 20 min)"));

    /* 3. a deep hour at -18 mV/h: the slope comes back */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 60, 3900, -18, false, 0);
    CHECK(drain_slope(&d, DRAIN_DEEP, &s, &n));
    CHECK(n == 60);
    CHECK(NEAR(s, -18, 1.5));                            /* the mV rounding of 0.3 mV steps leaves a hair */
    CHECK(!drain_slope(&d, DRAIN_EAR, &s, &n) && n == 0);
    CHECK(d.today_min[DRAIN_DEEP] == 60 && d.today_min[DRAIN_EAR] == 0 && d.boot_min[DRAIN_DEEP] == 60);
    CHECK(d.last_kind == DRAIN_DEEP);

    /* 4. an awake-ear hour after it at -41 mV/h: the window is the ear's now, the deep hour forgotten */
    feed(&d, 20261009, DRAIN_EAR, 60, 3882, -41, false, 0);
    CHECK(drain_slope(&d, DRAIN_EAR, &s, &n) && n == 60);
    CHECK(NEAR(s, -41, 1.5));
    CHECK(!drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 0);   /* the window forgot the deep hour */
    CHECK(d.today_min[DRAIN_DEEP] == 60 && d.today_min[DRAIN_EAR] == 60);   /* the tally did not */
    CHECK(d.boot_min[DRAIN_DEEP] == 60 && d.boot_min[DRAIN_EAR] == 60);

    /* 5. both kinds inside one window: each its own line */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 30, 3900, -18, false, 0);
    feed(&d, 20261009, DRAIN_EAR, 30, 3891, -41, false, 0);
    CHECK(drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 30 && NEAR(s, -18, 2.5));
    CHECK(drain_slope(&d, DRAIN_EAR, &s, &n) && n == 30 && NEAR(s, -41, 2.5));

    /* 6. too few minutes: `--`, with the count said */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_EAR, 19, 3900, -41, false, 0);
    CHECK(!drain_slope(&d, DRAIN_EAR, &s, &n) && n == 19 && s == 0);
    drain_text(&d, line, sizeof line);
    CHECK(strstr(line, "awake -- (19 of 20 min)") != NULL);
    feed(&d, 20261009, DRAIN_EAR, 1, 3887, -41, false, 0);
    CHECK(drain_slope(&d, DRAIN_EAR, &s, &n) && n == 20);   /* the twentieth minute: it speaks */

    /* 7. the cable's minutes are no part of any line (and the charge's rise does not read as a negative drain) */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 30, 3900, -18, false, 0);
    feed(&d, 20261009, DRAIN_DEEP, 20, 3900, +600, true, 0);   /* on the cable, climbing */
    CHECK(drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 30 && NEAR(s, -18, 2.5));
    CHECK(d.today_min[DRAIN_DEEP] == 50);                     /* the tally counts the minutes, cable or not */
    feed(&d, 20261009, DRAIN_DEEP, 25, 3900, +600, true, 0);  /* 45 cable minutes: 15 off-cable left in the window */
    CHECK(!drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 15);

    /* 8. lit minutes are neither kind */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_LIT, 60, 3900, -120, false, 0);
    CHECK(!drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 0);
    CHECK(!drain_slope(&d, DRAIN_EAR, &s, &n) && n == 0);
    CHECK(d.boot_min[DRAIN_LIT] == 60);
    CHECK(d.last_kind == DRAIN_LIT);

    /* 9. the ADC's wobble (+-10 mV, mean zero) leaves the line near the truth */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_EAR, 60, 3900, -41, false, 10);
    CHECK(drain_slope(&d, DRAIN_EAR, &s, &n) && n == 60 && NEAR(s, -41, 4));

    /* 10. a flat cell: the slope is 0, not `--` */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 40, 3900, 0, false, 0);
    CHECK(drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 40 && s == 0);

    /* 11. the date's turn: today's tally starts over, the boot's goes on; no clock (0) turns nothing */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 10, 3900, -18, false, 0);
    feed(&d, 0, DRAIN_EAR, 5, 3897, -41, false, 0);
    CHECK(d.today_min[DRAIN_DEEP] == 10 && d.today_min[DRAIN_EAR] == 5 && d.date == 20261009);
    feed(&d, 20261010, DRAIN_EAR, 3, 3894, -41, false, 0);
    CHECK(d.today_min[DRAIN_DEEP] == 0 && d.today_min[DRAIN_EAR] == 3 && d.date == 20261010);
    CHECK(d.boot_min[DRAIN_DEEP] == 10 && d.boot_min[DRAIN_EAR] == 8);
    CHECK(d.seq == 18);

    /* 12. a zero reading still fills its minute (never an empty slot mid-window) and never makes a line */
    drain_begin(&d);
    for (int i = 0; i < 25; i++) drain_minute(&d, 20261009, 0, false, DRAIN_DEEP);
    CHECK(d.seq == 25 && d.today_min[DRAIN_DEEP] == 25);
    CHECK(drain_slope(&d, DRAIN_DEEP, &s, &n) && n == 25 && s == 0);

    /* 13. a kind out of range is the lit one */
    drain_begin(&d);
    drain_minute(&d, 20261009, 3900, false, 7);
    drain_minute(&d, 20261009, 3900, false, -1);
    CHECK(d.boot_min[DRAIN_LIT] == 2);

    /* 14. the text line's shape with both lines speaking */
    drain_begin(&d);
    feed(&d, 20261009, DRAIN_DEEP, 30, 3900, -18, false, 0);
    feed(&d, 20261009, DRAIN_EAR, 30, 3891, -41, false, 0);
    int len = drain_text(&d, line, sizeof line);
    CHECK(len > 0 && len < (int)sizeof line);
    CHECK(strstr(line, "ear: deep 30 min / awake 30 min today (since boot 30 / 30, lit 0); drain deep -1") != NULL);
    CHECK(strstr(line, "mV/h over 30 min, awake -4") != NULL);
    CHECK(strstr(line, "mV/h over 30 min") != NULL);

    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
