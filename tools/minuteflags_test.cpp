/* Host test of THE HOUSE IN THE ROW (src/minuteflags.h): the day log's
 * three reserved flag bits -- HELD, LINKED, NOTICE -- from samples of
 * the house, the door and the wisp count over a minute: sticky within
 * the minute, cleared by the take, the wisp count rebased, a count
 * that wrapped still a notice, and the bits exactly 5-7 (the row's
 * bits 0-4 untouched: no byte added, the format unchanged).
 * Build: c++ -std=gnu++17 -I src -o minuteflags_test tools/minuteflags_test.cpp
 * Run:   ./minuteflags_test
 */
#include "../src/minuteflags.h"
#include <stdio.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main(void)
{
    mf_t f;
    /* the bits are the reserved three, and only those */
    check("HELD is bit 5", MF_HELD == 0x20);
    check("LINKED is bit 6", MF_LINKED == 0x40);
    check("NOTICE is bit 7", MF_NOTICE == 0x80);
    check("the three never touch bits 0-4 (WORN SHAKEN CHARGING UP EAR)", (MF_ALL & 0x1f) == 0 && MF_ALL == 0xe0);
    check("the state is a few bytes", sizeof(mf_t) <= 8);

    /* an empty minute: nothing */
    mf_begin(&f, 0);
    check("a fresh minute has no bits", f.bits == 0);
    check("a minute of nothing files nothing", mf_take(&f, 0) == 0);
    /* sixty samples of the house on the wrist, alone, no wisp */
    for (int s = 0; s < 60; s++) mf_sample(&f, true, false, 0);
    check("held every second: HELD alone", mf_take(&f, 0) == MF_HELD);
    /* the lease toggled mid-minute: one second of holding is the bit */
    for (int s = 0; s < 59; s++) mf_sample(&f, false, false, 0);
    mf_sample(&f, true, false, 0);
    check("one second held: HELD (sticky; the minute counts for both houses)", mf_take(&f, 0) == MF_HELD);
    /* a door answered for a moment */
    mf_sample(&f, false, true, 0);
    for (int s = 0; s < 59; s++) mf_sample(&f, false, false, 0);
    check("a door up for one sample: LINKED", mf_take(&f, 0) == MF_LINKED);
    /* a wisp flew in: the count moved past the base */
    mf_sample(&f, false, false, 0);
    mf_sample(&f, false, false, 1);
    check("the wisp count moved by one: NOTICE", mf_take(&f, 1) == MF_NOTICE);
    /* the take rebased: the same count next minute is no notice */
    mf_sample(&f, false, false, 1);
    check("the same count next minute: nothing", mf_take(&f, 1) == 0);
    /* three at once */
    mf_sample(&f, true, true, 4);
    check("held, linked and a wisp: all three", mf_take(&f, 4) == MF_ALL);
    /* two wisps in a minute are one bit */
    mf_sample(&f, false, false, 6);
    mf_sample(&f, false, false, 9);
    check("three wisps in a minute: one NOTICE bit", mf_take(&f, 9) == MF_NOTICE);
    /* the take clears: the next minute starts from nothing */
    mf_sample(&f, true, true, 9);
    (void)mf_take(&f, 9);
    check("after the take the bits are clear", f.bits == 0 && f.wisp_base == 9);
    /* the take without a sample in between: the base alone moved -- still a notice the next minute */
    mf_sample(&f, false, false, 10);
    check("a wisp between takes with one sample: NOTICE", mf_take(&f, 10) == MF_NOTICE);
    /* a count that wrapped (a very long life): unsigned, still moved */
    mf_begin(&f, 0xffffffffu);
    mf_sample(&f, false, false, 0);
    check("the wisp count wrapped past zero: still NOTICE", mf_take(&f, 0) == MF_NOTICE);
    /* mf_begin discards what was gathered (a clockless minute with no file) */
    mf_sample(&f, true, true, 3);
    mf_begin(&f, 3);
    check("mf_begin discards the minute and takes the count as the base", f.bits == 0 && f.wisp_base == 3 && mf_take(&f, 3) == 0);
    /* a stray high bit in the state never leaks past the three */
    f.bits = 0xff;
    check("the take masks to the three bits", mf_take(&f, 3) == MF_ALL);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
