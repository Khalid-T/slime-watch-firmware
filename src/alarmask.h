/* THE ALARM FROM THE DESK (THE OPERATOR NIGHT, design/watch-night-2026-10-12.md
 * T13; 2026-10-13): the desk's ask `alarm HH:MM` / `alarm off` / `alarm`
 * over whichever door is open -- read here, pure, so the wire's grammar
 * is proven on the laptop (tools/alarmask_test.cpp) and link.cpp's branch
 * stays five lines. The words after `alarm`:
 *
 *   (nothing)   ALARMASK_STATUS   the alarm as it stands
 *   off         ALARMASK_OFF      cleared, the hour remembered for the ALARM row's next tap
 *   H:MM/HH:MM  ALARMASK_SET      0..23 : 00..59, *minute_out = the minute of the day
 *   anything else ALARMASK_BAD    `no alarm` (the shell knows the shape; nothing fails quietly)
 *
 * The answer (alarmask_answer): `ok alarm 07:30` / `ok alarm off` for a
 * set, a clear or a look; `no clock` for a SET on a wrist whose clock
 * nobody told (an alarm is a minute of the local day: without the day
 * it is nothing -- the rim's dial refuses the same way); `no alarm` for
 * a word the grammar does not know. `off` and the look never need the
 * clock. The same NVS road as the ALARM row (main.cpp app_alarm_set);
 * the alarm set from the desk lights nothing before its minute, as none
 * does (ruling 136). No strtok, no sscanf, no buffer: the line is read
 * in place. */
#pragma once
#include <stdbool.h>
#include <stdio.h>

enum { ALARMASK_BAD = -1, ALARMASK_STATUS = 0, ALARMASK_SET = 1, ALARMASK_OFF = 2 };

/* the words after `alarm` (NULL or "" = the look); *minute_out on SET */
static inline int alarmask_parse(const char *words, int *minute_out)
{
    if (minute_out) *minute_out = -1;
    if (!words || !words[0]) return ALARMASK_STATUS;
    if (words[0] == 'o' && words[1] == 'f' && words[2] == 'f' && !words[3]) return ALARMASK_OFF;
    const char *p = words;
    int hh = 0, n = 0;
    while (*p >= '0' && *p <= '9' && n < 2) { hh = hh * 10 + (*p - '0'); p++; n++; }
    if (n == 0 || *p != ':') return ALARMASK_BAD;
    p++;
    if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9') || p[2]) return ALARMASK_BAD;
    int mm = (p[0] - '0') * 10 + (p[1] - '0');
    if (hh > 23 || mm > 59) return ALARMASK_BAD;
    if (minute_out) *minute_out = hh * 60 + mm;
    return ALARMASK_SET;
}

/* the answer line (no newline): kind as parsed, `minute` the alarm as it
 * stands AFTER the act (-1 = off), `clock` = the wrist knows the day */
static inline void alarmask_answer(int kind, int minute, bool clock, char *out, int cap)
{
    if (kind == ALARMASK_BAD) snprintf(out, cap, "no alarm");
    else if (kind == ALARMASK_SET && !clock) snprintf(out, cap, "no clock");
    else if (minute < 0 || minute >= 1440) snprintf(out, cap, "ok alarm off");
    else snprintf(out, cap, "ok alarm %02d:%02d", minute / 60, minute % 60);
}
