/* DISK KEEPING: which day files go. The day log grows ~23 KB a day and
 * the partition is 3456 KB -- five months fill it. This is the decision
 * alone, pure (no filesystem, no Arduino): daylog.cpp walks /day into a
 * list of (date, size, synced) and removes what this marks; tools/
 * daykeep_test.cpp feeds it made-up disks on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* THE KEEP: the newest DAY_KEEP day files stay whatever their sync state
 * (120 x ~23 KB = ~2.8 MB of the 3.4 MB: four months to look back on,
 * and the desk has had four months to fetch them). Older files that
 * are SYNCED go first, oldest first. An older file the desk has not
 * acked is kept -- it is the only copy -- until it is synced, or until
 * the partition passes DAY_PRUNE_PCT, when the oldest goes regardless:
 * a full disk would stop today's minutes, and today matters more than
 * a day the desk was too far away to hear. */
#define DAY_KEEP       120
#define DAY_PRUNE_PCT  85

typedef struct {
    uint32_t date;      /* YYYYMMDD */
    uint32_t size;      /* the .bin's bytes */
    bool     synced;    /* its .ok is there */
} daykeep_file_t;

/* the verdict per file */
enum { DAYKEEP_KEEP = 0, DAYKEEP_OLD_SYNCED = 1, DAYKEEP_DISK_FULL = 2 };

/* ``files`` (any order; sorted in place by date, ascending), the
 * partition's used and total bytes, today's date (today's file is
 * never removed). Fills ``verdict[i]`` for each file; returns how many
 * are to go. */
int daykeep_decide(daykeep_file_t *files, int n, uint32_t used, uint32_t total, uint32_t today,
                   uint8_t *verdict);

/* THE STATURE WEEK (design/watch-inspiration.md C.3, watch-ideas.md D27;
 * 2026-09-23): a body carries its weeks. WRIST_SQUASH drifts a quarter
 * of an art pixel TALLER after a week over his norm and as much ROUNDER
 * after a week under it -- the same dome standing a little differently,
 * never a new shape (sprite-v1). The desk's own rule (behaviour.py's
 * _feel_stature, config STATURE_STEP / STATURE_LAZY_AT) read off the
 * day files instead of a kept number: THE WEEK is the STATURE_DAYS
 * calendar days before today, THE NORM the one that stood before it --
 * the median of the STATURE_NORM_DAYS newest files older than the week
 * (face.cpp's norm, the same rule: a day under STATURE_MIN_STEPS was
 * the watch on a shelf and is left out; fewer than
 * STATURE_NORM_MIN_DAYS of them = no norm yet, nothing moves: a body
 * needs two weeks to carry one). Each day of the week at or over that
 * norm stands the dome STATURE_STEP_PX taller, under STATURE_LAZY of
 * it as much rounder, between the two nothing; a day with no file, or
 * a shelf day, counts nothing (Finch never punishes a day he did not
 * wear it). The sum is clamped to +-STATURE_PX: seven days over the
 * norm ARE the quarter pixel, and the files are the memory (no NVS: a
 * week later the drift has walked off by itself). His OWN norm: a
 * steady week at any level is a week over it (the desk's >= 1.0), so
 * a lifestyle is never a slump -- only a slump is. Read once at the
 * date's turn beside the norm, from the same file walk
 * (wrist_recent_days, STATURE_FILES newest). The ruling owed
 * (design/watch-map.md 73): the range and whether it shrinks at all --
 * STATURE_STEP_PX is one number both ways on purpose, so "never
 * shrinks" is a second constant at 0. */
#define STATURE_PX             0.25f                          /* the range: a quarter art px each way (blob_radii's ry; gel_stature_squash turns it into a squash) */
#define STATURE_DAYS           7                              /* the week judged: the calendar days 1..7 before today */
#define STATURE_STEP_PX        (STATURE_PX / STATURE_DAYS)    /* one day's drift, either way (symmetric: the ruling) */
#define STATURE_LAZY           0.5f                           /* under this fraction of the norm the day was lazy (the desk's STATURE_LAZY_AT) */
#define STATURE_NORM_DAYS      7                              /* face.cpp's NORM_DAYS: the median of this many */
#define STATURE_NORM_MIN_DAYS  3                              /* ... and not a norm at all under this many (face.cpp's NORM_MIN_DAYS) */
#define STATURE_MIN_STEPS      200                            /* face.cpp's NORM_MIN_STEPS: under it the watch was on a shelf, not his day */
#define STATURE_FILES          14                             /* the files the read asks for: the week judged + the week its norms come from */

/* ``dates`` / ``totals``: the newest ``n`` day files before today,
 * NEWEST FIRST (wrist_recent_days' order), each a YYYYMMDD and its
 * step total; ``today`` YYYYMMDD (0 = no clock: the newest STATURE_DAYS
 * files stand for the week, the files after them for the norm).
 * Returns the drift in art px, + taller / - rounder, clamped to
 * +-STATURE_PX; ``over`` / ``under`` (either may be NULL) get the count
 * of days that pulled each way. */
float daykeep_stature(const uint32_t *dates, const uint32_t *totals, int n, uint32_t today,
                      int *over, int *under);
/* days from a YYYYMMDD to a later one (negative the other way); the
 * civil calendar, for the week's window */
int32_t daykeep_days_between(uint32_t from_yyyymmdd, uint32_t to_yyyymmdd);

#ifdef __cplusplus
}
#endif
