/* THE LOOK BACK AFTER THE FALL (design/watch-night-2026-10-12.md C2,
 * THE OPERATOR NIGHT): a creature that fell to a new wall looks back at
 * where it was. After the slump ends (the fall's last phase, FALL_LAND,
 * running out -- never earlier: a body mid-splat looks at nothing) the
 * gaze turns toward the wall it came from, read in ITS NEW FRAME, for
 * LOOKBACK_S, then forward. The four walls go round in walls.h's order
 * (0 bottom / 1 right / 2 top / 3 left) and the next wall round is
 * always to his right (the body's right on the face is (cos phi,
 * -sin phi): from the bottom wall that is screen right, the right
 * wall; from the right wall screen up, the top wall ...), so:
 *   the old wall = the next round   -> his right along the floor (gaze_x +LOOKBACK_GX)
 *   the old wall = the one before   -> his left (gaze_x -LOOKBACK_GX)
 *   the old wall = the one opposite -> a half turn's fall through the
 *                                      centre: up (gaze_y LOOKBACK_GY)
 *   the same wall                   -> nothing (there was no fall).
 * Never during the brace in the sense that the brace is never broken:
 * the squint stays (eye_kind is the brace's for its minute) and the
 * look rides under it -- the pupils turn, the lids do not. The far
 * tap's own px: 2 along, 1 up. No new shape, no light, no sound.
 *
 * Pure C, no Arduino: the two walls and the seconds since the slump
 * ended in, the gaze out. tools/lookback_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/lookback_test tools/lookback_test.cpp -x c src/lookback.c && /tmp/lookback_test
 * (idle.cpp's LOOK_BACK_S is the idle glance's own return; these are
 * LOOKBACK_*, no collision.) */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOOKBACK_S   1.0f    /* the look's second */
#define LOOKBACK_GX  2       /* along his floor, the far tap's own px */
#define LOOKBACK_GY  -1      /* up, for the fall through the centre (negative = up, the glance's own sign) */

/* where the old wall lies in the new frame: +1 his right, -1 his left,
 * 2 opposite (through the centre), 0 the same wall */
int lookback_side(int old_wall, int new_wall);

/* the gaze `since_s` after the slump ended: true while the look runs
 * (gx, gy set), false once it is over or there was no fall (both 0) */
bool lookback_gaze(int old_wall, int new_wall, float since_s, int *gx, int *gy);

/* "his left" / "his right" / "up, through the centre" / "nowhere" */
const char *lookback_name(int old_wall, int new_wall);

#ifdef __cplusplus
}
#endif
