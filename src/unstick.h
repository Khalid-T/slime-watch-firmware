/* THE BUS UNSTUCK (T4, THE OPERATOR NIGHT 2026-10-12): bus 0 (GPIO 6/7,
 * the touch and the IMU) had no timeout and sense() returned silently on
 * a failed IMU read for ever -- a slave holding SDA low (a read cut by a
 * reset mid-byte, a glitch) froze every frame 50 ms at the default
 * timeout and the gel stopped feeling gravity with no line said.
 *
 * The rule, pure: every IMU read in sense() is noted ANSWERED (the chip
 * ACKed, data or no data -- "no new sample" is not a failure) or not.
 * UNSTICK_DEAD_N unanswered reads in a row (50: ~1.3 s at the 40 fps
 * sense rate, 2.5 s at the idle 20 Hz -- each failed read costs the 20 ms
 * timeout, so it is a second or two of a stiff gel, never a frozen
 * glass) ask for the recovery: bus0_unstick() in bus.cpp -- Wire.end(),
 * SCL clocked nine times by hand with SDA released (the slave finishes
 * the byte it was stuck in and lets go), a STOP, Wire.begin() again,
 * imu_begin() and a touch reset. The IMU answering after that judges the
 * recovery GOOD; UNSTICK_DEAD_N more unanswered reads judge it FAILED and
 * the next one waits the ladder: UNSTICK_WAIT_MS doubling to
 * UNSTICK_WAIT_MAX_MS (1, 2, 4 .. 60 s; a dead chip is not clocked
 * forty times a minute). An answer resets the ladder.
 *
 * The touch is counted but never recovered here: its polls go through
 * the same bus, so a bus stuck shows on both; a touch silent WHILE THE
 * IMU ANSWERS is the chip itself (asleep on its own, or dead), which is
 * touch_let_sleep's business -- the verdict names it, nothing else
 * moves. Doctor's words live here (unstick_bom, unstick_verdict) so the
 * host test proves them.
 *
 * Pure: the clock and the outcomes are parameters; no Arduino;
 * tools/unstick_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define UNSTICK_DEAD_N        50        /* unanswered IMU reads in a row before the bus is unstuck */
#define UNSTICK_WAIT_MS       1000      /* the ladder's first rung: the gap after a FAILED recovery */
#define UNSTICK_WAIT_MAX_MS   60000     /* the ladder's top: a try a minute, no more */
#define UNSTICK_VERDICT_S     3         /* the IMU silent this long (past the first recovery) is the verdict */
#define UNSTICK_TOUCH_S       10        /* the touch unanswered this long while the IMU answers is the chip's own silence */
#define UNSTICK_DEAD_CAP      60000     /* the count saturates (a day of silence is not a wrap) */

typedef struct {
    bool     ever;            /* the IMU answered at least once since boot (never: doctor's "no IMU found" is the word) */
    bool     pending;         /* a recovery ran and no read has judged it yet */
    uint16_t dead;            /* unanswered IMU reads in a row, capped */
    uint16_t tries;           /* recoveries run since boot */
    uint16_t failed;          /* recoveries after which the IMU stayed silent */
    uint16_t run;             /* failed recoveries in a row since the last answer (the ladder's rung) */
    uint32_t silent_ms;       /* when the run of silence began (0 = answering) */
    uint32_t tried_ms;        /* when the last recovery ran (0 = never) */
    uint32_t good_ms;         /* when a recovery was last judged good (0 = never) */
    uint32_t wait_ms;         /* the ladder: the gap before the next recovery */
    uint32_t touch_silent_ms; /* when the touch's run of unanswered polls began (0 = answering) */
} unstick_t;

static inline uint32_t unstick_stamp(uint32_t now) { return now ? now : 1; }

static inline void unstick_init(unstick_t *u)
{
    u->ever = false; u->pending = false;
    u->dead = 0; u->tries = 0; u->failed = 0; u->run = 0;
    u->silent_ms = 0; u->tried_ms = 0; u->good_ms = 0;
    u->wait_ms = UNSTICK_WAIT_MS;
    u->touch_silent_ms = 0;
}

/* one IMU read's outcome; true = unstick the bus now (then unstick_tried) */
static inline bool unstick_note(unstick_t *u, bool answered, uint32_t now)
{
    if (answered) {
        if (u->pending) { u->pending = false; u->good_ms = unstick_stamp(now); }
        u->ever = true;
        u->dead = 0;
        u->run = 0;
        u->silent_ms = 0;
        u->wait_ms = UNSTICK_WAIT_MS;
        return false;
    }
    if (!u->silent_ms) u->silent_ms = unstick_stamp(now);
    if (u->dead < UNSTICK_DEAD_CAP) u->dead++;
    if (u->dead < UNSTICK_DEAD_N) return false;
    if (u->pending) {                       /* the last recovery did not bring it back: the next rung */
        u->pending = false;
        u->failed++;
        if (u->run++) u->wait_ms = u->wait_ms >= UNSTICK_WAIT_MAX_MS / 2 ? UNSTICK_WAIT_MAX_MS : u->wait_ms * 2;
    }
    if (u->tried_ms && now - u->tried_ms < u->wait_ms) return false;   /* the ladder's gap */
    return true;
}

/* the recovery ran (whatever it found: the next reads judge it) */
static inline void unstick_tried(unstick_t *u, uint32_t now)
{
    u->tries++;
    u->pending = true;
    u->tried_ms = unstick_stamp(now);
    u->dead = 0;
}

/* the touch's last poll, answered or not (counted, never recovered here) */
static inline void unstick_touch(unstick_t *u, bool alive, uint32_t now)
{
    if (alive) u->touch_silent_ms = 0;
    else if (!u->touch_silent_ms) u->touch_silent_ms = unstick_stamp(now);
}

static inline uint32_t unstick_silent_s(const unstick_t *u, uint32_t now)
{
    return u->silent_ms ? (now - u->silent_ms) / 1000 : 0;
}

static inline uint32_t unstick_touch_silent_s(const unstick_t *u, uint32_t now)
{
    return u->touch_silent_ms ? (now - u->touch_silent_ms) / 1000 : 0;
}

/* the ones that worked (a pending one is not yet counted either way) */
static inline unsigned unstick_good(const unstick_t *u)
{
    return (unsigned)(u->tries - u->failed - (u->pending ? 1 : 0));
}

/* "M min ago" / "S s ago" */
static inline int unstick_ago(uint32_t now, uint32_t then, char *out, size_t cap)
{
    uint32_t s = (now - then) / 1000;
    if (s >= 60) return snprintf(out, cap, "%lu min ago", (unsigned long)(s / 60));
    return snprintf(out, cap, "%lu s ago", (unsigned long)s);
}

/* doctor's bom words: `bus0 ok` | `bus0 ok (2 recoveries, last 3 min ago)`
 * | `bus0 SILENT 12 s (2 recoveries, 3 failed)` */
static inline int unstick_bom(const unstick_t *u, uint32_t now, char *out, size_t cap)
{
    unsigned good = unstick_good(u);
    if (u->silent_ms && u->ever)
        return snprintf(out, cap, "bus0 SILENT %lu s (%u recover%s, %u failed)",
                        (unsigned long)unstick_silent_s(u, now), good, good == 1 ? "y" : "ies", (unsigned)u->failed);
    if (!u->tries) return snprintf(out, cap, "bus0 ok");
    char ago[24] = "never";
    if (u->good_ms) unstick_ago(now, u->good_ms, ago, sizeof ago);
    return snprintf(out, cap, "bus0 ok (%u recover%s, last %s%s)", good, good == 1 ? "y" : "ies", ago,
                    u->failed ? ", some failed" : "");
}

/* doctor's verdict line, or 0 when nothing here is wrong (then the chain
 * goes on). The IMU's silence outranks the touch's: a stuck bus shows on
 * both and only one of them is a bus. */
static inline int unstick_verdict(const unstick_t *u, uint32_t now, bool glass_lit, char *out, size_t cap)
{
    if (u->ever && u->silent_ms && unstick_silent_s(u, now) >= UNSTICK_VERDICT_S)
        return snprintf(out, cap, "the IMU has been silent %lu s (bus0 stuck: %u recover%s failed)",
                        (unsigned long)unstick_silent_s(u, now), (unsigned)u->failed, u->failed == 1 ? "y" : "ies");
    if (glass_lit && !u->silent_ms && u->touch_silent_ms && unstick_touch_silent_s(u, now) >= UNSTICK_TOUCH_S)
        return snprintf(out, cap, "the touch has been silent %lu s while the IMU answers (the chip, not bus0: asleep on its own, or dead)",
                        (unsigned long)unstick_touch_silent_s(u, now));
    return 0;
}
