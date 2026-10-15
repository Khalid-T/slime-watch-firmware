/* THE HOUR'S GLANCE: the :00 edge, the gates, the clock, the rim
 * point. See hourlook.h. */
#include "hourlook.h"
#include <math.h>

static struct {
    int   minute_prev;   /* the last frame's minute, -1 = none yet */
    float left;          /* the glance's seconds left, 0 = none */
    int   count;
} H = { -1, 0, 0 };

int hourlook_tick(const hourlook_in_t *in)
{
    int ev = HOURLOOK_EV_NONE;
    int prev = H.minute_prev;
    H.minute_prev = in->minute;
    if (H.left > 0) {
        if (!in->lit || !in->holding || in->page) H.left = 0;      /* the glass gone, the house empty, a page: forward */
        else { H.left -= in->dt; if (H.left < 1e-4f) H.left = 0; }   /* frames of float add up a hair short */
    }
    /* the edge: the minute TURNED to :00 (a clock set at :00 is no turn) */
    bool edge = prev >= 0 && in->minute >= 0 && in->minute != prev && in->minute % 60 == 0;
    if (edge && in->lit && in->holding && !in->busy && !in->page) {
        H.left = HOURLOOK_S; H.count++;
        ev = HOURLOOK_EV_GLANCE;
    }
    return ev;
}

bool  hourlook_on(void)     { return H.left > 0; }
float hourlook_left_s(void) { return H.left; }
int   hourlook_count(void)  { return H.count; }

void hourlook_point(int tick_deg, float cx, float cy, float r, float *x, float *y)
{
    float a = (float)tick_deg * (float)M_PI / 180.0f;   /* 0 = 12 o'clock, clockwise as the face draws it */
    *x = cx + r * sinf(a);
    *y = cy - r * cosf(a);
}
