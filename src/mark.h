/* THE MARK (the optimization pass, reiterated 2026-09-10): each overlay
 * drawer says whether it drew something DIFFERENT from last frame, and
 * where.
 *
 * The first pass hashed all 450 tiles of the frame every frame, even
 * when the raster cache said the gel had not changed and no overlay
 * had: the diff was the only judge. Now every drawer keeps a mark -- a
 * running hash of what it drew (each block's place, size and colour,
 * or a value the pixels are a pure function of, like an arc's fraction)
 * and the box those blocks covered. At the frame's end main.cpp asks
 * each mark: the hash the same as last frame's and the box the same =
 * the drawer's pixels are the same, whatever is under them (the gel's
 * own box covers what the gel changed beneath). Changed = the union of
 * this frame's box and last frame's (what appeared, what went) joins
 * the frame's HINT, and only the hint's tiles are hashed; no hint at
 * all = nothing is hashed, nothing pushed.
 *
 * A mark is a sequence hash (FNV-1a over the marked values, a 1-in-4-
 * billion miss like the tile hash); it never reads the framebuffer. A
 * safety scan of the whole frame runs once a second in main.cpp against
 * any drawer that forgets to mark (the hashes it refreshes are cheap).
 *
 * Plain C, header-only: the host tests build the drawers as they are. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "dirty.h"

typedef struct {
    uint32_t sig;               /* this frame's sequence hash (0 = nothing marked yet) */
    int      n;                 /* marks this frame (0 = the box is empty) */
    int      x0, y0, x1, y1;    /* this frame's box, x1/y1 exclusive */
    uint32_t last_sig;          /* the frame before's */
    int      last_n, lx0, ly0, lx1, ly1;
    bool     primed;            /* a frame has been taken */
} mark_t;

static inline void mark_val(mark_t *m, uint32_t v)
{
    m->sig = (m->sig ^ v) * 16777619u;
}

static inline void mark_box(mark_t *m, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (m->n == 0) { m->x0 = x; m->y0 = y; m->x1 = x + w; m->y1 = y + h; }
    else {
        if (x < m->x0) m->x0 = x;
        if (y < m->y0) m->y0 = y;
        if (x + w > m->x1) m->x1 = x + w;
        if (y + h > m->y1) m->y1 = y + h;
    }
    m->n++;
}

/* a block drawn: its place, size and colour into the hash, its area into the box */
static inline void mark_rect(mark_t *m, int x, int y, int w, int h, uint32_t c)
{
    mark_val(m, (uint32_t)x); mark_val(m, (uint32_t)y);
    mark_val(m, (uint32_t)w); mark_val(m, (uint32_t)h); mark_val(m, c);
    mark_box(m, x, y, w, h);
}

/* a float's bits into the hash (an arc's fraction, a fade's level) */
static inline void mark_f(mark_t *m, float f)
{
    union { float f; uint32_t u; } u; u.f = f;
    mark_val(m, u.u);
}

static inline dirty_rect_t mark_clip(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DIRTY_W) x1 = DIRTY_W;
    if (y1 > DIRTY_H) y1 = DIRTY_H;
    dirty_rect_t r = { x0, y0, x1 - x0, y1 - y0 };
    if (r.w <= 0 || r.h <= 0) r.x = r.y = r.w = r.h = 0;
    return r;
}

/* The frame's end: true when this frame's marks differ from last
 * frame's (a different sequence, or a different box -- the first frame
 * ever counts as different); *box = the union of the two frames' boxes,
 * clipped to the glass (empty when nothing was drawn either frame). The
 * mark is reset for the next frame: a drawer whose draw is skipped
 * marks nothing, and nothing after something is a change. */
static inline bool mark_take(mark_t *m, dirty_rect_t *box)
{
    bool changed = !m->primed || m->sig != m->last_sig || m->n != m->last_n
                   || (m->n && (m->x0 != m->lx0 || m->y0 != m->ly0 || m->x1 != m->lx1 || m->y1 != m->ly1));
    dirty_rect_t now = m->n ? mark_clip(m->x0, m->y0, m->x1, m->y1) : mark_clip(0, 0, 0, 0);
    dirty_rect_t was = (m->primed && m->last_n) ? mark_clip(m->lx0, m->ly0, m->lx1, m->ly1) : mark_clip(0, 0, 0, 0);
    /* the union of the two (dirty_union's rule, inline: the drawers'
     * host tests link no dirty.c) */
    dirty_rect_t u = now;
    if (now.w <= 0) u = was;
    else if (was.w > 0) {
        int x0 = now.x < was.x ? now.x : was.x, y0 = now.y < was.y ? now.y : was.y;
        int x1 = now.x + now.w > was.x + was.w ? now.x + now.w : was.x + was.w;
        int y1 = now.y + now.h > was.y + was.h ? now.y + now.h : was.y + was.h;
        u.x = x0; u.y = y0; u.w = x1 - x0; u.h = y1 - y0;
    }
    *box = changed ? u : mark_clip(0, 0, 0, 0);
    m->last_sig = m->sig; m->last_n = m->n;
    m->lx0 = m->x0; m->ly0 = m->y0; m->lx1 = m->x1; m->ly1 = m->y1;
    m->primed = true;
    m->sig = 0; m->n = 0;
    return changed;
}

/* the next take reports a change whatever it finds (the panel was cleared) */
static inline void mark_force(mark_t *m) { m->primed = false; }
