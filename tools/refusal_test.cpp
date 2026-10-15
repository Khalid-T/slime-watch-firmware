/* Host test of THE REFUSAL SAID ONCE's ledger (src/refusal.h): a word
 * said once then counted, a changed word said again and counted afresh,
 * two verbs apart, the status line with and without a clock (and across
 * midnight), the reset at a new session, the table full (said as before,
 * uncounted, named), a long line cut to the row, a tight buffer.
 * Build: c++ -std=gnu++17 -I src -o refusal_test tools/refusal_test.cpp
 * Run:   ./refusal_test
 */
#include "../src/refusal.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main(void)
{
    refusal_t t;
    refusal_reset(&t);
    char s[320];

    /* empty */
    check("empty: no rows", refusal_rows(&t) == 0);
    check("empty: `refusals: none`", refusal_status(&t, s, sizeof s, 1000, 9 * 60) == 0 && !strcmp(s, "refusals: none"));

    /* once per word: the first says, the rest count */
    check("first `no gate` on the beat speaks", refusal_note(&t, "beat", "no gate", 10000));
    bool spoke = false;
    for (int i = 0; i < 211; i++) spoke |= refusal_note(&t, "beat", "no gate", 10000 + 5000u * (i + 1));
    check("211 more `no gate` say nothing", !spoke);
    check("one row", refusal_rows(&t) == 1);
    check("the count is 212", t.row[0].count == 212);
    check("first-seen kept at the first", t.row[0].first_ms == 10000);

    /* the status line with a clock: now = 09:29, first heard 211 * 5 s = 17.6 min ago -> 09:12 */
    uint32_t now = 10000 + 5000u * 211;
    int rows = refusal_status(&t, s, sizeof s, now, 9 * 60 + 29);
    check("status: one row", rows == 1);
    check("status: `refusals: no gate x 212 since 09:12 (beat)`", !strcmp(s, "refusals: no gate x 212 since 09:12 (beat)"));

    /* no clock: minutes ago; under two minutes, seconds */
    refusal_status(&t, s, sizeof s, now, -1);
    check("no clock: `since 17 min ago`", !strcmp(s, "refusals: no gate x 212 since 17 min ago (beat)"));
    refusal_status(&t, s, sizeof s, 10000 + 45000, -1);
    check("no clock, young: `since 45 s ago`", !strcmp(s, "refusals: no gate x 212 since 45 s ago (beat)"));

    /* across midnight: now 00:05, first heard 17.6 min ago -> 23:48 */
    refusal_status(&t, s, sizeof s, now, 5);
    check("across midnight: 23:48", strstr(s, "since 23:48 (beat)") != NULL);

    /* the word changes: said again, counted afresh from now */
    check("`no box` on the beat speaks (the word changed)", refusal_note(&t, "beat", "no box", now + 5000));
    check("still one row (per verb)", refusal_rows(&t) == 1);
    check("count back to 1", t.row[0].count == 1 && !strcmp(t.row[0].word, "no box"));
    check("first-seen moved to the change", t.row[0].first_ms == now + 5000);
    check("`no box` again says nothing", !refusal_note(&t, "beat", "no box", now + 10000));
    check("back to `no gate` speaks again (a change is a change)", refusal_note(&t, "beat", "no gate", now + 15000));

    /* a second verb is its own row */
    check("`none` on the sky speaks", refusal_note(&t, "sky", "none", now + 20000));
    check("`none` on the sky again says nothing", !refusal_note(&t, "sky", "none", now + 25000));
    check("two rows", refusal_rows(&t) == 2);
    refusal_status(&t, s, sizeof s, now + 30000, 9 * 60 + 31);
    check("status: both rows, comma between, the beat first", !strcmp(s, "refusals: no gate x 1 since 09:31 (beat), none x 2 since 09:31 (sky)"));

    /* the reset at a new session */
    refusal_reset(&t);
    check("reset: no rows", refusal_rows(&t) == 0);
    check("reset: `none`", refusal_status(&t, s, sizeof s, now, -1) == 0 && !strcmp(s, "refusals: none"));
    check("after the reset the old word speaks again", refusal_note(&t, "beat", "no gate", now));

    /* the table full: a seventh verb is said every time, uncounted, named */
    const char *verbs[] = { "claim", "come", "release", "word", "sky" };
    for (int i = 0; i < 5; i++) check("a verb takes a row", refusal_note(&t, verbs[i], "no verb", now));
    check("six rows", refusal_rows(&t) == 6);
    check("the seventh verb speaks", refusal_note(&t, "status", "no verb", now));
    check("...and speaks again (uncounted)", refusal_note(&t, "status", "no verb", now + 1000));
    check("still six rows", refusal_rows(&t) == 6);
    check("the overflow is tallied", t.unkept == 2);
    refusal_status(&t, s, sizeof s, now, -1);
    check("status names the overflow", strstr(s, "(+2 unkept: the table was full)") != NULL);
    check("a kept verb still counts while full", !refusal_note(&t, "claim", "no verb", now + 2000) && t.row[1].count == 2);

    /* a long line is cut to the row; the cut word is the one compared */
    refusal_reset(&t);
    check("a long line speaks", refusal_note(&t, "status", "no such thing as status", now));
    check("the row keeps 11 chars", strlen(t.row[0].word) == REFUSAL_WORD - 1 && !strcmp(t.row[0].word, "no such thi"));
    check("the same long line again says nothing", !refusal_note(&t, "status", "no such thing as status", now));

    /* a tight buffer: terminated, never past the cap */
    char tiny[24];
    memset(tiny, 'x', sizeof tiny);
    refusal_status(&t, tiny, 20, now, -1);
    check("tight buffer terminated inside the cap", tiny[19] == 0 && strlen(tiny) == 19 && tiny[20] == 'x');

    /* the count saturates */
    refusal_reset(&t);
    refusal_note(&t, "beat", "no gate", 0);
    t.row[0].count = 0xFFFF;
    refusal_note(&t, "beat", "no gate", 0);
    check("the count saturates at 65535", t.row[0].count == 0xFFFF);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
