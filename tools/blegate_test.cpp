/* THE BLUETOOTH GATE's decision on the laptop (blegate.cpp has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/blegate_test tools/blegate_test.cpp src/blegate.cpp && /tmp/blegate_test
 * Ruling 58 (Khalid 2026-09-10 21:50, "do BLE only when WiFi is down"),
 * as a truth table: every one of the 16 combinations of enabled / dark
 * / wifi_on / docked, and the edges as ble.cpp walks them -- the wake
 * (up), the dial (WiFi up: down FIRST), the linger's end (WiFi off:
 * back up), the dark house (down), the dock (up: the radio never dials
 * there), the switch OFF (never up, whatever else); and blegate_both,
 * the assert's predicate: never both. */
#include "blegate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int checks = 0;
static void is(bool enabled, bool dark, bool wifi_on, bool docked, bool want, const char *what)
{
    blegate_in_t in = { enabled, dark, wifi_on, docked, false };
    bool got = blegate_should_be_up(&in);
    printf("  %-56s enabled %d dark %d wifi %d dock %d -> %s (%s)%s\n", what, enabled, dark, wifi_on, docked,
           got ? "UP" : "down", blegate_why(&in), got == want ? "" : "   <-- WRONG");
    assert(got == want);
    /* the word agrees with the verdict */
    assert((got && !strcmp(blegate_why(&in), "up")) || (!got && strcmp(blegate_why(&in), "up")));
    checks += 2;
}

int main(void)
{
    puts("-- the whole table: up only when enabled, not dark, and the WiFi radio is off (the dock never vetoes)");
    for (int e = 0; e < 2; e++) for (int d = 0; d < 2; d++) for (int w = 0; w < 2; w++) for (int k = 0; k < 2; k++)
        is(e, d, w, k, e && !d && !w, "the table");

    puts("-- the edges as ble.cpp walks them");
    is(true,  false, false, false, true,  "the wake: the radio off, the glass lit -> BLE up");
    is(true,  false, true,  false, false, "a dial (THE RADIO BUDGET): WiFi up -> BLE down FIRST");
    is(true,  false, false, false, true,  "the linger's end: net_radio_off -> BLE back");
    is(true,  true,  false, false, false, "the dark house: neither radio");
    is(true,  true,  true,  false, false, "the night's errand (the radio up with the glass dark): still no BLE");
    is(true,  false, false, true,  true,  "the serial dock: the radio never dials there -> BLE up");
    is(true,  false, true,  true,  false, "docked but the radio up by hand (wifi-scan): WiFi wins, BLE down");
    is(false, false, false, false, false, "the BLUETOOTH row OFF: never up");
    is(false, false, false, true,  false, "OFF on the dock: still never");

    puts("-- THE DARK EAR (ruling 78, his way 10-06): a peer bonded lifts the dark veto, nothing else");
    for (int e = 0; e < 2; e++) for (int d = 0; d < 2; d++) for (int w = 0; w < 2; w++) for (int k = 0; k < 2; k++) {
        blegate_in_t in = { (bool)e, (bool)d, (bool)w, (bool)k, true };
        assert(blegate_should_be_up(&in) == (e && !w)); checks++;          /* dark or lit: up when on and the WiFi radio is off */
    }
    { blegate_in_t in = { true, true, false, false, true };  assert(!strcmp(blegate_why(&in), "up")); checks++; }
    { blegate_in_t in = { true, true, true, false, true };   assert(!strcmp(blegate_why(&in), "the WiFi radio is up")); checks++; }
    { blegate_in_t in = { false, true, false, false, true }; assert(!strcmp(blegate_why(&in), "off (SETTINGS)")); checks++; }

    puts("-- the words");
    { blegate_in_t in = { false, true, true, false, false }; assert(!strcmp(blegate_why(&in), "off (SETTINGS)")); checks++; }
    { blegate_in_t in = { true, true, true, false, false };  assert(!strcmp(blegate_why(&in), "the dark house")); checks++; }
    { blegate_in_t in = { true, false, true, false, false }; assert(!strcmp(blegate_why(&in), "the WiFi radio is up")); checks++; }

    puts("-- never both: the assert's predicate");
    assert(blegate_both(true, true)); checks++;
    assert(!blegate_both(true, false)); checks++;
    assert(!blegate_both(false, true)); checks++;
    assert(!blegate_both(false, false)); checks++;
    /* and the gate itself never asks for both: whatever else, wifi_on -> not up */
    for (int e = 0; e < 2; e++) for (int d = 0; d < 2; d++) for (int k = 0; k < 2; k++) {
        blegate_in_t in = { (bool)e, (bool)d, true, (bool)k, false };
        assert(!blegate_both(blegate_should_be_up(&in), true)); checks++;
    }

    printf("%d checks, 0 failed\n", checks);
    return 0;
}
