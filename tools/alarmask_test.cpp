/* Host test of THE ALARM FROM THE DESK (src/alarmask.h): the ask's
 * grammar -- the look, `off`, H:MM / HH:MM inside the day, a bad word --
 * and the answer line: `ok alarm 07:30` / `ok alarm off` / `no clock`
 * (a set with no clock; off and the look never need one) / `no alarm`.
 * Build: c++ -std=gnu++17 -I src -o alarmask_test tools/alarmask_test.cpp
 * Run:   ./alarmask_test
 */
#include "../src/alarmask.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int parse(const char *w, int *m) { int x = -7; int k = alarmask_parse(w, &x); if (m) *m = x; return k; }

static const char *answer(int kind, int minute, bool clock)
{
    static char buf[24];
    memset(buf, 'x', sizeof buf);
    alarmask_answer(kind, minute, clock, buf, sizeof buf);
    return buf;
}

int main(void)
{
    int m;
    /* the look */
    check("nothing after `alarm` is the look", parse("", &m) == ALARMASK_STATUS && m == -1);
    check("NULL is the look too", parse(NULL, &m) == ALARMASK_STATUS && m == -1);
    /* off */
    check("`off` clears", parse("off", &m) == ALARMASK_OFF && m == -1);
    check("`offx` is not off", parse("offx", &m) == ALARMASK_BAD);
    check("`OFF` is not off (the desk folds case)", parse("OFF", &m) == ALARMASK_BAD);
    /* the hour */
    check("07:30 = 450", parse("07:30", &m) == ALARMASK_SET && m == 450);
    check("7:30 = 450 (one digit is fine)", parse("7:30", &m) == ALARMASK_SET && m == 450);
    check("00:00 = 0", parse("00:00", &m) == ALARMASK_SET && m == 0);
    check("23:59 = 1439", parse("23:59", &m) == ALARMASK_SET && m == 1439);
    check("24:00 is out of the day", parse("24:00", &m) == ALARMASK_BAD && m == -1);
    check("12:60 is out of the hour", parse("12:60", &m) == ALARMASK_BAD);
    check("7:5 needs two minute digits", parse("7:5", &m) == ALARMASK_BAD);
    check("007:30 is not an hour", parse("007:30", &m) == ALARMASK_BAD);
    check(":30 has no hour", parse(":30", &m) == ALARMASK_BAD);
    check("07:30x has a tail", parse("07:30x", &m) == ALARMASK_BAD);
    check("07:30 7 has a tail", parse("07:30 7", &m) == ALARMASK_BAD);
    check("7pm is not the grammar", parse("7pm", &m) == ALARMASK_BAD);
    check("0730 has no colon", parse("0730", &m) == ALARMASK_BAD);
    check("-1:00 is bad", parse("-1:00", &m) == ALARMASK_BAD);
    check("a NULL minute_out is tolerated", alarmask_parse("07:30", NULL) == ALARMASK_SET);
    /* the answers */
    check("set with a clock: ok alarm 07:30", !strcmp(answer(ALARMASK_SET, 450, true), "ok alarm 07:30"));
    check("set with no clock: no clock", !strcmp(answer(ALARMASK_SET, 450, false), "no clock"));
    check("off: ok alarm off (clock or not)", !strcmp(answer(ALARMASK_OFF, -1, false), "ok alarm off")
                                               && !strcmp(answer(ALARMASK_OFF, -1, true), "ok alarm off"));
    check("the look, set: ok alarm 23:59", !strcmp(answer(ALARMASK_STATUS, 1439, true), "ok alarm 23:59"));
    check("the look, off, no clock: ok alarm off", !strcmp(answer(ALARMASK_STATUS, -1, false), "ok alarm off"));
    check("the look, 00:00: ok alarm 00:00", !strcmp(answer(ALARMASK_STATUS, 0, true), "ok alarm 00:00"));
    check("a bad word: no alarm", !strcmp(answer(ALARMASK_BAD, 450, true), "no alarm"));
    check("a minute past the day answers off", !strcmp(answer(ALARMASK_STATUS, 1440, true), "ok alarm off"));
    /* the cap */
    { char s[8]; alarmask_answer(ALARMASK_STATUS, 450, true, s, sizeof s); check("a short cap is cut, terminated", !strcmp(s, "ok alar")); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
