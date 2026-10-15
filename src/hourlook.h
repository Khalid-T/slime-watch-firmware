/* THE HOUR'S GLANCE (design/watch-night-2026-10-12.md C15, THE
 * OPERATOR NIGHT): when the minute turns to :00 while the glass is lit,
 * HOLDING, the body free and no page up, one glance toward the stacked
 * hands' rim point -- the hour tick's angle (face.cpp's
 * `face_hour_tick_deg`: at :00 the minute's thin shine tick lies at 12
 * and the hour's stout shade tick at its hour, the hands as the face
 * draws them) -- for HOURLOOK_S, then forward. A creature in a bowl
 * with a clock on the rim looks at the hand when it moves. Never a
 * sound, never a bump, never a light; the hour text's own six seconds
 * (glance.cpp) untouched; the :00 that comes with the glass dark, the
 * house empty, a page up or the body busy is missed, not caught up (a
 * moment, not a debt).
 *
 * The module is the edge, the gates and the clock, and the rim point
 * from the angle (12 up, clockwise, HOURLOOK_R from the centre: the
 * hour tick's band). main turns the point into the gaze with the far
 * tap's own `look_at` (the same road a tap off him takes: 2 px along
 * his floor, 1 up, in HIS frame), for HOURLOOK_S.
 *
 * Pure C, no Arduino. tools/hourlook_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/hourlook_test tools/hourlook_test.cpp -x c src/hourlook.c -lm && /tmp/hourlook_test */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOURLOOK_S   1.5f     /* the glance, then forward (the far tap's own second and a half) */
#define HOURLOOK_R   110.0f   /* the rim point's radius: the middle of the hour tick's band (face.cpp r 107-112) */

typedef struct {
    float dt;
    int   minute;        /* clock_local_minute(): the minute of the local day, -1 = no clock */
    bool  lit;           /* the glass is on */
    bool  holding;       /* house_holding() */
    bool  busy;          /* idle's busy, a thing of idle's, the word: something else has the body */
    bool  page;          /* THE WATCH PAGE up */
} hourlook_in_t;

enum { HOURLOOK_EV_NONE = 0, HOURLOOK_EV_GLANCE };   /* the minute turned :00 and the glance began */

int   hourlook_tick(const hourlook_in_t *in);   /* one call a frame; the event */
bool  hourlook_on(void);                        /* the glance runs */
float hourlook_left_s(void);                    /* seconds of it left, 0 = none */
int   hourlook_count(void);                     /* glances since boot */

/* the rim point at the hour tick's angle (degrees clockwise from 12
 * o'clock, face_hour_tick_deg's), r from the glass's centre (cx, cy) */
void  hourlook_point(int tick_deg, float cx, float cy, float r, float *x, float *y);

#ifdef __cplusplus
}
#endif
