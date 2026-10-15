/* THE LOOK BACK AFTER THE FALL on the laptop (lookback.c is pure C):
 *   c++ -std=gnu++17 -I src -o /tmp/lookback_test tools/lookback_test.cpp -x c src/lookback.c && /tmp/lookback_test
 * Every pair of walls: the old wall's side in the new frame (the next
 * round is his right, the one before his left, the opposite is up);
 * the gaze for the second (the far tap's px), nothing at 1.0 s, nothing
 * before the slump ended (a negative since), nothing for the same wall;
 * the names. */
#include "lookback.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *W[4] = { "bottom", "right", "top", "left" };

int main(void)
{
    puts("-- the old wall's side in the new frame");
    for (int n = 0; n < 4; n++) {
        CHECK(lookback_side((n + 1) & 3, n) == 1, "fell from the %s to the %s: the old wall is to his right", W[(n + 1) & 3], W[n]);
        CHECK(lookback_side((n + 3) & 3, n) == -1, "fell from the %s to the %s: the old wall is to his left", W[(n + 3) & 3], W[n]);
        CHECK(lookback_side((n + 2) & 3, n) == 2, "fell from the %s to the %s: through the centre, the old wall is up", W[(n + 2) & 3], W[n]);
        CHECK(lookback_side(n, n) == 0, "the %s wall to itself: no fall, nowhere", W[n]);
    }
    /* the concrete ones a wrist does: the watch read level (bottom), rolled to the right wall and back */
    CHECK(lookback_side(0, 1) == -1, "bottom -> right: it came from his left (screen down is his left on the right wall)");
    CHECK(lookback_side(1, 0) == 1, "right -> bottom: it came from his right (screen right)");
    CHECK(lookback_side(0, 2) == 2, "bottom -> top: through the centre");

    puts("-- the gaze for its second");
    int gx, gy;
    CHECK(lookback_gaze(1, 0, 0.0f, &gx, &gy) && gx == LOOKBACK_GX && gy == 0, "right -> bottom at 0 s: the gaze %+d,%+d (his right)", gx, gy);
    CHECK(lookback_gaze(1, 0, 0.99f, &gx, &gy) && gx == LOOKBACK_GX, "... still at 0.99 s");
    CHECK(!lookback_gaze(1, 0, 1.0f, &gx, &gy) && gx == 0 && gy == 0, "... forward at %.0f s", LOOKBACK_S);
    CHECK(lookback_gaze(3, 0, 0.5f, &gx, &gy) && gx == -LOOKBACK_GX && gy == 0, "left -> bottom: the gaze %+d,%+d (his left)", gx, gy);
    CHECK(lookback_gaze(2, 0, 0.5f, &gx, &gy) && gx == 0 && gy == LOOKBACK_GY, "top -> bottom (through the centre): the gaze %+d,%+d (up)", gx, gy);
    CHECK(!lookback_gaze(0, 0, 0.5f, &gx, &gy) && gx == 0 && gy == 0, "the same wall: no look");
    CHECK(!lookback_gaze(1, 0, -0.1f, &gx, &gy), "before the slump ended: nothing (the body mid-splat looks at nothing)");

    puts("-- the names");
    CHECK(!strcmp(lookback_name(1, 0), "his right"), "right -> bottom: %s", lookback_name(1, 0));
    CHECK(!strcmp(lookback_name(3, 0), "his left"), "left -> bottom: %s", lookback_name(3, 0));
    CHECK(!strcmp(lookback_name(2, 0), "up, through the centre"), "top -> bottom: %s", lookback_name(2, 0));
    CHECK(!strcmp(lookback_name(0, 0), "nowhere"), "bottom -> bottom: %s", lookback_name(0, 0));

    printf("lookback_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
