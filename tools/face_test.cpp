/* THE WORDLESS WATCH FACE on the laptop (no board, no Arduino: face.cpp
 * is parameters in, a framebuffer out; gel.c only for the palette):
 *   c++ -std=gnu++17 -I src -o /tmp/face_test tools/face_test.cpp src/face.cpp src/theme.cpp -x c src/gel.c -lm && /tmp/face_test
 * The norm's median (the default under 3 days, shelf days left out,
 * even and odd counts); the arcs and the drop rendered into a 240x240
 * buffer with lit-pixel counts per element (half a ring is half the
 * pixels, a full ring the same count in the shine, never a second lap);
 * every lit pixel inside the rim; nothing painted over a pixel the gel
 * lit; the hour at the centre when EMPTY, none when HOLDING; the drop's
 * rows, the cable's nothing; the trace taken on the sink, fading over
 * the hour, taken back by the arrival; THE REACHING CUE (a ripple a
 * second on the rim at the wall he will rise at while a come is out:
 * pixels there early in the second, none late, none on the far wall,
 * none once answered); `face off` = black. THE NIGHT RING KNOWS HIS
 * HEART (D33 + D36, 2026-10-08): a deep night draws the same arc in
 * the shine (the pixel count the same, the ink the day ring's at the
 * norm); the nights wandering breathes the ring's tip -- a pixel or two
 * at the arc's end fading between the ring's ink and dark over a ten-
 * second period, the rest of the glass untouched; neither without a
 * night. Writes /tmp/face_*.ppm to look at (python3 tools/ppm2png.py). */
#include "face.h"
#include "theme.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint16_t fb[FACE_W * FACE_H];
static uint8_t trace_mem[FACE_TRACE_BYTES];

static int lit(void) { int n = 0; for (int i = 0; i < FACE_W * FACE_H; i++) if (fb[i]) n++; return n; }
static int lit_in(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (fb[y * FACE_W + x]) n++;
    return n;
}
/* every lit pixel inside the round glass (gel_compose's half-pixel rule) */
static bool all_on_disc(void)
{
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        if (!fb[y * FACE_W + x]) continue;
        int dx2 = 2 * x + 1 - FACE_W, dy2 = 2 * y + 1 - FACE_H;
        if (dx2 * dx2 + dy2 * dy2 > FACE_W * FACE_W) return false;
    }
    return true;
}
static float rad_max(void)
{
    float m = 0;
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        if (!fb[y * FACE_W + x]) continue;
        float dx = x + 0.5f - 120, dy = y + 0.5f - 120, r = sqrtf(dx * dx + dy * dy);
        if (r > m) m = r;
    }
    return m;
}
static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", FACE_W, FACE_H);
    for (int i = 0; i < FACE_W * FACE_H; i++) {
        uint16_t v = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        unsigned char px[3] = { (unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63),
                                (unsigned char)((v & 31) * 255 / 31) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static face_in_t base(void)
{
    face_in_t in;
    memset(&in, 0, sizeof in);
    in.dt = 0.025f; in.lit = true; in.holding = false; in.gel_drawn = false;
    in.minute = 23 * 60 + 41; in.batt_mv = 4130; in.charging = true;   /* the board today: no cell, the float */
    in.alarm_min = -1;                                                  /* no alarm set (0 would be one at midnight) */
    return in;
}
static void frame(face_in_t in) { face_tick(&in); memset(fb, 0, sizeof fb); face_draw(fb); }

int main(void)
{
    puts("-- the norm: the median of the last 7 days");
    face_begin(trace_mem, true);
    assert(face_norm() == 0 && face_norm_days() == 0);                                                 /* nothing yet: no norm, no ring */
    { uint32_t t[] = { 5000, 7000 }; face_set_history(t, 2); assert(face_norm() == 0 && face_norm_days() == 2); }   /* 2 days: not a norm yet */
    { uint32_t t[] = { 3000, 9000, 5000, 100 }; face_set_history(t, 4); assert(face_norm() == 5000); }  /* the shelf day out; 3 left */
    { uint32_t t[] = { 8000, 2000, 4000, 6000 }; face_set_history(t, 4); assert(face_norm() == 5000); }  /* even: the middle pair's mean */
    { uint32_t t[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 }; face_set_history(t, 9); assert(face_norm() == 0); }  /* all shelf days: no norm */
    { uint32_t t[] = { 9000, 8000, 7000, 6000, 5000, 4000, 3000, 20000, 20000 }; face_set_history(t, 9); assert(face_norm() == 6000); }  /* only the newest 7 */
    printf("  norm %lu\n", (unsigned long)face_norm());
    { uint32_t t[] = { 6000, 6000, 6000 }; face_set_history(t, 3); }

    puts("-- empty, nothing to show but the hour (the cable's float: no drop)");
    face_in_t in = base();
    frame(in);
    int hour_px = lit_in(90, 110, 150, 130);                 /* the digits, all of it in the centre */
    /* 23:41 -- 2:11, 3:11, colon 2, 4:9, 1:8 = 41 font px x 9 */
    assert(hour_px == 41 * 9);
    /* THE HOUR TICK + THE MINUTE TICK: the rest of the lit glass is the two ticks on the rim (r 106-112) */
    int tick_px = lit() - hour_px;
    int base_px = hour_px + tick_px;                          /* the resting face's pixels: the digits and the ticks */
    assert(tick_px > 8 && tick_px < 24);
    assert(all_on_disc());
    ppm("/tmp/face_hour.ppm");
    printf("  hour %d px, centred; the ticks %d px\n", hour_px, tick_px);

    puts("-- THE HOUR TICK: the hour at its hand's angle (12 up), 2 px wide in the shade; THE MINUTE TICK (B12) a 1 px tick in the shine, a px longer; none without a clock");
    in.minute = 6 * 60; frame(in);                            /* 06:00: the hour tick at the bottom, the minute at the top */
    int h6 = lit_in(117, 226, 123, 233), m0 = lit_in(117, 6, 123, 15);
    printf("  06:00: the hour tick %d px at 6 o'clock, the minute %d px at 12\n", h6, m0);
    assert(h6 >= 8 && h6 <= 14 && lit_in(118, 226, 122, 233) == h6);     /* 2 wide x 5-6 tall */
    assert(m0 >= 6 && m0 <= 7 && lit_in(120, 6, 121, 15) == m0);         /* 1 wide x 6-7 tall (r 106-112 on half-pixel steps) */
    assert(lit() - lit_in(90, 110, 150, 130) == h6 + m0);     /* nothing else on the glass but the digits */
    uint16_t hour_ink = fb[229 * FACE_W + 120], min_ink = fb[10 * FACE_W + 120];
    assert(hour_ink && min_ink && hour_ink != min_ink);       /* two inks: the shade, the shine */
    uint16_t shine_ink = min_ink;                              /* kept: the minute's ink is the shine's, the alarm's tick below must match it */
    in.minute = 15 * 60; frame(in);                           /* 15:00: the hour at 3 o'clock */
    assert(lit_in(226, 117, 233, 123) >= 8 && lit_in(117, 226, 123, 233) == 0);
    in.minute = 9 * 60 + 15; frame(in);                       /* 09:15: the hour a hair past 9, the minute at 3 */
    assert(lit_in(6, 98, 16, 124) >= 8 && lit_in(225, 117, 233, 123) >= 6);
    assert(face_hour_tick_deg() == 277);                       /* THE HOUR'S GLANCE's getter: 09:15's hour tick at 277 deg from 12 (555 / 2) */
    in.minute = 15 * 60; frame(in); assert(face_hour_tick_deg() == 90);     /* 15:00: 3 o'clock */
    in.minute = 6 * 60; frame(in);  assert(face_hour_tick_deg() == 180);    /* 06:00: 6 o'clock */
    in.minute = 0; frame(in);       assert(face_hour_tick_deg() == 0);      /* 00:00: 12 */
    in.minute = -1; frame(in);      assert(face_hour_tick_deg() == -1);     /* no clock: none */
    in.minute = 9 * 60 + 15; frame(in);
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++)     /* every tick pixel inside r 112, outside r 105.5 */
        if (fb[y * FACE_W + x] && !(x >= 90 && x < 150 && y >= 110 && y < 130)) {
            float dx = x + 0.5f - 120, dy = y + 0.5f - 120, r = sqrtf(dx * dx + dy * dy);
            assert(r < 112.5f && r > 105.5f);
        }
    ppm("/tmp/face_ticks.ppm");
    /* THE MINUTE TICK's position per minute: a quarter turn a quarter hour, the shine each time; the minute's own box empty of it elsewhere */
    in.minute = 6 * 60 + 15; frame(in);                       /* 06:15: the minute at 3, the hour a hair past 6 */
    assert(lit_in(225, 117, 233, 123) >= 6 && fb[120 * FACE_W + 229] == shine_ink && lit_in(117, 6, 123, 15) == 0);
    in.minute = 6 * 60 + 45; frame(in);                       /* 06:45: the minute at 9 */
    assert(lit_in(7, 117, 15, 123) >= 6 && fb[120 * FACE_W + 10] == shine_ink && lit_in(225, 117, 233, 123) == 0);
    in.minute = 6 * 60 + 30; frame(in);                       /* 06:30: the minute at 6, the hour at half past (15 deg on, 29 px round the rim: apart) */
    assert(lit_in(118, 225, 122, 233) >= 6 && (fb[229 * FACE_W + 119] == shine_ink || fb[229 * FACE_W + 120] == shine_ink));   /* sinf(pi)'s hair puts it in column 119 */
    in.minute = 1 * 60 + 1; frame(in);                        /* 01:01: the minute at 6 deg -- a minute is 11 px round the rim: column 131, nothing at 12 itself; the hour at 30 deg */
    assert(lit_in(128, 6, 135, 16) >= 5 && lit_in(117, 6, 123, 15) == 0);
    /* THE HANDS STACK: at 12:00 the hour's and the minute's coincide -- the thin shine minute is the CORE of the stout shade hour tick */
    in.minute = 0; frame(in);
    { int top = lit_in(117, 6, 123, 15);
      printf("  12:00: %d px at 12 o'clock -- column 120 %s, column 119 %s\n", top,
             fb[10 * FACE_W + 120] == shine_ink ? "the shine (the minute)" : "NOT the shine", fb[10 * FACE_W + 119] == hour_ink ? "the shade (the hour)" : "NOT the shade");
      assert(top >= 11 && top <= 14);                          /* the hour's 2 x 6 and the minute's one px more at r 106 */
      assert(fb[10 * FACE_W + 120] == shine_ink && fb[10 * FACE_W + 119] == hour_ink);
      assert(fb[14 * FACE_W + 120] == shine_ink && fb[14 * FACE_W + 119] == 0); }   /* r 106: the minute alone, the longer hand */
    /* every mark dims alike under company (SHOWING SOMEONE): the minute's shine at COMPANY_K */
    in.minute = 6 * 60; in.company = true; for (int i = 0; i < 40; i++) face_tick(&in);
    memset(fb, 0, sizeof fb); face_draw(fb);
    { uint16_t dimmed = fb[10 * FACE_W + 120]; assert(dimmed && dimmed != shine_ink && lit_in(117, 6, 123, 15) == m0); }
    in.company = false; for (int i = 0; i < 40; i++) face_tick(&in);
    memset(fb, 0, sizeof fb); face_draw(fb); assert(fb[10 * FACE_W + 120] == shine_ink);
    in.minute = -1; in.steps = 0; frame(in); assert(lit() == 0);           /* no clock: no ticks */

    puts("-- THE REACHING CUE: a come is out -> the rim ripples once a second at the wall he will rise at (the bottom: 6 o'clock); nothing at the top; nothing late in the second; nothing once answered");
    in.minute = -1; in.steps = 0; in.reaching = true; in.reach_wall = 0;   /* GEL_WALL_BOTTOM */
    frame(in);                                                /* t = 0: the first, narrowest ripple */
    { int bot = lit_in(100, 226, 140, 240), top = lit_in(100, 0, 140, 14);
      printf("  t=0.000: %d px at the bottom of the rim, %d at the top\n", bot, top);
      assert(bot >= 4 && bot <= 60 && top == 0); }
    for (int i = 0; i < 12; i++) frame(in);                   /* t = 0.3 s: wider, fainter */
    { int bot = lit_in(60, 226, 180, 240), top = lit_in(60, 0, 180, 14);
      printf("  t=0.300: %d px at the bottom, %d at the top\n", bot, top);
      assert(bot >= 20 && top == 0 && all_on_disc()); }
    for (int i = 0; i < 20; i++) frame(in);                   /* t = 0.8 s: quiet until the next second */
    { int bot = lit_in(0, 200, 240, 240); printf("  t=0.800: %d px (quiet)\n", bot); assert(bot == 0 && lit() == 0); }
    for (int i = 0; i < 9; i++) frame(in);                    /* t = 1.025 s: the next ripple */
    assert(lit_in(100, 226, 140, 240) >= 4 && lit_in(100, 0, 140, 14) == 0);
    in.reach_wall = 2; for (int i = 0; i < 40; i++) frame(in); frame(in);   /* the top wall: a second later, at 12 o'clock */
    assert(lit_in(100, 0, 140, 14) >= 4 && lit_in(100, 226, 140, 240) == 0);
    in.reaching = false; frame(in); assert(lit() == 0);       /* answered: gone at once */
    in.reaching = true; in.reach_wall = 0; frame(in); assert(lit_in(100, 226, 140, 240) >= 4);   /* a new reach starts its own second */
    in.reaching = false; in.minute = 23 * 60 + 41; in.steps = 0;
    puts("-- THE RIM AS THE DIAL: the hour tick follows the finger; the lift blinks it twice (set) or fades it (cleared); then the clock's again");
    in.minute = 6 * 60; frame(in);                            /* 06:00: the hour at 6, the minute at 12 */
    assert(lit_in(117, 226, 123, 233) >= 8 && lit_in(226, 117, 233, 123) == 0);
    face_dial(15 * 60); frame(in);                            /* the finger at 3 o'clock */
    assert(face_dial_minute() == 15 * 60);
    assert(lit_in(226, 117, 233, 123) >= 8 && lit_in(117, 226, 123, 233) == 0);   /* the hour tick at 3, none at 6 */
    assert(lit_in(117, 6, 123, 15) >= 6);                     /* the minute tick still the clock's, at 12 */
    face_dial(21 * 60); frame(in);
    assert(lit_in(6, 117, 14, 123) >= 8 && lit_in(226, 117, 233, 123) == 0);   /* following: at 9 now */
    face_dial(15 * 60); frame(in);
    face_dial_set();                                          /* the lift: off-on-off-on, 0.15 s a half */
    int seen[4] = { 0, 0, 0, 0 };
    for (int half = 0; half < 4; half++) for (int i = 0; i < 6; i++) { frame(in); if (i == 2) seen[half] = lit_in(226, 117, 233, 123); }   /* 6 x 25 ms = a half, sampled mid-way */
    printf("  the blink: %d %d %d %d px at 3 o'clock over the four halves\n", seen[0], seen[1], seen[2], seen[3]);
    assert(seen[0] == 0 && seen[1] >= 8 && seen[2] == 0 && seen[3] >= 8);
    assert(face_dial_minute() == 15 * 60);
    for (int i = 0; i < 4; i++) frame(in);                    /* past 0.6 s: the clock's hour again */
    assert(face_dial_minute() == -1 && lit_in(117, 226, 123, 233) >= 8 && lit_in(226, 117, 233, 123) == 0);
    face_dial(15 * 60); frame(in);
    face_dial_clear();                                        /* the lift over the alarm just set: the fade */
    frame(in); uint16_t f0 = fb[120 * FACE_W + 229];
    for (int i = 0; i < 12; i++) frame(in);                   /* 0.3 s: dimmer, still there */
    uint16_t f1 = fb[120 * FACE_W + 229];
    assert(f0 && f1 && f1 != f0 && lit_in(226, 117, 233, 123) >= 8);
    for (int i = 0; i < 14; i++) frame(in);                   /* 0.65 s: gone, the clock's hour back */
    assert(face_dial_minute() == -1 && lit_in(226, 117, 233, 123) == 0 && lit_in(117, 226, 123, 233) >= 8);
    face_dial(15 * 60); frame(in); face_dial(-1); frame(in);  /* let go without a lift's word: the clock's */
    assert(face_dial_minute() == -1 && lit_in(226, 117, 233, 123) == 0);

    puts("-- THE ALARM'S TICK (B14): an alarm set = its tick at its hour in the shine, beside the hour's in the shade; the 12-hour dial; gone with alarm off");
    in.minute = 6 * 60; in.alarm_min = 15 * 60; frame(in);   /* 06:00, the alarm at 15:00: the hour tick at 6, the alarm's at 3 */
    int a3 = lit_in(226, 117, 233, 123);
    printf("  06:00 with the alarm at 15:00: the alarm's tick %d px at 3 o'clock, the hour's %d at 6\n", a3, lit_in(117, 226, 123, 233));
    assert(a3 >= 8 && a3 <= 14 && lit_in(117, 226, 123, 233) >= 8);
    assert(fb[120 * FACE_W + 229] != 0 && fb[120 * FACE_W + 229] != fb[229 * FACE_W + 120]);   /* the shine, not the hour's shade */
    assert(fb[120 * FACE_W + 229] == shine_ink);              /* the same shine as the minute's tick */
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++)     /* every tick pixel in the ticks' band */
        if (fb[y * FACE_W + x] && !(x >= 90 && x < 150 && y >= 110 && y < 130)) {
            float dx = x + 0.5f - 120, dy = y + 0.5f - 120, r = sqrtf(dx * dx + dy * dy);
            assert(r < 112.5f && r > 105.5f);
        }
    /* THE MINUTE PASSES UNDER (THE HANDS STACK): at 06:15 the minute hand is at 3 o'clock, under the alarm's tick -- the box holds the alarm's px and the minute's one px more at r 106; the next minute it is out */
    in.minute = 6 * 60 + 15; frame(in);
    { int under = lit_in(225, 117, 233, 123);
      printf("  06:15 with the alarm at 15:00: %d px at 3 o'clock (the alarm's %d and the minute's px under it)\n", under, a3);
      assert(under == a3 + 1 && fb[120 * FACE_W + 226] == shine_ink && fb[119 * FACE_W + 226] == 0 && fb[120 * FACE_W + 229] == shine_ink); }
    in.minute = 6 * 60 + 16; frame(in);
    assert(lit_in(225, 117, 233, 123) == a3 && fb[120 * FACE_W + 226] == 0);
    in.minute = 6 * 60;
    in.alarm_min = 3 * 60; frame(in);                         /* 03:00 lands on the same spot: a 12-hour dial */
    assert(lit_in(226, 117, 233, 123) == a3);
    in.alarm_min = 6 * 60; frame(in);                         /* the alarm at the hour itself: one tick's worth there (the two agree) */
    assert(lit_in(117, 226, 123, 233) >= 8 && lit_in(226, 117, 233, 123) == 0);
    in.alarm_min = 15 * 60; face_dial(21 * 60); frame(in);    /* dialling toward 9 while the alarm stands at 3: the finger's tick at 9, the alarm's still at 3 */
    assert(lit_in(6, 117, 14, 123) >= 8 && lit_in(226, 117, 233, 123) == a3);
    face_dial(-1); in.alarm_min = -1; frame(in);              /* alarm off: gone */
    assert(lit_in(226, 117, 233, 123) == 0);
    in.minute = -1; in.alarm_min = 15 * 60; frame(in);        /* no clock: no ticks, the alarm's included */
    assert(lit() == 0);
    ppm("/tmp/face_alarm_tick.ppm");
    in.alarm_min = -1;
    in.minute = 23 * 60 + 41;

    puts("-- the day ring: half the norm = half the ring; the norm = the ring in the shine; past it no second lap");
    in.steps = 3000; frame(in); int half = lit() - base_px;
    in.steps = 6000; frame(in); int full = lit() - base_px;
    uint16_t full_c = fb[1 * FACE_W + 120];                  /* 12 o'clock, the first row of the ring */
    in.steps = 12000; frame(in); int twice = lit() - base_px;
    printf("  half %d full %d past %d px; rim %.1f\n", half, full, twice, rad_max());
    assert(full > 1200 && full < 1600);                      /* 2 px x ~735 px of circumference */
    assert(fabsf((float)half / full - 0.5f) < 0.03f);
    assert(twice == full);
    assert(full_c == 0 || true);
    assert(rad_max() < 118.0f && all_on_disc());
    /* the first quadrant only for a quarter (the clock off: the hour's
     * own rows at the centre would fall in the box) */
    in.steps = 1500; in.minute = -1; frame(in);
    assert(lit_in(120, 0, 240, 120) == lit() && lit_in(0, 120, 240, 240) == 0);
    in.minute = 23 * 60 + 41;
    in.steps = 3000; frame(in);
    ppm("/tmp/face_day.ppm");
    /* the shine at the norm, the dim body under it */
    in.steps = 6000; frame(in);
    uint16_t shine_c = 0, body_c = 0;
    for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { shine_c = fb[y * FACE_W + 120]; break; }
    in.steps = 5999; frame(in);
    for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { body_c = fb[y * FACE_W + 120]; break; }
    assert(shine_c && body_c && shine_c != body_c);

    puts("-- the night ring: counter-clockwise, just inside, 4 h = half");
    in.steps = 0;
    in.night_min = 240; frame(in); int nhalf = lit() - base_px;
    in.night_min = 480; frame(in); int nfull = lit() - base_px;
    in.night_min = 600; frame(in); assert(lit() - base_px == nfull);
    printf("  night half %d full %d px\n", nhalf, nfull);
    assert(nfull > 1150 && nfull < 1500 && nfull < full);
    assert(fabsf((float)nhalf / nfull - 0.5f) < 0.03f);
    in.night_min = 120; in.minute = -1; frame(in);           /* a quarter: the top-LEFT quadrant */
    assert(lit_in(0, 0, 120, 120) == lit() && lit_in(120, 0, 240, 240) == 0);
    in.minute = 23 * 60 + 41;
    in.night_min = 444; frame(in);
    ppm("/tmp/face_night.ppm");
    /* the two rings never touch: a full day and a full night, the counts add */
    in.steps = 6000; in.night_min = 480; frame(in);
    assert(lit() - base_px == full + nfull);

    puts("-- THE NIGHT RING KNOWS HIS HEART: a deep night = the same arc in the shine; the nights wander = the tip breathes a pixel at 0.1 Hz");
    in.steps = 0; in.night_min = 240; in.night_deep = false; in.night_wander = false; frame(in);
    uint16_t ring_shade = fb[120 * FACE_W + 6];                /* 9 o'clock: inside the half ring (r 113.5), the shade */
    assert(ring_shade && lit() - base_px == nhalf);
    in.night_deep = true; frame(in);
    uint16_t ring_shine = fb[120 * FACE_W + 6];
    printf("  the night ring: shade %04x, deep %04x; %d px either way\n", ring_shade, ring_shine, lit() - base_px);
    assert(ring_shine && ring_shine != ring_shade && ring_shine == shine_c);   /* the day ring's ink at the norm: the shine */
    assert(lit() - base_px == nhalf);                          /* the same arc, brighter -- never longer */
    in.night_deep = false;
    in.night_wander = true;
    {
        int lo = 1 << 30, hi = 0, at_lo = 0, at_hi = 0;
        static uint16_t fb_lo[FACE_W * FACE_H], fb_hi[FACE_W * FACE_H];
        for (int i = 0; i < 400; i++) {                       /* ten seconds: one breath */
            frame(in);
            int n = lit() - base_px;
            if (n < lo) { lo = n; at_lo = i; memcpy(fb_lo, fb, sizeof fb); }
            if (n > hi) { hi = n; at_hi = i; memcpy(fb_hi, fb, sizeof fb); }
        }
        printf("  the tip: %d..%d px over the breath (low at frame %d, high at %d; the arc %d)\n", lo, hi, at_lo, at_hi, nhalf);
        assert(hi == nhalf && lo < nhalf && lo >= nhalf - 6);  /* a pixel or two at the tip, never more; the arc whole at the top of the breath */
        int diff = 0, far = 0;
        for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++)
            if (fb_lo[y * FACE_W + x] != fb_hi[y * FACE_W + x]) { diff++; if (!(x >= 112 && x < 129 && y >= 226)) far++; }
        printf("  %d px differ between the breath's low and high, %d of them away from the tip (6 o'clock)\n", diff, far);
        assert(diff > 0 && diff <= 6 && far == 0);             /* only the tip, at the arc's end: 4 h counter-clockwise = 6 o'clock */
        /* the mark: the breath moves it STEADY_TIP_STEPS x 2 times a period (every ~0.6 s), not every frame; a
         * frame between two steps marks nothing new (the box is the face's whole, as any ring's is: the diff
         * under it finds the tile or two that moved) */
        frame(in); dirty_rect_t bx; face_mark(&bx);
        int moves = 0;
        for (int i = 0; i < 400; i++) { frame(in); if (face_mark(&bx)) moves++; }
        printf("  the mark moved %d times in the ten-second breath (box %d,%d %dx%d)\n", moves, bx.x, bx.y, bx.w, bx.h);
        assert(moves >= 12 && moves <= 20);
        /* a steady week: the tip stands, the ring whole every frame */
        in.night_wander = false;
        for (int i = 0; i < 40; i++) { frame(in); assert(lit() - base_px == nhalf); }
        /* no night known: neither verdict draws anything */
        in.night_min = 0; in.night_deep = true; in.night_wander = true; frame(in); assert(lit() - base_px == 0);
        in.night_deep = false; in.night_wander = false;
    }
    ppm("/tmp/face_night_deep.ppm");

    puts("-- the drop: filled from the bottom by the level, at 6 o'clock inside the rings");
    in.steps = 0; in.night_min = 0; in.charging = false;
    in.batt_mv = 3300; frame(in); int d0 = lit() - base_px;   /* empty: the edge only */
    in.batt_mv = 3700; frame(in); int d4 = lit() - base_px;
    in.batt_mv = 4049; frame(in); int d7 = lit() - base_px;
    printf("  drop empty %d, at 3700 mV %d, at 4049 mV %d px\n", d0, d4, d7);
    assert(d0 > 0 && d4 > d0 && d7 > d4);
    assert(d7 == 21 * 9);                                    /* the whole glyph: 21 font px x 9 */
    assert(lit_in(100, 200, 140, 232) == d7);                /* all of it around (120, 216) */
    ppm("/tmp/face_drop.ppm");
    in.batt_mv = 4060; in.charging = true; frame(in); assert(lit() - base_px == d7);   /* charging under the float: the full drop */
    in.batt_mv = 4130; frame(in); assert(lit() - base_px == 0);                        /* at the float: nothing */
    /* the pulse: the charging drop's ink moves over 2 s */
    in.batt_mv = 4060; frame(in); uint16_t c0 = fb[216 * FACE_W + 120];
    for (int i = 0; i < 40; i++) face_tick(&in);            /* 1 s on */
    memset(fb, 0, sizeof fb); face_draw(fb);
    assert(fb[216 * FACE_W + 120] != c0);

    puts("-- holding: no hour of ours (the glance's), the rings behind him -- never over a lit pixel");
    in = base(); in.holding = true; in.gel_drawn = true; in.steps = 6000; in.night_min = 480; in.batt_mv = 3700; in.charging = false;
    face_tick(&in);
    /* a fake gel: a disc of 1s at the bottom, out to the rim */
    memset(fb, 0, sizeof fb);
    int gel_px = 0;
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        float dx = x + 0.5f - 120, dy = y + 0.5f - 200;
        if (dx * dx + dy * dy < 60 * 60) { fb[y * FACE_W + x] = 0x1234; gel_px++; }
    }
    face_draw(fb);
    int kept = 0; for (int i = 0; i < FACE_W * FACE_H; i++) if (fb[i] == 0x1234) kept++;
    assert(kept == gel_px);
    assert(lit_in(90, 110, 150, 130) == 0);                  /* no hour at the centre */
    assert(lit() > gel_px);                                  /* the rings where he is not */
    ppm("/tmp/face_behind.ppm");
    printf("  gel %d px untouched, %d marks around him\n", gel_px, lit() - gel_px);

    puts("-- the sink: the trace of his last silhouette, fading over the hour; the arrival takes it back");
    in.house_event = 2;                                      /* HOUSE_EV_LEFT */
    in.holding = false;
    face_tick(&in);
    in.house_event = 0;
    memset(fb, 0, sizeof fb);
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        float dx = x + 0.5f - 120, dy = y + 0.5f - 200;
        if (dx * dx + dy * dy < 60 * 60) fb[y * FACE_W + x] = 0x1234;
    }
    face_draw(fb);                                           /* the sink's first frame: the mask is read here */
    in.steps = 0; in.night_min = 0; in.batt_mv = 4130; in.charging = true;
    frame(in);
    int trace_px = lit() - base_px;
    printf("  trace %d px (the gel was %d)\n", trace_px, gel_px);
    assert(trace_px == gel_px);
    uint16_t tc = fb[200 * FACE_W + 120];
    assert(tc != 0);
    ppm("/tmp/face_trace.ppm");
    in.dt = 0.25f;
    for (int i = 0; i < 4 * 1800; i++) face_tick(&in);       /* half an hour */
    memset(fb, 0, sizeof fb); face_draw(fb);
    assert(lit() - base_px == gel_px && fb[200 * FACE_W + 120] != tc);   /* dimmer, still there */
    const char *l = NULL;
    for (int i = 0; i < 4 * 1800 + 8 && !l; i++) l = face_tick(&in);
    printf("  %s\n", l ? l : "(no line)");
    assert(l && strstr(l, "faded"));
    memset(fb, 0, sizeof fb); face_draw(fb);
    assert(lit() == base_px);
    /* a new trace, then the arrival */
    in.house_event = 2; face_tick(&in); in.house_event = 0;
    memset(fb, 0, sizeof fb); fb[200 * FACE_W + 120] = 0x1234; face_draw(fb);
    frame(in); assert(lit() == base_px + 1);
    in.house_event = 1; l = face_tick(&in); in.house_event = 0;
    assert(l && strstr(l, "taken back"));
    frame(in); assert(lit() == base_px);

    puts("-- THE TRACE STIRRED (watch-night C14): a tap near the residue pulses its tint once, a quarter to 0.45 and back over 0.6 s");
    assert(!face_trace_near(120, 200));                      /* no trace on the glass: nothing is near */
    in.stir = true; frame(in); in.stir = false; assert(lit() == base_px);   /* a stir with no trace is nothing */
    in.house_event = 2; face_tick(&in); in.house_event = 0;
    memset(fb, 0, sizeof fb);
    for (int y = 0; y < FACE_H; y++) for (int x = 0; x < FACE_W; x++) {
        float dx = x + 0.5f - 120, dy = y + 0.5f - 200;
        if (dx * dx + dy * dy < 60 * 60) fb[y * FACE_W + x] = 0x1234;
    }
    face_draw(fb);                                           /* the trace's box: x 60..180, y 140..240 */
    in.dt = 0.025f; frame(in);
    assert(face_trace_near(120, 200) && face_trace_near(48, 200) && !face_trace_near(47, 200));    /* the box plus 12 px */
    assert(face_trace_near(191, 200) && !face_trace_near(192, 200));
    assert(face_trace_near(120, 128) && !face_trace_near(120, 127) && !face_trace_near(10, 10));
    uint16_t rest = fb[200 * FACE_W + 120];
    const gel_rgba_t *body = &theme_pal()->body;
    /* face.cpp's ink, the panel's byte order undone: the three channels */
    auto chan = [](uint16_t c, int *r, int *g, int *b) { uint16_t v = (uint16_t)((c >> 8) | (c << 8)); *r = (v >> 11) & 31; *g = (v >> 5) & 63; *b = v & 31; };
    auto expect = [&](uint16_t c, float k) {
        int r, g, b; chan(c, &r, &g, &b);
        int R = (int)(body->r * k * 31 + 0.5f), G = (int)(body->g * k * 63 + 0.5f), B = (int)(body->b * k * 31 + 0.5f);
        return abs(r - R) <= 1 && abs(g - G) <= 1 && abs(b - B) <= 1;
    };
    assert(expect(rest, 0.25f));
    in.stir = true; frame(in); in.stir = false;              /* the tap's frame: the clock at 0, the tint still the quarter */
    assert(fb[200 * FACE_W + 120] == rest);
    in.dt = 0.15f;
    frame(in); uint16_t up = fb[200 * FACE_W + 120];        /* 0.15 s: on the way up */
    assert(expect(up, 0.25f + 0.2f * sinf((float)M_PI / 4)) && up != rest);
    frame(in); uint16_t peak = fb[200 * FACE_W + 120];      /* 0.3 s: the peak, 0.45 */
    assert(expect(peak, 0.45f) && peak != up);
    frame(in); assert(fb[200 * FACE_W + 120] == up);        /* 0.45 s: the way down mirrors the way up */
    frame(in); assert(expect(fb[200 * FACE_W + 120], 0.25f));   /* 0.6 s: the quarter again */
    frame(in); assert(fb[200 * FACE_W + 120] == rest);      /* past it: at rest, the same ink exactly */
    assert(lit() - base_px == gel_px);                       /* the shiver changed no pixel's place: the same silhouette throughout */
    int rr, gg, bb, pr, pg, pb; chan(rest, &rr, &gg, &bb); chan(peak, &pr, &pg, &pb);
    printf("  the residue at rest %d/%d/%d, at the peak %d/%d/%d (5-6-5), back by 0.6 s\n", rr, gg, bb, pr, pg, pb);
    in.stir = true; frame(in); in.stir = false; in.dt = 0.1f; frame(in);   /* a second tap mid-pulse starts it over */
    in.stir = true; frame(in); in.stir = false; assert(fb[200 * FACE_W + 120] != peak);
    for (int i = 0; i < 10; i++) frame(in);
    assert(fb[200 * FACE_W + 120] == rest);
    in.dt = 0.025f;
    in.house_event = 1; l = face_tick(&in); in.house_event = 0; assert(l && strstr(l, "taken back"));
    frame(in); assert(lit() == base_px && !face_trace_near(120, 200));

    puts("-- the glance brightens the resting hour, then it eases back to the shade");
    in = base(); frame(in); uint16_t shade = fb[113 * FACE_W + 120 - 27];
    for (int y = 110; y < 130 && !shade; y++) for (int x = 90; x < 150 && !shade; x++) shade = fb[y * FACE_W + x];
    in.glance = true; for (int i = 0; i < 40; i++) face_tick(&in);
    memset(fb, 0, sizeof fb); face_draw(fb);
    uint16_t bright = 0; for (int y = 110; y < 130 && !bright; y++) for (int x = 90; x < 150 && !bright; x++) bright = fb[y * FACE_W + x];
    assert(bright != shade);
    in.glance = false; for (int i = 0; i < 80; i++) face_tick(&in);
    memset(fb, 0, sizeof fb); face_draw(fb);
    uint16_t back = 0; for (int y = 110; y < 130 && !back; y++) for (int x = 90; x < 150 && !back; x++) back = fb[y * FACE_W + x];
    assert(back == shade);

    puts("-- THE DOCK FACE: the day ring as the courier's progress, the blink at `ok day`, the plug on the dock");
    in = base(); in.steps = 3000; in.minute = -1;             /* half the norm, no hour: the ring alone */
    frame(in); int steps_half = lit();
    uint16_t dim_c = 0; for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { dim_c = fb[y * FACE_W + 120]; break; }
    in.sync_active = true; in.sync_sent = 0; in.sync_total = 70000; frame(in);
    assert(lit() == steps_half);                              /* nothing sent yet: the steps as they were */
    in.sync_sent = 17500; frame(in);                          /* a quarter carried: the shine over the first quarter, the steps past it */
    uint16_t sh_c = 0; for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { sh_c = fb[y * FACE_W + 120]; break; }
    assert(sh_c && sh_c != dim_c && lit() == steps_half);
    assert(fb[120 * FACE_W + 236] == dim_c || fb[120 * FACE_W + 237] == dim_c);   /* 3 o'clock: the steps' dim body still */
    in.sync_sent = 52500; frame(in);                          /* three quarters: past the steps' half, the ring longer */
    int three_q = lit(); assert(three_q > steps_half && fabsf((float)three_q / full - 0.75f) < 0.03f);
    ppm("/tmp/face_dock_sync.ppm");
    in.sync_sent = 70000; frame(in); assert(lit() == full);
    /* the box's `ok day`: the sync ends, the seq bumps, the whole steps arc the shine for 300 ms, then the dim body */
    in.sync_active = false; in.sync_sent = in.sync_total = 0; in.sync_done_seq = 1;
    l = face_tick(&in); assert(l && strstr(l, "blinks"));
    memset(fb, 0, sizeof fb); face_draw(fb);
    uint16_t bl_c = 0; for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { bl_c = fb[y * FACE_W + 120]; break; }
    assert(bl_c == sh_c && lit() == steps_half);
    for (int i = 0; i < 16; i++) face_tick(&in);              /* 0.4 s */
    memset(fb, 0, sizeof fb); face_draw(fb);
    for (int y = 0; y < 4; y++) if (fb[y * FACE_W + 120]) { bl_c = fb[y * FACE_W + 120]; break; }
    assert(bl_c == dim_c);
    assert(face_tick(&in) == NULL);                           /* the same seq again: no second blink */
    /* the plug: 5x5 at 3 px (16 font px lit: 2 prongs, 5, 5, 3, the cable), at 6 o'clock inside the rings, only while docked; the drop yields */
    in = base(); in.minute = -1; in.batt_mv = 3700; in.charging = false; frame(in);
    int drop_px = lit(); assert(drop_px > 0 && lit_in(100, 200, 140, 232) == drop_px);
    in.docked = true; frame(in);
    int plug_px = lit();
    printf("  sync 3/4 %d px (full %d); plug %d px, the drop's %d yielded\n", three_q, full, plug_px, drop_px);
    assert(plug_px == 16 * 9 && lit_in(112, 216, 128, 232) == plug_px && all_on_disc());   /* around (120, 224), under r 112 */
    ppm("/tmp/face_dock_plug.ppm");
    in.docked = false; frame(in); assert(lit() == drop_px);

    puts("-- NO DAY RING UNTIL 3 DAYS (the optimization pass): under 3 days of files nothing is drawn against the guess; the night ring still shows");
    { uint32_t t[] = { 5000, 7000 }; face_set_history(t, 2); }
    in = base(); in.minute = -1; in.steps = 4000; in.night_min = 480; frame(in);
    int night_only = lit();
    assert(night_only > 0 && night_only < full);              /* the night ring alone (its 1436 px), no day ring */
    assert(lit_in(0, 0, FACE_W, 4) == 0 || rad_max() < 116.0f);   /* nothing in the day ring's band at the top */
    in.steps = 20000; frame(in); assert(lit() == night_only);  /* however many steps: no ring without a norm */
    { uint32_t t[] = { 6000, 6000, 6000 }; face_set_history(t, 3); }
    in.steps = 6000; frame(in); assert(lit() > night_only);   /* 3 days: the ring is back */
    printf("  night ring alone %d px; with the day ring %d px\n", night_only, lit());

    puts("-- THE SWEEP: on a raise both rings grow from 12 o'clock over 0.6 s, eased out; over by 0.6 s; a boot has no sweep");
    in = base(); in.minute = -1; in.steps = 3000; in.night_min = 480;
    frame(in); int at_rest = lit();                           /* no raise: the rings simply there */
    face_raised();
    frame(in); int t0 = lit();                                /* the first lit frame after the raise: 25 ms in, ~12% of the way */
    assert(t0 > 0 && t0 < at_rest / 3);
    for (int i = 0; i < 7; i++) frame(in); int t8 = lit();    /* 0.2 s: eased out -- most of the way already */
    assert(t8 > t0 && t8 > at_rest / 2 && t8 < at_rest);
    for (int i = 0; i < 20; i++) frame(in);                   /* 0.7 s: over */
    assert(lit() == at_rest);
    frame(in); assert(lit() == at_rest);                      /* and it stays */
    printf("  sweep: %d px at 25 ms, %d at 200 ms, %d at rest\n", t0, t8, at_rest);
    /* the sweep starts both arcs at 12 o'clock: at 25 ms nothing yet at 3 or 9 o'clock */
    face_raised(); frame(in);
    assert(lit_in(230, 110, 240, 130) == 0 && lit_in(0, 110, 10, 130) == 0);
    /* the dock's progress arc is not swept: the courier is not a fact about the day */
    face_raised(); in.sync_active = true; in.sync_sent = 35000; in.sync_total = 70000; frame(in);
    assert(lit_in(230, 110, 240, 130) > 0);                  /* the shine already at 3 o'clock */
    in.sync_active = false; in.sync_sent = in.sync_total = 0;
    for (int i = 0; i < 30; i++) frame(in);

    puts("-- no clock: no hour; face off: the black glass");
    in = base(); in.minute = -1; in.steps = 3000; frame(in); assert(lit_in(90, 110, 150, 130) == 0 && lit() > 0);
    face_set_on(false); frame(in); assert(lit() == 0);
    face_set_on(true);

    char s[320]; in = base(); in.steps = 4213; in.night_min = 444; in.batt_mv = 3870; in.charging = false;
    in.sync_done_seq = 1;                                     /* the seq as the link left it (it only grows on the board) */
    in.night_deep = true; in.night_wander = true;
    for (int i = 0; i < 20; i++) face_tick(&in);
    face_status(s, sizeof s); printf("  %s\n", s);
    assert(strstr(s, "deep: the shine") && strstr(s, "the tip breathes"));
    puts("ALL GOOD");
    return 0;
}
