/* THE DOCK HANDS HIM OVER on the laptop (dockhand.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/dockhand_test tools/dockhand_test.cpp -x c src/dockhand.c && /tmp/dockhand_test
 * The cable's rising edge while HOLDING = the release (`release
 * docked`); while EMPTY nothing; the falling edge after that release,
 * worn within the minute = the claim back; still (the shelf) = nothing
 * (the ordinary claim when worn); an undock with no handover before it
 * (the house emptied by the desk's own knock, or docked empty) = nothing;
 * his switch off = nothing either way, and an undock after a release
 * made while it was on still claims only if it is on now; no edge = no
 * word; the boot on the cable is no edge; the names. */
#include "dockhand.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static dockhand_t S;
static char why[96];
static int step(bool on, bool cable, bool holding, uint32_t still_ms)
{
    dockhand_in_t in; in.on = on; in.cable = cable; in.holding = holding; in.still_ms = still_ms;
    int d = dockhand_tick(&S, &in, why, sizeof why);
    if (why[0]) printf("       %s -> %s\n", why, dockhand_name(d));
    return d;
}

int main(void)
{
    printf("-- the boot off the cable, holding: no edge, no word\n");
    dockhand_begin(&S, false);
    CHECK(step(true, false, true, 100) == DOCKHAND_NONE && !why[0], "steady off the cable: nothing, no why");
    printf("-- the cable goes in while holding: the dock's release\n");
    CHECK(step(true, true, true, 100) == DOCKHAND_RELEASE && strstr(why, "release docked"), "docked while holding -> the release");
    CHECK(step(true, true, false, 5000) == DOCKHAND_NONE && !why[0], "still on the cable (now empty): nothing more");
    printf("-- the cable comes out worn: the claim back\n");
    CHECK(step(true, false, false, 2000) == DOCKHAND_CLAIM && strstr(why, "worn"), "undocked, moved 2 s ago -> the claim");
    CHECK(step(true, false, false, 2000) == DOCKHAND_NONE && !why[0], "steady off: nothing");
    printf("-- the cable comes out onto the shelf: nothing (the ordinary claim when worn)\n");
    CHECK(step(true, true, true, 100) == DOCKHAND_RELEASE, "docked again while holding -> the release");
    CHECK(step(true, false, false, DOCKHAND_WORN_MS) == DOCKHAND_NONE && strstr(why, "shelf"), "undocked still a minute -> nothing, says the shelf");
    CHECK(step(true, false, false, 100) == DOCKHAND_NONE && !why[0], "...and a later move is not an edge");
    printf("-- docked while EMPTY: nothing to hand over; the undock then claims nothing\n");
    CHECK(step(true, true, false, 100) == DOCKHAND_NONE && strstr(why, "not holding"), "docked empty -> nothing");
    CHECK(step(true, false, false, 100) == DOCKHAND_NONE && strstr(why, "nothing was handed over"), "undocked worn -> nothing (no handover before it)");
    printf("-- the house emptied on the cable by another hand (the desk's `lost` knock): the undock is not ours\n");
    dockhand_begin(&S, true);                       /* booted on the cable, holding */
    CHECK(step(true, true, true, 100) == DOCKHAND_NONE && !why[0], "the boot on the cable is no edge");
    CHECK(step(true, false, false, 100) == DOCKHAND_NONE && strstr(why, "nothing was handed over"), "undocked after the desk's own take -> nothing");
    printf("-- his switch off: the dock is a charger\n");
    dockhand_begin(&S, false);
    CHECK(step(false, true, true, 100) == DOCKHAND_NONE && strstr(why, "off"), "docked holding, hand off -> nothing (he stays)");
    CHECK(step(false, false, true, 100) == DOCKHAND_NONE, "undocked, hand off -> nothing");
    printf("-- released while on, switched off before the undock: the claim is kept back too\n");
    dockhand_begin(&S, false);
    CHECK(step(true, true, true, 100) == DOCKHAND_RELEASE, "docked holding, on -> the release");
    CHECK(step(false, false, false, 100) == DOCKHAND_NONE && strstr(why, "off"), "undocked worn, off now -> nothing");
    printf("-- the names\n");
    CHECK(!strcmp(dockhand_name(DOCKHAND_NONE), "nothing") && !strcmp(dockhand_name(DOCKHAND_RELEASE), "the dock's release")
          && !strcmp(dockhand_name(DOCKHAND_CLAIM), "the undock's claim") && !strcmp(dockhand_name(7), "?"), "the three names and the stranger");
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
