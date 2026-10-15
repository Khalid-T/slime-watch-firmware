/* THE NOTICE'S OWN REACTION on the laptop (notice.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/notice_test tools/notice_test.cpp -x c src/notice.c && /tmp/notice_test
 * The whole table: every category's haptic, plays and halo (a ringing
 * call hops and pulses; a missed call and a voicemail bump twice; the
 * social, mail, business, location and other kinds bump once; a
 * reminder hops once; health purrs; news and entertainment are the
 * mote alone); the flags over it (silent = nothing felt, the halo kept;
 * pre-existing = nothing felt and the plain halo; important = the wide
 * halo, unless it pulses); the desk's no-category and an unknown
 * category = the default; the rank (none < purr < bump < hop, two plays
 * over one); the names. */
#include "notice.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *kind_name(int k) { return k == HAPTIC_BUMP ? "bump" : k == HAPTIC_PURR ? "purr" : k == HAPTIC_HOP3 ? "hop" : "none"; }

/* one category under one set of flags: the three expected */
static void row(int cat, int flags, int haptic, int repeats, int halo)
{
    notice_react_t r; memset(&r, 0x55, sizeof r);
    notice_react(cat, flags, &r);
    CHECK(r.haptic == haptic && r.repeats == repeats && r.halo == halo, "%s%s%s%s -> %s x%d, the %s halo",
          notice_cat_name(cat), (flags & NOTICE_FLAG_SILENT) ? " silent" : "", (flags & NOTICE_FLAG_IMPORTANT) ? " important" : "",
          (flags & NOTICE_FLAG_PREEXISTING) ? " pre-existing" : "", kind_name(r.haptic), r.repeats, notice_halo_name(r.halo));
}

int main(void)
{
    puts("-- the table, no flags: each category's own shape");
    row(NOTICE_CAT_OTHER,         0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_CALL,          0, HAPTIC_HOP3, 1, NOTICE_HALO_PULSING);
    row(NOTICE_CAT_MISSED_CALL,   0, HAPTIC_BUMP, 2, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_VOICEMAIL,     0, HAPTIC_BUMP, 2, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_SOCIAL,        0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_SCHEDULE,      0, HAPTIC_HOP3, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_EMAIL,         0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_NEWS,          0, -1,          0, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_HEALTH,        0, HAPTIC_PURR, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_BUSINESS,      0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_LOCATION,      0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_ENTERTAINMENT, 0, -1,          0, NOTICE_HALO_NORMAL);

    puts("-- the desk's ask (no category) and a category past the table: the default, one bump, the plain halo");
    row(NOTICE_CAT_NONE, 0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_N, 0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    row(99, 0, HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL);
    {
        notice_react_t d, n;
        notice_react_default(&d); notice_react(NOTICE_CAT_NONE, 0, &n);
        CHECK(!memcmp(&d, &n, sizeof d) && d.haptic == HAPTIC_BUMP && d.repeats == 1 && d.halo == NOTICE_HALO_NORMAL, "notice_react_default is that default");
    }

    puts("-- SILENT: nothing felt, whatever the kind; the halo keeps its shape");
    row(NOTICE_CAT_CALL,        NOTICE_FLAG_SILENT, -1, 0, NOTICE_HALO_PULSING);
    row(NOTICE_CAT_MISSED_CALL, NOTICE_FLAG_SILENT, -1, 0, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_HEALTH,      NOTICE_FLAG_SILENT, -1, 0, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_NONE,        NOTICE_FLAG_SILENT, -1, 0, NOTICE_HALO_NORMAL);

    puts("-- IMPORTANT: the wide halo -- unless it already pulses; the haptic as the kind's");
    row(NOTICE_CAT_EMAIL,       NOTICE_FLAG_IMPORTANT, HAPTIC_BUMP, 1, NOTICE_HALO_WIDE);
    row(NOTICE_CAT_MISSED_CALL, NOTICE_FLAG_IMPORTANT, HAPTIC_BUMP, 2, NOTICE_HALO_WIDE);
    row(NOTICE_CAT_NEWS,        NOTICE_FLAG_IMPORTANT, -1, 0, NOTICE_HALO_WIDE);
    row(NOTICE_CAT_CALL,        NOTICE_FLAG_IMPORTANT, HAPTIC_HOP3, 1, NOTICE_HALO_PULSING);
    row(NOTICE_CAT_SOCIAL,      NOTICE_FLAG_IMPORTANT | NOTICE_FLAG_SILENT, -1, 0, NOTICE_HALO_WIDE);

    puts("-- PRE-EXISTING (a replay): nothing felt and the plain halo, over every other flag");
    row(NOTICE_CAT_CALL,   NOTICE_FLAG_PREEXISTING, -1, 0, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_EMAIL,  NOTICE_FLAG_PREEXISTING | NOTICE_FLAG_IMPORTANT, -1, 0, NOTICE_HALO_NORMAL);
    row(NOTICE_CAT_SOCIAL, NOTICE_FLAG_PREEXISTING | NOTICE_FLAG_SILENT | NOTICE_FLAG_IMPORTANT, -1, 0, NOTICE_HALO_NORMAL);

    puts("-- the rank: none < purr < bump < hop, two plays over one of a kind");
    {
        notice_react_t none, purr, bump, bump2, hop, hop2;
        notice_react(NOTICE_CAT_NEWS, 0, &none); notice_react(NOTICE_CAT_HEALTH, 0, &purr); notice_react(NOTICE_CAT_EMAIL, 0, &bump);
        notice_react(NOTICE_CAT_MISSED_CALL, 0, &bump2); notice_react(NOTICE_CAT_SCHEDULE, 0, &hop); hop2 = hop; hop2.repeats = 2;
        CHECK(notice_react_rank(&none) == 0, "nothing felt: rank 0");
        CHECK(notice_react_rank(&purr) > notice_react_rank(&none), "a purr over nothing");
        CHECK(notice_react_rank(&bump) > notice_react_rank(&purr), "a bump over a purr");
        CHECK(notice_react_rank(&bump2) > notice_react_rank(&bump), "two bumps over one");
        CHECK(notice_react_rank(&hop) > notice_react_rank(&bump2), "a hop over two bumps");
        CHECK(notice_react_rank(&hop2) > notice_react_rank(&hop), "two hops over one");
        notice_react_t silent; notice_react(NOTICE_CAT_CALL, NOTICE_FLAG_SILENT, &silent);
        CHECK(notice_react_rank(&silent) == 0, "a silent call: rank 0 (the halo pulses, nothing is felt)");
        CHECK(notice_react_rank(NULL) == 0, "NULL: 0");
        notice_react_t odd = { 7, 1, 0 };
        CHECK(notice_react_rank(&odd) == 0, "a kind past haptic.h's three: 0");
    }

    puts("-- the names");
    CHECK(!strcmp(notice_cat_name(NOTICE_CAT_CALL), "call") && !strcmp(notice_cat_name(NOTICE_CAT_MISSED_CALL), "missed call")
          && !strcmp(notice_cat_name(NOTICE_CAT_ENTERTAINMENT), "entertainment") && !strcmp(notice_cat_name(NOTICE_CAT_NONE), "none")
          && !strcmp(notice_cat_name(NOTICE_CAT_N), "?") && !strcmp(notice_cat_name(-7), "?"), "the categories' names");
    CHECK(!strcmp(notice_halo_name(NOTICE_HALO_NORMAL), "normal") && !strcmp(notice_halo_name(NOTICE_HALO_WIDE), "wide")
          && !strcmp(notice_halo_name(NOTICE_HALO_PULSING), "pulsing") && !strcmp(notice_halo_name(9), "?"), "the halos' names");
    { notice_react(NOTICE_CAT_CALL, 0, NULL); notice_react_default(NULL); CHECK(true, "a NULL out: nothing written, nothing tripped"); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
