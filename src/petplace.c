/* THE PET'S PLACE: the tap's point in his frame, cut in thirds; the
 * flank's lean and the base's slide as roads of time. See petplace.h. */
#include "petplace.h"
#include <math.h>

#define GLASS_PX 240

void petplace_frame(int wall, int dx, int dy, int *along, int *down)
{
    switch (wall & 3) {
    default: *along =  dx; *down =  dy; break;      /* the bottom wall: the screen's frame */
    case 1:  *along = -dy; *down =  dx; break;      /* the right wall: down is screen right, his right is screen up */
    case 2:  *along = -dx; *down = -dy; break;      /* the top wall: down is screen up */
    case 3:  *along =  dy; *down = -dx; break;      /* the left wall: down is screen left, his right is screen down */
    }
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

petplace_t petplace_of(int x, int y, int wall, int bx, int by, int bw, int bh)
{
    petplace_t p = { PETPLACE_CROWN, 0, 0, 0 };
    if (bw <= 0 || bh <= 0) return p;               /* nothing painted: the pet as today */
    /* the box's two corners in his frame: a quarter turn keeps a box a box */
    int a0, d0, a1, d1;
    petplace_frame(wall, bx - GLASS_PX / 2, by - GLASS_PX / 2, &a0, &d0);
    petplace_frame(wall, bx + bw - 1 - GLASS_PX / 2, by + bh - 1 - GLASS_PX / 2, &a1, &d1);
    int top = imin(d0, d1), bottom = imax(d0, d1);  /* his crown edge, his base edge */
    int centre = (imin(a0, a1) + imax(a0, a1)) / 2;
    int along, down;
    petplace_frame(wall, x - GLASS_PX / 2, y - GLASS_PX / 2, &along, &down);
    p.depth = down - top;
    p.along = along - centre;
    int h = bottom - top + 1;
    if (p.depth < h / 3) { p.place = PETPLACE_CROWN; p.side = 0; return p; }   /* the crown's third (and above it) */
    p.place = p.depth >= (2 * h) / 3 ? PETPLACE_BASE : PETPLACE_FLANK;
    p.side = p.along > PETPLACE_DEAD_PX ? 1 : p.along < -PETPLACE_DEAD_PX ? -1 : 0;
    return p;
}

float petplace_press_k(int place)
{
    return place == PETPLACE_FLANK ? PETPLACE_FLANK_PRESS_K : 1.0f;
}

/* the stroke's envelope reversed in sense, shortened in time: up over
 * the way in, then a straight ease back to 0 by PETPLACE_S */
static float lean_road(float since_s)
{
    if (since_s < 0) return 0;
    if (since_s < PETPLACE_LEAN_IN_S) return since_s / PETPLACE_LEAN_IN_S;
    if (since_s >= PETPLACE_S) return 0;
    return (PETPLACE_S - since_s) / (PETPLACE_S - PETPLACE_LEAN_IN_S);
}

/* the shove's road: out fast (a smoothstep), then home through its
 * place, a little past, back -- (1 - u) cos(pi u) -- one settle */
static float slide_road(float since_s)
{
    if (since_s < 0 || since_s >= PETPLACE_SLIDE_S) return 0;
    if (since_s < PETPLACE_SLIDE_OUT_S) {
        float t = since_s / PETPLACE_SLIDE_OUT_S;
        return t * t * (3.0f - 2.0f * t);
    }
    float u = (since_s - PETPLACE_SLIDE_OUT_S) / (PETPLACE_SLIDE_S - PETPLACE_SLIDE_OUT_S);
    return (1.0f - u) * cosf((float)M_PI * u);
}

void petplace_fx(const petplace_t *p, float since_s, float *a1, float *slide_px)
{
    *a1 = 0; *slide_px = 0;
    if (!p || p->side == 0) return;
    if (p->place == PETPLACE_FLANK) *a1 = -(float)p->side * PETPLACE_LEAN * lean_road(since_s);
    else if (p->place == PETPLACE_BASE) *slide_px = -(float)p->side * PETPLACE_SLIDE_PX * slide_road(since_s);
}

bool petplace_done(float since_s)
{
    return since_s >= PETPLACE_S;
}

const char *petplace_name(int place)
{
    return place == PETPLACE_FLANK ? "flank" : place == PETPLACE_BASE ? "base" : "crown";
}
