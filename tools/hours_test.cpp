/* THE HOURS' LIGHT on the laptop (hours.cpp has no Arduino in it; gel.c
 * only for GEL_PAL_BLUE):
 *   c++ -std=gnu++17 -I src -o /tmp/hours_test tools/hours_test.cpp src/hours.cpp src/theme.cpp -x c src/gel.c -lm && /tmp/hours_test
 * The tint at 12:00 is GEL_PAL_BLUE byte for byte; at 06:30 / 19:00 /
 * 02:00 every role moved by no more than the caps (warm 12%, cool 10%)
 * and the outline (and the eye, the same ink) byte-identical; no clock
 * = neutral; the backlight's factor by phase, never under the floor,
 * BL_SLEEP left alone; the ease over ~2 s, no step; the grain's quiet
 * hours move the night. THE REAL SUN (E39, 2026-09-23): the fixed
 * path is hours_compute_sun at 06:30 / 18:30 byte for byte round the
 * whole day; a September sun (07:04 / 19:13) peaks the amber at 07:04
 * and 19:13 and the night yields to THAT dawn (06:30 in December is
 * still the night, in June already the day); the module: a refused
 * pair changes nothing, a snapped pair is the tint at once, a told
 * pair slides over HOURS_SUN_EASE_S with the tint never stepping,
 * back to the fixed hours the same way; the status names the sun. */
#include "../src/hours.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float role_move(const gel_rgba_t *a, const gel_rgba_t *b)
{
    float m = fabsf(a->r - b->r);
    if (fabsf(a->g - b->g) > m) m = fabsf(a->g - b->g);
    if (fabsf(a->b - b->b) > m) m = fabsf(a->b - b->b);
    return m;
}
static void report(const char *when, const gel_pal_t *p, float cap)
{
    const gel_pal_t *B = &GEL_PAL_BLUE;
    float mb = role_move(&p->body, &B->body), ms = role_move(&p->shade, &B->shade), mh = role_move(&p->shine, &B->shine);
    CHECK(mb <= cap + 1e-6f && ms <= cap + 1e-6f && mh <= cap + 1e-6f,
          "%s: body moved %.3f, shade %.3f, shine %.3f (cap %.2f)", when, mb, ms, mh, cap);
    CHECK(mb > 0 || ms > 0 || mh > 0, "%s: something moved", when);
    CHECK(memcmp(&p->outline, &B->outline, sizeof p->outline) == 0, "%s: the outline byte-identical", when);
    CHECK(memcmp(&p->eye, &B->eye, sizeof p->eye) == 0, "%s: the eye byte-identical", when);
    CHECK(p->body.a == B->body.a && p->shade.a == B->shade.a && p->shine.a == B->shine.a, "%s: the alphas untouched", when);
}

int main(void)
{
    gel_pal_t p; float warm, cool, k;

    puts("-- the tint by the hour");
    hours_compute(12 * 60, NULL, &p, &warm, &cool, &k);
    CHECK(memcmp(&p, &GEL_PAL_BLUE, sizeof p) == 0, "12:00: GEL_PAL_BLUE byte for byte (warm %.2f cool %.2f)", warm, cool);
    CHECK(k == 1.0f, "12:00: the backlight full (x%.2f)", k);
    hours_compute(-1, NULL, &p, &warm, &cool, &k);
    CHECK(memcmp(&p, &GEL_PAL_BLUE, sizeof p) == 0 && k == 1.0f, "no clock: neutral, full");

    hours_compute(6 * 60 + 30, NULL, &p, &warm, &cool, &k);
    CHECK(warm > 0.99f && cool < 0.01f, "06:30: dawn at its peak (warm %.2f), the night yielded (cool %.2f)", warm, cool);
    report("06:30", &p, 0.12f);
    CHECK(p.shine.r > GEL_PAL_BLUE.shine.r && p.shine.b < GEL_PAL_BLUE.shine.b, "06:30: the shine warmer (r %.3f -> %.3f)", GEL_PAL_BLUE.shine.r, p.shine.r);
    CHECK(fabsf(k - 0.70f) < 0.01f, "06:30: the backlight x%.2f (~0.70)", k);

    hours_compute(19 * 60, NULL, &p, &warm, &cool, &k);
    CHECK(warm > 0.6f && warm < 0.7f && cool == 0, "19:00: dusk on its way down (warm %.2f)", warm);
    report("19:00", &p, 0.12f);
    CHECK(k > 0.70f && k < 1.0f, "19:00: the backlight x%.2f between", k);

    hours_compute(2 * 60, NULL, &p, &warm, &cool, &k);
    CHECK(cool > 0.99f && warm == 0, "02:00: the night (cool %.2f)", cool);
    report("02:00", &p, 0.10f);
    CHECK(p.shade.r < GEL_PAL_BLUE.shade.r && p.shade.g < GEL_PAL_BLUE.shade.g && p.shade.b < GEL_PAL_BLUE.shade.b, "02:00: the shade darker");
    CHECK(p.shine.g < GEL_PAL_BLUE.shine.g && p.shine.b > GEL_PAL_BLUE.shine.b, "02:00: the shine cooler");
    CHECK(fabsf(k - 0.45f) < 0.01f, "02:00: the backlight x%.2f (~0.45)", k);

    puts("-- the edges: smooth, no step");
    {
        float prev_k = -1; bool smooth = true; float maxstep = 0;
        for (int m = 0; m < 1440; m++) {
            hours_compute(m, NULL, &p, &warm, &cool, &k);
            if (prev_k >= 0 && fabsf(k - prev_k) > maxstep) maxstep = fabsf(k - prev_k);
            if (prev_k >= 0 && fabsf(k - prev_k) > 0.02f) smooth = false;
            prev_k = k;
        }
        hours_compute(0, NULL, &p, &warm, &cool, &k);
        if (fabsf(k - prev_k) > 0.02f) smooth = false;
        CHECK(smooth, "the factor never steps more than 0.02 a minute round the day (max %.3f)", maxstep);
    }
    hours_compute(22 * 60 + 30, NULL, &p, &warm, &cool, &k);
    CHECK(cool > 0.45f && cool < 0.55f, "22:30: the night ramping in (cool %.2f)", cool);
    hours_compute(20 * 60 + 30, NULL, &p, &warm, &cool, &k);
    CHECK(cool == 0 && warm == 0, "20:30: the evening, neutral");
    hours_compute(5 * 60, NULL, &p, &warm, &cool, &k);
    CHECK(cool > 0.99f && warm == 0, "05:00: still the night (cool %.2f)", cool);

    puts("-- the grain's quiet hours");
    hours_compute(2 * 60, "0-5", &p, &warm, &cool, &k);
    CHECK(cool > 0.99f, "quiet 0-5: 02:00 is the night");
    hours_compute(23 * 60 + 30, "0-5", &p, &warm, &cool, &k);
    CHECK(cool > 0.45f && cool < 0.55f, "quiet 0-5: 23:30 ramping in (cool %.2f)", cool);
    hours_compute(22 * 60, "0-5", &p, &warm, &cool, &k);
    CHECK(cool == 0, "quiet 0-5: 22:00 not yet");
    hours_compute(23 * 60 + 45, "23:30-7", &p, &warm, &cool, &k);
    CHECK(cool > 0.99f, "quiet 23:30-7: 23:45 is the night");
    hours_compute(2 * 60, "-", &p, &warm, &cool, &k);
    CHECK(cool > 0.99f, "quiet \"-\": the default");
    hours_compute(2 * 60, "junk", &p, &warm, &cool, &k);
    CHECK(cool > 0.99f, "quiet \"junk\": the default");

    puts("-- the module: once a minute, the ease, the levels");
    hours_begin(40);
    hours_tick(0.025f, 12 * 60, NULL);
    CHECK(memcmp(hours_pal(), &GEL_PAL_BLUE, sizeof p) == 0, "12:00 ticked: the palette is GEL_PAL_BLUE");
    CHECK(hours_level(200) == 200 && hours_level(40) == 40 && hours_level(16) == 16, "12:00: levels 200 / 40 / 16 as given");
    hours_tick(0.025f, 2 * 60, NULL);          /* the clock jumps to the night */
    CHECK(hours_bl_k() > 0.9f, "02:00 just turned: the factor still x%.2f (eases, no step)", hours_bl_k());
    float k_half = 0;
    for (int i = 0; i < 40; i++) { hours_tick(0.025f, 2 * 60, NULL); }
    k_half = hours_bl_k();
    CHECK(k_half < 0.9f && k_half > 0.5f, "after 1 s: x%.2f, on its way", k_half);
    for (int i = 0; i < 120; i++) hours_tick(0.025f, 2 * 60, NULL);
    CHECK(fabsf(hours_bl_k() - 0.45f) < 0.03f, "after 4 s: x%.2f, there", hours_bl_k());
    CHECK(hours_level(200) == 90 && hours_level(40) == 40 && hours_level(16) == 16, "02:00: awake 200 -> %d, dim 40 -> %d (the floor), sleep 16 -> %d (left alone)",
          hours_level(200), hours_level(40), hours_level(16));
    CHECK(memcmp(&hours_pal()->outline, &GEL_PAL_BLUE.outline, sizeof p.outline) == 0, "02:00 ticked: the outline byte-identical");
    CHECK(hours_phase() == HOURS_NIGHT, "the phase: %s", hours_phase_name());
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    CHECK(hours_phase() == HOURS_DAWN, "06:30: %s", hours_phase_name());
    hours_tick(0.025f, 18 * 60 + 30, NULL);
    CHECK(hours_phase() == HOURS_DUSK, "18:30: %s", hours_phase_name());
    hours_tick(0.025f, 21 * 60, NULL);
    CHECK(hours_phase() == HOURS_EVENING, "21:00: %s", hours_phase_name());
    hours_tick(0.025f, 12 * 60, NULL);
    CHECK(hours_phase() == HOURS_DAY, "12:00: %s", hours_phase_name());
    hours_tick(0.025f, -1, NULL);
    CHECK(hours_phase() == HOURS_NOCLOCK && memcmp(hours_pal(), &GEL_PAL_BLUE, sizeof p) == 0, "no clock: %s, neutral", hours_phase_name());
    hours_begin(40);
    hours_tick(0.025f, 2 * 60, "23-7");
    CHECK(fabsf(hours_bl_k() - 0.45f) < 0.01f, "a boot at 02:00: the factor snaps to the hour's (x%.2f), no fade-in from full", hours_bl_k());
    char st[360]; hours_status(st, sizeof st); printf("       %s\n", st);

    puts("-- THE REAL SUN: the fixed hours are the sun at 06:30 / 18:30");
    {
        bool same_all = true; gel_pal_t q; float w2, c2, k2;
        for (int m = -1; m < 1440; m++) {
            hours_compute(m, NULL, &p, &warm, &cool, &k);
            hours_compute_sun(m, NULL, (float)HOURS_SUN_FIXED_RISE, (float)HOURS_SUN_FIXED_SET, &q, &w2, &c2, &k2);
            if (memcmp(&p, &q, sizeof p) != 0 || warm != w2 || cool != c2 || k != k2) same_all = false;
        }
        CHECK(same_all, "hours_compute == hours_compute_sun(06:30, 18:30) at every minute and no clock: the old light byte for byte");
    }
    {   /* Toronto, 2026-09-23: 07:04 / 19:13 */
        const float R = 7 * 60 + 4, S = 19 * 60 + 13;
        hours_compute_sun(7 * 60 + 4, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm > 0.99f, "September: the amber peaks at the real sunrise 07:04 (warm %.2f)", warm);
        report("07:04 (real sun)", &p, 0.12f);
        hours_compute_sun(6 * 60 + 30, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm > 0.6f && warm < 0.65f, "September 06:30: on the way up (warm %.2f), not the peak", warm);
        CHECK(cool > 0.35f && cool < 0.40f, "September 06:30: the night still there under it (cool %.2f: 1 - warm, the quiet hours to 7)", cool);
        hours_compute_sun(5 * 60 + 30, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm == 0 && cool > 0.99f, "September 05:30: the night whole (warm %.2f cool %.2f)", warm, cool);
        hours_compute_sun(19 * 60 + 13, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm > 0.99f && cool == 0, "September: the amber peaks at the real sunset 19:13 (warm %.2f)", warm);
        hours_compute_sun(18 * 60 + 30, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm > 0.5f && warm < 0.55f, "September 18:30: the old peak is only half way now (warm %.2f)", warm);
        hours_compute_sun(21 * 60, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(warm == 0 && cool == 0, "September 21:00: the evening, neutral");
        hours_compute_sun(12 * 60, NULL, R, S, &p, &warm, &cool, &k);
        CHECK(memcmp(&p, &GEL_PAL_BLUE, sizeof p) == 0 && k == 1.0f, "September noon: GEL_PAL_BLUE byte for byte, the backlight full");
    }
    {   /* December (07:48 / 16:43) and June (05:36 / 21:03): the night yields to the REAL dawn */
        float wd, cd, wj, cj, wf, cf;
        hours_compute_sun(6 * 60 + 30, NULL, 7 * 60 + 48, 16 * 60 + 43, &p, &wd, &cd, &k);
        hours_compute_sun(6 * 60 + 30, NULL, 5 * 60 + 36, 21 * 60 + 3, &p, &wj, &cj, &k);
        hours_compute(6 * 60 + 30, NULL, &p, &wf, &cf, &k);
        CHECK(wf > 0.99f && cf < 0.01f, "fixed 06:30: the dawn's peak (warm %.2f cool %.2f)", wf, cf);
        CHECK(wd < 0.15f && cd > 0.85f, "December 06:30: still the night (warm %.2f cool %.2f)", wd, cd);
        CHECK(wj > 0.35f && wj < 0.45f && cj > 0.55f && cj < 0.65f, "June 06:30: the dawn passing (warm %.2f, the sun up an hour), the quiet hours to 7 still holding the night under it (cool %.2f)", wj, cj);
        hours_compute_sun(7 * 60 + 30, NULL, 5 * 60 + 36, 21 * 60 + 3, &p, &wj, &cj, &k);
        CHECK(wj == 0 && cj > 0.45f && cj < 0.55f, "June 07:30: the dawn over, the night ramping out by the quiet hours' own edge (cool %.2f) -- the night is his sleep, the sun only moves the dawn (ruling 72)", cj);
        hours_compute_sun(16 * 60 + 43, NULL, 7 * 60 + 48, 16 * 60 + 43, &p, &wd, &cd, &k);
        CHECK(wd > 0.99f && fabsf(k - 0.70f) < 0.01f, "December 16:43: the dusk's amber (warm %.2f), the backlight x%.2f", wd, k);
        hours_compute_sun(16 * 60 + 43, NULL, 5 * 60 + 36, 21 * 60 + 3, &p, &wj, &cj, &k);
        CHECK(wj == 0 && k == 1.0f, "June 16:43: plain day");
        bool smooth = true; float prev_k = -1, maxstep = 0;
        for (int m = 0; m < 1440; m++) {
            hours_compute_sun(m, NULL, 7 * 60 + 48, 16 * 60 + 43, &p, &warm, &cool, &k);
            if (prev_k >= 0 && fabsf(k - prev_k) > maxstep) maxstep = fabsf(k - prev_k);
            if (prev_k >= 0 && fabsf(k - prev_k) > 0.02f) smooth = false;
            prev_k = k;
        }
        CHECK(smooth, "December: the factor never steps more than 0.02 a minute round the day (max %.3f)", maxstep);
    }

    puts("-- THE REAL SUN: the module (the refusals, the snap, the ease, the status)");
    hours_begin(40);
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    CHECK(!hours_sun_known(), "a fresh module: the fixed hours (%s)", hours_sun_known() ? "known?!" : "none known");
    float w0; hours_compute(6 * 60 + 30, NULL, &p, &w0, &cool, &k);
    CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "06:30 fixed: the dawn's palette");
    CHECK(!hours_sun(19 * 60, 7 * 60, false), "refused: the set before the rise");
    CHECK(!hours_sun(25 * 60, 26 * 60, false), "refused: past the day");
    CHECK(!hours_sun(-1, 19 * 60, false), "refused: half a pair");
    CHECK(!hours_sun_known(), "...and nothing changed");
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "...the palette the same");
    CHECK(hours_sun(7 * 60 + 48, 16 * 60 + 43, true) && hours_sun_known(), "a boot's pair from NVS, snapped: 07:48 / 16:43");
    { int r, s; hours_sun_get(&r, &s); CHECK(r == 7 * 60 + 48 && s == 16 * 60 + 43, "hours_sun_get: %02d:%02d / %02d:%02d", r / 60, r % 60, s / 60, s % 60); }
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    hours_compute_sun(6 * 60 + 30, NULL, 7 * 60 + 48, 16 * 60 + 43, &p, &warm, &cool, &k);
    CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "snapped: the December palette at once (warm %.2f cool %.2f)", warm, cool);
    CHECK(hours_phase() == HOURS_NIGHT, "06:30 in December: %s", hours_phase_name());
    hours_tick(0.025f, 7 * 60 + 48, NULL);
    CHECK(hours_phase() == HOURS_DAWN, "07:48 in December: %s", hours_phase_name());
    hours_tick(0.025f, 16 * 60 + 43, NULL);
    CHECK(hours_phase() == HOURS_DUSK, "16:43 in December: %s", hours_phase_name());
    hours_tick(0.025f, 18 * 60 + 30, NULL);
    CHECK(hours_phase() == HOURS_EVENING, "18:30 in December: %s (the fixed dusk's peak is the evening now)", hours_phase_name());
    hours_status(st, sizeof st); CHECK(strstr(st, "sun 07:48-16:43") != NULL, "the status names the sun: %s", strstr(st, "sun ") ? strstr(st, "sun ") : st);
    /* the ease: from December's pair to June's at 06:30, the amber climbing over the minute, never a step */
    hours_begin(40);
    CHECK(hours_sun(7 * 60 + 48, 16 * 60 + 43, true), "December again, snapped");
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    float wstart = 0, wend = 0; hours_compute_sun(6 * 60 + 30, NULL, 7 * 60 + 48, 16 * 60 + 43, &p, &wstart, &cool, &k);
    hours_compute_sun(6 * 60 + 30, NULL, 5 * 60 + 36, 21 * 60 + 3, &p, &wend, &cool, &k);
    CHECK(hours_sun(5 * 60 + 36, 21 * 60 + 3, false), "June's pair told, eased");
    { int r, s; hours_sun_get(&r, &s); CHECK(r == 5 * 60 + 36 && s == 21 * 60 + 3, "the target is June's at once"); }
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    hours_status(st, sizeof st); CHECK(strstr(st, "(easing in)") != NULL, "the status says it eases");
    {
        float prev_shine_r = hours_pal()->shine.r, maxpal = 0;
        for (int i = 0; i < (int)(HOURS_SUN_EASE_S / 0.025f) + 4; i++) {
            hours_tick(0.025f, 6 * 60 + 30, NULL);
            float d = fabsf(hours_pal()->shine.r - prev_shine_r); if (d > maxpal) maxpal = d;
            prev_shine_r = hours_pal()->shine.r;
        }
        CHECK(maxpal < 0.002f, "the shine never steps more than 0.002 a tick through the minute (max %.4f)", maxpal);
        hours_compute_sun(6 * 60 + 30, NULL, 5 * 60 + 36, 21 * 60 + 3, &p, &warm, &cool, &k);
        CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "after the minute: June's palette exactly (warm %.2f -> %.2f)", wstart, warm);
        hours_status(st, sizeof st); CHECK(strstr(st, "sun 05:36-21:03") && !strstr(st, "easing"), "the status: the sun, settled");
    }
    /* half way through an ease the windows are half way */
    hours_begin(40);
    CHECK(hours_sun(7 * 60 + 48, 16 * 60 + 43, true) && hours_sun(5 * 60 + 36, 21 * 60 + 3, false), "December snapped, June told");
    for (int i = 0; i < (int)(HOURS_SUN_EASE_S * 0.5f / 0.025f); i++) hours_tick(0.025f, 6 * 60 + 30, NULL);
    hours_compute_sun(6 * 60 + 30, NULL, (7 * 60 + 48 + 5 * 60 + 36) * 0.5f, (16 * 60 + 43 + 21 * 60 + 3) * 0.5f, &p, &warm, &cool, &k);
    CHECK(fabsf(hours_pal()->shine.r - p.shine.r) < 0.003f, "half a minute in: the windows half way between the two suns (shine r %.3f ~ %.3f)", hours_pal()->shine.r, p.shine.r);
    /* back to the fixed hours, eased the same way */
    CHECK(hours_sun(-1, -1, false) && !hours_sun_known(), "-1 / -1: the fixed hours again, eased");
    hours_status(st, sizeof st); CHECK(strstr(st, "sun fixed") && strstr(st, "(easing back)"), "the status: fixed, easing back");
    for (int i = 0; i < (int)(HOURS_SUN_EASE_S / 0.025f) + 4; i++) hours_tick(0.025f, 6 * 60 + 30, NULL);
    hours_compute(6 * 60 + 30, NULL, &p, &warm, &cool, &k);
    CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "after the minute: the fixed dawn's palette byte for byte");
    CHECK(hours_sun(-1, -1, true) && hours_sun(-1, -1, false), "the same pair again: taken, nothing to move");
    hours_tick(0.025f, 6 * 60 + 30, NULL);
    CHECK(memcmp(hours_pal(), &p, sizeof p) == 0, "...and the palette untouched");
    hours_tick(0.025f, -1, NULL);
    CHECK(hours_phase() == HOURS_NOCLOCK && memcmp(hours_pal(), &GEL_PAL_BLUE, sizeof p) == 0, "no clock with a sun known: neutral all the same");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
