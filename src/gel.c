/* The gel: watchface.py's raster_cell + compose, in C, with the
 * geometry turned to gravity's angle and the base bent to the bowl.
 * See gel.h. Every number here that is not a blit offset is render.py's.
 *
 * Coordinates: the BODY frame is x toward the body's right, y DOWN
 * toward the floor, origin at the base centre (the dome lies at y < 0).
 * A frame_t turns body points into cell pixels: gravity's direction on
 * the cell is (sin a, cos a), the body's right (cos a, -sin a).
 */
#include "gel.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

/* render.PALETTES["blue"]: body / shade / outline / shine / eye */
const gel_pal_t GEL_PAL_BLUE = {
    {0.129f, 0.588f, 0.878f, 0.78f},
    {0.106f, 0.310f, 0.659f, 0.84f},
    {0.086f, 0.149f, 0.361f, 0.96f},
    {0.486f, 0.847f, 0.941f, 0.90f},
    {0.086f, 0.149f, 0.361f, 1.00f},
};

const char *const GEL_WALL_NAMES[4] = { "bottom", "right", "top", "left" };

/* RIPPLE_CURRENTS[0]: gentle slosh */
static const float CUR_C1 = 0.030f, CUR_W1 = 1.0f, CUR_C3 = 0.055f,
                   CUR_W3 = 1.0f, CUR_C5 = 0.030f, CUR_W5 = -2.0f;

#define ARC_N   9                           /* samples along the base's arc */
#define BAND_N  9                           /* samples along a bent band's edge */
#define MAX_PTS (GEL_N_POINTS + 1 + ARC_N + 2 * BAND_N)
#define CELL_N  (GEL_CELL_W * GEL_CELL_H)

typedef struct { float x, y; } pt_t;
typedef struct { float c, s; } frame_t;    /* cos / sin of the drawn angle */

/* a mask with the rows it touches, so a paint walks only those. One BIT
 * a pixel (the optimization pass: it was a byte a pixel -- two 4 KB
 * masks of internal RAM, and a 4 KB memset per polygon; now 512 B each
 * and a paint skips an empty row in one test). The cell is 64 wide, so
 * a row is exactly one word. */
_Static_assert(GEL_CELL_W == 64, "a mask row is one 64-bit word");
typedef struct { uint64_t row[GEL_CELL_H]; int y0, y1; } mask_t;

/* THE PAINTED BOX (the optimization pass, reiterated): every pixel a
 * paint touches lands in this box, so a raster clears only what the
 * last one painted, packs only what this one did, and the compose
 * blits only the box -- the cell is 64x64 for the turn, the dome ~30x18
 * of it. Exact by construction: paint() is the one writer of the cell. */
static struct { int x0, y0, x1, y1; int valid; } pbox;   /* x1/y1 exclusive */
static inline void pbox_row(int y, uint64_t bits)
{
    int xa = __builtin_ctzll(bits), xb = 64 - __builtin_clzll(bits);
    if (!pbox.valid) { pbox.x0 = xa; pbox.x1 = xb; pbox.y0 = y; pbox.y1 = y + 1; pbox.valid = 1; return; }
    if (xa < pbox.x0) pbox.x0 = xa;
    if (xb > pbox.x1) pbox.x1 = xb;
    if (y < pbox.y0) pbox.y0 = y;
    if (y + 1 > pbox.y1) pbox.y1 = y + 1;
}

/* python's round(): half to even */
static inline float pyround(float v) { return rintf(v); }
static inline float fmax1(float v) { return v < 1.0f ? 1.0f : v; }

int gel_wall_of(float phi)
{
    int q = (int)lrintf(phi / ((float)M_PI / 2.0f));   /* the nearest quarter: the wall the pull is closest to */
    return q & 3;
}

float gel_snap_angle(float angle)
{
    const float step = GEL_ANGLE_STEP_DEG * (float)M_PI / 180.0f;
    return rintf(angle / step) * step;
}

void gel_blob_radii(float squash, float *rx, float *ry)
{
    *rx = GEL_BASE_RX * (1.0f + 0.45f * squash);
    *ry = GEL_BASE_RY * (1.0f - 0.45f * squash);
}

/* THE STATURE WEEK: blob_radii's ry line solved for the squash */
float gel_stature_squash(float taller_px)
{
    return -taller_px / (GEL_BASE_RY * 0.45f);
}

/* the frame for an angle: the exact quarters snap to 0 / 1 so a
 * quarter turn is byte-for-byte the old quarter blit (no 6e-8 drift
 * across a pixel boundary) */
static frame_t frame_of(float angle)
{
    frame_t f;
    float q = angle / ((float)M_PI / 2.0f);
    float qr = rintf(q);
    if (fabsf(q - qr) < 1e-4f) {
        int k = (int)qr & 3;
        f.c = (k == 0) ? 1.0f : (k == 2) ? -1.0f : 0.0f;
        f.s = (k == 1) ? 1.0f : (k == 3) ? -1.0f : 0.0f;
    } else {
        f.c = cosf(angle);
        f.s = sinf(angle);
    }
    return f;
}

/* body -> cell: the base centre at the cell's middle, the body's right
 * along (c, -s), its down along (s, c) */
static inline pt_t xf(const frame_t *f, float bx, float by)
{
    pt_t p = { GEL_CELL_CX + bx * f->c + by * f->s,
               GEL_CELL_CY - bx * f->s + by * f->c };
    return p;
}

/* the bowl: how far up the glass's curve a point at x sits, relative to
 * the base centre on the rim (the circle's sagitta at x) */
static inline float sag(float x)
{
    const float lim = GEL_BOWL_R * 0.98f;
    if (x > lim) x = lim;
    if (x < -lim) x = -lim;
    return GEL_BOWL_R - sqrtf(GEL_BOWL_R * GEL_BOWL_R - x * x);
}

/* render.dome_points, line for line, in the body frame: the outline of
 * the dome right to left over the top, its ends on base_off (0 for the
 * outer dome, negative for the inner ones), every point lifted by the
 * bowl's sag at its x; then the base closed along the ARC (the straight
 * edge render.py closes with is the arc here); then turned. */
static int dome_poly(pt_t *out, const frame_t *f, float base_off, float rx, float ry,
                     float a2, float a1, float phase, float lump, float ripple, float rphase)
{
    const float e = 2.0f / GEL_DOME_N;
    const float tau = 2.0f * (float)M_PI * rphase;
    float bx[GEL_N_POINTS + 1], by[GEL_N_POINTS + 1];
    int n = 0;
    for (int i = 0; i <= GEL_N_POINTS; i++) {
        float th = (float)M_PI * i / GEL_N_POINTS;   /* 0 = right, pi = left */
        float ct = cosf(th), st = sinf(th);
        float k = 1.0f + a2 * cosf(2.0f * th + phase) + a1 * ct;
#ifdef GEL_CROWN_LUMP
        k += GEL_LUMP_AMP * lump * st * st * st;   /* the desk's own meal: the crown lifted (render.py's LUMP_AMP term; the head that grew) */
#endif
        if (ripple > 0.0f)
            k += ripple * (CUR_C1 * cosf(th + CUR_W1 * tau)
                           + CUR_C3 * cosf(3.0f * th + CUR_W3 * tau)
                           + CUR_C5 * cosf(5.0f * th - CUR_W5 * tau));
        float sx = powf(fabsf(ct), e);
        float sy = powf(fabsf(st), e);
        float c = 1.0f - sy;                          /* pull in near the floor */
        float hug = c * c * c * c;
        sx *= 1.0f - GEL_CHAMFER * hug;
#ifndef GEL_CROWN_LUMP
        /* THE BELLY (gel.h): the meal widens the lower flank, x only --
         * the weight sin(th) (1 - sy)^GEL_BELLY_POW peaks ~23 deg up, is
         * nothing at the foot and nothing at the crown, so he reads
         * fuller, never taller (Khalid 09-10: the crown lump was "its
         * head enlarges") */
        if (lump > 0.0f) sx *= 1.0f + GEL_BELLY_AMP * GEL_BELLY_NORM * lump * st * powf(c, GEL_BELLY_POW);
#endif
        bx[i] = rx * k * (ct >= 0.0f ? sx : -sx);
        by[i] = base_off - ry * k * sy;
        /* up the bowl's side: the full sag at the ends, fading up the
         * flank by the chamfer's own near-the-floor weight (the dome
         * above keeps its silhouette; a wide puddle is not a banana) */
        by[i] -= sag(bx[i]) * hug;
        out[n++] = xf(f, bx[i], by[i]);
    }
    /* the base: from the left end back to the right end along the arc */
    float xl = bx[GEL_N_POINTS], xr = bx[0];
    for (int i = 1; i <= ARC_N; i++) {
        float x = xl + (xr - xl) * i / (ARC_N + 1);
        out[n++] = xf(f, x, base_off - sag(x));
    }
    return n;
}

/* --- the raster primitives (watchface.py's, on the 1x cell) ------------------ */

/* polygon_mask: integer scanline fill of the closed polygon -- a pixel is
 * in when its CENTRE is, even-odd over the edges crossing the row's
 * centre line. Bits, one byte per cell pixel. */
static void polygon_mask(mask_t *m, const pt_t *pts, int n)
{
    memset(m->row, 0, sizeof m->row);
    float ymin = pts[0].y, ymax = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < ymin) ymin = pts[i].y;
        if (pts[i].y > ymax) ymax = pts[i].y;
    }
    int y0 = (int)floorf(ymin), y1 = (int)ceilf(ymax) + 1;
    if (y0 < 0) y0 = 0;
    if (y1 > GEL_CELL_H) y1 = GEL_CELL_H;
    m->y0 = y0;
    m->y1 = y1 > y0 ? y1 : y0;
    float xs[MAX_PTS];
    for (int y = y0; y < y1; y++) {
        float yc = y + 0.5f;
        int k = 0;
        for (int i = 0; i < n; i++) {
            pt_t p = pts[i], q = pts[(i + 1) % n];
            if ((p.y <= yc) == (q.y <= yc)) continue;   /* both above or both below */
            xs[k++] = p.x + (yc - p.y) * (q.x - p.x) / (q.y - p.y);
        }
        for (int i = 1; i < k; i++) {                   /* insertion sort, k tiny */
            float v = xs[i];
            int j = i - 1;
            while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
            xs[j + 1] = v;
        }
        for (int i = 0; i + 1 < k; i += 2) {
            int xa = (int)ceilf(xs[i] - 0.5f);          /* first centre >= xl */
            int xb = (int)ceilf(xs[i + 1] - 0.5f);      /* first centre >= xr (excluded) */
            if (xa < 0) xa = 0;
            if (xb > GEL_CELL_W) xb = GEL_CELL_W;
            if (xb > xa) {                              /* the bits xa..xb-1 of the row */
                uint64_t run = (xb - xa >= 64) ? ~0ULL : ((1ULL << (xb - xa)) - 1);
                m->row[y] |= run << xa;
            }
        }
    }
}

/* paint: source-over of one colour through a mask (and the clip, if
 * given) on the premultiplied float cell. Over the black glass only the
 * premultiplied colour matters: no alpha channel is kept. */
static void paint(float *cell, const mask_t *m, const mask_t *clip, gel_rgba_t c)
{
    float sr = c.r * c.a, sg = c.g * c.a, sb = c.b * c.a, ia = 1.0f - c.a;
    int y0 = m->y0, y1 = m->y1;
    if (clip) {
        if (clip->y0 > y0) y0 = clip->y0;
        if (clip->y1 < y1) y1 = clip->y1;
    }
    for (int y = y0; y < y1; y++) {
        uint64_t bits = m->row[y];
        if (clip) bits &= clip->row[y];
        if (bits) pbox_row(y, bits);
        while (bits) {                              /* each set bit, ascending x (the same pixels as before, one at a time) */
            int x = __builtin_ctzll(bits);
            bits &= bits - 1;
            float *px = cell + (y * GEL_CELL_W + x) * 3;
            px[0] = sr + px[0] * ia;
            px[1] = sg + px[1] * ia;
            px[2] = sb + px[2] * ia;
        }
    }
}

/* rect: render.px -- an integer rectangle of the body frame (x, y off
 * the base centre), turned into a quad and filled through the same
 * scanline rule (at the quarters it is the rectangle, pixel for pixel),
 * through the body clip */
static void rect(float *cell, mask_t *m, const mask_t *clip, const frame_t *f,
                 int x, int y, int w, int h, gel_rgba_t c)
{
    if (w <= 0 || h <= 0) return;
    pt_t q[4] = { xf(f, x, y), xf(f, x + w, y), xf(f, x + w, y + h), xf(f, x, y + h) };
    polygon_mask(m, q, 4);
    paint(cell, m, clip, c);
}

/* band: a horizontal band of the body frame, x0..x0+w wide, rows y..y+h,
 * BENT to the bowl (each edge follows the base's arc): the meniscus */
static void band(float *cell, mask_t *m, const mask_t *clip, const frame_t *f,
                 int x0, int y, int w, int h, gel_rgba_t c)
{
    if (w <= 0 || h <= 0) return;
    pt_t q[2 * BAND_N];
    for (int i = 0; i < BAND_N; i++) {
        float x = x0 + (float)w * i / (BAND_N - 1);
        q[i] = xf(f, x, y - sag(x));                           /* the top edge, left to right */
        q[2 * BAND_N - 1 - i] = xf(f, x, y + h - sag(x));      /* the bottom edge, back */
    }
    polygon_mask(m, q, 2 * BAND_N);
    paint(cell, m, clip, c);
}

/* the desk's integer placement, kept to the letter: round(cx + v) on
 * the desk's cell with an integer-valued centre column, off the pivot */
static inline int rx_(float v) { return (int)pyround(GEL_CELL_CX + v) - GEL_CELL_CX; }
static inline int ry_(float v) { return (int)pyround(GEL_CELL_CY + v) - GEL_CELL_CY; }

/* _eye_rects: one eye's glyph as rects (dx, dy, w, h, alpha-mult) off
 * its anchor. The subset the wrist wears; the table is the desk's. */
typedef struct { int dx, dy, w, h; float am; } eye_rect_t;

static int eye_rects(int kind, int side, int gy, int eye_w, eye_rect_t *o)
{
    int n = 0;
#define ER(DX, DY, W, H, AM) do { o[n].dx = (DX); o[n].dy = (DY); o[n].w = (W); \
                                  o[n].h = (H); o[n].am = (AM); n++; } while (0)
    switch (kind) {
    case GEL_EYE_CLOSED:  ER(0, 1, eye_w, 1, 1.0f); break;
    case GEL_EYE_SLEEP:   ER(0, 2, eye_w, 1, 1.0f); break;
    case GEL_EYE_WIDE:    ER(0, -1, eye_w, 4, 1.0f); break;
    case GEL_EYE_HAPPY:   /* an upturned arc: the anime ^ ^ */
        ER(1, 0, 3, 1, 1.0f); ER(0, 1, 1, 1, 1.0f); ER(4, 1, 1, 1, 1.0f); break;
    case GEL_EYE_CONTENT: /* the bars a touch lower, outer-top pixel shaved */
        ER(side < 0 ? 1 : 0, 1, 4, 1, 1.0f); ER(0, 2, 5, 1, 1.0f); break;
    case GEL_EYE_DROOPY:  /* heavy lids over thin bars */
        ER(0, 1, eye_w, 1, 0.45f); ER(0, 2, eye_w, 1, 1.0f); break;
    case GEL_EYE_SQUINT:  ER(0, gy + 1, eye_w, 1, 1.0f); break;
    default:              ER(0, gy, eye_w, 2, 1.0f); break;       /* open */
    }
#undef ER
    return n;
}

/* the desk's cell-cache quantisation (render._quant / watchface.Face.cell) */
static inline float q32(float v) { return rintf(v * 32.0f) / 32.0f; }
static inline float q8(float v) { return rintf(v * 8.0f) / 8.0f; }

/* THE RASTER CACHE (the optimization pass): the inputs are quantised
 * (32 steps of squash / a1 / a2, 8 of presence and gel, the snapped
 * angle), so at rest the same cell is asked for many frames running --
 * the breath crosses a squash quantum a few times a second, the gel
 * clock ticks an eighth once a second. When the quantised inputs, the
 * palette and the output buffer are the last call's, the cell already
 * holds the answer and nothing is rastered. Pixel-identical: the same
 * function on the same inputs. */
static struct {
    const uint16_t *out;
    gel_pal_t pal;
    float angle, squash, a2, a1, phase, gel, eye_alpha;
    int eye_kind, gaze_x, gaze_y;
    int valid;
    int bx, by, bw, bh;      /* the painted box of the cell in `out` (art px; w 0 = nothing) */
} last;
static struct { int x, y, w, h; } face_box;   /* the last compose's painted rect on the face */
void gel_cell_box(int *x, int *y, int *w, int *h)
{
    *x = last.valid ? last.bx : 0; *y = last.valid ? last.by : 0;
    *w = last.valid ? last.bw : GEL_CELL_W; *h = last.valid ? last.bh : GEL_CELL_H;
}
void gel_face_box(int *x, int *y, int *w, int *h) { *x = face_box.x; *y = face_box.y; *w = face_box.w; *h = face_box.h; }
static uint32_t rasters = 0, hits = 0;
void gel_raster_stats(uint32_t *rastered, uint32_t *cached) { if (rastered) *rastered = rasters; if (cached) *cached = hits; }
void gel_raster_forget(void) { last.valid = 0; }

int gel_raster_cell(uint16_t *out, const gel_pal_t *pal, const gel_cell_params_t *p)
{
    /* the float working cell: 64x64x3 floats = 48 KB. On the board it
     * lives in PSRAM (the TLS session needs the internal heap: with the
     * cell internal, the box dial died in mbedtls's SHA buffers); on the
     * laptop's preview a static will do. */
#ifdef ESP_PLATFORM
    static float *cell = NULL;
    if (!cell) {
        cell = (float *)heap_caps_malloc(sizeof(float) * CELL_N * 3, MALLOC_CAP_SPIRAM);
        if (!cell) cell = (float *)malloc(sizeof(float) * CELL_N * 3);
        memset(cell, 0, sizeof(float) * CELL_N * 3);   /* once: from here on only the painted box is ever cleared */
    }
#else
    static float cell[CELL_N * 3];
#endif
    static mask_t mask, clip;
    pt_t pts[MAX_PTS];
    int n, full_pack;

    const frame_t f = frame_of(p->angle);
    const float squash = q32(p->squash), a2 = q32(p->a2), a1 = q32(p->a1);
    const float lump = q32(p->lump < 0.0f ? 0.0f : p->lump > 1.0f ? 1.0f : p->lump);
    const float eye_alpha = q8(p->eye_alpha);
    float gel = q8(p->gel);
    gel -= floorf(gel);
    /* the cache: the quantised inputs (eye_kind / gaze only matter with
     * the eyes present) against the last raster into this same buffer */
    {
        int ek = eye_alpha > 0.01f ? p->eye_kind : -1;
        int gx = eye_alpha > 0.01f ? p->gaze_x : 0, gy = eye_alpha > 0.01f ? p->gaze_y : 0;
        if (last.valid && last.out == out && last.angle == p->angle && last.squash == squash && last.a2 == a2
            && last.a1 == a1 && last.phase == p->phase && last.gel == gel && last.eye_alpha == eye_alpha
            && last.eye_kind == ek && last.gaze_x == gx && last.gaze_y == gy
            && memcmp(&last.pal, pal, sizeof *pal) == 0) { hits++; return 0; }
        full_pack = !last.valid || last.out != out;   /* a buffer we have not written whole: every word of it this time */
        last.out = out; last.pal = *pal; last.angle = p->angle; last.squash = squash; last.a2 = a2; last.a1 = a1;
        last.phase = p->phase; last.gel = gel; last.eye_alpha = eye_alpha; last.eye_kind = ek; last.gaze_x = gx; last.gaze_y = gy;
        last.valid = 1;
        rasters++;
    }
    /* the float cell: clear what the last raster painted (the rest is
     * still zero -- the box is exact, paint() is the one writer), the
     * whole 48 KB only the first time. Pixel-identical to the full clear
     * it replaces, at ~1/6 of the PSRAM traffic. */
    if (!pbox.valid) memset(cell, 0, sizeof(float) * CELL_N * 3);
    else for (int y = pbox.y0; y < pbox.y1; y++)
        memset(cell + (y * GEL_CELL_W + pbox.x0) * 3, 0, sizeof(float) * 3 * (pbox.x1 - pbox.x0));
    const int prev_x0 = pbox.valid ? pbox.x0 : 0, prev_y0 = pbox.valid ? pbox.y0 : 0;
    const int prev_x1 = pbox.valid ? pbox.x1 : 0, prev_y1 = pbox.valid ? pbox.y1 : 0;
    pbox.valid = 0;
    const float ph = p->phase, rip = GEL_RIPPLE;
    float rx, ry;
    gel_blob_radii(squash, &rx, &ry);
    if (ry < 4.0f) ry = 4.0f;

    /* 1. outline */
    n = dome_poly(pts, &f, 0.0f, rx, ry, a2, a1, ph, lump, rip, gel);
    polygon_mask(&mask, pts, n);
    paint(cell, &mask, 0, pal->outline);
    /* the body clip: inset 1px all round and 2px along the ground */
    n = dome_poly(pts, &f, -2.0f, fmax1(rx - 2.0f), fmax1(ry - 3.0f), a2, a1, ph, lump, rip, gel);
    polygon_mask(&clip, pts, n);
    /* 2. the body starts entirely in shade */
    paint(cell, &clip, 0, pal->shade);
    /* 3. the lit body: the same dome lifted off the floor */
    n = dome_poly(pts, &f, -3.0f, fmax1(rx - 2.0f), fmax1(ry - 3.0f), a2, a1, ph, lump, rip, gel);
    polygon_mask(&mask, pts, n);
    paint(cell, &mask, &clip, pal->body);
    /* 4. gel depth: the pale inner dome, the meniscus band (bent to the bowl) */
    n = dome_poly(pts, &f, -6.0f, fmax1(rx - 5.0f), fmax1(ry - 8.0f), a2, a1, ph, lump, rip, gel);
    polygon_mask(&mask, pts, n);
    {
        gel_rgba_t up = { fminf(1.0f, pal->body.r * 1.18f), fminf(1.0f, pal->body.g * 1.14f),
                          fminf(1.0f, pal->body.b * 1.08f), 0.55f };
        paint(cell, &mask, &clip, up);
        gel_rgba_t men = { pal->shade.r * 0.82f, pal->shade.g * 0.82f, pal->shade.b * 0.90f, 0.70f };
        band(cell, &mask, &clip, &f, rx_(-rx + 3.0f), -5, (int)pyround(rx * 2.0f - 6.0f), 2, men);
    }
    /* 5. a little extra shade down the left flank */
    {
        int fw = (int)pyround(rx * 0.26f);
        if (fw < 2) fw = 2;
        rect(cell, &mask, &clip, &f, rx_(-rx), ry_(-ry * 0.58f), fw, (int)pyround(ry * 0.58f), pal->shade);
    }
    /* 6. micro-bubbles rising on the gel clock */
    {
        gel_rgba_t gl = { pal->shine.r, pal->shine.g, pal->shine.b, 0.45f };
        static const float bub[4][3] = {
            {-0.45f, 0.24f, 0.00f}, {0.34f, 0.38f, 0.37f},
            {-0.12f, 0.16f, 0.61f}, {0.10f, 0.52f, 0.85f} };
        for (int i = 0; i < 4; i++) {
            float t = gel + bub[i][2];
            t -= floorf(t);
            float bx = bub[i][0] + 0.06f * sinf(2.0f * (float)M_PI * t);
            float by = bub[i][1] + 0.34f * t;
            rect(cell, &mask, &clip, &f, rx_(rx * bx), ry_(-ry * by), 1, 1, gl);
        }
    }
    /* 7. (the suspended item: not on the wrist) */
    /* 8. highlights: the shoulder patch and the low-left counter-glint */
    {
        int hx = rx_(rx * 0.30f), hy = ry_(-ry * 0.86f);
        int hw = (int)pyround(rx * 0.42f), hh = (int)pyround(ry * 0.20f);
        if (hw < 3) hw = 3;
        if (hh < 2) hh = 2;
        rect(cell, &mask, &clip, &f, hx, hy, hw, hh, pal->shine);
        int hw2 = (int)pyround(rx * 0.28f);
        if (hw2 < 2) hw2 = 2;
        rect(cell, &mask, &clip, &f, hx + 1, hy + hh, hw2, 2, pal->shine);
        rect(cell, &mask, &clip, &f, rx_(-rx * 0.55f), ry_(-ry * 0.30f), 2, 1, pal->shine);
    }
    /* 9. eyes: two bars, inside the body clip, at their resting places */
    if (eye_alpha > 0.01f) {
        const int eye_w = 5, gap = 3;
        int gx = p->gaze_x, gy = p->gaze_y;
        int ey = ry_(-ry * 0.45f) - 1;
        float ea = pal->eye.a * (eye_alpha < 1.0f ? eye_alpha : 1.0f);
        for (int side = -1; side <= 1; side += 2) {
            int ex = side > 0 ? gap + gx : -gap - eye_w + gx;
            eye_rect_t er[3];
            int m = eye_rects(p->eye_kind, side, gy, eye_w, er);
            for (int i = 0; i < m; i++) {
                gel_rgba_t col = { pal->eye.r, pal->eye.g, pal->eye.b, ea * er[i].am };
                rect(cell, &mask, &clip, &f, ex + er[i].dx, ey + er[i].dy, er[i].w, er[i].h, col);
            }
        }
    }

    /* over the black glass a premultiplied pixel IS the result: round to
     * 8 bits (np.round: half to even) and pack the panel's 565. Only the
     * union of this raster's box and the last one's is packed (outside
     * it the out cell already holds the zeros a zero float packs to);
     * the whole cell when out is a buffer this cache has not filled. */
    int px0 = 0, py0 = 0, px1 = GEL_CELL_W, py1 = GEL_CELL_H;
    if (!full_pack) {
        px0 = prev_x1 > prev_x0 ? prev_x0 : GEL_CELL_W; py0 = prev_y1 > prev_y0 ? prev_y0 : GEL_CELL_H;
        px1 = prev_x1; py1 = prev_y1;
        if (pbox.valid) {
            if (pbox.x0 < px0) px0 = pbox.x0;
            if (pbox.y0 < py0) py0 = pbox.y0;
            if (pbox.x1 > px1) px1 = pbox.x1;
            if (pbox.y1 > py1) py1 = pbox.y1;
        }
    }
    for (int y = py0; y < py1; y++)
        for (int x = px0; x < px1; x++) {
            const int i = y * GEL_CELL_W + x;
            const float *px = cell + i * 3;
            int r = (int)rintf(px[0] * 255.0f), g = (int)rintf(px[1] * 255.0f), b = (int)rintf(px[2] * 255.0f);
            if (r < 0) r = 0;
            if (r > 255) r = 255;
            if (g < 0) g = 0;
            if (g > 255) g = 255;
            if (b < 0) b = 0;
            if (b > 255) b = 255;
            uint16_t w = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            out[i] = (uint16_t)((w >> 8) | (w << 8));
        }
    last.bx = pbox.valid ? pbox.x0 : 0; last.by = pbox.valid ? pbox.y0 : 0;
    last.bw = pbox.valid ? pbox.x1 - pbox.x0 : 0; last.bh = pbox.valid ? pbox.y1 - pbox.y0 : 0;
    return 1;
}

/* --- the disc is the glass: the place moved until every block is inside --- */

#define FIT_IN_MAX 12.0f            /* the most the base is ever drawn inward for a symmetric overflow (face px) */
int gel_fit_inside(const uint16_t *cell, float phi, float *lift, float *slide)
{
    if (!(last.valid && last.out == cell) || last.bw <= 0 || last.bh <= 0) return 0;
    if (*lift < 0.0f) return 0;                 /* under the rim on purpose (the house, the peekaboo) */
    const frame_t f = frame_of(phi);            /* the wall's exact sin / cos */
    const float R = GEL_FACE / 2.0f, R2 = R * R;
    int moved = 0;
    for (int pass = 0; pass < 8; pass++) {
        /* the compose's own placement of the cell */
        float d = GEL_FLOOR_DROP - *lift;
        int ox = (int)lrintf(d * f.s + *slide * f.c), oy = (int)lrintf(d * f.c - *slide * f.s);
        int x0 = GEL_FACE / 2 + ox - GEL_CELL_CX * GEL_SCALE, y0 = GEL_FACE / 2 + oy - GEL_CELL_CY * GEL_SCALE;
        float need_lift = 0, need_neg = 0, need_pos = 0, need_in = 0;   /* the least move each way, over every corner outside: less lift (the crown); a shift toward -across (a right corner out) or +across (a left one); more lift (the base drawn inward, for a corner out on BOTH sides -- a splat as wide as the chord) */
        for (int cy = last.by; cy < last.by + last.bh; cy++)
            for (int cx = last.bx; cx < last.bx + last.bw; cx++) {
                if (!cell[cy * GEL_CELL_W + cx]) continue;   /* black: not painted */
                for (int k = 0; k < 4; k++) {   /* the block's four corner pixels, their centres */
                    float X = x0 + cx * GEL_SCALE + ((k & 1) ? GEL_SCALE - 1 : 0) + 0.5f - R;
                    float Y = y0 + cy * GEL_SCALE + ((k & 2) ? GEL_SCALE - 1 : 0) + 0.5f - R;
                    if (X * X + Y * Y <= R2) continue;
                    float along = X * f.s + Y * f.c;         /* down gravity, from the centre */
                    float across = X * f.c - Y * f.s;        /* toward the body's right */
                    if (along < 0 && fabsf(along) > fabsf(across)) {   /* the crown, past the far rim: hang it higher */
                        float lim = R2 > across * across ? sqrtf(R2 - across * across) : 0;
                        float need = -along - lim;
                        if (need > need_lift) need_lift = need;
                    } else {                                          /* a flank or a base corner past the side: the body shifts away from it */
                        float lim = R2 > along * along ? sqrtf(R2 - along * along) : 0;
                        float need = fabsf(across) - lim;
                        if (across > 0) { if (need > need_neg) need_neg = need; }
                        else            { if (need > need_pos) need_pos = need; }
                        if (along > 0 && R2 > across * across) {           /* the same corner, brought in along gravity instead */
                            float in = along - sqrtf(R2 - across * across);
                            if (in > need_in) need_in = in;
                        }
                    }
                }
            }
        if (need_lift <= 0 && need_neg <= 0 && need_pos <= 0) break;
        if (need_lift > 0) {
            float dl = need_lift + 1.0f;        /* a pixel of margin for the compose's rounding */
            if (dl > *lift) dl = *lift;
            if (dl > 0) { *lift -= dl; moved = 1; }
        }
        /* the shift across: away from the side that pokes out (past the
         * axis if it must -- a leaning puddle sits a hair off centre);
         * out on both sides, the difference is all a shift can do; never
         * further than the rim's own radius */
        float ds;
        if (need_pos > 0 && need_neg > 0) {
            /* out on both sides (a splat as wide as the chord): no shift
             * helps -- the base is drawn a hair inward instead, where the
             * chord is wider (a few px at most, under a block: invisible);
             * past FIT_IN_MAX the excess is centred and what is left is the
             * glass's own limit */
            if (need_in > 0 && need_in <= FIT_IN_MAX) { *lift += need_in + 1.0f; moved = 1; continue; }
            ds = (need_pos - need_neg) * 0.5f;
        } else ds = (need_pos > 0 ? need_pos + 1.0f : 0) - (need_neg > 0 ? need_neg + 1.0f : 0);
        if (fabsf(ds) < 0.5f) ds = 0;
        if (ds != 0) {
            float ns = *slide + ds;
            if (ns > GEL_FLOOR_DROP) ns = GEL_FLOOR_DROP;
            if (ns < -GEL_FLOOR_DROP) ns = -GEL_FLOOR_DROP;
            if (ns != *slide) { *slide = ns; moved = 1; }
            else break;
        } else if (need_lift <= 0) break;
    }
    return moved;
}

/* --- the round face: a nearest blit of the cell at the place on the rim --- */

void gel_compose(uint16_t *fb, const uint16_t *cell, float phi, float lift, float slide)
{
    /* the base centre on the face: GEL_FLOOR_DROP - lift down gravity,
     * slide across it, rounded as one vector to a whole pixel */
    float sn = sinf(phi), cs = cosf(phi), d = GEL_FLOOR_DROP - lift;
    int ox = (int)lrintf(d * sn + slide * cs);
    int oy = (int)lrintf(d * cs - slide * sn);
    /* the cell's top-left on the face (its (CX, CY) corner at the centre + offset) */
    int x0 = GEL_FACE / 2 + ox - GEL_CELL_CX * GEL_SCALE;
    int y0 = GEL_FACE / 2 + oy - GEL_CELL_CY * GEL_SCALE;
    /* the cell's painted box (the whole cell when this cell is not the
     * cache's: the preview's, a test's) scaled onto the face and clipped;
     * outside the box the cell is black, which the cleared glass already is */
    int bx = 0, by = 0, bw = GEL_CELL_W, bh = GEL_CELL_H;
    if (last.valid && last.out == cell) { bx = last.bx; by = last.by; bw = last.bw; bh = last.bh; }
    int xa = x0 + bx * GEL_SCALE, ya = y0 + by * GEL_SCALE;
    int xb = xa + bw * GEL_SCALE, yb = ya + bh * GEL_SCALE;
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > GEL_FACE) xb = GEL_FACE;
    if (yb > GEL_FACE) yb = GEL_FACE;
    memset(fb, 0, (size_t)GEL_FACE * GEL_FACE * sizeof *fb);
    face_box.x = xa; face_box.y = ya; face_box.w = xb > xa ? xb - xa : 0; face_box.h = yb > ya ? yb - ya : 0;
    if (!face_box.w || !face_box.h) { face_box.w = face_box.h = 0; return; }
    const int R2 = GEL_FACE, rr4 = R2 * R2;         /* the round mask, in half-pixels */
    for (int y = ya; y < yb; y++) {
        int dy2 = 2 * y + 1 - R2;
        /* the row's chord: the x with (2x+1-R2)^2 <= rr4 - dy2^2, by an
         * exact integer square root -- the same pixels the per-pixel test
         * chose, without the test (the optimization pass, reiterated) */
        int lim = rr4 - dy2 * dy2;
        if (lim < 0) continue;
        int s = (int)sqrtf((float)lim);
        while (s * s > lim) s--;
        while ((s + 1) * (s + 1) <= lim) s++;
        int xl = (R2 - s) / 2, xr = (R2 - 1 + s) / 2;           /* 2x+1 >= R2-s ; 2x+1 <= R2+s */
        if (xl < xa) xl = xa;
        if (xr >= xb) xr = xb - 1;
        if (xr < xl) continue;
        const uint16_t *src = cell + ((y - y0) / GEL_SCALE) * GEL_CELL_W;
        uint16_t *row = fb + y * GEL_FACE;
        /* the blit: each cell pixel is GEL_SCALE face pixels wide; a
         * counter steps the source instead of a divide a pixel */
        int sx = (xl - x0) / GEL_SCALE, k = (xl - x0) - sx * GEL_SCALE;
        uint16_t v = src[sx];
        for (int x = xl; x <= xr; x++) {
            row[x] = v;
            if (++k == GEL_SCALE && x < xr) { k = 0; v = src[++sx]; }
        }
    }
}
