/* THE AIR DOOR's bytes into lines (bleuart.cpp): the desk writes the
 * link's lines (design/watch-link.md: newline-ended, LINK_LINE_MAX at
 * most) into the NUS RX characteristic in whatever pieces its MTU
 * allows -- a line may arrive in three writes, or three lines in one.
 * This gathers to the newline and hands each line on; a CR is dropped
 * (a desk on a terminal); a line past the cap is dropped whole, counted,
 * and gathering resumes after its newline (never half a line passed on).
 *
 * Pure (no Arduino): tools/bleline_test.cpp walks the splits on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BLELINE_MAX 180          /* = LINK_LINE_MAX (link.cpp): one line, never more */

typedef struct {
    char     buf[BLELINE_MAX + 1];
    int      n;
    bool     overlong;           /* the line in hand is past the cap: dropping to its newline */
    uint32_t lines, dropped;     /* the tallies, for `ble` */
} bleline_t;

typedef void (*bleline_fn)(const char *line, void *arg);

void bleline_init(bleline_t *l);
/* feed bytes; every whole line goes to fn (without its newline). Returns the lines handed on. */
int  bleline_feed(bleline_t *l, const uint8_t *d, int n, bleline_fn fn, void *arg);
void bleline_reset(bleline_t *l);   /* the central left: whatever was half-gathered goes */
#ifdef __cplusplus
}
#endif
