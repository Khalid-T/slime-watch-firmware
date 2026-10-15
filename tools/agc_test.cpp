// c++ -std=gnu++17 -I src -o /tmp/agc_test tools/agc_test.cpp && /tmp/agc_test
#include "agc.h"
#include <cstdio>
static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
/* a window that returns dc = k * pa (linear in the current, like skin) */
static long skin(uint8_t pa, long k) { long d = (long)pa * k; return d > AGC_FULL ? AGC_FULL : d; }
int main()
{
    uint8_t pa;
    /* in the band at once: settled, pa untouched */
    pa = 0x3F; CHECK(agc_step(100000, &pa) == AGC_SETTLED && pa == 0x3F);
    CHECK(agc_step(AGC_DC_LO, &pa) == AGC_SETTLED); CHECK(agc_step(AGC_DC_HI, &pa) == AGC_SETTLED);
    /* over: halved; the clipped ceiling too */
    pa = 0x7F; CHECK(agc_step(AGC_FULL, &pa) == AGC_AGAIN && pa == 0x3F);
    pa = 0x3F; CHECK(agc_step(AGC_DC_HI + 1, &pa) == AGC_AGAIN && pa == 0x1F);
    /* over at the floor: bright */
    pa = AGC_PA_MIN; CHECK(agc_step(AGC_FULL, &pa) == AGC_BRIGHT && pa == AGC_PA_MIN);
    pa = 0x09; CHECK(agc_step(AGC_FULL, &pa) == AGC_AGAIN && pa == AGC_PA_MIN);
    /* under: up in proportion, capped at x2, never less than +4 */
    pa = 0x1F; CHECK(agc_step(1000, &pa) == AGC_AGAIN && pa == 0x3E);            /* x2 cap */
    pa = 0x40; CHECK(agc_step(50000, &pa) == AGC_AGAIN && pa > 0x40 && pa <= 0x80);
    pa = 0x40; CHECK(agc_step(AGC_DC_LO - 1, &pa) == AGC_AGAIN && pa >= 0x44);  /* just under: at least +4 */
    pa = 0x1F; CHECK(agc_step(0, &pa) == AGC_AGAIN && pa == 0x3E);               /* a zero read does not divide */
    pa = 0xF8; CHECK(agc_step(9161, &pa) == AGC_AGAIN && pa == AGC_PA_MAX);      /* the bench's own row */
    /* dark: at the ceiling and still under */
    pa = AGC_PA_MAX; CHECK(agc_step(9408, &pa) == AGC_DARK && pa == AGC_PA_MAX);
    /* the whole loop against linear skin: a finger (k ~ 850: 0x3F -> 53 k as the bench), a bright finger, a dim wrist */
    long ks[] = { 850, 4000, 300, 60000, 40 };
    for (long k : ks) {
        pa = AGC_PA_START; agc_verdict_t v = AGC_AGAIN; int steps = 0;
        while (v == AGC_AGAIN && steps < AGC_STEPS_MAX) { v = agc_step(skin(pa, k), &pa); steps++; }
        long dc = skin(pa, k);
        if (k == 60000) CHECK(v == AGC_BRIGHT);
        else if (k == 40) CHECK(v == AGC_DARK);
        else CHECK(v == AGC_SETTLED && dc >= AGC_DC_LO && dc <= AGC_DC_HI && steps <= AGC_STEPS_MAX);
    }
    CHECK(agc_ma(0xFF) > 50.9f && agc_ma(0xFF) < 51.1f);
    /* THE AGC PER LED (2026-10-08): two channels, each its own current -- the wrist's own row (09-25: red at
     * k ~ 40 was pinned at 0xF8 while ir at k ~ 850 sat at 75 %): now ir settles in two or three rounds and
     * red walks to the ceiling on its own, DARK, without dragging ir with it */
    {
        agc2_t a;
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        CHECK(a.pa[0] == AGC_PA_START && a.pa[1] == AGC_PA_START && a.v[0] == AGC_AGAIN && a.v[1] == AGC_AGAIN && a.steps == 0 && agc2_choosing(&a));
        /* both in the band at once: settled, nothing moved */
        CHECK(agc2_step(&a, 100000, 120000) == AGC_SETTLED && a.pa[0] == AGC_PA_START && a.pa[1] == AGC_PA_START && !agc2_choosing(&a));
        /* a finger on both (k 850 / 900): both settle, each at its own pa, inside the rounds */
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        agc_verdict_t v = AGC_AGAIN; int rounds = 0;
        while (v == AGC_AGAIN && rounds < AGC_STEPS_MAX + 1) { v = agc2_step(&a, skin(a.pa[0], 850), skin(a.pa[1], 900)); rounds++; }
        CHECK(v == AGC_SETTLED && skin(a.pa[0], 850) >= AGC_DC_LO && skin(a.pa[0], 850) <= AGC_DC_HI && skin(a.pa[1], 900) >= AGC_DC_LO && skin(a.pa[1], 900) <= AGC_DC_HI && rounds <= AGC_STEPS_MAX);
        /* THE WRIST'S ROW: ir gives plenty (k 850), red almost nothing (k 40): ir settles and STAYS where it settled while red climbs to the ceiling -- the pair's word is DARK (one channel sees nothing), ir's own verdict SETTLED and its pa untouched by red's climb */
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        v = AGC_AGAIN; rounds = 0; uint8_t ir_settled_pa = 0;
        while (v == AGC_AGAIN && rounds < AGC_STEPS_MAX + 1) {
            v = agc2_step(&a, skin(a.pa[0], 40), skin(a.pa[1], 850)); rounds++;
            if (a.v[1] == AGC_SETTLED && !ir_settled_pa) ir_settled_pa = a.pa[1];
        }
        CHECK(v == AGC_DARK && a.v[0] == AGC_DARK && a.pa[0] == AGC_PA_MAX);
        CHECK(a.v[1] == AGC_SETTLED && a.pa[1] == ir_settled_pa && skin(a.pa[1], 850) >= AGC_DC_LO && skin(a.pa[1], 850) <= AGC_DC_HI);
        CHECK(a.pa[1] < 0x80);                                                   /* ir never followed red up: the 09-25 fault */
        /* one pressed flat (k 60000), the other fine: BRIGHT, the fine one settled */
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        v = AGC_AGAIN; rounds = 0;
        while (v == AGC_AGAIN && rounds < AGC_STEPS_MAX + 1) { v = agc2_step(&a, skin(a.pa[0], 60000), skin(a.pa[1], 850)); rounds++; }
        CHECK(v == AGC_BRIGHT && a.v[0] == AGC_BRIGHT && a.pa[0] == AGC_PA_MIN && a.v[1] == AGC_SETTLED);
        /* dark beats bright in the pair's word (nothing on the window is the thing to say) */
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        v = AGC_AGAIN; rounds = 0;
        while (v == AGC_AGAIN && rounds < AGC_STEPS_MAX + 1) { v = agc2_step(&a, skin(a.pa[0], 40), skin(a.pa[1], 60000)); rounds++; }
        CHECK(v == AGC_DARK);
        /* the rounds run out with a channel still choosing (a window that flickers over and under, round by round: a hand that will not hold): the pair settles for what it has, that channel's verdict still AGAIN, the other's kept */
        agc2_begin(&a, AGC_PA_START, AGC_PA_START);
        v = AGC_AGAIN; rounds = 0;
        while (v == AGC_AGAIN && rounds < AGC_STEPS_MAX + 1) { v = agc2_step(&a, (rounds & 1) ? 1000 : AGC_FULL, 100000); rounds++; }
        CHECK(v == AGC_SETTLED && rounds == AGC_STEPS_MAX && a.v[0] == AGC_AGAIN && a.v[1] == AGC_SETTLED && !agc2_choosing(&a));
    }
    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
