/* THE DIRTY RECT on the laptop (dirty.c has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/dirty_test tools/dirty_test.cpp -x c src/dirty.c && /tmp/dirty_test
 * The first scan is the whole frame; the same frame again is nothing; one
 * pixel is its tile; two far pixels are their bounding box (the union);
 * a pixel changed back is nothing; the union of rects; a forced scan is
 * the whole frame; 20,000 random single-word changes are every one seen
 * (no hash miss in practice); a moving gel-sized blob's rect never wider
 * than the blob plus a tile each way. REITERATED: the hinted scan (only
 * the tiles under the hint are hashed and refreshed, an empty hint
 * hashes nothing, NULL is the whole frame), the four-lane hash against
 * a block moving within a tile (the cancellation a rotate hash allows),
 * and mark.h's take (same marks = no change; a different block, a
 * skipped draw, a first frame = a change with the union box). */
#include "dirty.h"
#include "mark.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t fb[DIRTY_W * DIRTY_H];
static dirty_t d;

static void expect(dirty_rect_t r, int x, int y, int w, int h)
{
    if (r.x != x || r.y != y || r.w != w || r.h != h) {
        printf("  rect %d,%d %dx%d, wanted %d,%d %dx%d\n", r.x, r.y, r.w, r.h, x, y, w, h);
        assert(0);
    }
}

static int tx(int x) { return x / DIRTY_TW * DIRTY_TW; }
static int ty(int y) { return y / DIRTY_TH * DIRTY_TH; }

int main(void)
{
    dirty_rect_t r;
    printf("-- tiles %dx%d, %d of them\n", DIRTY_TW, DIRTY_TH, DIRTY_TILES);
    assert(DIRTY_TILES == 225);
    puts("-- the first scan is the whole frame; the same frame again is nothing");
    memset(fb, 0, sizeof fb);
    dirty_begin(&d);
    assert(dirty_scan(&d, fb, &r)); expect(r, 0, 0, DIRTY_W, DIRTY_H);
    assert(d.tiles_dirty == DIRTY_TILES && d.tiles_scanned == DIRTY_TILES);
    assert(!dirty_scan(&d, fb, &r)); expect(r, 0, 0, 0, 0);
    assert(d.tiles_dirty == 0);

    puts("-- one pixel is its tile (aligned)");
    fb[100 * DIRTY_W + 37] = 0x1234;
    assert(dirty_scan(&d, fb, &r)); expect(r, tx(37), ty(100), DIRTY_TW, DIRTY_TH);
    assert(d.tiles_dirty == 1);
    assert(!dirty_scan(&d, fb, &r));

    puts("-- two far pixels are their bounding box: the union");
    fb[10 * DIRTY_W + 5] = 0xffff;
    fb[230 * DIRTY_W + 233] = 0xffff;
    assert(dirty_scan(&d, fb, &r)); expect(r, 0, 0, DIRTY_W, DIRTY_H);
    assert(d.tiles_dirty == 2);

    puts("-- a pixel changed back to what the panel shows is nothing");
    fb[10 * DIRTY_W + 5] = 0xffff;                      /* the same value again */
    assert(!dirty_scan(&d, fb, &r));
    fb[10 * DIRTY_W + 5] = 0; assert(dirty_scan(&d, fb, &r)); expect(r, 0, 0, DIRTY_TW, DIRTY_TH);
    fb[10 * DIRTY_W + 5] = 0xffff; assert(dirty_scan(&d, fb, &r)); expect(r, 0, 0, DIRTY_TW, DIRTY_TH);

    puts("-- the union of rects");
    { dirty_rect_t a = { 10, 10, 20, 20 }, b = { 100, 5, 10, 10 }, e = { 0, 0, 0, 0 };
      expect(dirty_union(a, b), 10, 5, 100, 25);
      expect(dirty_union(b, a), 10, 5, 100, 25);
      expect(dirty_union(a, e), 10, 10, 20, 20);
      expect(dirty_union(e, b), 100, 5, 10, 10);
      expect(dirty_union(e, e), 0, 0, 0, 0);
      expect(dirty_union(a, a), 10, 10, 20, 20); }

    puts("-- a forced scan is the whole frame, then nothing again");
    dirty_force(&d);
    assert(dirty_scan(&d, fb, &r)); expect(r, 0, 0, DIRTY_W, DIRTY_H);
    assert(!dirty_scan(&d, fb, &r));

    puts("-- 20,000 random single-word changes: every one seen, in the right tile");
    srand(12345);
    for (int k = 0; k < 20000; k++) {
        int i = rand() % (DIRTY_W * DIRTY_H);
        uint16_t v = (uint16_t)rand();
        if (v == fb[i]) v ^= 1;
        fb[i] = v;
        int x = i % DIRTY_W, y = i / DIRTY_W;
        assert(dirty_scan(&d, fb, &r));
        expect(r, tx(x), ty(y), DIRTY_TW, DIRTY_TH);
        assert(d.tiles_dirty == 1);
    }

    puts("-- a gel-sized blob (130 x 100) sliding across: the rect is the blob's union with its last place, a tile's slack at most");
    memset(fb, 0, sizeof fb);
    dirty_force(&d); dirty_scan(&d, fb, &r);
    int px = 20, py = 70;
    for (int step = 0; step < 30; step++) {
        memset(fb, 0, sizeof fb);
        int nx = px + step * 3, ny = py + (step % 4);
        for (int y = ny; y < ny + 100; y++) for (int x = nx; x < nx + 130; x++) fb[y * DIRTY_W + x] = 0x07e0;
        assert(dirty_scan(&d, fb, &r));
        int lx = step ? px + (step - 1) * 3 : nx, ly = step ? py + ((step - 1) % 4) : ny;
        int ux0 = lx < nx ? lx : nx, uy0 = ly < ny ? ly : ny;
        int ux1 = (lx + 130) > (nx + 130) ? lx + 130 : nx + 130, uy1 = (ly + 100) > (ny + 100) ? ly + 100 : ny + 100;
        assert(r.x <= ux0 && r.x + DIRTY_TW > ux0);
        assert(r.y <= uy0 && r.y + DIRTY_TH > uy0);
        assert(r.x + r.w >= ux1 && r.x + r.w - DIRTY_TW < ux1);
        assert(r.y + r.h >= uy1 && r.y + r.h - DIRTY_TH < uy1);
        assert(r.w * r.h < DIRTY_W * DIRTY_H / 2);          /* never near the whole frame */
    }
    printf("  the last slide: rect %d,%d %dx%d = %d px of %d (%.0f%%)\n", r.x, r.y, r.w, r.h, r.w * r.h,
           DIRTY_W * DIRTY_H, 100.0 * r.w * r.h / (DIRTY_W * DIRTY_H));

    puts("-- a still blob with a breath: only the rows that moved");
    memset(fb, 0, sizeof fb);
    for (int y = 100; y < 200; y++) for (int x = 55; x < 185; x++) fb[y * DIRTY_W + x] = 0x07e0;
    dirty_scan(&d, fb, &r);
    for (int x = 55; x < 185; x++) fb[99 * DIRTY_W + x] = 0x07e0;      /* the crown a pixel taller */
    assert(dirty_scan(&d, fb, &r)); expect(r, tx(55), ty(99), tx(184) + DIRTY_TW - tx(55), DIRTY_TH);

    puts("-- the hinted scan: only the tiles under the hint are hashed; a change outside waits for a whole scan");
    memset(fb, 0, sizeof fb);
    dirty_force(&d); dirty_scan(&d, fb, &r);
    fb[50 * DIRTY_W + 50] = 0x1111;                     /* inside the hint */
    fb[200 * DIRTY_W + 200] = 0x2222;                   /* outside it */
    { dirty_rect_t hint = { 40, 40, 30, 30 };
      assert(dirty_scan_rect(&d, fb, &hint, &r)); expect(r, tx(50), ty(50), DIRTY_TW, DIRTY_TH);
      assert(d.tiles_dirty == 1);
      assert(d.tiles_scanned == 9);                    /* 40..70 spans tiles 2..4 each way: 3 x 3 */
    }
    { dirty_rect_t hint = { 40, 40, 30, 30 };           /* the same hint again: nothing under it moved */
      assert(!dirty_scan_rect(&d, fb, &hint, &r)); expect(r, 0, 0, 0, 0);
      assert(d.tiles_scanned == 9); }
    { dirty_rect_t none = { 0, 0, 0, 0 };               /* an empty hint hashes nothing at all */
      assert(!dirty_scan_rect(&d, fb, &none, &r)); assert(d.tiles_scanned == 0); }
    assert(dirty_scan_rect(&d, fb, NULL, &r));          /* NULL = the whole frame: the outside change is found now */
    expect(r, tx(200), ty(200), DIRTY_TW, DIRTY_TH);
    assert(d.tiles_scanned == DIRTY_TILES && d.tiles_dirty == 1);
    { dirty_rect_t hint = { -20, -20, 300, 300 };       /* a hint past the glass is clipped */
      assert(!dirty_scan_rect(&d, fb, &hint, &r)); assert(d.tiles_scanned == DIRTY_TILES); }
    { dirty_rect_t hint = { 236, 236, 30, 30 };         /* the last tile alone */
      fb[239 * DIRTY_W + 239] = 0x3333;
      assert(dirty_scan_rect(&d, fb, &hint, &r)); expect(r, 224, 224, DIRTY_TW, DIRTY_TH);
      assert(d.tiles_scanned == 1); }
    puts("-- a forced scan ignores the hint");
    dirty_force(&d);
    { dirty_rect_t none = { 0, 0, 0, 0 };
      assert(dirty_scan_rect(&d, fb, &none, &r)); expect(r, 0, 0, DIRTY_W, DIRTY_H);
      assert(d.tiles_scanned == DIRTY_TILES); }

    puts("-- the four-lane hash: a solid block moving by whole rows within a tile is always a different hash");
    {
        int misses = 0, tries = 0;
        for (int h0 = 1; h0 <= DIRTY_TH - 1; h0++)
            for (int dy = 1; dy + h0 <= DIRTY_TH; dy++) {
                memset(fb, 0, sizeof fb);
                for (int y = 0; y < h0; y++) for (int x = 0; x < DIRTY_TW; x++) fb[y * DIRTY_W + x] = 0xabcd;
                uint32_t a = dirty_tile_hash(fb, 0, 0);
                memset(fb, 0, sizeof fb);
                for (int y = dy; y < dy + h0; y++) for (int x = 0; x < DIRTY_TW; x++) fb[y * DIRTY_W + x] = 0xabcd;
                uint32_t b = dirty_tile_hash(fb, 0, 0);
                tries++;
                if (a == b) misses++;
            }
        printf("  %d block moves, %d hash collisions\n", tries, misses);
        assert(misses == 0);
        /* and a block moving by whole words along the row */
        misses = 0; tries = 0;
        for (int w0 = 1; w0 < DIRTY_TW / 2; w0++)
            for (int dx = 1; dx + w0 <= DIRTY_TW / 2; dx++) {
                memset(fb, 0, sizeof fb);
                for (int y = 0; y < 6; y++) for (int x = 0; x < w0 * 2; x++) fb[y * DIRTY_W + x] = 0x5678;
                uint32_t a = dirty_tile_hash(fb, 0, 0);
                memset(fb, 0, sizeof fb);
                for (int y = 0; y < 6; y++) for (int x = dx * 2; x < (dx + w0) * 2; x++) fb[y * DIRTY_W + x] = 0x5678;
                uint32_t b = dirty_tile_hash(fb, 0, 0);
                tries++;
                if (a == b) misses++;
            }
        printf("  %d sideways moves, %d hash collisions\n", tries, misses);
        assert(misses == 0);
    }

    puts("-- mark.h: the same marks are no change; a different block, a skipped draw, the first frame are");
    {
        mark_t m; memset(&m, 0, sizeof m);
        dirty_rect_t b;
        mark_rect(&m, 10, 10, 5, 5, 0x1234);
        assert(mark_take(&m, &b)); expect(b, 10, 10, 5, 5);             /* the first frame ever */
        mark_rect(&m, 10, 10, 5, 5, 0x1234);
        assert(!mark_take(&m, &b)); expect(b, 0, 0, 0, 0);              /* the same again */
        mark_rect(&m, 10, 10, 5, 5, 0x1235);                            /* the colour moved */
        assert(mark_take(&m, &b)); expect(b, 10, 10, 5, 5);
        mark_rect(&m, 100, 10, 5, 5, 0x1235);                           /* the block moved: the union of both places */
        assert(mark_take(&m, &b)); expect(b, 10, 10, 95, 5);
        assert(mark_take(&m, &b)); expect(b, 100, 10, 5, 5);            /* the draw skipped: what was there is gone */
        assert(!mark_take(&m, &b)); expect(b, 0, 0, 0, 0);              /* nothing after nothing */
        mark_rect(&m, -10, -10, 20, 20, 1); mark_rect(&m, 230, 230, 20, 20, 2);
        assert(mark_take(&m, &b)); expect(b, 0, 0, 240, 240);           /* clipped to the glass */
        mark_val(&m, 7); mark_box(&m, 0, 0, 240, 240);                  /* a value + the whole frame (an arc) */
        assert(mark_take(&m, &b)); expect(b, 0, 0, 240, 240);
        mark_val(&m, 7); mark_box(&m, 0, 0, 240, 240);
        assert(!mark_take(&m, &b));
        mark_f(&m, 0.25f); mark_box(&m, 0, 0, 240, 240);
        assert(mark_take(&m, &b));
        mark_f(&m, 0.25f); mark_box(&m, 0, 0, 240, 240);
        assert(!mark_take(&m, &b));
        mark_force(&m);
        mark_f(&m, 0.25f); mark_box(&m, 0, 0, 240, 240);
        assert(mark_take(&m, &b)); expect(b, 0, 0, 240, 240);          /* forced: the same marks count */
    }
    puts("ALL GOOD");
    return 0;
}
