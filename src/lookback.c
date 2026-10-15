/* THE LOOK BACK AFTER THE FALL: the old wall's side in the new frame,
 * the gaze for its second. See lookback.h. */
#include "lookback.h"

int lookback_side(int old_wall, int new_wall)
{
    int o = old_wall & 3, n = new_wall & 3;
    if (o == n) return 0;
    if (o == ((n + 1) & 3)) return 1;       /* the next wall round is to his right */
    if (o == ((n + 3) & 3)) return -1;      /* the one before, to his left */
    return 2;                               /* opposite: the fall came through the centre */
}

bool lookback_gaze(int old_wall, int new_wall, float since_s, int *gx, int *gy)
{
    *gx = 0; *gy = 0;
    int side = lookback_side(old_wall, new_wall);
    if (!side || since_s < 0 || since_s >= LOOKBACK_S) return false;
    if (side == 2) *gy = LOOKBACK_GY;
    else *gx = side * LOOKBACK_GX;
    return true;
}

const char *lookback_name(int old_wall, int new_wall)
{
    switch (lookback_side(old_wall, new_wall)) {
    case 1:  return "his right";
    case -1: return "his left";
    case 2:  return "up, through the centre";
    default: return "nowhere";
    }
}
