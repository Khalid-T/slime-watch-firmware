#include "word.h"
#include "app.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */
#include "board.h"
#include "mark.h"
#include "font5x7.h"     /* THE BUBBLE READS (2026-09-10): a 5x7 with a real lowercase -- the 3x5 could not draw m or w */
#include <Arduino.h>
#include <string.h>
#include <math.h>

static mark_t mk;                       /* THE MARK (mark.h): what this frame drew */
bool word_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

/* --- the tunables ------------------------------------------------------------ */
static const int PX      = 2;           /* the font's pixel on the glass (the clock's glance uses 3): a glyph 10 x 14 px */
static const int LINES   = 3;           /* the bubble's height, at most */
static const int RADIUS  = 70;          /* the bubble's centre from the face's centre, at its wall (THE PLACE: opposite the gel) */
static const int MARGIN  = 6;           /* px kept from the round rim */
static const int G_W     = FONT5X7_W;   /* the glyph: 5 cells wide, 7 tall (font5x7.h) */
static const int G_H     = FONT5X7_H;
static const int CH_W    = G_W + 1;     /* cells per glyph column: 5 + 1 gap (a line of 10 glyphs is 118 px) */
static const int LINE_H  = G_H + 2;     /* cells per line: 7 + 2 (18 px a line at PX 2; three lines 50 px) */
static const int SIDE_COLS = 8;         /* on the left / right walls the lines are capped so the bubble stays in its own half of the glass */
/* THE PLATE: a dark bubble under the lines so the letters cut on black
 * glass, on the rings and on the gel alike -- the palette's darkest
 * (the outline's ink, darkened) padded PAD px round each line, a 1 px
 * rim in the shine at a low alpha round that. The old 1 px outline
 * pass round each letter (which did not cut on the gel's blue) is gone. */
static const int   PAD        = 2;
static const float PLATE_K    = 0.55f;  /* the plate: the outline colour x this (near-black navy, not the glass's own black) */
static const float RIM_K      = 0.30f;  /* the rim: the shine x this */
#define WORD_MAX 140

enum { W_NONE, W_HELD, W_SHOWN };
static int  wst = W_NONE;
static char text[WORD_MAX + 1] = "";

/* --- the font: 5x7, font5x7.h (each row 5 bits, msb left) --------------------- */
static const uint8_t *glyph(char c)
{
    if (c < FONT5X7_FIRST || c > FONT5X7_LAST) c = '?';
    return FONT5X7[c - FONT5X7_FIRST];
}

/* --- the state ----------------------------------------------------------------- */

void word_set(const char *s)
{
    if (!s || !s[0]) return;
    if (wst != W_NONE) LOG("word: '%s' replaces the one %s", s, wst == W_HELD ? "held" : "showing");
    size_t n = 0;
    for (const char *p = s; *p && n < WORD_MAX; p++)
        if (*p >= 32 && *p < 127) text[n++] = *p;          /* printable ASCII only */
    text[n] = 0;
    wst = W_HELD;
    LOG("word: %s", text);
}

static void show(const char *how)
{
    wst = W_SHOWN;
    LOG("word: shown (%s)", how);
}

void word_glance(void) { if (wst == W_HELD) show("a raise"); }

bool word_tap(void)
{
    if (wst == W_SHOWN) { wst = W_NONE; text[0] = 0; LOG("word: dismissed"); return true; }
    if (wst == W_HELD) show("a tap");
    return false;
}

bool word_showing(void) { return wst == W_SHOWN; }
bool word_held(void) { return wst == W_HELD; }
const char *word_text(void) { return wst == W_NONE ? "" : text; }

/* --- the bubble ------------------------------------------------------------------ */

static inline uint16_t rgb565(float r, float g, float b, float k)
{
    int R = (int)(r * k * 31 + 0.5f), G = (int)(g * k * 63 + 0.5f), B = (int)(b * k * 31 + 0.5f);
    uint16_t c = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((c >> 8) | (c << 8));            /* the panel's byte order */
}

static void fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c)
{
    mark_rect(&mk, x, y, w, h, c);
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= SCREEN_H) continue;
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= SCREEN_W) continue;
            fb[yy * SCREEN_W + xx] = c;
        }
    }
}

/* the disc's half-chord at a line whose middle sits dy from the face's
 * centre: at the line's far edge (the plate and its rim counted), the
 * margin off the rim */
static int half_chord(int dy)
{
    float r = SCREEN_W / 2.0f - MARGIN;
    float d = fabsf((float)dy) + (G_H * PX) / 2.0f + PAD + 1;
    if (d >= r) return 0;
    return (int)sqrtf(r * r - d * d);
}

/* how many glyphs fit a span of ``w`` px, the plate's padding counted */
static int cols_in(int w)
{
    int cols = (w - 2 * (PAD + 1) + PX) / (CH_W * PX);
    return cols < 1 ? 1 : cols;
}

/* greedy word wrap into at most LINES lines of cols[i] glyphs; a word
 * longer than a line is broken; what does not fit is cut, the last
 * glyph a '+' */
static int wrap(const char *s, const int *cols, char out[LINES][WORD_MAX + 1])
{
    int li = 0, n = 0;
    const char *p = s;
    while (*p == ' ') p++;
    while (*p && li < LINES) {
        int cap = cols[li];
        const char *q = p; int len = 0;
        while (q[len] && q[len] != ' ' && len < cap) len++;
        if (len == 0) { p++; continue; }
        if (n && n + 1 + len > cap) {
            /* the next word does not fit: the line ends here */
            out[li][n] = 0; li++; n = 0;
            continue;
        }
        if (n) out[li][n++] = ' ';
        memcpy(out[li] + n, q, len); n += len;
        p = q + len;
        while (*p == ' ') p++;
        if (n >= cap && *p) { out[li][n] = 0; li++; n = 0; }
    }
    if (li < LINES) { out[li][n] = 0; if (n) li++; }
    if (*p && li == LINES) {                          /* cut: the last glyph says so */
        int last = li - 1;
        int len = strlen(out[last]);
        if (len >= cols[last]) len = cols[last] - 1;
        out[last][len] = '+'; out[last][len + 1] = 0;
    }
    return li;
}

/* THE LAYOUT at a wall: the bubble's centre RADIUS out along the wall's
 * outward direction (less ``inset``: inward of the hour when they share
 * the wall), the lines wrapped to the disc's chord at each row, their
 * union box. The same layout serves word_box (THE PLACE's test) and
 * the draw. */
static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { 1, 0, -1, 0 };
static int layout(int at, int inset, char lines[LINES][WORD_MAX + 1], int lcx[LINES], int *y0_out, place_box_t *box)
{
    const int dx = DX[at & 3];
    int cx = SCREEN_W / 2 + dx * (RADIUS - inset), cy = SCREEN_H / 2 + DY[at & 3] * (RADIUS - inset);
    /* the lines' room. The top and bottom walls put the bubble near the
     * rim, each line on the face's axis, as wide as the disc's chord at
     * its row (10 / 14 / 16 glyphs at the top). The sides put it on the
     * widest rows, where a line is laid in its OWN SPAN between the rim
     * (that row's chord) and the centre less the margin, centred in it,
     * capped at SIDE_COLS -- so the bubble stays in its half of the
     * glass and never pokes past the rim (8 glyphs: 94 px) */
    int cols[LINES];
    for (int i = 0; i < LINES; i++) {
        int dy = cy - SCREEN_H / 2 + (i - (LINES - 1) / 2) * LINE_H * PX;
        int hc = half_chord(dy);
        if (!dx) { cols[i] = hc ? cols_in(2 * hc) : 1; lcx[i] = cx; continue; }
        int rim = SCREEN_W / 2 - hc, mid = SCREEN_W / 2 - MARGIN;        /* the left wall's span: the rim to the centre */
        int lo = rim + inset, hi = mid + inset;                          /* inward of the hour: the whole span moves in */
        if (dx > 0) { int t = lo; lo = SCREEN_W - hi; hi = SCREEN_W - t; }   /* the right wall: mirrored */
        int c = hc ? cols_in(hi - lo) : 1;
        cols[i] = c > SIDE_COLS ? SIDE_COLS : c;
        lcx[i] = (lo + hi) / 2;
    }
    int n = wrap(text, cols, lines);
    int y0 = cy - (n * LINE_H * PX) / 2 + PX;
    *y0_out = y0;
    /* the box: the lines' union, the plate and its rim included (what THE PLACE tests) */
    int bx0 = SCREEN_W, bx1 = 0;
    for (int i = 0; i < n; i++) {
        int w = ((int)strlen(lines[i]) * CH_W - 1) * PX;
        int x0 = lcx[i] - w / 2 - (PAD + 1), x1 = x0 + w + 2 * (PAD + 1);
        if (x0 < bx0) bx0 = x0;
        if (x1 > bx1) bx1 = x1;
    }
    box->x = n ? bx0 : 0; box->y = n ? y0 - (PAD + 1) : 0;
    box->w = n ? bx1 - bx0 : 0; box->h = n ? (n - 1) * LINE_H * PX + G_H * PX + 2 * (PAD + 1) : 0;
    return n;
}

void word_box(int at, int inset, place_box_t *out)
{
    out->x = out->y = out->w = out->h = 0;
    if (wst != W_SHOWN || !text[0]) return;
    char lines[LINES][WORD_MAX + 1];
    int lcx[LINES], y0;
    layout(at, inset, lines, lcx, &y0, out);
}

void word_draw(uint16_t *fb, int at, int inset)
{
    if (wst != W_SHOWN || !text[0]) return;
    const gel_rgba_t *s = &theme_pal()->shine, *o = &theme_pal()->outline;
    uint16_t ink = rgb565(s->r, s->g, s->b, 1.0f);
    uint16_t plate = rgb565(o->r, o->g, o->b, PLATE_K);   /* the palette's darkest, darker: the bubble's glass */
    uint16_t rim = rgb565(s->r, s->g, s->b, RIM_K);       /* its edge, faint */
    char lines[LINES][WORD_MAX + 1];
    int lcx[LINES], y0;
    place_box_t box;
    int n = layout(at, inset, lines, lcx, &y0, &box);
    if (!n) return;
    /* the plate under every line first (each line's rim, then its
     * fill: adjacent lines' plates touch, so the rim between them is
     * painted over and the bubble reads as one), then the ink */
    for (int pass = 0; pass < 3; pass++)
        for (int i = 0; i < n; i++) {
            int len = strlen(lines[i]);
            int w = (len * CH_W - 1) * PX;
            int x0 = lcx[i] - w / 2, y = y0 + i * LINE_H * PX;
            if (pass == 0) fill(fb, x0 - PAD - 1, y - PAD - 1, w + 2 * (PAD + 1), G_H * PX + 2 * (PAD + 1), rim);
            else if (pass == 1) fill(fb, x0 - PAD, y - PAD, w + 2 * PAD, G_H * PX + 2 * PAD, plate);
            else
                for (int k = 0; k < len; k++) {
                    const uint8_t *g = glyph(lines[i][k]);
                    for (int row = 0; row < G_H; row++)
                        for (int col = 0; col < G_W; col++)
                            if (g[row] & (1 << (G_W - 1 - col)))
                                fill(fb, x0 + (k * CH_W + col) * PX, y + row * PX, PX, PX, ink);
                }
        }
}
