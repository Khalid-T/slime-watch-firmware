/* THE DIM'S DROWSE on the laptop (drowse.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/drowse_test tools/drowse_test.cpp -x c src/drowse.c -lm && /tmp/drowse_test
 * The dim edge (DOWN: the lids DROOPY, the slump and the lids settling
 * over 2 s, the breath a notch slower -- x0.85 at rest, toward the
 * resting rate from a quick breath, untouched when already slower); a
 * touch and a raise (UP: the lids up at once, the slump and the breath
 * back over 0.4 s); the dark (silent, nothing left); the refusals
 * (asleep / stirring, the empty glass, a page) and the mid-drowse
 * endings with no event (the pet gone, a page up); the phase chain (no
 * jump at either edge, the wrap and the snapped inhale as one forward
 * step); the LONG preset's 30 s of dim: the same droopy look at the
 * 30th second as at the 2nd, never any other kind -- a look, never Zs. */
#include "drowse.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static drowse_in_t in = { 0.04f, true, false, false, true, false, false, 0.55f, 0.55f };
static drowse_fx_t fx;
static int last_ev = DROWSE_EV_NONE;
static int events = 0;

/* n frames at 25 fps; the last non-NONE event of the run is kept */
static void frames(int n)
{
    for (int i = 0; i < n; i++) {
        int ev = drowse_tick(&in);
        if (ev != DROWSE_EV_NONE) { last_ev = ev; events++; }
        drowse_fx(&fx);
    }
}
static int take_ev(void) { int e = last_ev; last_ev = DROWSE_EV_NONE; return e; }
static float near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

int main(void)
{
    drowse_begin();

    puts("-- the awake glass: nothing");
    frames(50);
    CHECK(take_ev() == DROWSE_EV_NONE && fx.eye_kind == -1 && fx.eye_alpha == 0 && fx.squash == 0 && fx.breath_k == 1.0f && fx.level == 0,
          "lit, not dim: no event, the draw's own eyes, no slump, the breath its own");

    puts("-- the phase chain with the notch off (x1: every wake, the glass having been dark): it lands where rest's phase does, a turn apart at most");
    {
        const float TAU = 2.0f * (float)M_PI;
        float ph = drowse_breath_phase(1.0f);
        CHECK(ph == 1.0f, "the first call takes the input as it is (%.2f)", ph);
        for (float x = 1.1f; x < 6.2f; x += 0.1f) ph = drowse_breath_phase(x);   /* rest's own pace: ~0.14 rad a frame */
        ph = drowse_breath_phase(0.1f);                                        /* the wrap */
        CHECK(near(fmodf(ph, TAU), 0.1f, 1e-3f), "across the wrap: the chain at %.3f = the input's 0.1 (mod a turn)", fmodf(ph, TAU));
        ph = drowse_breath_phase(0.0f);                                        /* THE WAKING BREATH's snap: 0.1 -> 0 */
        CHECK(near(fmodf(ph + TAU, TAU), 0.0f, 1e-3f) || near(fmodf(ph + TAU, TAU), TAU, 1e-3f), "a snapped inhale (-> 0): the chain on the inhale's start too (%.3f mod a turn)", fmodf(ph + TAU, TAU));
        ph = drowse_breath_phase(1.0f);
        ph = drowse_breath_phase(0.0f);                                        /* a snap from 1.0 back to 0: within half a turn, read the short way (backward) -- the same place */
        CHECK(near(fmodf(ph + 2 * TAU, TAU), 0.0f, 1e-2f) || near(fmodf(ph + 2 * TAU, TAU), TAU, 1e-2f), "a snap from 1.0 to 0: still the inhale's start (%.3f mod a turn) -- under x1 the chain is rest's phase", fmodf(ph + 2 * TAU, TAU));
    }

    puts("-- the dim edge: DOWN; the lids droopy, settling over 2 s; the breath x0.85 at rest");
    in.dim = true;
    frames(1);
    CHECK(take_ev() == DROWSE_EV_DOWN && drowse_on(), "the first dim frame: DOWN, drowsing");
    CHECK(fx.eye_kind == GEL_EYE_DROOPY && fx.eye_alpha > 0 && fx.eye_alpha < 0.05f, "the lids DROOPY from the first frame, coming in (alpha %.3f)", fx.eye_alpha);
    frames(25);
    CHECK(near(fx.level, 0.52f, 0.03f) && near(fx.squash, DROWSE_SQUASH * fx.level, 0.001f) && near(fx.eye_alpha, fx.level, 0.001f),
          "1 s in: half way (level %.2f, slump %.3f, lids %.2f)", fx.level, fx.squash, fx.eye_alpha);
    CHECK(near(fx.breath_k, 1.0f - 0.15f * fx.level, 0.005f), "the breath eased by the level: x%.3f", fx.breath_k);
    frames(30);
    CHECK(fx.level == 1.0f && near(fx.squash, DROWSE_SQUASH, 0.0005f) && fx.eye_alpha == 1.0f && fx.eye_kind == GEL_EYE_DROOPY,
          "2.2 s in: the full drowse (slump +%.2f, the lids in, droopy)", fx.squash);
    CHECK(near(fx.breath_k, DROWSE_BREATH_K, 0.001f), "at rest (0.55 of 0.55): the breath x%.2f -- a notch under the resting rate", fx.breath_k);
    CHECK(take_ev() == DROWSE_EV_NONE && events == 1, "no second event while it holds");

    puts("-- the notch from other rates");
    in.breath_hz = 0.80f; frames(1);
    CHECK(near(fx.breath_k * 0.80f, 0.68f, 0.002f), "a quick breath (the walk's 0.80): to %.2f Hz -- a notch toward the resting rate, not all the way", fx.breath_k * 0.80f);
    in.breath_hz = 0.50f; frames(1);
    CHECK(near(fx.breath_k * 0.50f, 0.4675f, 0.002f), "0.50 Hz: floored at 0.85 x rest = %.4f Hz (never under the drowsy rest)", fx.breath_k * 0.50f);
    in.breath_hz = 0.40f; frames(1);
    CHECK(fx.breath_k == 1.0f, "the hour's sit (0.40, already slower): left alone, x%.2f -- the drowse never quickens", fx.breath_k);
    in.breath_hz = 0.0f; frames(1);
    CHECK(fx.breath_k == 1.0f, "no rate known (0): x1");
    in.breath_hz = 0.55f; frames(1);

    puts("-- the phase chain under the notch: rest's delta x0.85, no jump at the edge or the wrap");
    {
        float before = drowse_breath_phase(1.0f);
        float ph = drowse_breath_phase(1.1f);
        CHECK(near(ph - before, 0.1f * DROWSE_BREATH_K, 1e-4f), "+0.1 in -> +%.3f out (x0.85)", ph - before);
        before = ph;
        ph = drowse_breath_phase(6.2f);                         /* rest's phase just under the wrap (a step over half a turn reads the short way: the sky's chain has the same limit; rest moves ~0.14 a frame) */
        before = drowse_breath_phase(6.25f);
        ph = drowse_breath_phase(0.1f);                         /* the wrap: 6.25 -> 0.1 is +0.133 */
        CHECK(near(ph - before, (0.1f + 2.0f * (float)M_PI - 6.25f) * DROWSE_BREATH_K, 1e-3f), "the wrap read as a forward step of +%.3f (x0.85 of +0.133)", ph - before);
    }

    puts("-- the LONG preset: 30 s of dim is the same look at the 30th second as at the 2nd; never any other kind");
    {
        int other = 0; float lvl_min = 1.0f;
        for (int i = 0; i < 30 * 25; i++) { frames(1); if (fx.eye_kind != GEL_EYE_DROOPY) other++; if (fx.level < lvl_min) lvl_min = fx.level; }
        CHECK(other == 0 && lvl_min == 1.0f && fx.eye_kind == GEL_EYE_DROOPY && drowse_on(), "750 frames of dim: droopy on every one, the level full -- a look, never Zs (there is no Z in the module)");
        CHECK(take_ev() == DROWSE_EV_NONE, "and no event along the way");
    }

    puts("-- a touch: UP at once -- the lids up this frame, the slump and the breath back over 0.4 s");
    in.touch = true; frames(1);
    CHECK(take_ev() == DROWSE_EV_UP && !drowse_on(), "the touch's frame: UP, not drowsing");
    CHECK(fx.eye_kind == -1 && fx.eye_alpha == 0, "the lids are the draw's again at once (main's engage brings them up open)");
    CHECK(fx.level > 0.85f && fx.level < 0.95f && fx.squash > 0.025f, "the slump still easing (level %.2f)", fx.level);
    in.touch = false; in.dim = false;                          /* main's state goes AWAKE on the touch */
    frames(12);
    CHECK(fx.level == 0 && fx.squash == 0 && fx.breath_k == 1.0f, "0.5 s on: gone (level 0, no slump, the breath its own)");
    CHECK(take_ev() == DROWSE_EV_NONE, "the dim's end after the touch: no second UP");

    puts("-- a raise: the dim ends (main's 'moved'), UP");
    in.dim = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_DOWN && fx.level == 1.0f, "dim again: DOWN, full by 2.4 s");
    in.dim = false; frames(1);
    CHECK(take_ev() == DROWSE_EV_UP && fx.eye_kind == -1, "the dim gone on a lit glass (a raise): UP, the lids up");
    frames(12);

    puts("-- the dark: taken silently, nothing left for the next lit frame");
    in.dim = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_DOWN, "dim: DOWN");
    in.lit = false; in.dim = false; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on() && fx.level == 0 && fx.eye_kind == -1 && fx.squash == 0 && fx.breath_k == 1.0f,
          "the dark: no event, no look left (level 0 at once)");
    in.lit = true; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE && fx.eye_kind == -1, "the next lit frame (the wake): nothing from the drowse");
    puts("-- the dark by hand (main's go_dark: no frame runs between the dark and the wake, so the module is told)");
    in.dim = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_DOWN && drowse_on(), "dim: DOWN");
    drowse_dark();
    drowse_fx(&fx);
    CHECK(!drowse_on() && fx.level == 0 && fx.eye_kind == -1 && fx.squash == 0 && fx.breath_k == 1.0f, "drowse_dark: not drowsing, no look left, the breath its own");
    in.dim = false; frames(1);                                 /* the wake's first frame: lit, not dim */
    CHECK(take_ev() == DROWSE_EV_NONE && fx.eye_kind == -1, "the wake's first frame after it: no UP, no engage (the dim did not end on a lit glass)");

    puts("-- the refusals: asleep / stirring, the empty glass, a page");
    in.dim = true; in.dozing = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on() && fx.eye_kind == -1 && fx.level == 0, "dozing under the dim: no drowse (THE NIGHT LIGHT owns that look)");
    in.dozing = false; in.holding = false; frames(60);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on() && fx.eye_kind == -1, "the empty glass: no face to droop");
    in.holding = true; in.page = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on(), "a page up: his tool, not the pet's dusk");
    in.page = false; in.dim = false; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE, "the dim ending with nothing drowsing: no UP");

    puts("-- mid-drowse endings with no event: the pet leaves, a page comes up, the night begins");
    in.dim = true; frames(60); CHECK(take_ev() == DROWSE_EV_DOWN, "dim: DOWN");
    in.holding = false; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on() && fx.eye_kind == -1 && fx.level >= 0.85f, "the pet leaves mid-drowse: the look eases out (level %.2f), no event (no engage on an empty glass)", fx.level);
    frames(12); in.holding = true; in.dim = false; frames(1); in.dim = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_DOWN, "dim again: DOWN");
    in.page = true; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on(), "a page up mid-drowse: ended, no event");
    in.page = false; frames(12); in.dim = false; frames(1); in.dim = true; frames(60);
    CHECK(take_ev() == DROWSE_EV_DOWN, "dim again: DOWN");
    in.dozing = true; frames(1);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on(), "the night begins under the dim: ended, no event (the sleep look takes over)");
    in.dozing = false; in.dim = false; frames(12);

    puts("-- a touch that holds before the dim ends: no drowse begins under a finger");
    in.touch = true; in.dim = true; frames(30);
    CHECK(take_ev() == DROWSE_EV_NONE && !drowse_on(), "dim with a finger down: nothing");
    in.touch = false; frames(1);
    CHECK(take_ev() == DROWSE_EV_DOWN, "the finger lifted, still dim: DOWN");

    puts("-- NULLs");
    CHECK(drowse_tick(NULL) == DROWSE_EV_NONE, "a NULL frame: NONE, nothing tripped");
    drowse_fx(NULL); CHECK(true, "a NULL out: nothing written");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
