/* THE DIRTY RECT -- dirty.h says why; this is how. */
#pragma GCC optimize ("O2")             /* the core builds -Os; the hash loop wants its unrolling */
#include "dirty.h"
#include <string.h>

_Static_assert(DIRTY_W % DIRTY_TW == 0 && DIRTY_H % DIRTY_TH == 0, "the tiles must tile the frame");
_Static_assert(DIRTY_TW % 8 == 0, "a tile row must be whole groups of four 32-bit words");

void dirty_begin(dirty_t *d)
{
    memset(d, 0, sizeof *d);
    d->primed = false;
}

void dirty_force(dirty_t *d) { d->primed = false; }

/* Four lanes of FNV-1a over the tile's 32-bit words (lane k takes every
 * fourth word from k): each lane is a multiplicative chain, so a word
 * moving to another place in the tile changes its lane's hash (a plain
 * xor-and-rotate would let a block moving a whole period of the rotate
 * cancel out); four chains let the multiplies pipeline where one chain
 * stalls on its own latency. The lanes are folded with rotates so the
 * lane keeps its identity, then one avalanche so a change in the last
 * word moves every bit. */
static inline uint32_t rotl32(uint32_t v, int s) { return (v << s) | (v >> (32 - s)); }

uint32_t dirty_tile_hash(const uint16_t *fb, int col, int row)
{
    const uint32_t P = 16777619u;
    uint32_t h0 = 2166136261u, h1 = 0x9e3779b9u, h2 = 0x85ebca6bu, h3 = 0xc2b2ae35u;
    const uint32_t *w = (const uint32_t *)(fb + (row * DIRTY_TH) * DIRTY_W + col * DIRTY_TW);
    for (int r = 0; r < DIRTY_TH; r++, w += DIRTY_W / 2) {
        for (int i = 0; i < DIRTY_TW / 2; i += 4) {
            h0 = (h0 ^ w[i])     * P;
            h1 = (h1 ^ w[i + 1]) * P;
            h2 = (h2 ^ w[i + 2]) * P;
            h3 = (h3 ^ w[i + 3]) * P;
        }
    }
    uint32_t h = h0 ^ rotl32(h1, 8) ^ rotl32(h2, 16) ^ rotl32(h3, 24);
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}

dirty_rect_t dirty_union(dirty_rect_t a, dirty_rect_t b)
{
    if (a.w <= 0 || a.h <= 0) return b;
    if (b.w <= 0 || b.h <= 0) return a;
    int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    int x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    dirty_rect_t u = { x0, y0, x1 - x0, y1 - y0 };
    return u;
}

bool dirty_scan_rect(dirty_t *d, const uint16_t *fb, const dirty_rect_t *hint, dirty_rect_t *out)
{
    int c0 = DIRTY_COLS, c1 = -1, r0 = DIRTY_ROWS, r1 = -1;
    /* the tiles to hash: all of them for a whole-frame scan, the ones
     * under the hint otherwise (clipped; an empty hint is no tile) */
    int ca = 0, cb = DIRTY_COLS, ra = 0, rb = DIRTY_ROWS;
    bool all = !d->primed;
    if (!all && hint) {
        if (hint->w <= 0 || hint->h <= 0) { ca = cb = ra = rb = 0; }
        else {
            int x0 = hint->x < 0 ? 0 : hint->x, y0 = hint->y < 0 ? 0 : hint->y;
            int x1 = hint->x + hint->w, y1 = hint->y + hint->h;
            if (x1 > DIRTY_W) x1 = DIRTY_W;
            if (y1 > DIRTY_H) y1 = DIRTY_H;
            ca = x0 / DIRTY_TW; cb = (x1 + DIRTY_TW - 1) / DIRTY_TW;
            ra = y0 / DIRTY_TH; rb = (y1 + DIRTY_TH - 1) / DIRTY_TH;
            if (cb <= ca || rb <= ra) { ca = cb = ra = rb = 0; }
        }
    }
    uint32_t n = 0, scanned = 0;
    for (int row = ra; row < rb; row++) {
        for (int col = ca; col < cb; col++) {
            uint32_t h = dirty_tile_hash(fb, col, row);
            uint32_t *slot = &d->hash[row * DIRTY_COLS + col];
            scanned++;
            if (all || *slot != h) {
                *slot = h;
                n++;
                if (col < c0) c0 = col;
                if (col > c1) c1 = col;
                if (row < r0) r0 = row;
                if (row > r1) r1 = row;
            }
        }
    }
    d->primed = true;
    d->tiles_dirty = n;
    d->tiles_scanned = scanned;
    if (c1 < 0) { out->x = out->y = out->w = out->h = 0; return false; }
    out->x = c0 * DIRTY_TW;
    out->y = r0 * DIRTY_TH;
    out->w = (c1 - c0 + 1) * DIRTY_TW;
    out->h = (r1 - r0 + 1) * DIRTY_TH;
    return true;
}

bool dirty_scan(dirty_t *d, const uint16_t *fb, dirty_rect_t *out)
{
    return dirty_scan_rect(d, fb, NULL, out);
}
