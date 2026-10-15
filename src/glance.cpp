#include "glance.h"
#include "hourfmt.h"
#include "place.h"
#include "app.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */
#include "board.h"
#include "mark.h"
#include <Arduino.h>
#include <string.h>

static mark_t mk;                       /* THE MARK (mark.h): what this frame drew */
bool glance_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

static const float GLANCE_S = 6.0f;
static const float GLANCE_SHORT_S = 4.0f;   /* BATTERY MANNERS: the stay under 20 % (the hour read, the glass not kept) */
static const float GLANCE_FADE_S = 0.5f;
static const int   PX = 3;              /* the font's pixel on the glass */
static const int   RADIUS = 70;         /* the hour's centre from the face's centre, at its wall (THE PLACE: the top, or the bottom while the gel is on the top wall) */
/* THE SECOND LOOK (design/watch-ideas.md B15, 2026-09-10): a raise
 * within SECOND_LOOK_S of the last glance means he looked and missed
 * it (the sun on the glass, a glance too quick), so the hour is held
 * SECOND_LOOK_X times as long -- 12 s. One timer, nothing else. */
static const float SECOND_LOOK_S = 30.0f;
static const float SECOND_LOOK_X = 2.0f;

static float glance_t = 0;
static char  shown[6] = "";
static bool  h12 = false;               /* MORE SETTINGS' CLOCK row: `7:41` (hourfmt.h) instead of `07:41` */
static bool  shade = false;             /* THE NIGHT LIGHT: the shade colour, not the shine */
static bool  short_stay = false;        /* BATTERY MANNERS: the shorter stay */
static uint32_t last_show_ms = 0;       /* THE SECOND LOOK: when the hour last came up on a raise (0 = never) */

/* 3x5 digits and the colon: each row 3 bits, msb left */
static const uint8_t DIGITS[10][5] = {
    { 0b111, 0b101, 0b101, 0b101, 0b111 },  /* 0 */
    { 0b010, 0b110, 0b010, 0b010, 0b111 },  /* 1 */
    { 0b111, 0b001, 0b111, 0b100, 0b111 },  /* 2 */
    { 0b111, 0b001, 0b111, 0b001, 0b111 },  /* 3 */
    { 0b101, 0b101, 0b111, 0b001, 0b001 },  /* 4 */
    { 0b111, 0b100, 0b111, 0b001, 0b111 },  /* 5 */
    { 0b111, 0b100, 0b111, 0b101, 0b111 },  /* 6 */
    { 0b111, 0b001, 0b010, 0b010, 0b010 },  /* 7 */
    { 0b111, 0b101, 0b111, 0b101, 0b111 },  /* 8 */
    { 0b111, 0b101, 0b111, 0b001, 0b111 },  /* 9 */
};
static const uint8_t COLON[5] = { 0b000, 0b010, 0b000, 0b010, 0b000 };

void glance_show(void)
{
    if (!clock_is_set()) { LOG("glance: no clock to show"); return; }
    struct tm lt; uint32_t ep;
    clock_local(&lt, &ep);
    hour_fmt(shown, sizeof shown, lt.tm_hour * 60 + lt.tm_min, h12, false);
    /* THE SECOND LOOK: a raise soon after the last glance holds the hour twice as long */
    uint32_t now = millis();
    bool second = last_show_ms && (now - last_show_ms) <= (uint32_t)(SECOND_LOOK_S * 1000.0f);
    last_show_ms = now ? now : 1;
    glance_t = (short_stay ? GLANCE_SHORT_S : GLANCE_S) * (second ? SECOND_LOOK_X : 1.0f);
    LOG("glance: %s%s%s", shown, second ? " (the second look: held twice as long)" : "", short_stay ? " (the cell under 20 %: held shorter)" : "");
}

void glance_set_short(bool on) { short_stay = on; }
void glance_set_h12(bool on) { h12 = on; }

void glance_tick(float dt)
{
    if (glance_t > 0) {
        glance_t -= dt;
        if (glance_t <= 0) { glance_t = 0; shown[0] = 0; }
        else if (glance_t > GLANCE_FADE_S) {
            /* the minute may turn while it shows */
            struct tm lt; uint32_t ep;
            clock_local(&lt, &ep);
            hour_fmt(shown, sizeof shown, lt.tm_hour * 60 + lt.tm_min, h12, false);
        }
    }
}

bool glance_on(void) { return glance_t > 0; }
void glance_set_shade(bool on) { shade = on; }

static inline uint16_t rgb565(float r, float g, float b, float k)
{
    int R = (int)(r * k * 31 + 0.5f), G = (int)(g * k * 63 + 0.5f), B = (int)(b * k * 31 + 0.5f);
    uint16_t c = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((c >> 8) | (c << 8));            /* the panel's byte order */
}

static void block(uint16_t *fb, int x, int y, uint16_t c)
{
    mark_rect(&mk, x, y, PX, PX, c);
    for (int yy = y; yy < y + PX; yy++) {
        if (yy < 0 || yy >= SCREEN_H) continue;
        for (int xx = x; xx < x + PX; xx++) {
            if (xx < 0 || xx >= SCREEN_W) continue;
            fb[yy * SCREEN_W + xx] = c;
        }
    }
}

/* the hour's spot AT a wall: the wall's outward direction, RADIUS out
 * (less an inset), the five glyphs centred there -- the box place.cpp
 * tests, and the clockwork's number's spot too */
static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { 1, 0, -1, 0 };
void glance_box(int at, int inset, place_box_t *out)
{
    int cx = SCREEN_W / 2 + DX[at & 3] * (RADIUS - inset), cy = SCREEN_H / 2 + DY[at & 3] * (RADIUS - inset);
    int w = 5 * 4 - 1;
    out->x = cx - (w * PX) / 2; out->y = cy - (5 * PX) / 2; out->w = w * PX; out->h = 5 * PX;
}

void glance_draw(uint16_t *fb, int at)
{
    if (glance_t <= 0 || !shown[0]) return;
    /* the shine, dimming over the last half second */
    float k = glance_t < GLANCE_FADE_S ? glance_t / GLANCE_FADE_S : 1.0f;
    k = 0.15f + 0.85f * k;
    const gel_rgba_t *s = shade ? &theme_pal()->shade : &theme_pal()->shine;   /* the night light: the shade only */
    uint16_t c = rgb565(s->r, s->g, s->b, k);
    /* AT the wall THE PLACE chose (place.h: the top, or the bottom while the gel is on the top wall) */
    int cx = SCREEN_W / 2 + DX[at & 3] * RADIUS, cy = SCREEN_H / 2 + DY[at & 3] * RADIUS;
    /* five glyphs, 3 cells wide, one cell apart */
    int n = strlen(shown);
    int w = n * 4 - 1;
    int x0 = cx - (w * PX) / 2, y0 = cy - (5 * PX) / 2;
    for (int i = 0; i < n; i++) {
        const uint8_t *g = shown[i] == ':' ? COLON : DIGITS[(shown[i] - '0') % 10];
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (g[row] & (1 << (2 - col)))
                    block(fb, x0 + (i * 4 + col) * PX, y0 + row * PX, c);
    }
}
