/* Host test of THE FLASH FROM THE DESK's hold (src/holdask.h): the
 * ask's grammar -- the look, `off` / `0`, the seconds, the cap at 300,
 * a bad word -- the hold's clock (until / expired / left, wrap-safe)
 * and the answer line: `ok hold 300` / `ok hold lit` / `ok hold off` /
 * `no hold` / `no network` / `no spent`.
 * Build: c++ -std=gnu++17 -I src -o holdask_test tools/holdask_test.cpp
 * Run:   ./holdask_test
 */
#include "../src/holdask.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int parse(const char *w, uint32_t *s) { uint32_t x = 77; int k = holdask_parse(w, &x); if (s) *s = x; return k; }

static const char *answer(int kind, bool held, uint32_t left, bool nets, bool spent)
{
    static char buf[24];
    memset(buf, 'x', sizeof buf);
    holdask_answer(kind, held, left, nets, spent, buf, sizeof buf);
    return buf;
}

int main(void)
{
    uint32_t s;
    /* the grammar */
    check("nothing after `hold` is the look", parse("", &s) == HOLDASK_STATUS && s == 0);
    check("NULL is the look too", parse(NULL, &s) == HOLDASK_STATUS && s == 0);
    check("`off` releases", parse("off", &s) == HOLDASK_OFF && s == 0);
    check("`0` releases", parse("0", &s) == HOLDASK_OFF && s == 0);
    check("`000` releases", parse("000", &s) == HOLDASK_OFF && s == 0);
    check("`offx` is bad", parse("offx", &s) == HOLDASK_BAD);
    check("`OFF` is bad (the desk folds case)", parse("OFF", &s) == HOLDASK_BAD);
    check("`120` = 120 s", parse("120", &s) == HOLDASK_SET && s == 120);
    check("`1` = 1 s", parse("1", &s) == HOLDASK_SET && s == 1);
    check("`300` = the cap exactly", parse("300", &s) == HOLDASK_SET && s == 300);
    check("`301` is capped to 300", parse("301", &s) == HOLDASK_SET && s == 300);
    check("`99999` is capped to 300", parse("99999", &s) == HOLDASK_SET && s == 300);
    check("seven digits is a bad word, not a cap", parse("1000000", &s) == HOLDASK_BAD && s == 0);
    check("`-5` is bad", parse("-5", &s) == HOLDASK_BAD);
    check("`5s` is bad", parse("5s", &s) == HOLDASK_BAD);
    check("`5 0` is bad (a tail)", parse("5 0", &s) == HOLDASK_BAD);
    check("`five` is bad", parse("five", &s) == HOLDASK_BAD);
    check("a NULL secs_out is tolerated", holdask_parse("30", NULL) == HOLDASK_SET);
    check("HOLDASK_CAP_S is 300 (ruling 151)", HOLDASK_CAP_S == 300u);

    /* the clock */
    check("until = now + s*1000", holdask_until(1000, 30) == 31000);
    check("0 seconds = untimed (0)", holdask_until(1000, 0) == 0);
    check("a wrap landing on 0 is nudged to 1", holdask_until(0xFFFFFFFFu - 999u, 1) == 1);
    check("not expired before its end", !holdask_expired(31000, 30999));
    check("expired at its end", holdask_expired(31000, 31000));
    check("expired after its end", holdask_expired(31000, 40000));
    check("untimed never expires", !holdask_expired(0, 0xFFFFFFFFu) && !holdask_expired(0, 0));
    check("expiry across the millis wrap", holdask_expired(500, 0xFFFFFFF0u) == false && holdask_expired(500, 600));
    check("a hold set before the wrap ends after it", holdask_until(0xFFFFFFFFu - 500u, 2) == 1499u && !holdask_expired(1499u, 0xFFFFFFFFu) && holdask_expired(1499u, 1500u));
    check("left: 30 s at the start", holdask_left_s(31000, 1000) == 30);
    check("left: rounded up (29.001 s -> 30)", holdask_left_s(31000, 1999) == 30);
    check("left: 1 ms is 1 s", holdask_left_s(31000, 30999) == 1);
    check("left: 0 at the end", holdask_left_s(31000, 31000) == 0);
    check("left: 0 past the end", holdask_left_s(31000, 99999) == 0);
    check("left: 0 for untimed", holdask_left_s(0, 5000) == 0);

    /* the answers */
    check("set: ok hold 300", !strcmp(answer(HOLDASK_SET, true, 300, true, false), "ok hold 300"));
    check("set with no network: no network", !strcmp(answer(HOLDASK_SET, true, 300, false, false), "no network"));
    check("set while spent off the cable: no spent", !strcmp(answer(HOLDASK_SET, true, 300, true, true), "no spent"));
    check("no network outranks spent", !strcmp(answer(HOLDASK_SET, true, 300, false, true), "no network"));
    check("off: ok hold off", !strcmp(answer(HOLDASK_OFF, false, 0, true, false), "ok hold off"));
    check("off never needs a network", !strcmp(answer(HOLDASK_OFF, false, 0, false, true), "ok hold off"));
    check("look, no hold: ok hold off", !strcmp(answer(HOLDASK_STATUS, false, 0, true, false), "ok hold off"));
    check("look, a timed hold: ok hold 42", !strcmp(answer(HOLDASK_STATUS, true, 42, true, false), "ok hold 42"));
    check("look, the console's hold: ok hold lit", !strcmp(answer(HOLDASK_STATUS, true, 0, true, false), "ok hold lit"));
    check("look never needs a network", !strcmp(answer(HOLDASK_STATUS, true, 42, false, true), "ok hold 42"));
    check("bad: no hold", !strcmp(answer(HOLDASK_BAD, false, 0, true, false), "no hold"));
    check("every answer fits the branch's 24 B", strlen(answer(HOLDASK_STATUS, true, 300, true, false)) < 16);

    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
