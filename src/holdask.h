/* THE FLASH FROM THE DESK (THE OPERATOR NIGHT, design/watch-night-2026-10-12.md
 * T14; 2026-10-13): the desk's ask `hold <s>` over whichever door is
 * open -- the radio kept up for N seconds as the console's `link hold`
 * keeps it this lit spell, so the desk can espota a build without a
 * hand on the dock. Read here, pure, so the wire's grammar and the
 * hold's clock are proven on the laptop (tools/holdask_test.cpp) and
 * link.cpp's branch stays a few lines. The words after `hold`:
 *
 *   (nothing)     HOLDASK_STATUS   the hold as it stands
 *   off / 0       HOLDASK_OFF      released (back on the budget)
 *   1..n          HOLDASK_SET      that many seconds, CAPPED at HOLDASK_CAP_S
 *   anything else HOLDASK_BAD      `no hold` (nothing fails quietly)
 *
 * THE CAP (ruling 151): 300 s. A flash is a minute and a half on a good
 * LAN; five minutes covers a slow one, and a hold the desk forgot ends
 * on its own -- a radio left up is the cell's night. The dark house
 * ends it sooner (link_dark, as the console's hold). The answer
 * (holdask_answer): `ok hold <s>` (the seconds granted, or left on the
 * look), `ok hold lit` (the console's untimed hold, on the look),
 * `ok hold off`, `no hold` (a bad word), `no network` (no network
 * remembered: nothing to bring up -- the forced radio would be
 * nothing), `no spent` (the cell under the spent line and off the
 * cable: THE LAST DROP'S MANNERS refuse the dial, ruling 118). No
 * strtok, no sscanf, no buffer: the line is read in place. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define HOLDASK_CAP_S 300u          /* ruling 151: the longest hold the desk may ask */

enum { HOLDASK_BAD = -1, HOLDASK_STATUS = 0, HOLDASK_SET = 1, HOLDASK_OFF = 2 };

/* the words after `hold` (NULL or "" = the look); *secs_out on SET, capped */
static inline int holdask_parse(const char *words, uint32_t *secs_out)
{
    if (secs_out) *secs_out = 0;
    if (!words || !words[0]) return HOLDASK_STATUS;
    if (words[0] == 'o' && words[1] == 'f' && words[2] == 'f' && !words[3]) return HOLDASK_OFF;
    uint32_t v = 0; int n = 0;
    for (const char *p = words; *p; p++, n++) {
        if (*p < '0' || *p > '9' || n >= 6) return HOLDASK_BAD;   /* digits only; six is already past the cap */
        v = v * 10u + (uint32_t)(*p - '0');
    }
    if (v == 0) return HOLDASK_OFF;
    if (v > HOLDASK_CAP_S) v = HOLDASK_CAP_S;
    if (secs_out) *secs_out = v;
    return HOLDASK_SET;
}

/* the hold's clock: when it ends (millis), 0 = untimed (the console's) */
static inline uint32_t holdask_until(uint32_t now_ms, uint32_t secs)
{
    if (!secs) return 0;
    uint32_t u = now_ms + secs * 1000u;
    return u ? u : 1u;                      /* 0 means untimed: a wrap that lands there is nudged */
}

/* a timed hold past its end (wrap-safe); an untimed one never */
static inline bool holdask_expired(uint32_t until_ms, uint32_t now_ms)
{
    return until_ms != 0 && (int32_t)(now_ms - until_ms) >= 0;
}

/* seconds left on a timed hold, rounded up; 0 when untimed or over */
static inline uint32_t holdask_left_s(uint32_t until_ms, uint32_t now_ms)
{
    if (!until_ms) return 0;
    int32_t d = (int32_t)(until_ms - now_ms);
    return d <= 0 ? 0 : ((uint32_t)d + 999u) / 1000u;
}

/* the answer line (no newline): kind as parsed; `held` = a hold is on
 * AFTER the act, `left_s` its seconds (0 = untimed); `nets` = a network
 * is remembered; `spent` = the dial is refused (spent and off the cable) */
static inline void holdask_answer(int kind, bool held, uint32_t left_s, bool nets, bool spent, char *out, int cap)
{
    if (kind == HOLDASK_BAD) snprintf(out, cap, "no hold");
    else if (kind == HOLDASK_SET && !nets) snprintf(out, cap, "no network");
    else if (kind == HOLDASK_SET && spent) snprintf(out, cap, "no spent");
    else if (!held) snprintf(out, cap, "ok hold off");
    else if (left_s) snprintf(out, cap, "ok hold %lu", (unsigned long)left_s);
    else snprintf(out, cap, "ok hold lit");
}
