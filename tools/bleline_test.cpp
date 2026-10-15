/* THE AIR DOOR's line gathering on the laptop (bleline.c has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/bleline_test tools/bleline_test.cpp -x c src/bleline.c && /tmp/bleline_test
 * One line in three writes, three lines in one write, a CR dropped, a
 * bare newline nothing, a line past the cap dropped whole with the next
 * one intact, the reset at a disconnect. */
#include "bleline.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int checks = 0;
#define CHECK(c) do { assert(c); checks++; } while (0)
static char got[8][BLELINE_MAX + 1]; static int ngot = 0;
static void take(const char *line, void *arg) { (void)arg; if (ngot < 8) strcpy(got[ngot++], line); }
static int feed(bleline_t *l, const char *s) { return bleline_feed(l, (const uint8_t *)s, (int)strlen(s), take, NULL); }

int main(void)
{
    bleline_t l; bleline_init(&l);
    puts("-- one line in three writes (a 20-byte MTU)");
    CHECK(feed(&l, "hi 0123456789abcdef0") == 0);
    CHECK(feed(&l, "123456789abcdef") == 0);
    CHECK(feed(&l, "\n") == 1);
    CHECK(ngot == 1 && !strcmp(got[0], "hi 0123456789abcdef0123456789abcdef"));

    puts("-- three lines in one write, a CR dropped, a bare newline nothing");
    ngot = 0;
    CHECK(feed(&l, "ok\r\nok held\n\nno verb\n") == 3);
    CHECK(ngot == 3 && !strcmp(got[0], "ok") && !strcmp(got[1], "ok held") && !strcmp(got[2], "no verb"));
    CHECK(l.lines == 4 && l.dropped == 0);

    puts("-- a line past the cap: dropped whole, the next one intact");
    ngot = 0;
    char big[BLELINE_MAX + 40]; memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0;
    CHECK(feed(&l, big) == 0);
    CHECK(l.overlong);
    CHECK(feed(&l, "\nok\n") == 1);
    CHECK(ngot == 1 && !strcmp(got[0], "ok"));
    CHECK(l.dropped == 1);

    puts("-- exactly the cap is a line");
    ngot = 0;
    char cap[BLELINE_MAX + 2]; memset(cap, 'y', BLELINE_MAX); cap[BLELINE_MAX] = '\n'; cap[BLELINE_MAX + 1] = 0;
    CHECK(feed(&l, cap) == 1);
    CHECK((int)strlen(got[0]) == BLELINE_MAX);

    puts("-- the reset at a disconnect: the half line goes");
    ngot = 0;
    CHECK(feed(&l, "hi 12") == 0);
    bleline_reset(&l);
    CHECK(feed(&l, "ok\n") == 1 && !strcmp(got[0], "ok"));

    printf("bleline: %d checks ok\n", checks);
    return 0;
}
