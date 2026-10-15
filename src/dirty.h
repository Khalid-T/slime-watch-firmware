/* THE DIRTY RECT (the optimization pass, 2026-09-10; Khalid 23:55: "once
 * you fill it up, optimize it, make it more efficient -- just the
 * watch/screen").
 *
 * Forty full 115 KB frames a second over the SPI bus was the biggest
 * awake draw, and most of them were the same picture: at rest the gel's
 * cell only changes a few times a second (the breath crosses a squash
 * quantum, the gel clock ticks an eighth), the rings and the hour almost
 * never. So the frame is composed as before and then DIFFED against
 * what the panel already shows, and only the changed part is pushed.
 *
 * How: the 240x240 frame is cut into DIRTY_TW x DIRTY_TH tiles (16 x 16:
 * 15 x 15 = 225 of them) and a tile is hashed (four lanes of FNV-1a
 * over its 32-bit words, the lanes' multiplies pipelining where one
 * chain would stall, then one avalanche); a tile whose hash moved is
 * dirty, and the push is the bounding box of the dirty tiles
 * (tile-aligned). REITERATED: the scan takes a HINT -- the box the
 * drawers say could have changed (mark.h: the gel's own box from the
 * raster cache and the compose, each overlay's mark) -- and hashes only
 * the tiles under it; the tiles outside keep their hashes, which are
 * still true. No hint = no hashing at all. A whole-frame scan is the
 * hint's absence (NULL) or the first scan after dirty_force.
 * Why a diff under the hint and not the hint itself: the hint is a
 * bounding box (the gel's whole cell box on a blink), the diff finds
 * the rows that really moved, and the diff catches everything drawn by
 * anyone inside it. A hash miss (two different tiles with one 32-bit
 * hash: 1 in 4 billion per changed tile) leaves that tile stale until it
 * next changes; the gel's tiles change within the second, so the worst
 * case is one stale mark for a while, never a wrong creature.
 *
 * No Arduino in here: tools/dirty_test.cpp proves it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRTY_W     240
#define DIRTY_H     240
#define DIRTY_TW    16                  /* tile width, px (8 words of 2 px) */
#define DIRTY_TH    16                  /* tile height, rows */
#define DIRTY_COLS  (DIRTY_W / DIRTY_TW)
#define DIRTY_ROWS  (DIRTY_H / DIRTY_TH)
#define DIRTY_TILES (DIRTY_COLS * DIRTY_ROWS)

typedef struct { int x, y, w, h; } dirty_rect_t;     /* w == 0: nothing */

/* the last frame's tile hashes: DIRTY_TILES words (900 B; PSRAM on the board) */
typedef struct {
    uint32_t hash[DIRTY_TILES];
    bool     primed;                    /* false: the next scan reports the whole frame */
    uint32_t tiles_dirty;               /* the last scan's count, for the report line */
    uint32_t tiles_scanned;             /* the last scan's tiles hashed (0 = nothing was) */
} dirty_t;

void dirty_begin(dirty_t *d);
/* the next scan reports the whole frame whatever it finds (the panel
 * woke from sleep, or was cleared: its memory is not ours to trust) */
void dirty_force(dirty_t *d);
/* hash every tile of fb (DIRTY_W x DIRTY_H words of RGB565), remember
 * them, and return the bounding box of the tiles that changed since the
 * last scan; false (and out->w == 0) when nothing did */
bool dirty_scan(dirty_t *d, const uint16_t *fb, dirty_rect_t *out);
/* the same over the tiles under hint only (NULL or an empty hint after
 * the first scan = nothing hashed, nothing dirty; NULL before it = the
 * whole frame) -- the tiles outside keep their hashes */
bool dirty_scan_rect(dirty_t *d, const uint16_t *fb, const dirty_rect_t *hint, dirty_rect_t *out);
/* the bounding box of two rects (either may be empty) */
dirty_rect_t dirty_union(dirty_rect_t a, dirty_rect_t b);
/* one tile's hash (exposed for the test) */
uint32_t dirty_tile_hash(const uint16_t *fb, int col, int row);

#ifdef __cplusplus
}
#endif
