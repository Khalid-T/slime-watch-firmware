/* Host test of THE HTTP DOOR'S KNOCK (src/httpgate.h): the query cut
 * off the path, the table (the page and the face open; doctor, status,
 * the day list and files, POST /pet signed; lease and art their own),
 * the signed message's shape (the desk's door_what in watchlink.py
 * makes the same string: test_watch.py's parity test), the verdict over
 * the REAL judge (src/radio.cpp's radio_knock_judge, the knock's own):
 * a bare board takes every ask and says `taken unsigned` once, a
 * provisioned board refuses a bare or mis-signed ask `no key`, a stale
 * stamp `stale` (KNOCK_STALE_MIN), an unset clock checks the mac alone.
 * Build: c++ -std=gnu++17 -I tools/stub -I src -o httpgate_test tools/httpgate_test.cpp src/radio.cpp
 * Run:   ./httpgate_test
 */
#include "../src/httpgate.h"
#include "../src/radio.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* link_sign_check's shape on the host: the query's stamp and mac into a
 * knock_t, "ours" the mac the key would make (handed in: no HMAC here),
 * the judge's verdict. */
static int sign_check(const char *stamp_s, const char *mac_s, bool have_key, const char *ours,
                      bool clock, uint32_t now_min, char *why, int cap)
{
    knock_t k; memset(&k, 0, sizeof k);
    if (stamp_s && stamp_s[0] && mac_s && mac_s[0]) {
        k.stamp = (uint32_t)strtoul(stamp_s, NULL, 10);
        int n = 0;
        while (n < KNOCK_MAC_HEX && isxdigit((unsigned char)mac_s[n])) { k.mac[n] = (char)tolower((unsigned char)mac_s[n]); n++; }
        k.mac[n] = 0;
        k.signed_ = n == KNOCK_MAC_HEX && mac_s[n] == 0;
    }
    return radio_knock_judge(&k, have_key, have_key && k.signed_ ? ours : NULL, clock, now_min, why, cap);
}

int main(void)
{
    char p[64], what[HTTPGATE_WHAT_CAP], why[24];
    const char *status, *body; bool say;
    static const char OURS[] = "0123456789abcdef0123456789abcdef";

    /* the split */
    strcpy(p, "/doctor?stamp=29876543&mac=abcd");
    char *q = httpgate_split(p);
    check("split: the path bare", !strcmp(p, "/doctor"));
    check("split: the query returned", !strcmp(q, "stamp=29876543&mac=abcd"));
    strcpy(p, "/status.json");
    q = httpgate_split(p);
    check("split: no query = empty string, path kept", !strcmp(p, "/status.json") && q && !*q);
    strcpy(p, "/day/?");
    q = httpgate_split(p);
    check("split: a bare `?` = empty query", !strcmp(p, "/day/") && !*q);
    strcpy(p, "/screen.bmp?t=1700000000");
    q = httpgate_split(p);
    check("split: the page's <img> cache-buster comes off", !strcmp(p, "/screen.bmp") && !strcmp(q, "t=1700000000"));

    /* the table */
    check("open: GET /", !httpgate_gated(false, "/"));
    check("open: GET /index.html", !httpgate_gated(false, "/index.html"));
    check("open: GET /screen.bmp", !httpgate_gated(false, "/screen.bmp"));
    check("open: GET /screen.bin", !httpgate_gated(false, "/screen.bin"));
    check("signed: GET /doctor", httpgate_gated(false, "/doctor"));
    check("signed: GET /status.json", httpgate_gated(false, "/status.json"));
    check("signed: GET /day", httpgate_gated(false, "/day"));
    check("signed: GET /day/", httpgate_gated(false, "/day/"));
    check("signed: GET /day/20261013.bin", httpgate_gated(false, "/day/20261013.bin"));
    check("open: POST /pet (the share page's pet button)", !httpgate_gated(true, "/pet"));
    check("not here: POST /lease (the knock's own)", !httpgate_gated(true, "/lease"));
    check("not here: POST /art (the art's own)", !httpgate_gated(true, "/art"));
    check("not here: GET /pet (a 404 either way)", !httpgate_gated(false, "/pet"));
    check("not here: GET /nowhere", !httpgate_gated(false, "/nowhere"));
    check("not here: GET /daybook (not under /day/)", !httpgate_gated(false, "/daybook"));

    /* the message */
    httpgate_what(false, "/doctor", what, sizeof what);
    check("what: `http GET /doctor`", !strcmp(what, "http GET /doctor"));
    httpgate_what(true, "/pet", what, sizeof what);
    check("what: `http POST /pet`", !strcmp(what, "http POST /pet"));
    httpgate_what(false, "/day/20261013.bin", what, sizeof what);
    check("what: `http GET /day/20261013.bin` fits the cap", !strcmp(what, "http GET /day/20261013.bin") && strlen(what) < HTTPGATE_WHAT_CAP);
    char tiny[12]; httpgate_what(false, "/status.json", tiny, sizeof tiny);
    check("what: cut to the cap, terminated", strlen(tiny) == 11 && !strncmp(tiny, "http GET /s", 11));

    /* a bare board: everything through, `taken unsigned` said once */
    httpgate_t g; memset(&g, 0, sizeof g);
    int rc = sign_check("", "", false, OURS, true, 29876543, why, sizeof why);
    check("bare board: the judge says unsigned, taken", rc == 0 && !strcmp(why, "unsigned"));
    check("bare board: through, the line due", httpgate_verdict(&g, rc, why, &status, &body, &say) == 0 && say && !strcmp(status, "200 OK"));
    rc = sign_check("", "", false, OURS, true, 29876543, why, sizeof why);
    check("bare board: the second ask through, the line NOT due", httpgate_verdict(&g, rc, why, &status, &body, &say) == 0 && !say);
    rc = sign_check("29876543", OURS, false, OURS, true, 29876543, why, sizeof why);
    check("bare board: a signed ask is taken too (the mac unread)", rc == 0 && httpgate_verdict(&g, rc, why, &status, &body, &say) == 0 && !say);

    /* a provisioned board */
    memset(&g, 0, sizeof g);
    rc = sign_check("", "", true, OURS, true, 29876543, why, sizeof why);
    check("key: a bare ask refused `no key`", rc == 1 && httpgate_verdict(&g, rc, why, &status, &body, &say) == 1 && !strcmp(status, "403 Forbidden") && !strcmp(body, "no key\n") && !say);
    rc = sign_check("29876543", "ffffffffffffffffffffffffffffffff", true, OURS, true, 29876543, why, sizeof why);
    check("key: a wrong mac refused `no key`", rc == 1 && httpgate_verdict(&g, rc, why, &status, &body, &say) == 1 && !strcmp(body, "no key\n"));
    rc = sign_check("29876543", "0123456789abcdef", true, OURS, true, 29876543, why, sizeof why);
    check("key: a short mac is not a signature: `no key`", rc == 1 && !strcmp(why, "no key"));
    rc = sign_check("29876543", OURS, true, OURS, true, 29876543, why, sizeof why);
    check("key: the right mac this minute: through, nothing said", rc == 0 && !strcmp(why, "ok") && httpgate_verdict(&g, rc, why, &status, &body, &say) == 0 && !say);
    rc = sign_check("29876543", "0123456789ABCDEF0123456789ABCDEF", true, OURS, true, 29876543, why, sizeof why);
    check("key: upper-case hex reads the same", rc == 0);
    rc = sign_check("29876542", OURS, true, OURS, true, 29876543, why, sizeof why);
    check("key: a stamp one minute off is fresh (KNOCK_STALE_MIN 1)", rc == 0);
    rc = sign_check("29876545", OURS, true, OURS, true, 29876543, why, sizeof why);
    check("key: a stamp two minutes off refused `stale`", rc == 1 && !strcmp(why, "stale") && httpgate_verdict(&g, rc, why, &status, &body, &say) == 1 && !strcmp(status, "403 Forbidden") && !strcmp(body, "stale\n"));
    rc = sign_check("29876545", OURS, true, OURS, false, 0, why, sizeof why);
    check("key, no clock: the mac alone is checked, the stamp not", rc == 0);
    rc = sign_check("29876545", "ffffffffffffffffffffffffffffffff", true, OURS, false, 0, why, sizeof why);
    check("key, no clock: a wrong mac still `no key`", rc == 1 && !strcmp(why, "no key"));
    check("key: `said_unsigned` never set on a provisioned board", !g.said_unsigned);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
