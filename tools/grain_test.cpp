/* THE GRAIN on the laptop (grain.cpp has no Arduino in it):
 *   c++ -std=gnu++17 -I src -o /tmp/grain_test tools/grain_test.cpp src/grain.cpp && /tmp/grain_test
 * The same seed gives the same knobs twice; two seeds differ; and the
 * desk-parity constants (grain.py's knobs(): random.Random(f"{seed}:1")
 * then uniform(0.85,1.18) uniform(0.85,1.15) uniform(0.85,1.15)
 * uniform(0.88,1.15) uniform(0.92,1.08) uniform(0.96,1.06)) match the
 * desk's Python bit for bit. The expected values below came from:
 *
 *   python3 -c "import random; r=random.Random('123456789:1'); print([r.uniform(0.85,1.18), r.uniform(0.85,1.15), r.uniform(0.85,1.15), r.uniform(0.88,1.15), r.uniform(0.92,1.08), r.uniform(0.96,1.06), r.uniform(0.90,1.10), r.uniform(0,5), r.uniform(0.85,1.15), r.uniform(0.85,1.15), r.uniform(0.85,1.15)])"
 *
 * (the last five are the wrist's own draws, the same stream after the
 * desk's six). Regard's and the mood's colours are checked at their
 * ends. */
#include "../src/grain.h"
#include "../src/gel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* python3 -c "..." as above, for '123456789:1' */
static const double EXPECT_A[11] = {
    1.1087126306536108, 0.9464350773823215, 1.0366907843103237, 1.0079864088728132,
    1.0094797576954566, 0.9705739672914065,
    1.0610351376591705, 2.1021616492399327, 0.9004300998196582, 1.0404161458964656, 1.0447909705112801,
};
/* the same for '2:1' */
static const double EXPECT_B[11] = {
    1.1033329899777236, 0.9616477663303431, 1.125898278324557, 1.1267366662872946,
    0.9376337341623378, 1.0223604499144638,
    0.9605654115809125, 4.151264348292692, 1.0430801513724393, 1.083258706847467, 0.9598725338265581,
};
static const char *const NAMES[11] = { "bored", "blink", "saccade", "dwell", "breath", "hop",
                                       "breath_amp", "favourite", "settle", "slide", "wake_hold" };

int main(void)
{
    double a[11], a2[11], b[11];
    grain_derive(123456789, 1, a);
    grain_derive(123456789, 1, a2);
    grain_derive(2, 1, b);

    printf("the same seed twice\n");
    CHECK(!memcmp(a, a2, sizeof a), "123456789:1 derived twice, identical");

    printf("the desk's parity: 123456789:1 (Python's own numbers)\n");
    for (int i = 0; i < 11; i++)
        CHECK(a[i] == EXPECT_A[i], "%-10s %.16g == %.16g%s", NAMES[i], a[i], EXPECT_A[i], i < 6 ? "  (the desk's, exact)" : "");
    printf("the desk's parity: 2:1\n");
    for (int i = 0; i < 11; i++)
        CHECK(b[i] == EXPECT_B[i], "%-10s %.16g == %.16g", NAMES[i], b[i], EXPECT_B[i]);

    printf("two seeds differ\n");
    int diff = 0;
    for (int i = 0; i < 11; i++) if (a[i] != b[i]) diff++;
    CHECK(diff == 11, "all 11 draws differ between the seeds (%d)", diff);

    printf("the knobs on the wrist\n");
    grain_begin();
    const grain_knobs_t *k = grain_knobs();
    CHECK(!grain_told() && grain_seed() == 0, "never told: the default seed 0, regard 0");
    CHECK(fabsf(k->eye_rise_s - 0.625f) < 1e-5f && k->linger == 1.0f && !k->cool && k->mood_eye < 0,
          "regard 0: eye-rise 0.625 s, linger x1, not cool, no mood eye");
    grain_update(123456789, 0.0f, 0, "-");
    k = grain_knobs();
    CHECK(fabs(k->bored - EXPECT_A[0]) < 1e-6 && fabs(k->blink - EXPECT_A[1]) < 1e-6 && fabs(k->breath - EXPECT_A[4]) < 1e-6,
          "told 123456789: bored %.4f blink %.4f breath %.4f are the desk's", k->bored, k->blink, k->breath);
    CHECK(k->favourite == 3, "favourite = 1 + floor(2.10) = 3 (the settle)");
    CHECK(fabs(k->idle_gap - EXPECT_A[0]) < 1e-6, "regard 0: idle-gap = bored (%.4f)", k->idle_gap);

    grain_update(123456789, 1.0f, 0, "happy");
    k = grain_knobs();
    CHECK(fabsf(k->eye_rise_s - 0.35f) < 1e-5f, "regard +1: eye-rise 0.35 s (%.3f)", k->eye_rise_s);
    CHECK(fabsf(k->linger - 1.4f) < 1e-5f, "regard +1: linger x1.4");
    CHECK(fabs(k->idle_gap - EXPECT_A[0] * 0.88) < 1e-6, "regard +1: idle-gap = bored x 0.88 (the desk's REGARD_DWELL_GAIN)");
    CHECK(!k->cool && k->mood_eye == GEL_EYE_HAPPY, "happy: the mood's eye is happy, not cool");

    grain_update(123456789, -1.0f, 1, "sad");
    k = grain_knobs();
    CHECK(fabsf(k->eye_rise_s - 0.90f) < 1e-5f, "regard -1: eye-rise 0.90 s (%.3f)", k->eye_rise_s);
    CHECK(k->linger == 1.0f && fabs(k->idle_gap - EXPECT_A[0]) < 1e-6, "regard -1: linger x1, idle-gap = bored (grudges do not pace the body)");
    CHECK(k->cool && k->mood_eye == GEL_EYE_DROOPY && grain_night(), "regard -1, sad, night=1: cool, droopy, night");
    grain_update(123456789, -0.2f, 0, "annoyed");
    CHECK(!grain_knobs()->cool && grain_knobs()->mood_eye == GEL_EYE_SQUINT, "regard -0.2 annoyed: not cool (under -0.25 is), the squint");
    grain_update(123456789, -0.2f, 0, "content");
    CHECK(grain_knobs()->mood_eye == GEL_EYE_CONTENT, "content: content");
    grain_update(123456789, -0.2f, 0, "whatever");
    CHECK(grain_knobs()->mood_eye == GEL_EYE_OPEN, "an unknown mood word: open");

    grain_update(2, 0.0f, 0, "-");
    k = grain_knobs();
    CHECK(fabs(k->bored - EXPECT_B[0]) < 1e-6 && k->favourite == 5, "seed 2: bored %.4f, favourite 5 (the glance up)", k->bored);

    char line[600];
    grain_status(line, sizeof line);
    printf("       %s\n", line);
    CHECK(strstr(line, "seed 2 (the desk's)") && strstr(line, "favourite the glance up"), "`grain` names the seed and the favourite");

    printf("THE BELLY TRAVELS: the desk's fullness as a resting lump, sinking over the hour\n");
    CHECK(grain_lump() == 0 && grain_belly_now() == 0 && strstr(line, "belly none"), "never told: no lump");
    grain_belly(0.8f);
    CHECK(fabsf(grain_lump() - 0.32f) < 1e-5f && fabsf(grain_belly_now() - 0.8f) < 1e-5f, "belly 0.8 at the line: the lump 0.32 (x0.4)");
    for (int i = 0; i < 1800 * 4; i++) grain_tick(0.25f);
    CHECK(fabsf(grain_lump() - 0.16f) < 1e-3f, "half an hour on: 0.16 (%.3f)", grain_lump());
    grain_status(line, sizeof line);
    CHECK(strstr(line, "belly 0.40 (lump 0.16, 30 min old)") != NULL, "`grain` says so: %s", strstr(line, "belly"));
    for (int i = 0; i < 1800 * 4 + 4; i++) grain_tick(0.25f);
    CHECK(grain_lump() == 0 && grain_belly_now() == 0, "the hour: gone");
    grain_belly(1.7f);
    CHECK(fabsf(grain_lump() - 0.4f) < 1e-5f, "belly clamped to 1: the lump 0.4 at most");
    grain_belly(0.5f); for (int i = 0; i < 40; i++) grain_tick(0.25f);
    grain_belly(0.0f);
    CHECK(grain_lump() == 0, "belly 0 on a later line: none");
    grain_belly(-0.3f);
    CHECK(grain_lump() == 0, "a negative belly: none");
    grain_belly(0.5f); grain_tick(0); grain_tick(-1.0f);
    CHECK(fabsf(grain_lump() - 0.2f) < 1e-5f, "a zero or negative dt sinks nothing");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
