/* Host test of THE DOOR'S OWN PROOF (src/proof.h): the nonce'd hello
 * line (its shape, its length under 64), a proving desk's answer judged
 * PROVEN, an old desk's bare answer UNPROVEN, a wrong mac2 UNPROVEN and
 * `wrong`, the old desk's `no hello` and the one bare retry, the asks a
 * read-only session gates (and the ones it does not), `time` taken only
 * while the clock is unset, the mac read whole (a 31-hex prefix, a
 * 33-hex word), a trailing CR. The HMAC behind the seam is a fake here
 * (the real primitive is link.cpp's, proven against the desk by
 * test_watchlink's `mac`); the desk's parity test (test_watch.py)
 * runs this with four words -- `proof_test <nonce2> <line> <msg> <mac>`
 * -- and the fake answers <mac> to <msg> only: the line the real desk
 * wrote must then judge PROVEN, which proves the grammar both sides
 * build ("<nonce2> desk") is one.
 * Build: c++ -std=gnu++17 -I src -o proof_test tools/proof_test.cpp
 * Run:   ./proof_test
 */
#include "../src/proof.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* the fake primitive: a table of one, else a hex spelling of the message's bytes (32 hex, repeated) */
static const char *table_msg = NULL, *table_mac = NULL;
static void fake_hmac(const char *msg, char *out33)
{
    if (table_msg && !strcmp(msg, table_msg)) { strncpy(out33, table_mac, 32); out33[32] = 0; return; }
    size_t n = strlen(msg);
    for (int i = 0; i < 16; i++) sprintf(out33 + 2 * i, "%02x", (unsigned)(unsigned char)msg[i % (n ? n : 1)] ^ (unsigned)i);
    out33[32] = 0;
}

static const char *MAC = "0123456789abcdef0123456789abcdef";   /* the wrist's answer to the door's nonce (not under test here) */

int main(int argc, char **argv)
{
    if (argc == 5) {
        /* the parity mode: argv = nonce2, the desk's line, the msg the desk HMAC'd, its mac */
        table_msg = argv[3]; table_mac = argv[4];
        proof_t p; proof_ask(&p, MAC, 0, 0);
        strncpy(p.nonce, argv[1], PROOF_NONCE_HEX); p.nonce[PROOF_NONCE_HEX] = 0;
        int st = proof_judge(&p, argv[2], fake_hmac);
        printf("%s%s\n", proof_name(&p), p.wrong ? " wrong" : "");
        return st == PROOF_PROVEN ? 0 : 1;
    }

    proof_t p;
    char l[64];

    /* the nonce and the hello line */
    proof_ask(&p, MAC, 0x0000abcdu, 0x12345678u);
    check("the nonce is two words as 16 hex", !strcmp(p.nonce, "0000abcd12345678"));
    check("the state is asked", p.state == PROOF_ASKED && !strcmp(proof_name(&p), "asked"));
    int n = proof_hello_line(&p, l, sizeof l);
    check("the hello carries the mac and the nonce", !strcmp(l, "hello watch 0123456789abcdef0123456789abcdef 0000abcd12345678\n"));
    check("the line is 62 chars: a 64-byte buffer holds it", n == 62 && (int)strlen(l) == 62);
    check("not read-only while asked", !proof_read_only(&p));

    /* a proving desk: the sixth word is HMAC("<nonce2> desk") */
    char want[33]; fake_hmac("0000abcd12345678 desk", want);
    char line[120]; snprintf(line, sizeof line, "ok hello kaptop desk 1757404800 %s", want);
    check("a proving desk lands PROVEN", proof_judge(&p, line, fake_hmac) == PROOF_PROVEN && !p.wrong);
    check("not read-only", !proof_read_only(&p) && !strcmp(proof_name(&p), "proven"));
    check("proven takes time with the clock set", proof_takes_time(&p, true) && proof_takes_time(&p, false));
    check("a second judge is a no-op", proof_judge(&p, "ok hello x y 0", fake_hmac) == PROOF_PROVEN);

    /* the same with a CR left on the line, and extra spaces */
    proof_ask(&p, MAC, 0x0000abcdu, 0x12345678u);
    snprintf(line, sizeof line, "ok  hello kaptop  desk 1757404800 %s\r", want);
    check("a trailing CR and doubled spaces still prove", proof_judge(&p, line, fake_hmac) == PROOF_PROVEN);

    /* an old desk: the bare answer */
    proof_ask(&p, MAC, 1, 2);
    check("an old desk's bare answer lands UNPROVEN", proof_judge(&p, "ok hello kaptop desk 1757404800", fake_hmac) == PROOF_UNPROVEN);
    check("...not `wrong`: it just did not say", !p.wrong);
    check("read-only", proof_read_only(&p) && !strcmp(proof_name(&p), "unproven"));
    check("time not taken with the clock set", !proof_takes_time(&p, true));
    check("time taken while the clock is unset", proof_takes_time(&p, false));

    /* a wrong mac2 */
    proof_ask(&p, MAC, 1, 2);
    check("a wrong mac2 lands UNPROVEN", proof_judge(&p, "ok hello kaptop desk 1757404800 ffffffffffffffffffffffffffffffff", fake_hmac) == PROOF_UNPROVEN);
    check("...and `wrong`", p.wrong && proof_read_only(&p));
    proof_ask(&p, MAC, 1, 2); fake_hmac("0000000100000002 desk", want);
    char cut[120]; snprintf(cut, sizeof cut, "ok hello kaptop desk 1757404800 %.31s", want);
    check("a 31-hex prefix of the right mac is wrong", proof_judge(&p, cut, fake_hmac) == PROOF_UNPROVEN && p.wrong);
    proof_ask(&p, MAC, 1, 2); snprintf(cut, sizeof cut, "ok hello kaptop desk 1757404800 %s0", want);
    check("a 33-hex word is wrong", proof_judge(&p, cut, fake_hmac) == PROOF_UNPROVEN && p.wrong);
    proof_ask(&p, MAC, 1, 2); snprintf(cut, sizeof cut, "ok hello kaptop desk 1757404800 %s", want);
    check("the right mac under the same nonce proves", proof_judge(&p, cut, fake_hmac) == PROOF_PROVEN);
    proof_ask(&p, MAC, 1, 3); snprintf(cut, sizeof cut, "ok hello kaptop desk 1757404800 %s", want);
    check("the right mac under ANOTHER nonce is wrong (a replayed hello is worth nothing)", proof_judge(&p, cut, fake_hmac) == PROOF_UNPROVEN && p.wrong);

    /* the old desk's `no hello` and the one bare retry */
    proof_ask(&p, MAC, 1, 2);
    check("`no key` is not the retry's word", !proof_refused(&p, "no key") && p.state == PROOF_ASKED);
    check("`no hello` to the nonce'd hello asks the bare retry", proof_refused(&p, "no hello") && p.state == PROOF_RETRY);
    proof_hello_line(&p, l, sizeof l);
    check("the retry is the bare hello of old", !strcmp(l, "hello watch 0123456789abcdef0123456789abcdef\n"));
    check("a second `no hello` (the dock's echo) is not a retry", !proof_refused(&p, "no hello") && p.state == PROOF_RETRY);
    check("not read-only yet (nothing is asked between)", !proof_read_only(&p));
    check("the retry's bare answer lands UNPROVEN", proof_judge(&p, "ok hello kaptop desk 1757404800", fake_hmac) == PROOF_UNPROVEN && !p.wrong);
    proof_ask(&p, MAC, 1, 2); proof_refused(&p, "no hello"); fake_hmac("0000000100000002 desk", want);
    snprintf(cut, sizeof cut, "ok hello kaptop desk 1757404800 %s", want);
    check("a mac on the retry's answer proves nothing (no nonce went out)", proof_judge(&p, cut, fake_hmac) == PROOF_UNPROVEN && !p.wrong);
    check("`no hello` after the judge is nothing", !proof_refused(&p, "no hello"));

    /* the gate */
    check("wifi add is gated", proof_gated_ask("wifi add home pw"));
    check("look is gated", proof_gated_ask("look form=sage colour=mint"));
    check("knock is gated", proof_gated_ask("knock lost 1234 abcd"));
    check("alarm set is gated", proof_gated_ask("alarm 07:30"));
    check("alarm off is gated", proof_gated_ask("alarm off"));
    check("the bare alarm look is gated too", proof_gated_ask("alarm"));
    check("wisp is not gated", !proof_gated_ask("wisp jarvas a word"));
    check("ble pair IS gated (the operator's ruling: the pairing window is a hand)", proof_gated_ask("ble pair 60"));
    check("a stray verb is not gated (it is `no verb` as before)", !proof_gated_ask("alarmist") && !proof_gated_ask("lookout") && !proof_gated_ask("wifi list"));

    /* reset */
    proof_reset(&p);
    check("reset is none", p.state == PROOF_NONE && !proof_read_only(&p) && !strcmp(proof_name(&p), "none"));
    check("a judge in none is none", proof_judge(&p, "ok hello a b 0", fake_hmac) == PROOF_NONE);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
