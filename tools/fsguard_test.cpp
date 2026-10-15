/* Host test of THE DISK ASKS BEFORE IT FORMATS' table (src/fsguard.h):
 * the mount's answer x the NVS key x the hand, every cell -- a disk
 * that mounts is never formatted (the board on his wrist tonight: ten
 * day files, no key), a blank first flash formats once, a failed mount
 * on a formatted disk refuses, and `day format` is the one hand.
 * Build: c++ -std=gnu++17 -I src -o fsguard_test tools/fsguard_test.cpp
 * Run:   ./fsguard_test
 */
#include "../src/fsguard.h"
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
    /* the boot's four cells */
    check("mounted + key: OK, nothing", fsg_decide(true, true, false) == FSG_OK);
    check("mounted + no key (his wrist tonight: ten day files): SET_KEY, never a format", fsg_decide(true, false, false) == FSG_SET_KEY);
    check("failed + no key (the first flash / uploadfs): FORMAT once", fsg_decide(false, false, false) == FSG_FORMAT);
    check("failed + key (formatted before): REFUSE -- the files may be there", fsg_decide(false, true, false) == FSG_REFUSE);
    /* the hand */
    check("`day format` on a failed disk with the key: HAND_FORMAT", fsg_decide(false, true, true) == FSG_HAND_FORMAT);
    check("`day format` on a failed disk with no key: HAND_FORMAT", fsg_decide(false, false, true) == FSG_HAND_FORMAT);
    check("`day format` on a disk that mounts: HAND_REFUSE (nothing to mend)", fsg_decide(true, true, true) == FSG_HAND_REFUSE);
    check("`day format` on a mounting disk with no key: HAND_REFUSE", fsg_decide(true, false, true) == FSG_HAND_REFUSE);
    /* which verdicts touch the partition */
    check("only FORMAT and HAND_FORMAT format", fsg_formats(FSG_FORMAT) && fsg_formats(FSG_HAND_FORMAT)
          && !fsg_formats(FSG_OK) && !fsg_formats(FSG_SET_KEY) && !fsg_formats(FSG_REFUSE) && !fsg_formats(FSG_HAND_REFUSE));
    /* the never: with the key set, no boot ever formats, whatever the mount said */
    bool never = true;
    for (int m = 0; m < 2; m++) if (fsg_formats(fsg_decide(m != 0, true, false))) never = false;
    check("with the key set, no boot formats (only the hand may)", never);
    /* and a mounted disk never formats, key or not, hand or not */
    never = true;
    for (int k = 0; k < 2; k++) for (int h = 0; h < 2; h++) if (fsg_formats(fsg_decide(true, k != 0, h != 0))) never = false;
    check("a disk that mounts is never formatted", never);
    /* the words: the refusal's line is doctor's, verbatim */
    check("REFUSE's word leads with DISK FAILED TO MOUNT", !strncmp(fsg_word(FSG_REFUSE), "DISK FAILED TO MOUNT", 20));
    check("REFUSE's word names the hand", strstr(fsg_word(FSG_REFUSE), "`day format`") != NULL);
    check("every verdict has a word", strcmp(fsg_word(FSG_OK), "?") && strcmp(fsg_word(FSG_SET_KEY), "?") && strcmp(fsg_word(FSG_FORMAT), "?")
          && strcmp(fsg_word(FSG_REFUSE), "?") && strcmp(fsg_word(FSG_HAND_FORMAT), "?") && strcmp(fsg_word(FSG_HAND_REFUSE), "?"));
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
