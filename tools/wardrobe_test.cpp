/* THE WRIST'S OWN MORPHS on the laptop (bored.cpp has no Arduino in it;
 * the stub dir is on the line for the pattern, as idle_test's):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/wardrobe_test tools/wardrobe_test.cpp src/bored.cpp src/grain.cpp src/vigor.cpp -lm && /tmp/wardrobe_test
 * The gate is pure (wardrobe_may_roll: lit, holding, mounted, the bare
 * glass, nothing of its own already on); nothing rolls in the dark, on
 * the empty glass, under a page / word / veil / costume; the first urge
 * not before 300 lit s; the dark and a pet hold the clock (the dark
 * pauses it, a pet winds it back); the cadence no more than the desk's
 * (a costume every 480+ lit s at most, about one urge in three); a wear
 * 180-420 lit s and the dark does not count against it; the pick never a
 * SAGE or a HUMAN, never the last twice; the die deterministic per seed.
 * wardrobe.cpp's wiring (the veil, the sense line, NVS untouched) is
 * read there; it needs the board. */
#include "../src/bored.h"
#include "../src/grain.h"
#include "../src/vigor.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float DT = 0.025f;
static bored_in_t in;
static float lit_s = 0;               /* lit, holding, bare seconds so far in a run */
/* what a run saw */
static int wears = 0, offs = 0;
static float wear_at[64], off_at[64];
static float min_gap = 1e9f, min_wear = 1e9f, max_wear = 0;

static void bare(void) { in.dt = DT; in.lit = in.holding = in.mounted = in.bare = true; in.own_worn = false; }
static void reset_seen(void) { wears = offs = 0; min_gap = 1e9f; min_wear = 1e9f; max_wear = 0; lit_s = 0; }

/* the frames: the wardrobe's wiring in three lines -- a WEAR puts the
 * roll's costume on (own_worn), an OFF takes it home */
static void run(float seconds)
{
    for (float t = 0; t < seconds; t += DT) {
        int ev = bored_tick(&in);
        if (in.lit && in.holding) lit_s += DT;
        if (ev == BORED_WEAR) {
            if (wears < 64) wear_at[wears] = lit_s;
            if (wears > 0 && lit_s - wear_at[wears - 1] < min_gap) min_gap = lit_s - wear_at[wears - 1];
            wears++;
            in.own_worn = true;
        } else if (ev == BORED_OFF) {
            if (offs < 64) off_at[offs] = lit_s;
            float w = lit_s - wear_at[wears - 1 < 63 ? wears - 1 : 63];
            if (w < min_wear) min_wear = w;
            if (w > max_wear) max_wear = w;
            offs++;
            in.own_worn = false;
            bored_end();                          /* wardrobe.cpp: the clocks start over at the veil home */
        }
    }
}

static float bored_of_status(void)
{
    char l[96]; bored_status(l, sizeof l);
    float a = -1, b = -1;
    if (sscanf(l, "roll: bored %f s of %f", &a, &b) == 2) return a;
    return -1;
}

/* the table for the pick: flags per entry */
static uint8_t table[8]; static int table_n = 0;
static uint8_t flags_at(int i) { return table[i]; }

int main(void)
{
    grain_begin();                                /* the nominal creature: bored x1.000 */
    printf("THE WRIST'S OWN MORPHS: the gate, the clocks, the pick\n");

    /* --- the gate, pure ------------------------------------------------ */
    bare();
    CHECK(wardrobe_may_roll(&in), "the gate: lit + holding + mounted + bare + nothing own = may roll");
    in.lit = false;      CHECK(!wardrobe_may_roll(&in), "the gate: dark = no");            in.lit = true;
    in.holding = false;  CHECK(!wardrobe_may_roll(&in), "the gate: the empty glass = no");  in.holding = true;
    in.mounted = false;  CHECK(!wardrobe_may_roll(&in), "the gate: no wardrobe mounted = no"); in.mounted = true;
    in.bare = false;     CHECK(!wardrobe_may_roll(&in), "the gate: a page / word / veil / costume / touch = no"); in.bare = true;
    in.own_worn = true;  CHECK(!wardrobe_may_roll(&in), "the gate: the roll's own costume already on = no"); in.own_worn = false;

    /* --- nothing where the gate is shut ------------------------------- */
    bored_begin(7);
    bare(); in.lit = false; reset_seen(); run(2 * 3600);
    CHECK(wears == 0, "2 h in the dark: nothing rolled");
    bare(); in.holding = false; reset_seen(); run(2 * 3600);
    CHECK(wears == 0, "2 h on the empty glass: nothing rolled");
    bare(); in.mounted = false; reset_seen(); run(2 * 3600);
    CHECK(wears == 0, "2 h with no wardrobe mounted: nothing rolled");
    bare(); in.bare = false; reset_seen(); run(2 * 3600);
    CHECK(wears == 0, "2 h with something on the glass or the body: nothing rolled");
    CHECK(bored_of_status() == 0, "...and the bored clock did not run (%.0f s)", bored_of_status());

    /* --- the first urge not before 300 lit s; the odds --------------- */
    bored_begin(7);
    bare(); reset_seen();
    bored_now(true);                              /* the urge now, the costume for certain */
    run(1.0f);
    CHECK(wears == 1, "`wardrobe roll`: the urge lands on the next bare frame (%d)", wears);
    CHECK(bored_wear_s() >= 180 && bored_wear_s() <= 420, "the rolled wear is 180-420 lit s (%.0f)", bored_wear_s());
    /* the wear's clock: lit seconds only -- the dark keeps the costume, not the clock */
    in.lit = false; run(3600);
    CHECK(offs == 0, "an hour dark under the roll's costume: still on (the clock is lit seconds)");
    in.lit = true; run(500);
    CHECK(offs == 1, "...then off after its lit spell (%d off)", offs);
    CHECK(min_wear >= 180 && min_wear <= 420, "the wear measured %.0f lit s", min_wear);

    /* the plain clock, the odds as built: a long lit day */
    bored_begin(11);
    bare(); reset_seen();
    float first_wear = -1;
    for (int i = 0; i < 20 * 3600 / 60 && first_wear < 0; i++) { run(60); if (wears) first_wear = wear_at[0]; }
    CHECK(first_wear >= 300, "the first costume of a fresh day not before 300 lit s (%.0f)", first_wear);
    run(20 * 3600 - (lit_s));
    CHECK(wears >= 8 && wears <= 90, "20 lit hours: %d costumes (the desk's odds: ~2 an hour of LIT time; a worn day is ~25 lit min)", wears);
    CHECK(min_gap >= 480, "never two costumes within 480 lit s (a wear + a bored spell; the closest %.0f)", min_gap);
    CHECK(min_wear >= 180 && max_wear <= 420, "every wear 180-420 lit s (%.0f-%.0f)", min_wear, max_wear);
    CHECK(offs == wears || offs == wears - 1, "every wear came off (%d on, %d off)", wears, offs);
    /* the urge passes about two times in three: the bored spells are
     * 300-660 s, so 20 h = 150 spells at most; costumes well under */
    CHECK(wears < 150 * 0.55f, "the urge passed more often than it wore (%d of at most 150 spells)", wears);

    /* --- the dark pauses the bored clock; a pet winds it back -------- */
    bored_begin(3);
    bare(); reset_seen(); run(200);
    CHECK(fabsf(bored_of_status() - 200) < 1, "200 lit s bored (%.0f)", bored_of_status());
    in.lit = false; run(3600);
    CHECK(fabsf(bored_of_status() - 200) < 1, "an hour dark: the bored clock held at %.0f (the dark does not reset it)", bored_of_status());
    in.lit = true; run(50);
    CHECK(fabsf(bored_of_status() - 250) < 1, "lit again: it runs on (%.0f)", bored_of_status());
    bored_pet();
    CHECK(bored_of_status() == 0, "a pet: back to zero (attention is boredom's antidote)");
    run(100);
    in.bare = false; run(3600);
    CHECK(fabsf(bored_of_status() - 100) < 1, "an hour with a page up: held at %.0f", bored_of_status());
    in.bare = true;

    /* --- the desk's look / `wardrobe off` end the wear: bored_end ----- */
    bored_begin(5);
    bare(); reset_seen(); bored_now(true); run(1);
    CHECK(wears == 1 && in.own_worn, "a costume on");
    run(30);
    bored_end(); in.own_worn = false;             /* wardrobe_wear(<the desk's look>) */
    CHECK(bored_of_status() == 0, "the desk's look ends it: the clocks start over");
    run(100);
    CHECK(fabsf(bored_of_status() - 100) < 1, "...and bore again from zero (%.0f)", bored_of_status());

    /* --- determinism: the same seed, the same day -------------------- */
    float a_at[64]; int a_n;
    bored_begin(99); bare(); reset_seen(); run(6 * 3600); a_n = wears; memcpy(a_at, wear_at, sizeof a_at);
    bored_begin(99); bare(); reset_seen(); run(6 * 3600);
    bool same = a_n == wears;
    for (int i = 0; same && i < a_n && i < 64; i++) if (a_at[i] != wear_at[i]) same = false;
    CHECK(same && a_n > 0, "seed 99 twice: the same %d costumes at the same lit seconds", a_n);
    bored_begin(100); bare(); reset_seen(); run(6 * 3600);
    bool diff = a_n != wears;
    for (int i = 0; !diff && i < a_n && i < 64; i++) if (a_at[i] != wear_at[i]) diff = true;
    CHECK(diff, "seed 100: a different day");

    /* --- the pick ------------------------------------------------------ */
    /* the table: 0 plain, 1 SAGE, 2 HUMAN, 3 plain+ACT, 4 plain */
    table_n = 5; table[0] = 0; table[1] = BORED_FLAG_SAGE; table[2] = BORED_FLAG_HUMAN; table[3] = 1; table[4] = 0;
    bored_begin(1);
    int seen[8] = { 0 }; bool bad = false, twice = false; int last = -1;
    for (int i = 0; i < 2000; i++) {
        int p = bored_pick(table_n, flags_at, last);
        if (p < 0 || p >= table_n) { bad = true; break; }
        if (table[p] & (BORED_FLAG_SAGE | BORED_FLAG_HUMAN)) bad = true;
        if (p == last) twice = true;
        seen[p]++; last = p;
    }
    CHECK(!bad, "2000 picks: never a SAGE, never a HUMAN");
    CHECK(!twice, "2000 picks: never the last one twice running");
    CHECK(seen[0] > 400 && seen[3] > 400 && seen[4] > 400, "every plain form comes round (%d %d %d)", seen[0], seen[3], seen[4]);
    CHECK(seen[1] == 0 && seen[2] == 0, "the sage and the human home never (%d %d)", seen[1], seen[2]);
    table_n = 2; table[0] = BORED_FLAG_SAGE; table[1] = BORED_FLAG_HUMAN;
    CHECK(bored_pick(table_n, flags_at, -1) == -1, "a rack of the sage and the human alone: nothing to roll");
    table_n = 1; table[0] = 0;
    CHECK(bored_pick(table_n, flags_at, 0) == 0, "one plain form, worn last: it is what there is");
    CHECK(bored_pick(0, flags_at, -1) == -1, "an empty rack: nothing");
    table_n = 5; table[0] = 0; table[1] = BORED_FLAG_SAGE; table[2] = BORED_FLAG_HUMAN; table[3] = 1; table[4] = 0;
    int p1[40], p2[40];
    bored_begin(42); for (int i = 0; i < 40; i++) p1[i] = bored_pick(table_n, flags_at, i ? p1[i - 1] : -1);
    bored_begin(42); for (int i = 0; i < 40; i++) p2[i] = bored_pick(table_n, flags_at, i ? p2[i - 1] : -1);
    CHECK(memcmp(p1, p2, sizeof p1) == 0, "seed 42 twice: the same 40 picks");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
