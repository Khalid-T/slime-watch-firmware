/* THE WISP on the laptop (wisp.cpp has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/wisp_test tools/wisp_test.cpp src/wisp.cpp src/theme.cpp -x c src/gel.c src/notice.c -lm && /tmp/wisp_test
 * The house's rules and the glance: one per app, three at most, the
 * fourth sends the oldest off; a glance sends a landed one off after
 * its hold and its fade; the DARK GLASS queues (three, the oldest
 * dropped) and the light flies them in -- the raise that lit the glass
 * is not their glance, the next one is; he leaves and the hovering ones
 * go; a notice cleared at its source takes its wisp.
 * THE WISP'S MANNERS (2026-10-09, E41-E43): THE WISP AT THE DOOR -- a
 * notice while EMPTY is kept at the air door (dim, one px, no halo, no
 * landing), the arrival flies it in as a landing, the hour's fade, the
 * queue's three; THE WISP'S INK -- the drawn mote is the theme's shine
 * for the desk, shade for the phone, body for the box (`jarvas`); THE
 * WISP EATEN -- the feed's end on a hovering wisp swallows it (the
 * short flight in, no bob, no halo), the small lump on the belly's
 * term, the ledger keeps the notice, the post freed.
 * THE NOTICE'S OWN REACTION (2026-10-06): the landing's take carries
 * the strongest reaction of the frame (a call's hop over a mail's
 * bump; a missed call's two bumps; a silent one is none; the desk's
 * default); the queue carries it through the dark; the halos (the
 * wide one draws a ring more, the pulsing one moves on its own and
 * stops at the glance and at the call's end); the ledger's message
 * and the app's name given later. */
#include "../src/wisp.h"
#include "../src/board.h"
#include "../src/theme.h"     /* THE WISP'S INK: the inks are the theme's */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 1.0f / 30.0f;
static float phi = 0.0f;
static bool holding = true, lit = true, glanced = false;

static uint32_t now_ms = 1000;                       /* the wall clock the ledger stamps by */
static void run(float seconds)
{
    for (float t = 0; t < seconds; t += DT) { now_ms += (uint32_t)(DT * 1000.0f); wisp_tick(now_ms, DT, phi, holding, lit, glanced); }
}

static void reset(void)
{
    /* everything off the glass: he leaves, a few seconds pass, he is back */
    holding = false; lit = true; glanced = false; run(5.0f);
    holding = true; run(0.1f);
}

int main(void)
{
    char line[240];
    printf("wisp: the house\n");
    holding = false; run(0.1f);
    CHECK(wisp_arrive("com.mail", "a letter", false) == WISP_OK, "EMPTY: ok -- it waits at the door (E43; `no house` is retired)");
    CHECK(wisp_count() == 0 && wisp_waiting() == 1 && wisp_at_door() == 1, "nothing on the glass, one at the door");
    CHECK(wisp_arrive("", NULL, true) == WISP_NO_HOUSE, "no app: nothing");
    wisp_leave("com.mail");
    CHECK(wisp_waiting() == 0, "cleared at the source while at the door: gone");
    holding = true; run(0.1f);
    CHECK(wisp_arrive("com.mail", "a letter", true) == WISP_OK, "HOLDING, lit: ok");
    CHECK(wisp_count() == 1 && wisp_waiting() == 0, "one on the glass, none waiting");
    wisp_status(line, sizeof line);
    CHECK(strstr(line, "com.mail \"a letter\"") != NULL, "status names it: %s", line);

    printf("wisp: the glance\n");
    CHECK(wisp_take_landed() == 0, "THE BUMPS: not landed yet (the flight in takes 1.4 s)");
    run(1.0f);
    CHECK(wisp_take_landed() == 0, "still flying at 1.0 s: no landing");
    run(2.0f);                                          /* landed (FLY_S 1.4), no glance yet */
    CHECK(wisp_take_landed() == 1, "landed once, taken once");
    CHECK(wisp_take_landed() == 0, "taken: nothing again (one bump per flight in)");
    CHECK(wisp_count() == 1, "hovering, unglanced: it stays");
    glanced = true; run(0.5f);
    CHECK(wisp_count() == 1, "glanced: it holds a moment (SEEN_S 1.2)");
    run(2.5f);                                          /* the hold + the fade (1.0) */
    CHECK(wisp_count() == 0, "then fades and is gone");
    glanced = false;

    printf("wisp: one per app, three at most\n");
    wisp_arrive("a", NULL, true); wisp_arrive("a", "again", true);
    CHECK(wisp_count() == 1, "the same app twice: one wisp (its halo swells)");
    wisp_arrive("b", NULL, true); wisp_arrive("c", NULL, true);
    CHECK(wisp_count() == 3, "three apps, three posts");
    wisp_arrive("d", NULL, true);
    CHECK(wisp_count() == 4, "a fourth: the oldest is fading while it flies in (4 structs)");
    run(2.0f);
    CHECK(wisp_count() == 3, "and the oldest is gone: three again");
    wisp_status(line, sizeof line);
    CHECK(strstr(line, "; a") == NULL && strstr(line, "; d") != NULL, "a went, d stays: %s", line);
    reset();
    CHECK(wisp_count() == 0, "reset: the glass clear");

    printf("wisp: the dark glass\n");
    lit = false; run(0.1f);
    CHECK(wisp_arrive("com.mail", NULL, true) == WISP_OK, "dark: ok (it waits)");
    CHECK(wisp_count() == 0 && wisp_waiting() == 1, "nothing on the glass, one waiting");
    run(10.0f);
    CHECK(wisp_count() == 0 && wisp_waiting() == 1, "the dark does not fly it in");
    wisp_arrive("com.mail", "newer", true);
    CHECK(wisp_waiting() == 1, "the same app again while waiting: still one");
    wisp_arrive("x", NULL, true); wisp_arrive("y", NULL, true); wisp_arrive("z", NULL, true);
    CHECK(wisp_waiting() == 3, "the queue holds three");
    wisp_status(line, sizeof line);
    CHECK(strstr(line, "(com.mail [desk])") == NULL && strstr(line, "(z [desk])") != NULL, "the oldest (com.mail) was dropped for the fourth: %s", line);
    wisp_leave("y");
    CHECK(wisp_waiting() == 2, "cleared at the source while waiting: out of the queue");
    /* the raise: the glass lights, and the wake IS a glance (main passes wake_t > 0) */
    lit = true; glanced = true; run(0.1f);
    CHECK(wisp_count() == 2 && wisp_waiting() == 0, "the light: both fly in, the queue empty");
    wisp_take_landed();
    run(4.0f);                                          /* a long glance: the hour showing after the wake */
    CHECK(wisp_take_landed() == 2, "THE BUMPS: both landed on the same frame -- two in one take (main bumps once)");
    CHECK(wisp_count() == 2, "the glance they flew in under is not theirs: they stay");
    glanced = false; run(0.2f);
    glanced = true; run(3.0f);
    CHECK(wisp_count() == 0, "the next glance is: gone");
    glanced = false;

    printf("wisp: he leaves\n");
    lit = false; run(0.1f);
    wisp_arrive("q", NULL, true);
    lit = true; run(0.1f);
    wisp_arrive("r", NULL, true);
    run(2.0f);
    CHECK(wisp_count() == 2, "two hovering");
    lit = false; run(0.1f);
    wisp_arrive("s", NULL, true);
    CHECK(wisp_waiting() == 1, "one waiting in the dark");
    holding = false; run(0.1f);
    CHECK(wisp_waiting() == 1 && wisp_at_door() == 1, "EMPTY: what waited unseen goes to the door with him gone (E43)");
    run(3.0f);
    CHECK(wisp_count() == 0, "and the hovering ones fade");
    holding = true; lit = true; run(0.1f);
    CHECK(wisp_count() == 1 && wisp_waiting() == 0, "his return: it flies in with him");
    run(2.0f); wisp_take_landed(); glanced = true; run(3.0f); glanced = false;
    CHECK(wisp_count() == 0, "...and the glance sends it off");

    printf("wisp: cleared at the source\n");
    wisp_arrive("m", NULL, true); run(2.0f);
    wisp_leave("m"); run(3.0f);
    CHECK(wisp_count() == 0, "a hovering wisp whose notice was cleared: gone without a glance");

    printf("wisp: the draw and the mark\n");
    wisp_arrive("d", NULL, true); run(2.0f);
    uint16_t *fb = (uint16_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
    dirty_rect_t box;
    wisp_draw(fb, phi);
    bool changed = wisp_mark(&box);
    int lit_px = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i]) lit_px++;
    CHECK(lit_px >= 9 && lit_px <= 25, "the mote: a 3x3 core and a halo (%d px)", lit_px);
    CHECK(changed && box.w > 0 && box.h > 0, "the mark: a box the first frame (%dx%d at %d,%d)", box.w, box.h, box.x, box.y);
    /* the air door is opposite the floor: phi 0 = floor at the bottom, so
     * a fresh wisp starts near the top of the glass */
    lit = false; run(0.1f);
    wisp_arrive("top", NULL, true);
    lit = true; run(DT);
    memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t));
    wisp_draw(fb, phi);
    int top_px = 0;
    for (int y = 0; y < 20; y++) for (int x = 0; x < SCREEN_W; x++) if (fb[y * SCREEN_W + x]) top_px++;
    CHECK(top_px > 0, "phi 0: the newcomer is at the top rim, the air door (%d px in the top 20 rows)", top_px);
    free(fb);

    printf("wisp: the ledger (THE WATCH PAGE's notices)\n");
    {
        const wisp_note_t *lg = NULL;
        int n = wisp_ledger(&lg);
        CHECK(n == WISP_LEDGER && lg && !strcmp(lg[0].app, "top"), "three kept, the newest first: %s, %s, %s", lg[0].app, lg[1].app, lg[2].app);
        uint32_t t_top = lg[0].at_ms;
        run(90.0f);
        wisp_arrive("com.mail", "a letter", true); run(0.1f);
        n = wisp_ledger(&lg);
        CHECK(n == 3 && !strcmp(lg[0].app, "com.mail") && !strcmp(lg[0].title, "a letter") && !strcmp(lg[1].app, "top"),
              "a fourth: the oldest out, com.mail first with its title, top second");
        CHECK(lg[0].at_ms > t_top && lg[0].at_ms - t_top >= 88000u, "stamped by the wall clock (%lu ms after top's; the test's 33 ms frames)", (unsigned long)(lg[0].at_ms - t_top));
        wisp_arrive("top", "again", true); run(0.1f);
        n = wisp_ledger(&lg);
        CHECK(n == 3 && !strcmp(lg[0].app, "top") && !strcmp(lg[0].title, "again") && !strcmp(lg[1].app, "com.mail"),
              "the same app again: moved to the top with the newer title, still three");
        holding = false; run(0.1f);
        wisp_arrive("nobody", NULL, false);
        n = wisp_ledger(&lg);
        CHECK(n == 3 && !strcmp(lg[0].app, "nobody"), "a notice the house was empty for IS kept now (it waits at the door: E43)");
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "the ledger: 3 (newest nobody") != NULL && strstr(line, "(nobody [desk] at the door)") != NULL, "status names the ledger and the door: %s", line);
        wisp_leave("nobody");
        holding = true; run(0.1f);
    }

    printf("wisp: THE NOTICE'S OWN REACTION (notice.h): the landing's take\n");
    {
        reset();
        notice_react_t call, mail, missed, silent_call, important;
        notice_react(NOTICE_CAT_CALL, 0, &call); notice_react(NOTICE_CAT_EMAIL, 0, &mail); notice_react(NOTICE_CAT_MISSED_CALL, 0, &missed);
        notice_react(NOTICE_CAT_CALL, NOTICE_FLAG_SILENT, &silent_call); notice_react(NOTICE_CAT_EMAIL, NOTICE_FLAG_IMPORTANT, &important);
        int kind = 99, reps = 99;
        wisp_take_landed();                             /* the ledger block's landings, untaken: emptied */
        CHECK(wisp_take_landed_react(&kind, &reps) == 0 && kind == -1 && reps == 0, "nothing landed: none, 0 plays");
        wisp_arrive("desk", "a build done", true); run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 1 && kind == HAPTIC_BUMP && reps == 1, "the desk's wisp (wisp_arrive): the default -- one bump, once");
        CHECK(wisp_take_landed_react(&kind, &reps) == 0 && kind == -1 && reps == 0, "taken: nothing again");
        wisp_arrive_react("com.apple.mobilephone", "Mum", NULL, &missed, true); run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 1 && kind == HAPTIC_BUMP && reps == 2, "a missed call: one bump, TWICE");
        wisp_arrive_react("com.mail", "a letter", "the fees are due on friday", &mail, true);
        wisp_arrive_react("com.phone2", "ringing", NULL, &call, true);
        run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 2 && kind == HAPTIC_HOP3 && reps == 1, "a mail and a call landing in one take: the call's hop is the touch (the strongest)");
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "com.phone2 \"ringing\" [desk] (ringing)") != NULL, "status names the ringing one: %s", line);
        reset();
        wisp_arrive_react("com.news", "headline", NULL, &silent_call, true); run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 1 && kind == -1 && reps == 0, "a silent notice lands: counted, nothing to play");
        wisp_arrive_react("com.news", "again", NULL, &mail, true); run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 0, "the same app again while it hovers: no landing, nothing felt (the halo swells)");
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "(ringing)") == NULL, "...and it wears the NEWER reaction: the ringing stopped (%s)", line);
        /* the queue carries it through the dark */
        reset(); lit = false; run(0.1f);
        wisp_arrive_react("com.phone2", NULL, NULL, &call, true);
        wisp_arrive_react("com.phone2", NULL, NULL, &missed, true);
        lit = true; run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 1 && kind == HAPTIC_BUMP && reps == 2, "queued in the dark, the reaction waits with it (the newer one: the missed call's two bumps)");
        /* the old take still empties the same count */
        wisp_arrive_react("x", NULL, NULL, &call, true); run(2.0f);
        CHECK(wisp_take_landed() == 1 && wisp_take_landed_react(&kind, &reps) == 0 && kind == -1, "wisp_take_landed empties the react take too");
        (void)important;
    }

    printf("wisp: the halos -- wide draws a ring more, pulsing moves on its own and stops at the glance\n");
    {
        reset();
        notice_react_t call, important, plain;
        notice_react(NOTICE_CAT_CALL, 0, &call); notice_react(NOTICE_CAT_EMAIL, NOTICE_FLAG_IMPORTANT, &important); notice_react_default(&plain);
        uint16_t *fb = (uint16_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
        auto lit_px = [&]() { int n = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i]) n++; return n; };
        auto sum_px = [&]() { unsigned long n = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) n += fb[i]; return n; };
        wisp_arrive_react("plain", NULL, NULL, &plain, true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        int plain_px = lit_px();
        CHECK(plain_px >= 9 && plain_px <= 25, "the plain halo: the core and one ring (%d px)", plain_px);
        reset();
        wisp_arrive_react("wide", NULL, NULL, &important, true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        int wide_px = lit_px();
        CHECK(wide_px > plain_px && wide_px <= 49, "the wide halo: a ring more, one px out (%d px > %d)", wide_px, plain_px);
        reset();
        wisp_arrive_react("ring", NULL, NULL, &call, true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        unsigned long a = sum_px();
        run(0.45f);                                     /* half a ring's period (RING_HZ 1.1): the swell at the other end */
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        unsigned long b = sum_px();
        CHECK(a != b, "the pulsing halo: the halo's weight differs half a period later (%lu vs %lu)", a, b);
        glanced = true; run(0.3f);                      /* the glance: it holds (SEEN_S 1.2), the ring stops */
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        unsigned long c1 = sum_px();
        run(0.45f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        unsigned long c2 = sum_px();
        CHECK(c1 == c2 && wisp_count() == 1, "glanced: the ring stops while it holds (%lu == %lu), still here", c1, c2);
        glanced = false; run(3.0f);
        CHECK(wisp_count() == 0, "...and it goes");
        wisp_arrive_react("ring", NULL, NULL, &call, true); run(3.0f);
        wisp_leave("ring"); run(3.0f);
        CHECK(wisp_count() == 0, "the call ended at its source (wisp_leave): the ringing wisp is gone without a glance");
        free(fb);
    }

    printf("wisp: the ledger's message and the app's name\n");
    {
        reset();
        const wisp_note_t *lg = NULL;
        notice_react_t mail; notice_react(NOTICE_CAT_EMAIL, 0, &mail);
        wisp_arrive_react("com.apple.mobilemail", "The university", "Your fees for the autumn term are due on Friday the 10th; see the attached invoice for the amount and the account", &mail, true);
        run(0.1f);
        int n = wisp_ledger(&lg);
        CHECK(n >= 1 && !strcmp(lg[0].app, "com.apple.mobilemail") && !strcmp(lg[0].title, "The university") && !strncmp(lg[0].msg, "Your fees for the autumn", 24)
              && strlen(lg[0].msg) == WISP_MSG_MAX - 1 && lg[0].name[0] == 0, "the message kept, cut to %d; no name yet", WISP_MSG_MAX - 1);
        wisp_ledger_name("com.apple.mobilemail", "Mail");
        wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].name, "Mail"), "the name given later: Mail");
        wisp_ledger_name("nobody", "Nobody");
        wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].name, "Mail"), "a name for an app not in the ledger: nothing");
        wisp_arrive_react("com.apple.mobilemail", "Again", NULL, &mail, true); run(0.1f);
        wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].name, "Mail") && !strcmp(lg[0].title, "Again") && lg[0].msg[0] == 0, "the same app again: the name kept, the newer title, no message this time");
        wisp_arrive("desk", "the old road", true); run(0.1f);
        wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].app, "desk") && lg[0].msg[0] == 0 && lg[0].name[0] == 0, "wisp_arrive: no message, no name");
        wisp_ledger_name("com.apple.mobilemail", "");
        wisp_ledger(&lg);
        CHECK(lg[1].name[0] == 0, "an empty name clears it");
    }

    printf("wisp: THE WISP AT THE DOOR (E43) -- dim at the air door while EMPTY, the arrival flies it in, the hour, the three\n");
    {
        reset();
        notice_react_t call; notice_react(NOTICE_CAT_CALL, 0, &call);
        uint16_t *fb = (uint16_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
        auto lit_px = [&]() { int n = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i]) n++; return n; };
        auto top_px = [&]() { int n = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < SCREEN_W; x++) if (fb[y * SCREEN_W + x]) n++; return n; };
        holding = false; lit = true; run(0.1f);
        int kind = 99, reps = 99;
        CHECK(wisp_arrive_from(WISP_SRC_PHONE, "com.whatsapp", "Mum", "call me", &call, false) == WISP_OK, "EMPTY, lit: ok");
        CHECK(wisp_count() == 0 && wisp_at_door() == 1, "no mote beside him (nobody there): one at the door");
        wisp_take_landed();                             /* the earlier blocks' landings, untaken: emptied */
        run(5.0f);
        CHECK(wisp_count() == 0 && wisp_take_landed_react(&kind, &reps) == 0 && kind == -1, "5 s on: still at the door, no landing, nothing felt");
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(lit_px() == 1 && top_px() == 1, "drawn as ONE dim px at the air door (phi 0: the top rim), no halo (%d px lit)", lit_px());
        uint16_t door_px = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i]) door_px = fb[i];
        uint16_t door_v = (uint16_t)((door_px >> 8) | (door_px << 8));
        const gel_rgba_t *shade = wisp_ink(WISP_SRC_PHONE);
        int full_g = (int)(shade->g * 63 + 0.5f), door_g = (door_v >> 5) & 63;
        CHECK(door_g > 0 && door_g < full_g, "...dimmer than the phone's full ink (green %d of %d)", door_g, full_g);
        wisp_arrive("desk-a", NULL, false); wisp_arrive("desk-b", NULL, false);
        CHECK(wisp_at_door() == 3 && wisp_waiting() == 3, "three at the door");
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(lit_px() == 1, "...still ONE dim px (the glass never counts)");
        wisp_arrive("desk-c", NULL, false);
        wisp_status(line, sizeof line);
        CHECK(wisp_at_door() == 3 && strstr(line, "(com.whatsapp") == NULL && strstr(line, "(desk-c [desk] at the door)") != NULL, "a fourth: the oldest waits no more (the queue's three): %s", line);
        const wisp_note_t *lg = NULL; wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].app, "desk-c") && !strcmp(lg[2].app, "desk-a"), "the ledger kept every one (the page keeps it either way)");
        /* THE ARRIVAL: holding turns true (the gel's rise) -- they fly in with him as landings */
        holding = true; run(0.1f);
        CHECK(wisp_count() == 3 && wisp_at_door() == 0 && wisp_waiting() == 0, "the arrival: all three fly in, the door empty");
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(lit_px() > 1, "the door's dim px is gone, the motes are on the glass (%d px)", lit_px());
        run(2.0f);
        CHECK(wisp_take_landed_react(&kind, &reps) == 3 && kind == HAPTIC_BUMP && reps == 1, "three landed (the desk's bumps; the call's hop went with the dropped one): felt now, through the gate");
        /* a dark arrival: the door's wisps wait for the first lit frame as the dark glass's do */
        reset(); holding = false; lit = false; run(0.1f);
        wisp_arrive("dark-door", NULL, false);
        CHECK(wisp_at_door() == 1, "EMPTY and dark: at the door");
        holding = true; run(0.5f);
        CHECK(wisp_count() == 0 && wisp_waiting() == 1 && wisp_at_door() == 0, "the arrival in the dark: off the door, waiting for the light");
        lit = true; run(0.1f);
        CHECK(wisp_count() == 1 && wisp_waiting() == 0, "the light: it flies in");
        /* the hour: unclaimed, it fades -- counted for main's log */
        reset(); holding = false; lit = true; run(0.1f);
        wisp_arrive("patient", NULL, false);
        run(WISP_DOOR_S - 30.0f);
        CHECK(wisp_at_door() == 1 && wisp_take_door_faded() == 0, "59.5 min at the door: still there");
        wisp_arrive("late", NULL, false);
        run(120.0f);                                    /* (the test's wall clock runs 33 ms a frame: a hair under its float seconds) */
        CHECK(wisp_at_door() == 1 && wisp_take_door_faded() == 1 && wisp_take_door_faded() == 0, "the hour: the first faded (taken once), the later one stays");
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "1 faded at the door") != NULL && strstr(line, "(late [desk] at the door)") != NULL, "status counts it: %s", line);
        wisp_arrive("patient", NULL, false);              /* the same app at the door twice keeps the FIRST clock */
        run(10.0f); wisp_arrive("late", "again", false); run(WISP_DOOR_S - 65.0f);
        CHECK(wisp_take_door_faded() == 1 && wisp_at_door() == 1, "the same app again does not reset the door's hour (late faded on its first clock)");
        wisp_leave("patient");
        holding = true; run(0.1f);
        free(fb);
    }

    printf("wisp: THE WISP'S INK (E42) -- the shine for the desk, the shade for the phone, the body for the box\n");
    {
        reset();
        uint16_t *fb = (uint16_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
        auto ink565 = [](const gel_rgba_t *c) {
            int r = (int)(c->r * 31 + 0.5f), g = (int)(c->g * 63 + 0.5f), b = (int)(c->b * 31 + 0.5f);
            return (uint16_t)((r << 11) | (g << 5) | b);
        };
        auto has565 = [&](uint16_t v) { uint16_t sw = (uint16_t)((v >> 8) | (v << 8)); int n = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i] == sw) n++; return n; };
        const gel_pal_t *pal = theme_pal();
        CHECK(wisp_ink(WISP_SRC_DESK) == &pal->shine && wisp_ink(WISP_SRC_PHONE) == &pal->shade && wisp_ink(WISP_SRC_BOX) == &pal->body && wisp_ink(99) == &pal->shine,
              "the three inks are the theme's shine / shade / body; anything else the shine");
        CHECK(wisp_source_of("com.mail") == WISP_SRC_DESK && wisp_source_of("jarvas") == WISP_SRC_BOX && wisp_source_of("Jarvas") == WISP_SRC_BOX && wisp_source_of(NULL) == WISP_SRC_DESK,
              "the app id names the source: `jarvas` (case folded) is the box, the rest the desk");
        wisp_arrive("com.mail", NULL, true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(has565(ink565(&pal->shine)) >= 9 && has565(ink565(&pal->shade)) == 0, "the desk's mote: a 3x3 core of the SHINE");
        reset();
        wisp_arrive_from(WISP_SRC_PHONE, "com.apple.MobileSMS", NULL, NULL, NULL, true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(has565(ink565(&pal->shade)) >= 9 && has565(ink565(&pal->shine)) == 0, "the phone's mote: the SHADE, no shine in it");
        reset();
        wisp_arrive("jarvas", "the box speaks", true); run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(has565(ink565(&pal->body)) >= 9 && has565(ink565(&pal->shine)) == 0, "the box's mote: the BODY colour");
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "jarvas \"the box speaks\" [box]") != NULL, "status names the ink: %s", line);
        /* the same app again while it hovers takes the newer ink; the queue carries the ink through the dark */
        wisp_arrive_from(WISP_SRC_PHONE, "jarvas", NULL, NULL, NULL, true); run(0.1f);
        wisp_status(line, sizeof line);
        CHECK(strstr(line, "jarvas \"the box speaks\" [phone]") != NULL, "the same app again: the newer ink (%s)", line);
        reset(); lit = false; run(0.1f);
        wisp_arrive_from(WISP_SRC_PHONE, "dark-phone", NULL, NULL, NULL, true);
        lit = true; run(3.0f);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(has565(ink565(&pal->shade)) >= 9, "queued in the dark: the ink waits with it");
        free(fb);
    }

    printf("wisp: THE WISP EATEN (E41) -- the feed's end on a hovering wisp\n");
    {
        reset();
        uint16_t *fb = (uint16_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
        auto lit_px = [&]() { int n = 0; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i]) n++; return n; };
        /* phi 0: the floor at the bottom; post 0 is (depth 18, along 74) = screen (194, 138); a feed ends in the base third (y > 160) */
        CHECK(wisp_feed_at(194, 170) == false && wisp_lump() == 0, "no wisp: the feed is a meal, nothing eaten, no lump");
        wisp_arrive("com.mail", "a letter", true); run(0.5f);
        CHECK(wisp_feed_at(194, 170) == false, "still flying in (0.5 of 1.4 s): not yet on its post, not eaten");
        run(2.0f); wisp_take_landed();
        CHECK(wisp_feed_at(194 - 40, 170 + 10) == false && wisp_count() == 1, "the feed's end 41 px off: not eaten (WISP_EAT_PX %g)", (double)WISP_EAT_PX);
        CHECK(wisp_feed_at(194, 170) == true, "the feed's end 32 px under the post: EATEN");
        CHECK(wisp_lump() > WISP_LUMP * 0.98f && wisp_lump() <= WISP_LUMP && wisp_eaten() == 1, "the small lump at the bite: %.2f (WISP_LUMP %.2f)", (double)wisp_lump(), (double)WISP_LUMP);
        CHECK(wisp_feed_at(194, 170) == false, "a second feed on the same spot: nothing left to eat");
        CHECK(wisp_eat_app("com.mail", 120, 150) == false, "T17: eat by app -- the one eaten already: nothing");
        run(DT * 3);
        memset(fb, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t)); wisp_draw(fb, phi);
        CHECK(wisp_count() == 1 && lit_px() > 0 && lit_px() <= 9, "on its way in: the core alone, no halo (%d px)", lit_px());
        run(WISP_EAT_S);
        CHECK(wisp_count() == 0, "swallowed (WISP_EAT_S %.2f): gone", (double)WISP_EAT_S);
        CHECK(wisp_take_landed() == 0, "no landing for a swallow");
        const wisp_note_t *lg = NULL; wisp_ledger(&lg);
        CHECK(!strcmp(lg[0].app, "com.mail") && !strcmp(lg[0].title, "a letter"), "the ledger keeps the notice: eaten is read, not lost");
        run(WISP_LUMP_S * 0.5f);
        float half = wisp_lump();
        CHECK(half > 0 && half < WISP_LUMP * 0.3f, "halfway through WISP_LUMP_S the lump is mostly gone (%.3f: the ease-out)", (double)half);
        run(WISP_LUMP_S * 0.5f + 1.0f);
        CHECK(wisp_lump() == 0, "and then none");
        /* the post is freed by the swallow: the next wisp takes it */
        wisp_arrive("a", NULL, true); wisp_arrive("b", NULL, true); wisp_arrive("c", NULL, true); run(2.0f); wisp_take_landed();
        CHECK(wisp_count() == 3, "three hovering");
        CHECK(wisp_feed_at(194, 170) == true, "the one on post 0 eaten");
        wisp_arrive("d", NULL, true); run(0.1f);
        wisp_status(line, sizeof line);
        CHECK(wisp_count() == 4 && strstr(line, "; a \"") == NULL && strstr(line, "; a [desk] (eaten)") != NULL && strstr(line, "; d [desk]") != NULL, "a fourth flies in to the freed post, none sent off: %s", line);
        run(2.0f);
        CHECK(wisp_count() == 3, "the swallow done: three again");
        /* the nearest wins; a glance still sends the rest off as ever */
        glanced = true; run(3.0f); glanced = false;
        CHECK(wisp_count() == 0, "the glance: the hovering ones go as they always did");
        wisp_take_landed();
        /* the eaten one is past the glance and past he-leaves (it is already inside him) */
        wisp_arrive("e", NULL, true); run(2.0f); wisp_take_landed();
        CHECK(wisp_feed_at(194, 170) == true, "eaten");
        glanced = true; run(DT * 2); glanced = false;
        CHECK(wisp_count() == 1, "a glance during the swallow does not restart it as a fade");
        run(1.0f);
        CHECK(wisp_count() == 0, "gone the short way");
        /* THE NOTICE CLEARED FROM THE WRIST (T17): the hold on the open card eats that app's hovering wisp the same way */
        wisp_arrive("f", "a letter", true); run(0.5f);
        CHECK(wisp_eat_app("f", 120, 150) == false, "T17: still flying in: not eaten by app either");
        run(2.0f); wisp_take_landed();
        uint32_t ate = wisp_eaten();
        CHECK(wisp_eat_app("g", 120, 150) == false && wisp_count() == 1, "T17: no wisp of that app: nothing");
        CHECK(wisp_eat_app("f", 120, 150) == true && wisp_eaten() == ate + 1 && wisp_lump() > WISP_LUMP * 0.98f, "T17: that app's hovering wisp eaten into the point given: the lump, the count");
        CHECK(wisp_eat_app("f", 120, 150) == false, "T17: once");
        run(WISP_EAT_S + DT * 2);
        wisp_ledger(&lg);
        CHECK(wisp_count() == 0 && !strcmp(lg[0].app, "f"), "T17: swallowed; the ledger keeps it (read, not lost)");
        free(fb);
    }

    printf("wisp: THE WISP WATCHED (C3) -- the newest mote's point for the gaze, its flight, the swell's take\n");
    {
        reset();
        int x = -1, y = -1; bool flying = true;
        CHECK(wisp_eye_point(&x, &y) == false && wisp_eye_seq(&flying) == 0 && !flying, "none: no point, seq 0, not flying");
        wisp_take_swelled();
        CHECK(wisp_take_swelled() == 0, "taken: nothing swelled since");
        wisp_arrive("a", NULL, true); run(DT);
        uint32_t s1 = wisp_eye_seq(&flying);
        CHECK(wisp_eye_point(&x, &y) && s1 > 0 && flying, "one flying in: a point (%d,%d), seq %lu, flying", x, y, (unsigned long)s1);
        CHECK(y < 40 && x > 100 && x < 140, "the first frame: at the air door (the top rim at phi 0)");
        run(0.7f);
        int xm = x, ym = y; wisp_eye_point(&xm, &ym);
        CHECK(ym > y && wisp_eye_seq(&flying) == s1 && flying, "halfway: lower on the glass (%d,%d), still flying", xm, ym);
        run(1.0f); wisp_take_landed();
        wisp_eye_point(&x, &y); wisp_eye_seq(&flying);
        CHECK(!flying && x == 194 && y == 138, "landed: at post 0 (%d,%d), not flying (the bob aside)", x, y);
        wisp_arrive("a", "again", true);
        CHECK(wisp_take_swelled() == 1 && wisp_take_swelled() == 0, "the same app again: one swell, taken once");
        wisp_arrive("b", NULL, true); run(DT);
        uint32_t s2 = wisp_eye_seq(&flying);
        CHECK(s2 > s1 && flying && wisp_eye_point(&x, &y) && y < 40, "a second app: the NEWEST is the point (seq %lu > %lu), flying from the door", (unsigned long)s2, (unsigned long)s1);
        wisp_leave("b"); run(DT);
        CHECK(wisp_eye_seq(&flying) == s2, "b cleared at its source: it holds its moment (SEEN_S) -- still the point");
        run(1.3f);
        CHECK(wisp_eye_seq(&flying) == s1, "... then fading: a is the newest again (wispeye.c will not watch it twice)");
        wisp_arrive("c", NULL, true); run(2.0f); wisp_take_landed();
        uint32_t s3 = wisp_eye_seq(&flying);
        CHECK(s3 > s2 && !flying, "c landed: the newest");
        CHECK(wisp_feed_at(194, 170) == true, "a (post 0) eaten by the feed");
        CHECK(wisp_eye_seq(&flying) == s3, "an eaten one is the feed's: c still the point");
        glanced = true; run(0.2f); glanced = false;
        CHECK(wisp_eye_seq(&flying) == s3, "glanced, holding its moment: still the point");
        run(2.5f);
        CHECK(wisp_eye_point(&x, &y) == false && wisp_eye_seq(&flying) == 0, "faded and gone: none");
        wisp_take_swelled();
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
