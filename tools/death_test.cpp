/* THE LAST DEATH (src/death.h) on the laptop: the reason's word, the
 * breadcrumb's validity by magic and sum, the counters' rule, the
 * hour's verdict, the day header's reserved bytes.
 * Build: c++ -std=gnu++17 -I src -o death_test tools/death_test.cpp
 * Run:   ./death_test
 * Every code has its word and a code off the enum is `unknown`; a
 * power-on, a software reset and a deep-sleep wake are not crashes
 * and everything else is (ruling 112); a sealed crumb is valid, a
 * cold boot's garbage and a flipped byte are not; the verb keeps 11
 * characters and the link 7; the counters saturate; the verdict names
 * a crash inside the hour and nothing older or chosen; the crc16 is
 * CCITT-FALSE (0x29B1 for the nine digits); the header's bytes count
 * up, saturate at 255 and carry the crc little-endian. */
#include "death.h"
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
    /* the words */
    check("power-on", !strcmp(death_word(DEATH_POWERON), "power-on"));
    check("software", !strcmp(death_word(DEATH_SOFTWARE), "software"));
    check("the task watchdog", !strcmp(death_word(DEATH_TASK_WDT), "the task watchdog"));
    check("brownout", !strcmp(death_word(DEATH_BROWNOUT), "brownout"));
    check("a panic", !strcmp(death_word(DEATH_PANIC), "a panic"));
    check("deep sleep", !strcmp(death_word(DEATH_DEEPSLEEP), "deep sleep"));
    check("unknown", !strcmp(death_word(DEATH_UNKNOWN), "unknown"));
    check("a code off the enum is unknown", !strcmp(death_word(-1), "unknown") && !strcmp(death_word(DEATH_N + 3), "unknown"));
    check("the words are sized by the enum", sizeof DEATH_WORDS / sizeof DEATH_WORDS[0] == DEATH_N);

    /* ruling 112: what counts as a crash */
    check("a power-on is not a crash", !death_is_crash(DEATH_POWERON));
    check("a software reset is not a crash", !death_is_crash(DEATH_SOFTWARE));
    check("a deep-sleep wake is not a crash", !death_is_crash(DEATH_DEEPSLEEP));
    check("the task watchdog is", death_is_crash(DEATH_TASK_WDT));
    check("the interrupt watchdog is", death_is_crash(DEATH_INT_WDT));
    check("another watchdog is", death_is_crash(DEATH_OTHER_WDT));
    check("a brownout is", death_is_crash(DEATH_BROWNOUT));
    check("a panic is", death_is_crash(DEATH_PANIC));
    check("an unknown death is", death_is_crash(DEATH_UNKNOWN));

    /* the counters */
    uint32_t boots = 0, crashes = 0;
    death_count(DEATH_POWERON, &boots, &crashes);
    check("the first boot: boots 1, crashes 0", boots == 1 && crashes == 0);
    death_count(DEATH_TASK_WDT, &boots, &crashes);
    check("a watchdog death: boots 2, crashes 1", boots == 2 && crashes == 1);
    death_count(DEATH_SOFTWARE, &boots, &crashes);
    check("a reboot: boots 3, crashes 1", boots == 3 && crashes == 1);
    boots = crashes = 0xFFFFFFFFu;
    death_count(DEATH_PANIC, &boots, &crashes);
    check("the counters saturate", boots == 0xFFFFFFFFu && crashes == 0xFFFFFFFFu);

    /* the breadcrumb */
    death_crumb_t c; memset(&c, 0x5A, sizeof c);
    check("the crumb is 36 bytes", sizeof c == 36);
    check("a cold boot's garbage is not a life", !death_crumb_valid(&c));
    death_crumb_clear(&c);
    check("a cleared crumb is valid, link idle, no verb", death_crumb_valid(&c) && !strcmp(c.link, "idle") && c.verb[0] == 0 && c.uptime_s == 0);
    death_crumb_verb(&c, "haptic");
    death_crumb_nap(&c, DEATH_NAP_EAR);
    death_crumb_link(&c, "linked");
    death_crumb_tick(&c, 3612);
    check("the crumb carries verb, nap, link, uptime", death_crumb_valid(&c) && !strcmp(c.verb, "haptic") && c.nap == DEATH_NAP_EAR && !strcmp(c.link, "linked") && c.uptime_s == 3612);
    death_crumb_verb(&c, "wake-engine-and-more");
    check("the verb keeps its first 11 characters", !strcmp(c.verb, "wake-engine") && strlen(c.verb) == 11 && death_crumb_valid(&c));
    death_crumb_link(&c, "connecting");
    check("the link keeps its first 7", !strcmp(c.link, "connect") && death_crumb_valid(&c));
    death_crumb_nap(&c, 77);
    check("a nap kind off the enum is none", c.nap == DEATH_NAP_NONE);
    c.uptime_s ^= 1;
    check("a flipped byte breaks the seal", !death_crumb_valid(&c));
    death_crumb_seal(&c);
    c.magic = 0;
    check("no magic, no life", !death_crumb_valid(&c));
    death_crumb_seal(&c);
    check("sealed again: valid", death_crumb_valid(&c));
    death_crumb_verb(&c, NULL);
    check("a NULL verb is no verb", c.verb[0] == 0 && death_crumb_valid(&c));

    /* the hour's verdict */
    char v[160];
    check("a crash 212 s ago is inside the hour", death_recent(DEATH_TASK_WDT, 212));
    check("a crash 3600 s ago is not", !death_recent(DEATH_TASK_WDT, 3600));
    check("a reboot is never named", !death_recent(DEATH_SOFTWARE, 1));
    death_crumb_verb(&c, "wom-test"); death_crumb_nap(&c, DEATH_NAP_NONE); death_crumb_tick(&c, 3600);
    int n = death_verdict(v, sizeof v, DEATH_TASK_WDT, 212, &c);
    check("the verdict names the death, the age, the life, the verb", n > 0 && !strcmp(v, "died of the task watchdog 212 s ago (3600 s up before it, during `wom-test`, nap none)"));
    n = death_verdict(v, sizeof v, DEATH_BROWNOUT, 30, NULL);
    check("the verdict without a crumb", n > 0 && !strcmp(v, "died of brownout 30 s ago (no breadcrumb)"));
    n = death_verdict(v, sizeof v, DEATH_POWERON, 30, &c);
    check("a power-on says nothing", n == 0 && v[0] == 0);
    n = death_verdict(v, sizeof v, DEATH_PANIC, 7200, &c);
    check("a panic two hours ago says nothing", n == 0);

    /* the first line */
    char l[200];
    death_crumb_verb(&c, "reboot"); death_crumb_nap(&c, DEATH_NAP_DEEP); death_crumb_link(&c, "linked"); death_crumb_tick(&c, 3612);
    death_line(l, sizeof l, DEATH_SOFTWARE, &c, 12, 1);
    check("the line with a crumb", !strcmp(l, "died: software, 3612 s up before it (verb `reboot`, nap deep, link linked) | boots 12, crashes 1"));
    death_line(l, sizeof l, DEATH_POWERON, NULL, 1, 0);
    check("the line without", !strcmp(l, "died: power-on (no breadcrumb) | boots 1, crashes 0"));
    death_line(l, 20, DEATH_POWERON, NULL, 1, 0);
    check("a short buffer is cut, not overrun", strlen(l) == 19);

    /* the day header's reserved bytes */
    check("crc16 of the nine digits is 0x29B1", death_crc16("123456789") == 0x29B1);
    check("crc16 of nothing is the init", death_crc16("") == 0xFFFF && death_crc16(NULL) == 0xFFFF);
    uint8_t res[11]; memset(res, 0, sizeof res);
    death_hdr_mark(res, true, false, 0x29B1);
    check("the first boot of the day: 1 / 0, the crc little-endian", res[0] == 1 && res[1] == 0 && res[2] == 0xB1 && res[3] == 0x29);
    death_hdr_mark(res, true, true, 0x1234);
    check("a crash's boot: 2 / 1, the crc overwritten", res[0] == 2 && res[1] == 1 && res[2] == 0x34 && res[3] == 0x12);
    death_hdr_mark(res, false, false, 0x1234);
    check("the day's turn on a running body: nothing counted", res[0] == 2 && res[1] == 1);
    res[0] = res[1] = 255;
    death_hdr_mark(res, true, true, 0);
    check("the bytes saturate at 255", res[0] == 255 && res[1] == 255);
    check("bytes 4..10 stay reserved", res[4] == 0 && res[10] == 0 && DEATH_HDR_USED == 4);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
