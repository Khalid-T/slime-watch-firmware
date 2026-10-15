/* THE PAIRING WINDOW's rule on the laptop (blepair.c has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/blepair_test tools/blepair_test.cpp -x c src/blepair.c && /tmp/blepair_test
 * The admit table (bonded / a stranger in the window / a stranger
 * outside), the window's clock (opens, counts down from 60, closes
 * itself at the end, closes early by hand, reopens from the top), the
 * millis wrap. */
#include "blepair.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int checks = 0;
#define CHECK(c) do { assert(c); checks++; } while (0)

int main(void)
{
    blepair_t p; blepair_init(&p);
    puts("-- closed at birth: a bonded peer is in, a stranger is refused");
    CHECK(!blepair_is_open(&p, 1000));
    CHECK(blepair_left_s(&p, 1000) == 0);
    CHECK(blepair_admit(&p, 1000, true) == BLEPAIR_BONDED);
    CHECK(blepair_admit(&p, 1000, false) == BLEPAIR_REFUSED);

    puts("-- opened: the count from 60, a stranger admitted, bonded still in");
    blepair_open(&p, 10000, 0);                        /* 0 = the default length */
    CHECK(p.secs == BLEPAIR_S);
    CHECK(blepair_is_open(&p, 10000));
    CHECK(blepair_left_s(&p, 10000) == 60);
    CHECK(blepair_left_s(&p, 10001) == 60);            /* rounded up */
    CHECK(blepair_left_s(&p, 11000) == 59);
    CHECK(blepair_left_s(&p, 69999) == 1);             /* the last fraction reads 1, never 0 */
    CHECK(blepair_admit(&p, 30000, false) == BLEPAIR_WINDOW);
    CHECK(blepair_admit(&p, 30000, true) == BLEPAIR_BONDED);

    puts("-- closes itself at the end");
    CHECK(!blepair_is_open(&p, 70000));
    CHECK(blepair_left_s(&p, 70000) == 0);
    CHECK(blepair_admit(&p, 70000, false) == BLEPAIR_REFUSED);

    puts("-- closed by hand (the bond landed, a tap), reopened from the top");
    blepair_open(&p, 100000, 30);
    CHECK(blepair_left_s(&p, 100000) == 30);
    blepair_close(&p);
    CHECK(!blepair_is_open(&p, 100000));
    blepair_open(&p, 120000, 60);
    CHECK(blepair_left_s(&p, 150000) == 30);
    blepair_open(&p, 150000, 60);                      /* a second tap restarts the clock */
    CHECK(blepair_left_s(&p, 150000) == 60);

    puts("-- the millis wrap");
    blepair_open(&p, 0xFFFFF000u, 60);
    CHECK(blepair_is_open(&p, 0x00000010u));           /* 4 s later, across the wrap */
    CHECK(blepair_left_s(&p, 0x00000010u) == 56);

    puts("-- the words");
    CHECK(!strcmp(blepair_why(BLEPAIR_BONDED), "bonded"));
    CHECK(strstr(blepair_why(BLEPAIR_WINDOW), "window") != NULL);
    CHECK(strstr(blepair_why(BLEPAIR_REFUSED), "no pairing window") != NULL);

    printf("blepair: %d checks ok\n", checks);
    return 0;
}
