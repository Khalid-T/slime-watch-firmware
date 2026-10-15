/* THE SKY on the laptop (sky.cpp has no Arduino in it; gel.c only for
 * the palette):
 *   c++ -std=gnu++17 -I src -o /tmp/sky_test tools/sky_test.cpp src/sky.cpp -x c src/gel.c -lm && /tmp/sky_test
 * The vocabulary (every word of the eight taken, junk refused and
 * nothing changed, two words held, a third dropped, "none" forgets);
 * nothing shows when none is known (the fx zero, the tint identity);
 * each word's effect: rain's fullness, cold's cooled shine and its
 * shiver on the raise (gone within 0.3 s), hot's slow breath and droopy
 * lids on the raise (gone after 4 s), storm's laps growing, the
 * tint-only words touching body/shade/shine and never the outline or
 * the eye; two words' tints multiplying; the hold -- forgotten at
 * SKY_HOLD_S with a log line, the status line before and after.
 * THE SKY'S WEIGHT (E38, 2026-09-23): nothing dry; rain ramps the
 * weight 0 -> 1 over exactly SKY_HEAVY_EASE_S (never a jump: half at
 * half), storm too, snow / the other words not at all; a change of sky
 * eases back out over the minute, a forget and the hold's end too; the
 * factor sky_heavy_k: 1.0 dry, 11/9 wet, 10/9 halfway. */
#include "sky.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool same(const gel_rgba_t *a, const gel_rgba_t *b) { return a->r == b->r && a->g == b->g && a->b == b->b; }
static bool ink_untouched(const gel_pal_t *p) { return same(&p->outline, &GEL_PAL_BLUE.outline) && same(&p->eye, &GEL_PAL_BLUE.eye); }
static void run(float s) { for (float t = 0; t < s; t += 0.02f) sky_tick(0.02f); }

int main(void)
{
    static const char *WORDS[] = { "rain", "snow", "cold", "hot", "clear", "storm", "wind", "dark" };
    char st[240];
    sky_begin();
    /* nothing known: nothing shows */
    assert(!sky_known());
    sky_fx_t fx; sky_fx(&fx);
    assert(fx.squash == 0 && fx.a2 == 0 && fx.eye_kind == -1 && fx.gel_extra == 0 && fx.breath_k == 1.0f);
    gel_pal_t pal = GEL_PAL_BLUE; sky_tint(&pal);
    assert(same(&pal.body, &GEL_PAL_BLUE.body) && same(&pal.shade, &GEL_PAL_BLUE.shade) && same(&pal.shine, &GEL_PAL_BLUE.shine));
    assert(sky_age_s() == 0 && !strcmp(sky_word(0), ""));
    sky_status(st, sizeof st); assert(strstr(st, "none known"));
    /* the vocabulary */
    for (int i = 0; i < 8; i++) { assert(sky_valid_word(WORDS[i])); assert(sky_set(WORDS[i], "test")); assert(!strcmp(sky_word(0), WORDS[i])); }
    assert(!sky_valid_word("sunny") && !sky_valid_word(""));
    assert(sky_set("rain", "test"));
    assert(!sky_set("rain sunny", "test"));                   /* one unknown word: refused, nothing changed */
    assert(!strcmp(sky_word(0), "rain") && !strcmp(sky_word(1), ""));
    assert(sky_set("rain cold dark", "the box"));             /* two held, the third dropped */
    assert(!strcmp(sky_word(0), "rain") && !strcmp(sky_word(1), "cold") && !strcmp(sky_word(2), ""));
    assert(sky_set("cold cold", "test") && !strcmp(sky_word(0), "cold") && !strcmp(sky_word(1), ""));
    assert(sky_set("none", "test") && !sky_known());
    /* rain: fuller, a greyer light */
    assert(sky_set("rain", "test"));
    sky_fx(&fx); assert(fabsf(fx.squash - SKY_RAIN_SQUASH) < 1e-6f && fx.eye_kind == -1 && fx.breath_k == 1.0f);
    pal = GEL_PAL_BLUE; sky_tint(&pal);
    assert(pal.body.r < GEL_PAL_BLUE.body.r && ink_untouched(&pal));
    /* cold: the shine cooled (less red, the blue kept), a shiver on the raise */
    assert(sky_set("cold", "test"));
    pal = GEL_PAL_BLUE; sky_tint(&pal);
    assert(pal.shine.r < GEL_PAL_BLUE.shine.r && pal.shine.b == GEL_PAL_BLUE.shine.b && ink_untouched(&pal));
    sky_fx(&fx); assert(fx.a2 == 0);
    sky_raise();
    float peak = 0; int frames = 0;
    for (float t = 0; t < 1.0f; t += 0.01f) {
        sky_tick(0.01f); sky_fx(&fx);
        if (fabsf(fx.a2) > peak) peak = fabsf(fx.a2);
        if (fx.a2 != 0) frames++;
        if (t > SKY_SHIVER_S + 0.02f) assert(fx.a2 == 0);      /* gone within 0.3 s */
    }
    assert(peak > 0.03f && peak <= SKY_SHIVER_A2 && frames > 10);
    /* hot: a slow breath, droopy lids after the raise for 4 s */
    assert(sky_set("hot", "test"));
    sky_fx(&fx); assert(fx.breath_k == SKY_HOT_BREATH_K && fx.eye_kind == -1);
    /* A DRINK ON A HOT DAY (A9): the one word main hands the drink */
    assert(sky_hot());
    sky_status(st, sizeof st); assert(strstr(st, "a drink welcome"));
    assert(sky_set("hot cold", "test") && sky_hot());                 /* hot beside another word still */
    assert(sky_set("cold", "test") && !sky_hot());
    assert(sky_set("none", "test") && !sky_hot());
    assert(sky_set("hot", "test") && sky_hot());
    sky_raise(); sky_fx(&fx); assert(fx.eye_kind == GEL_EYE_DROOPY);
    run(SKY_HOT_LID_S - 0.5f); sky_fx(&fx); assert(fx.eye_kind == GEL_EYE_DROOPY);
    run(1.0f); sky_fx(&fx); assert(fx.eye_kind == -1);
    {   /* the followed breath: rest's phase advancing 2 pi over 100 steps; hot = 0.7 of it; wraps handled */
        sky_begin(); assert(sky_set("hot", "test"));
        float p0 = sky_breath_phase(0.0f), p = p0;
        for (int i = 1; i <= 100; i++) p = sky_breath_phase(fmodf(2.0f * (float)M_PI * i / 100.0f, 2.0f * (float)M_PI));
        assert(fabsf((p - p0) - SKY_HOT_BREATH_K * 2.0f * (float)M_PI) < 1e-3f);
        sky_forget();
        float q = p;
        for (int i = 1; i <= 100; i++) q = sky_breath_phase(fmodf(2.0f * (float)M_PI * i / 100.0f, 2.0f * (float)M_PI));
        assert(fabsf((q - p) - 2.0f * (float)M_PI) < 1e-3f);   /* rest's own rate again, no jump */
    }
    /* storm: the laps grow, the light dark */
    assert(sky_set("storm", "test"));
    sky_fx(&fx); float l0 = fx.gel_extra;
    run(2.0f); sky_fx(&fx);
    assert(fabsf(fmodf(fx.gel_extra - l0 + 1.0f, 1.0f) - SKY_STORM_LAPS_HZ * 2.0f) < 0.02f);
    pal = GEL_PAL_BLUE; sky_tint(&pal); assert(pal.body.r < GEL_PAL_BLUE.body.r && ink_untouched(&pal));
    /* the tint-only words: the fx quiet, the ink untouched */
    static const char *TINT_ONLY[] = { "snow", "clear", "wind", "dark" };
    for (int i = 0; i < 4; i++) {
        assert(sky_set(TINT_ONLY[i], "test"));
        sky_fx(&fx); assert(fx.squash == 0 && fx.a2 == 0 && fx.eye_kind == -1 && fx.breath_k == 1.0f);
        sky_raise(); sky_fx(&fx); assert(fx.a2 == 0 && fx.eye_kind == -1);
        pal = GEL_PAL_BLUE; sky_tint(&pal);
        assert(ink_untouched(&pal));
        assert(!(same(&pal.body, &GEL_PAL_BLUE.body) && same(&pal.shade, &GEL_PAL_BLUE.shade) && same(&pal.shine, &GEL_PAL_BLUE.shine)));
        assert(pal.body.r <= 1 && pal.shine.g <= 1 && pal.shine.b <= 1);
    }
    /* two words multiply */
    assert(sky_set("dark cold", "test"));
    pal = GEL_PAL_BLUE; sky_tint(&pal);
    gel_pal_t d = GEL_PAL_BLUE; assert(sky_set("dark", "test")); sky_tint(&d);
    gel_pal_t c = GEL_PAL_BLUE; assert(sky_set("cold", "test")); sky_tint(&c);
    assert(fabsf(pal.shine.r - d.shine.r * c.shine.r / GEL_PAL_BLUE.shine.r) < 1e-5f);
    /* the hold: forgotten at SKY_HOLD_S, with a log line */
    assert(sky_set("rain cold", "the box"));
    sky_status(st, sizeof st); assert(strstr(st, "rain cold") && strstr(st, "the box"));
    const char *say = NULL; int said = 0;
    for (float t = 0; t < SKY_HOLD_S + 2.0f; t += 0.25f) { say = sky_tick(0.25f); if (say) { said++; assert(strstr(say, "forgotten")); } }
    assert(said == 1 && !sky_known());
    sky_fx(&fx); assert(fx.squash == 0 && fx.breath_k == 1.0f);
    pal = GEL_PAL_BLUE; sky_tint(&pal); assert(same(&pal.shine, &GEL_PAL_BLUE.shine));
    assert(sky_set("rain", "test")); run(10.0f); assert(sky_age_s() > 9.9f && sky_age_s() < 10.1f);
    /* THE SKY'S WEIGHT: rain and storm make the gel heavier, over a minute each way */
    sky_begin();
    sky_fx(&fx); assert(fx.heavy == 0 && sky_heavy() == 0);
    assert(fabsf(sky_heavy_k(0.0f) - 1.0f) < 1e-6f && fabsf(sky_heavy_k(1.0f) - 11.0f / 9.0f) < 1e-6f && fabsf(sky_heavy_k(0.5f) - 10.0f / 9.0f) < 1e-6f);
    assert(sky_heavy_k(2.0f) == sky_heavy_k(1.0f) && sky_heavy_k(-1.0f) == 1.0f);   /* clamped */
    static const char *DRY[] = { "snow", "cold", "hot", "clear", "wind", "dark" };
    for (int i = 0; i < 6; i++) { assert(sky_set(DRY[i], "test")); run(SKY_HEAVY_EASE_S + 5.0f); assert(sky_heavy() == 0); }   /* the other words: no weight, ever */
    assert(sky_set("rain", "the box"));
    sky_fx(&fx); assert(fx.heavy == 0);                        /* told, but nothing yet: no jump */
    {
        float prev = 0; float maxstep = 0;
        for (float t = 0; t < SKY_HEAVY_EASE_S * 0.5f; t += 0.02f) { sky_tick(0.02f); float h = sky_heavy(); if (h - prev > maxstep) maxstep = h - prev; prev = h; }
        assert(maxstep < 0.001f);                                /* a frame moves it under a thousandth */
        assert(sky_heavy() > 0.48f && sky_heavy() < 0.52f);      /* half in at half a minute */
    }
    run(SKY_HEAVY_EASE_S * 0.5f + 1.0f);
    sky_fx(&fx); assert(fx.heavy == 1.0f && sky_heavy() == 1.0f);
    sky_status(st, sizeof st); assert(strstr(st, "heavier") && strstr(st, "x1.22"));
    assert(sky_set("snow", "test"));                             /* the sky changed to a dry word: the weight eases out */
    assert(sky_heavy() == 1.0f);
    run(SKY_HEAVY_EASE_S * 0.5f); assert(sky_heavy() > 0.48f && sky_heavy() < 0.52f);
    run(SKY_HEAVY_EASE_S * 0.5f + 1.0f); assert(sky_heavy() == 0);
    assert(sky_set("storm cold", "test"));                       /* storm is wet too */
    run(SKY_HEAVY_EASE_S + 1.0f); assert(sky_heavy() == 1.0f);
    sky_forget(); run(0.5f); assert(sky_heavy() < 1.0f && sky_heavy() > 0.98f);   /* a forget: out over the minute, not at once */
    run(SKY_HEAVY_EASE_S); assert(sky_heavy() == 0);
    assert(sky_set("rain", "the box")); run(SKY_HEAVY_EASE_S + 1.0f); assert(sky_heavy() == 1.0f);
    while (sky_known()) sky_tick(0.25f);                          /* the hold's end forgets: the weight goes with it, eased */
    run(2.0f);
    assert(!sky_known() && sky_heavy() < 1.0f && sky_heavy() > 0.9f);
    run(SKY_HEAVY_EASE_S); assert(sky_heavy() == 0);
    printf("sky_test: ok -- 8 words, the refusals, the fx of each, the tints (ink untouched), the shiver, the droop, the slow breath, the hold, the weight (rain / storm over a minute, 11/9)\n");
    return 0;
}
