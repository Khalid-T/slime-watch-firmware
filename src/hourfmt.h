/* THE HOUR'S TEXT, one way for every drawer (MORE SETTINGS, 2026-09-10
 * 21:12: the CLOCK row -- 12H / 24H). The face's resting hour, the
 * glance's, the page's big hour and its alarm rows all spell a minute
 * of the day through this, so one switch turns them all. 24H is what
 * every drawer had: `07:41`. 12H drops the leading zero and counts
 * 1-12 (`7:41`, `12:05`); with ``meridiem`` it says which half
 * (`7:41 PM`) -- the page has room for it, the face and the glance
 * (a 3x5 digit font, five cells) do not and show the bare `7:41`.
 * Header-only, no Arduino: the host tests build the drawers alone. */
#pragma once
#include <stdio.h>
#include <stdbool.h>

static inline void hour_fmt(char *s, int cap, int minute, bool h12, bool meridiem)
{
    int h = (minute / 60) % 24, m = minute % 60;
    if (h < 0) h += 24;
    if (m < 0) m += 60;
    if (!h12) { snprintf(s, cap, "%02d:%02d", h, m); return; }
    int hh = h % 12;
    if (hh == 0) hh = 12;
    if (meridiem) snprintf(s, cap, "%d:%02d %s", hh, m, h < 12 ? "AM" : "PM");
    else snprintf(s, cap, "%d:%02d", hh, m);
}

/* a whole hour for the QUIET FROM / QUIET TO rows: `23:00` / `07:00`, or `11 PM` / `7 AM` */
static inline void hour_fmt_whole(char *s, int cap, int hour, bool h12)
{
    int h = hour % 24;
    if (h < 0) h += 24;
    if (!h12) { snprintf(s, cap, "%02d:00", h); return; }
    int hh = h % 12;
    if (hh == 0) hh = 12;
    snprintf(s, cap, "%d %s", hh, h < 12 ? "AM" : "PM");
}
