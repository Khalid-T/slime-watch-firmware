/* THE NOTICE CLEARED FROM THE WRIST (src/ancsact.h) on the laptop: the
 * six-byte PerformNotificationAction frame, the uid little-endian, the
 * negative action the only one ever built, the wrist's own fake uids
 * and the empty uid refused.
 * Build: c++ -std=gnu++17 -I src -o ancsact_test tools/ancsact_test.cpp
 * Run:   ./ancsact_test */
#include "ancsact.h"
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
    uint8_t f[ANCSACT_LEN + 2];
    /* the frame as Apple's spec lays it: CommandID 2, UID LE32, ActionID 1 */
    memset(f, 0xEE, sizeof f);
    check("uid 0x04030201 negative: six bytes", ancsact_frame(f, 0x04030201u, ANCSACT_NEGATIVE) == 6);
    check("... 02 01 02 03 04 01 (the uid little-endian)", f[0] == 2 && f[1] == 1 && f[2] == 2 && f[3] == 3 && f[4] == 4 && f[5] == 1);
    check("... nothing written past the six", f[6] == 0xEE && f[7] == 0xEE);
    check("uid 1: 02 01 00 00 00 01", ancsact_frame(f, 1, ANCSACT_NEGATIVE) == 6 && f[1] == 1 && f[2] == 0 && f[3] == 0 && f[4] == 0 && f[5] == ANCSACT_NEGATIVE);
    check("uid 0xFFFEFFFF (the last real one): built", ancsact_frame(f, 0xFFFEFFFFu, ANCSACT_NEGATIVE) == 6 && f[4] == 0xFF && f[3] == 0xFE);
    /* ruling 139: the positive action is never built, whatever is asked */
    memset(f, 0xEE, sizeof f);
    check("the POSITIVE action (answer / open): refused, nothing written", ancsact_frame(f, 7, ANCSACT_POSITIVE) == 0 && f[0] == 0xEE && f[5] == 0xEE);
    check("an action 2: refused", ancsact_frame(f, 7, 2) == 0 && f[0] == 0xEE);
    check("an action 255: refused", ancsact_frame(f, 7, 255) == 0 && f[0] == 0xEE);
    /* the uids the phone would not know */
    check("uid 0: refused (no notice has it)", ancsact_frame(f, 0, ANCSACT_NEGATIVE) == 0 && f[0] == 0xEE);
    check("a fake's uid (ANCSACT_FAKE_BASE): refused", ancsact_frame(f, ANCSACT_FAKE_BASE, ANCSACT_NEGATIVE) == 0 && f[0] == 0xEE);
    check("a fake's uid (ANCSACT_FAKE_BASE + 41): refused", ancsact_frame(f, ANCSACT_FAKE_BASE + 41, ANCSACT_NEGATIVE) == 0);
    check("ancsact_uid_real: 0 no, 1 yes, FAKE_BASE - 1 yes, FAKE_BASE no, 0xFFFFFFFF no",
          !ancsact_uid_real(0) && ancsact_uid_real(1) && ancsact_uid_real(ANCSACT_FAKE_BASE - 1) && !ancsact_uid_real(ANCSACT_FAKE_BASE) && !ancsact_uid_real(0xFFFFFFFFu));
    check("the constants: cmd 2, negative 1, positive 0, six bytes", ANCSACT_CMD == 2 && ANCSACT_NEGATIVE == 1 && ANCSACT_POSITIVE == 0 && ANCSACT_LEN == 6);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
