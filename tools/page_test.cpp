/* THE WATCH PAGE on the laptop (no board, no Arduino: page.cpp is
 * parameters in, a framebuffer out; gel.c only for the palette):
 *   c++ -std=gnu++17 -I src -o /tmp/page_test tools/page_test.cpp src/page.cpp src/theme.cpp -x c src/gel.c -lm && /tmp/page_test
 * The page down draws nothing; up, the hour and the date render (their
 * pixel counts are the font's own: "07:41" at 4 px = 38 blocks x 16,
 * "TUE 10 SEP" at 3 px = 79 blocks x 9), the steps, the cell, the
 * alarm (or "--"), the sky's word and the link dot in its two inks;
 * every lit pixel inside the disc AND inside the rings (r < 112); a
 * ring pixel at the rim survives, a face pixel under the page does
 * not; the mark on the way up (the disc), unchanged while it stays,
 * again on the way down; the 8 s, re-armed by a second show; no clock
 * = the dashes. THE SETTINGS PAGE: the fourth and fifth pages, their
 * rows' text at the font's own counts (ON in the shine, OFF in the
 * shade, the alarm's hour), a tap on a row = the row's action, the row
 * lit with its bar for 0.6 s and the 8 s again, a tap off the rows =
 * none (main closes), the bands' edges, page 2's status lines; no
 * store on the laptop, so what a row DOES is main's (the board's).
 * THE SWIPES (2026-09-23): page_prev walks the ring backwards (the
 * tool page -> ABOUT -> settings 6/6), page_next forward, the lines.
 * THE OPERATOR NIGHT (2026-10-12): THE BLUETOOTH PAGE (T16) after the
 * notices -- five lines in words, PAIRING the one row, a tap elsewhere
 * keeps it; SETTINGS 6 (T15) -- PULSE AUTO, DOCK HAND, EAR GEAR and
 * FORGET's two taps (the arm, SURE?, the 3 s, the page turning).
 * THE NOTICE OPENED (2026-10-06): the list's rows wear the ledger's
 * name (the id's last word without one) and the message's start when
 * there is no title; a tap on a row answers PAGE_NOTE_0 + the row and
 * opens it -- the name in the shine at the top, the title dimmed, the
 * message wrapped over rows inside the ring (a long word cut, five
 * rows at most), the age at the bottom; a tap on it keeps it; a drag
 * up (page_next) is the list again, not the next page; a tap off the
 * rows, or on NO NOTICES, is none (main closes).
 * Writes /tmp/page_*.ppm to look at (python3 tools/ppm2png.py). */
#include "page.h"
#include "gel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const int W = 240, H = 240;
static uint16_t fb[W * H];
static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int lit_in(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (fb[y * W + x]) n++;
    return n;
}
static int lit(void) { return lit_in(0, 0, W, H); }
/* every lit pixel inside a radius of the centre (pixel centres) */
static bool all_within(float r)
{
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (!fb[y * W + x]) continue;
        float dx = x + 0.5f - 120, dy = y + 0.5f - 120;
        if (dx * dx + dy * dy >= r * r) return false;
    }
    return true;
}
static uint16_t ink(const gel_rgba_t *c, float k)   /* the drawers' own, for the expected colours */
{
    int R = (int)(c->r * k * 31 + 0.5f), G = (int)(c->g * k * 63 + 0.5f), B = (int)(c->b * k * 31 + 0.5f);
    uint16_t v = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((v >> 8) | (v << 8));
}
static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t v = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        unsigned char px[3] = { (unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63),
                                (unsigned char)((v & 31) * 255 / 31) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static page_in_t base(void)
{
    page_in_t in;
    memset(&in, 0, sizeof in);
    in.minute = 7 * 60 + 41; in.wday = 2; in.mday = 10; in.mon = 8;     /* Tue 10 Sep, 07:41 */
    in.steps = 4213; in.batt_pct = 87; in.charging = false;
    in.alarm_min = 7 * 60 + 30; in.sky = "storm"; in.link = PAGE_LINK_FINE;
    for (int i = 0; i < 7; i++) in.week[i] = PAGE_NO_DAY;               /* no files (the tool page does not read them) */
    in.week[6] = in.steps;
    return in;
}
/* a frame: the face's marks first (a ring pixel at the rim, the face's
 * hour at the centre), then the page over them */
static const uint16_t RING = 0x1234, FACEHOUR = 0x5678;
static void frame(const page_in_t *in)
{
    memset(fb, 0, sizeof fb);
    fb[4 * W + 120] = RING;                 /* r 116: the day ring's band */
    fb[120 * W + 120] = FACEHOUR;           /* the centre: the face's resting hour */
    page_draw(fb, in);
}
/* a frame on a given page (page_show_which), the face's marks under it as in frame() */
static void frame_on(int which, const page_in_t *in)
{
    page_show_which(which, "the test"); page_tick(0.025f);
    frame(in);
}

int main(void)
{
    const gel_rgba_t *sn = &GEL_PAL_BLUE.shine, *sh = &GEL_PAL_BLUE.shade;
    page_begin();
    dirty_rect_t box;

    puts("-- down: nothing drawn");
    page_in_t in = base();
    check("begins down", !page_up() && page_left_s() == 0);
    frame(&in);
    check("down: the frame untouched (the ring and the face's hour stay)", lit() == 2 && fb[4 * W + 120] == RING && fb[120 * W + 120] == FACEHOUR);
    check("down: no mark", !page_mark(&box) || box.w == 0);
    check("down: no line", page_tick(0.025f) == NULL);

    puts("-- up: the drag");
    page_show("a drag up");
    const char *l = page_tick(0.025f);
    printf("  %s\n", l ? l : "(no line)");
    check("up, with its line", page_up() && l && !strncmp(l, "page: up (a drag up)", 20));
    check("8 s to go", page_left_s() > 7.9f && page_left_s() <= 8.0f);
    frame(&in);
    ppm("/tmp/page_up.ppm");
    check("every lit pixel inside the disc", all_within(120.0f));
    check("... and inside the rings (r < 112), the ring pixel aside", fb[4 * W + 120] == RING);
    fb[4 * W + 120] = 0;
    check("... every page pixel under r 112", all_within(112.0f));
    check("the face's hour under the page is cleared", fb[120 * W + 120] != FACEHOUR);
    /* the hour: "07:41" at 4 px, centred at 120 -- 76 x 20 at (82, 74); the font's blocks: 0=12 7=7 :=2 4=9 1=8 */
    int hour_px = lit_in(82, 74, 158, 94);
    printf("  hour box lit %d (want 608)\n", hour_px);
    check("the hour rendered, the font's own count (38 blocks x 16)", hour_px == 38 * 16);
    check("the hour in the shine", fb[74 * W + 82] == ink(sn, 1.0f));   /* 0's top-left block */
    /* the date: "TUE 10 SEP" at 3 px, 117 x 15 at (62, 104); T=7 U=11 E=10 1=8 0=12 S=11 E=10 P=10 */
    int date_px = lit_in(62, 104, 179, 119);
    printf("  date box lit %d (want 711)\n", date_px);
    check("the date rendered, the font's own count (79 blocks x 9)", date_px == 79 * 9);
    check("the date dimmer than the hour", fb[104 * W + 62] == ink(sn, 0.75f));   /* T's top-left block */
    /* the steps and the cell on one row */
    int steps_px = lit_in(20, 134, 120, 149), cell_px = lit_in(120, 134, 220, 149);
    printf("  steps group lit %d, cell group lit %d\n", steps_px, cell_px);
    check("the steps: the footprint and 4213", steps_px > 0);
    check("the cell: the drop and 87%", cell_px > 0);
    /* the alarm row, the sky row, the dot */
    int alarm_px = lit_in(60, 160, 180, 175), sky_px = lit_in(60, 186, 180, 201);
    printf("  alarm row lit %d, sky row lit %d\n", alarm_px, sky_px);
    check("the alarm: the bell and 07:30", alarm_px > 0);
    check("the sky's word", sky_px > 0);
    check("the link dot, green-ish (a door)", fb[40 * W + 120] != 0 && fb[40 * W + 120] != ink(sh, 1.0f));
    check("nothing above the dot or below the sky row", lit_in(0, 0, W, 35) == 0 && lit_in(0, 202, W, H) == 0);
    int up_px = lit();
    check("the mark on the way up: the page's disc", page_mark(&box) && box.x <= 8 && box.y <= 8 && box.x + box.w >= 232 && box.y + box.h >= 232);
    frame(&in);
    check("the same frame again: unchanged, no mark", !page_mark(&box) && lit() == up_px + 1);

    puts("-- the variations");
    page_in_t v = base(); v.alarm_min = -1; v.sky = ""; v.link = PAGE_LINK_NONE; v.batt_pct = 100; v.charging = true;
    frame(&v);
    ppm("/tmp/page_bare.ppm");
    int dash_px = lit_in(60, 160, 180, 175);
    printf("  alarm row lit %d (the dashes)\n", dash_px);
    check("no alarm: the bell and -- in the shade", dash_px > 0 && dash_px < alarm_px);
    check("no sky: the row empty", lit_in(60, 186, 180, 201) == 0);
    check("no door: the dot in the shade", fb[40 * W + 120] == ink(sh, 1.0f));
    check("100% fits the row", lit_in(120, 134, 220, 149) > cell_px);
    check("the mark: the numbers turned", page_mark(&box) && box.w > 0);
    check("still inside the rings", all_within(112.0f) || fb[4 * W + 120] == RING);
    page_in_t nc = base(); nc.minute = -1; nc.wday = -1;
    frame(&nc);
    ppm("/tmp/page_noclock.ppm");
    check("no clock: the dashes render (--:-- and -- -- ---)", lit_in(82, 74, 158, 94) > 0 && lit_in(61, 104, 179, 119) > 0);
    page_in_t big = base(); big.steps = 123456; big.batt_pct = 250;
    frame(&big);
    check("six digits of steps and a capped percent still inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
    for (int m = 0; m < 12; m++) {          /* every month, every day name: inside the rings */
        page_in_t d = base(); d.mon = m; d.wday = m % 7; d.mday = 28;
        frame(&d); fb[4 * W + 120] = 0;
        if (!all_within(112.0f)) { check("a date outside the rings", false); break; }
    }
    check("every month's date inside the rings", true);

    puts("-- the stay");
    page_show("serial");
    l = page_tick(0.025f);
    check("shown again: re-armed, the line says so", l && !strncmp(l, "page: up again (serial)", 23) && page_left_s() > 7.9f);
    /* THE PULSE ROW (2026-10-03): page_hold tops the stay back up, silently; down, it does nothing */
    for (int i = 0; i < 120; i++) page_tick(0.025f);
    check("3 s gone", page_left_s() < 5.1f && page_up());
    page_hold(); l = page_tick(0.025f);
    check("page_hold: 8 s again, no line, the same page", page_left_s() > 7.9f && !l && page_up());
    for (int i = 0; i < 300 && page_up(); i++) l = page_tick(0.025f);   /* 7.5 s */
    check("still up at 7.5 s", page_up());
    const char *down = NULL;
    for (int i = 0; i < 40 && !down; i++) { l = page_tick(0.025f); if (l) down = l; }
    printf("  %s\n", down ? down : "(no line)");
    check("down at 8 s, with its line", !page_up() && down && !strcmp(down, "page: down (its 8 s are up)"));
    frame(&in);
    check("down: nothing drawn, the mark says the disc went", lit() == 2 && page_mark(&box) && box.w > 0);
    page_show("a drag up"); page_tick(0.025f);
    page_hide("a tap");
    l = page_tick(0.025f);
    check("a tap closes it", !page_up() && l && !strcmp(l, "page: down (a tap)"));
    page_hide("a tap");
    check("hiding a hidden page: nothing", page_tick(0.025f) == NULL);
    char st[120]; page_status(st, sizeof st); printf("  %s\n", st);

    puts("-- THE PAGE GROWS: the tool page with a count running");
    {
        page_in_t t = base();
        static const uint32_t splits[3] = { 12000, 25400, 41000 };
        t.tool = PAGE_TOOL_STOPWATCH; t.tool_ms = 83000; t.laps = 3; t.lap_ms = splits;
        page_show("a drag up"); page_tick(0.025f);
        frame(&t);
        ppm("/tmp/page_stopwatch.ppm");
        int tool_row = lit_in(40, 104, 200, 119), laps_row = lit_in(40, 186, 200, 201);
        printf("  the tool row lit %d, the laps row %d\n", tool_row, laps_row);
        check("the count on the date's row: the stopwatch's mark, 1:23 and L3", tool_row > 0 && tool_row != date_px);
        check("the last two laps' splits on the sky's row (0:25 0:41), the sky's word yielding", laps_row > 0 && laps_row != sky_px);
        check("every pixel still inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        t.laps = 0; t.lap_ms = NULL; frame(&t);
        check("no laps: the sky's word is back on its row", lit_in(40, 186, 200, 201) == sky_px);
        t.tool = PAGE_TOOL_TIMER; t.tool_ms = 299000; frame(&t);
        int timer_row = lit_in(40, 104, 200, 119);
        check("a timer: the hourglass and 4:59 on the row", timer_row > 0 && timer_row != tool_row);
        page_hide("the test"); page_tick(0.025f);
    }

    puts("-- the drag up turns the page: the tool page, his week, the notices, bluetooth, settings 1-6, about, the tool page again; 8 s each time");
    page_show("a drag up"); l = page_tick(0.025f);
    check("up: the tool page", page_which() == PAGE_TOOL && !strcmp(page_name(page_which()), "the tool page"));
    for (int i = 0; i < 200; i++) page_tick(0.025f);        /* 5 s in */
    page_next("a drag up"); l = page_tick(0.025f);
    printf("  %s\n", l ? l : "(no line)");
    check("a second drag up: his week, the 8 s again, the line says so", page_which() == PAGE_WEEK && page_left_s() > 7.9f && l && !strncmp(l, "page: turned (a drag up) -- his week", 36));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a third: the notices", page_which() == PAGE_NOTICES_PAGE && l && strstr(l, "the notices"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a fourth: bluetooth (THE BLUETOOTH PAGE, T16)", page_which() == PAGE_BLE && l && strstr(l, "-- bluetooth"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a fifth: settings 1/6 (THE SETTINGS PAGE)", page_which() == PAGE_SETTINGS && l && strstr(l, "settings 1/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a sixth: settings 2/6", page_which() == PAGE_SETTINGS2 && l && strstr(l, "settings 2/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a seventh: settings 3/6 (MORE SETTINGS)", page_which() == PAGE_SETTINGS3 && l && strstr(l, "settings 3/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("an eighth: settings 4/6", page_which() == PAGE_SETTINGS4 && l && strstr(l, "settings 4/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a ninth: settings 5/6", page_which() == PAGE_SETTINGS5 && l && strstr(l, "settings 5/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("a tenth: settings 6/6 (THE HIDDEN SWITCHES, T15)", page_which() == PAGE_SETTINGS6 && l && strstr(l, "settings 6/6"));
    page_next("a drag up"); l = page_tick(0.025f);
    check("an eleventh: about", page_which() == PAGE_ABOUT && l && strstr(l, "-- about"));
    page_next("a drag up"); page_tick(0.025f);
    check("a twelfth: the tool page again", page_which() == PAGE_TOOL);
    page_hide("a tap"); page_tick(0.025f);
    check("closed: no page", page_which() == -1);
    page_next("a drag up"); page_tick(0.025f);
    check("a drag up on the dark glass opens the tool page", page_which() == PAGE_TOOL);
    page_hide("a tap"); page_tick(0.025f);

    puts("-- THE SWIPES (2026-09-23): a swipe toward his right turns the ring BACK (page_prev), toward his left forward (page_next)");
    page_show("a drag up"); page_tick(0.025f);
    for (int i = 0; i < 200; i++) page_tick(0.025f);        /* 5 s in */
    page_prev("a swipe right"); l = page_tick(0.025f);
    printf("  %s\n", l ? l : "(no line)");
    check("from the tool page, back: ABOUT (the ring's far end), the 8 s again, the line says so",
          page_which() == PAGE_ABOUT && page_left_s() > 7.9f && l && !strncmp(l, "page: turned back (a swipe right) -- about", 42));
    page_prev("a swipe right"); l = page_tick(0.025f);
    check("back again: settings 6/6", page_which() == PAGE_SETTINGS6 && l && strstr(l, "settings 6/6"));
    page_next("a swipe left"); l = page_tick(0.025f);
    check("a swipe left: forward to ABOUT (the same ring as the drag up)", page_which() == PAGE_ABOUT && l && !strncmp(l, "page: turned (a swipe left) -- about", 36));
    page_next("a swipe left"); page_tick(0.025f);
    check("...and on round to the tool page", page_which() == PAGE_TOOL);
    page_prev("a swipe right"); page_next("a swipe left"); page_tick(0.025f);
    check("back and forward: where it was", page_which() == PAGE_TOOL);
    page_show_which(PAGE_WEEK, "the test"); page_tick(0.025f);
    page_prev("a swipe right"); page_tick(0.025f);
    check("from his week, back: the tool page", page_which() == PAGE_TOOL);
    page_show_which(PAGE_SETTINGS, "the test"); page_tick(0.025f);
    page_prev("a swipe right"); page_tick(0.025f);
    check("from settings 1/6, back: bluetooth (T16: the page after the notices)", page_which() == PAGE_BLE);
    page_prev("a swipe right"); page_tick(0.025f);
    check("from bluetooth, back: the notices", page_which() == PAGE_NOTICES_PAGE);
    page_hide("a tap"); page_tick(0.025f);
    page_prev("a swipe right"); page_tick(0.025f);
    check("a swipe back on the dark glass opens the tool page (main never sends one: the bare glass's swipe is the shove)", page_which() == PAGE_TOOL);
    page_hide("a tap"); page_tick(0.025f);

    puts("-- HIS WEEK: seven rows, the bars against the week's best, the numbers, a day with no file, the night, the cell");
    {
        page_in_t w = base();
        static const uint32_t days[7] = { 6000, 3000, PAGE_NO_DAY, 0, 4500, 8000, 4213 };
        for (int i = 0; i < 7; i++) w.week[i] = days[i];
        w.night_min = 7 * 60 + 24;
        frame_on(PAGE_WEEK, &w);
        ppm("/tmp/page_week.ppm");
        check("every pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        check("the face's hour under the page is cleared", fb[120 * W + 120] != FACEHOUR);
        /* the rows: 62 + 18 i; each row's left edge is the page's chord less 6 px (page.cpp's half_at), the
         * bar's lane 44 px in from it, 70 px wide, rows y+3..y+12 */
        int bar[7];
        for (int i = 0; i < 7; i++) {
            int y = 62 + 18 * i, dy = (120 - y) > (y + 15 - 120) ? (120 - y) : (y + 15 - 120);
            int half = (int)sqrtf((float)(112 * 112 - dy * dy)) - 6, bx = 120 - half + 44;
            bar[i] = lit_in(bx, y + 3, bx + 70, y + 12);
        }
        printf("  the bars: %d %d %d %d %d %d %d px\n", bar[0], bar[1], bar[2], bar[3], bar[4], bar[5], bar[6]);
        check("the best day (8000) has the longest bar, 6000 three quarters of it, 3000 three eighths", bar[5] > bar[0] && bar[0] > bar[1] && bar[1] > 0
              && abs(bar[0] * 4 - bar[5] * 3) <= 3 * 9 && abs(bar[1] * 8 - bar[5] * 3) <= 3 * 9);
        check("a day of nothing: no bar; a day with no file: no bar either", bar[3] == 0 && bar[2] == 0);
        check("today's row (4213): a bar", bar[6] > 0);
        int row_nofile = lit_in(40, 62 + 18 * 2, 200, 62 + 18 * 2 + 15), row_zero = lit_in(40, 62 + 18 * 3, 200, 62 + 18 * 3 + 15);
        check("the no-file row reads -- (fewer pixels than a 0 row with its number)", row_nofile > 0 && row_zero > 0);
        int night_row = lit_in(40, 40, 200, 55);
        check("the night's hours with the moon on the top row (7:24)", night_row > 0);
        check("the cell on the bottom row", lit_in(60, 196, 180, 211) > 0);
        w.night_min = 0; frame(&w);
        check("no night known: the row is the dashes (fewer pixels)", lit_in(40, 40, 200, 55) > 0 && lit_in(40, 40, 200, 55) < night_row);
        for (int i = 0; i < 7; i++) w.week[i] = PAGE_NO_DAY;
        w.week[6] = 123456; frame(&w);
        check("a week of no files but today, six digits: inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        page_hide("the test"); page_tick(0.025f);
    }

    puts("-- THE NOTICES: the last three, the app's last name and how long ago, the title under; none = NO NOTICES");
    {
        page_in_t n = base();
        static const page_notice_t notes[3] = { { "com.mail", "A letter from the university about the fees", 40 },
                                                { "org.signal.app", "Khalid", 3 * 60 + 5 }, { "slime", NULL, 2 * 3600 + 100 } };
        n.notice = notes; n.notices = 3;
        frame_on(PAGE_NOTICES_PAGE, &n);
        ppm("/tmp/page_notices.ppm");
        check("every pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        int r0 = lit_in(0, 62, W, 77), r0t = lit_in(0, 80, W, 95), r1 = lit_in(0, 102, W, 117), r2 = lit_in(0, 142, W, 157), r2t = lit_in(0, 160, W, 175);
        printf("  rows lit: %d (title %d) / %d / %d (title %d)\n", r0, r0t, r1, r2, r2t);
        check("three notices: each on its row, the first two with a title under, the third without", r0 > 0 && r0t > 0 && r1 > 0 && r2 > 0 && r2t == 0);
        /* the age at the right of the first row: "40S" -> its glyphs end near the row's right edge */
        check("the age at the row's right edge", lit_in(150, 62, 240, 77) > 0);
        n.notices = 0; n.notice = NULL; frame(&n);
        check("no notices: one dim line at the centre", lit_in(60, 110, 180, 130) > 0 && lit_in(0, 62, W, 95) == 0);
        check("NO NOTICES: a tap on it is none (main closes), nothing open", page_tap(120, 70) == PAGE_ACT_NONE && page_note_open() == -1);
        page_hide("the test"); page_tick(0.025f);
    }

    puts("-- THE NOTICE OPENED (2026-10-06): the list with the ledger's names and messages; a tap opens one; a drag up is the list again");
    {
        const uint16_t BR = ink(sn, 1.0f), DM = ink(sn, 0.75f);
        auto count = [&](int y0, int y1, uint16_t c) { int n = 0; for (int y = y0; y < y1; y++) for (int x = 0; x < W; x++) if (fb[y * W + x] == c) n++; return n; };
        page_in_t n = base();
        /* the font's own counts (blocks x 9 px at 3 px): "MAIL" M 12 A 10 I 9 L 7 = 38; "MESSAGES" 12+10+11+11+10+11+10+11 = 86; "SLIME" 11+7+9+12+10 = 49 */
        static const page_notice_t notes[3] = {
            { "com.apple.mobilemail", "The university", 40, "Mail", "Your fees for the autumn term are due on Friday; see the attached invoice for the amount" },
            { "com.apple.MobileSMS", NULL, 3 * 60 + 5, "Messages", "Are you coming tonight" },
            { "slime", NULL, 2 * 3600 + 100, "", NULL } };
        n.notice = notes; n.notices = 3;
        frame_on(PAGE_NOTICES_PAGE, &n);
        ppm("/tmp/page_notices_named.ppm");
        check("row 0: the ledger's name MAIL in the shine (38 x 9 -- not MOBILEMA, the id's word), the title under it dimmed",
              count(62, 77, BR) == 38 * 9 && lit_in(0, 80, W, 95) > 0);
        check("row 1: MESSAGES (86 x 9); no title, so the message's start under it", count(102, 117, BR) == 86 * 9 && lit_in(0, 120, W, 135) > 0);
        check("row 2: no name -- the id's last word SLIME (49 x 9); nothing under it", count(142, 157, BR) == 49 * 9 && lit_in(0, 160, W, 175) == 0);
        check("every pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        check("nothing open yet", page_note_open() == -1);
        /* the taps on the list: the bands are the rows' pitch (58..97, 98..137, 138..177), inside the chord */
        for (int i = 0; i < 200; i++) page_tick(0.025f);        /* 5 s in */
        check("the bands' edges: 58 and 97 are row 0, 98 is row 1, 57 is nothing", page_tap(120, 58) == PAGE_NOTE_0 && (page_next("t"), page_tap(120, 97) == PAGE_NOTE_0)
              && (page_next("t"), page_tap(120, 98) == PAGE_NOTE_0 + 1) && (page_next("t"), page_tap(120, 57) == PAGE_ACT_NONE));
        check("row 2 at 150; under the last row (180), or off the chord (10, 70): none", page_tap(120, 150) == PAGE_NOTE_0 + 2 && (page_next("t"), page_tap(120, 180) == PAGE_ACT_NONE) && page_tap(10, 70) == PAGE_ACT_NONE);
        page_tick(0.025f);
        for (int i = 0; i < 200; i++) page_tick(0.025f);        /* 5 s in */
        int r = page_tap(120, 70); const char *l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a tap on row 0: PAGE_NOTE_0, opened, the line says so, the 8 s again", r == PAGE_NOTE_0 && page_note_open() == 0 && page_left_s() > 7.9f && l && strstr(l, "a tap on notice 1 -- opened"));
        check("still the notices page", page_which() == PAGE_NOTICES_PAGE);
        frame(&n);
        ppm("/tmp/page_notice_open.ppm");
        check("the open notice: MAIL in the shine at the top (38 x 9 at 40), centred", count(40, 55, BR) == 38 * 9 && lit_in(0, 40, 90, 55) == 0 && lit_in(150, 40, W, 55) == 0);
        check("the title THE UNIVERSITY dimmed under it (at 62)", count(62, 77, DM) > 0 && count(62, 77, BR) == 0);
        int rows = 0;
        for (int i = 0; i < PAGE_NOTE_ROWS; i++) if (lit_in(0, 86 + i * 18, W, 101 + i * 18) > 0) rows++;
        printf("  message rows lit: %d\n", rows);
        check("the message wrapped over five rows (86, 104, 122, 140, 158), in the shine", rows == 5 && count(86, 101, BR) > 0 && count(158, 173, BR) > 0);
        check("nothing between the rows (the pitch's gap)", lit_in(0, 101, W, 104) == 0 && lit_in(0, 173, W, 190) == 0);
        check("the age 40S AGO at the bottom (190), dimmed", count(190, 205, DM) > 0 && count(190, 205, BR) == 0);
        check("nothing drawn outside the ring: every pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        check("no list rows under the card (the list is gone while it is open)", lit_in(0, 206, W, H) == 0);
        int open_px = lit();
        check("the mark: the open notice is another frame", page_mark(&box) && box.w > 0);
        /* a tap on the open notice keeps it */
        for (int i = 0; i < 200; i++) page_tick(0.025f);
        r = page_tap(120, 120); l = page_tick(0.025f);
        check("a tap on the open notice: PAGE_NOTE_0 again, still open, the 8 s again", r == PAGE_NOTE_0 && page_note_open() == 0 && page_left_s() > 7.9f && l && strstr(l, "the open notice"));
        check("a tap off the disc while open: none", page_tap(2, 120) == PAGE_ACT_NONE);
        /* a drag up: the list, not the next page */
        page_next("a drag up"); l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a drag up on the open notice: the list again (not SETTINGS 1), the line says so, the 8 s again",
              page_which() == PAGE_NOTICES_PAGE && page_note_open() == -1 && page_left_s() > 7.9f && l && strstr(l, "the notice closed"));
        frame(&n);
        check("the list drawn again", lit() != open_px && count(62, 77, BR) == 38 * 9);
        page_next("a drag up"); page_tick(0.025f);
        check("a second drag up: THE BLUETOOTH PAGE (the ring as ever)", page_which() == PAGE_BLE);
        page_prev("a swipe right"); page_tick(0.025f);
        page_tap(120, 110); page_tick(0.025f);
        check("back on the notices, row 1 opened by its tap", page_note_open() == 1);
        page_prev("a swipe right"); page_tick(0.025f);
        check("a swipe right on an open notice: the list too", page_which() == PAGE_NOTICES_PAGE && page_note_open() == -1);
        /* THE NOTICE CLEARED FROM THE WRIST (T17): the hold on the open card answers the row and closes it to the list */
        check("a hold on the list: -1 (the lamp's as ever), nothing open", page_note_hold() == -1 && page_note_open() == -1);
        page_tap(120, 110); page_tick(0.025f);
        for (int i = 0; i < 200; i++) page_tick(0.025f);
        r = page_note_hold(); l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a hold on notice 2's card: row 1 answered, the list again, the 8 s again, the line says cleared",
              r == 1 && page_which() == PAGE_NOTICES_PAGE && page_note_open() == -1 && page_left_s() > 7.9f && l && strstr(l, "a hold on notice 2 -- cleared"));
        check("a second hold: -1 (nothing open)", page_note_hold() == -1);
        page_tap(120, 70); page_lamp("a long press"); page_tick(0.025f);
        check("under the lamp with a card open: -1 (the tap puts the lamp out first)", page_note_hold() == -1);
        page_hide("the test"); page_tick(0.025f);
        check("no page: -1", page_note_hold() == -1);
        page_show_which(PAGE_NOTICES_PAGE, "the test"); page_tick(0.025f); frame(&n);
        /* the second notice open: no title -- the name, the message, the age */
        page_tap(120, 110); page_tick(0.025f); frame(&n);
        ppm("/tmp/page_notice_open2.ppm");
        check("MESSAGES open: the name at the top (86 x 9), no title row, the message's two rows at 86 and 104 (22 glyphs; a row at 86 holds 16) and nothing after, 3M AGO at the bottom",
              count(40, 55, BR) == 86 * 9 && lit_in(0, 62, W, 77) == 0 && lit_in(0, 86, W, 101) > 0 && lit_in(0, 104, W, 119) > 0 && lit_in(0, 122, W, 173) == 0 && count(190, 205, DM) > 0);
        check("every pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        /* a long word is cut at the chord, never past the ring */
        page_in_t lw = base();
        static const page_notice_t longword[1] = { { "x", "T", 5, "X", "Supercalifragilisticexpialidociousness and then more words after the long one to fill a few rows of the glass" } };
        lw.notice = longword; lw.notices = 1;
        page_show_which(PAGE_NOTICES_PAGE, "the test"); page_tick(0.025f);
        page_tap(120, 70); page_tick(0.025f); frame(&lw);
        ppm("/tmp/page_notice_open_long.ppm");
        check("a word wider than the chord: cut there, every pixel inside the rings, the rows filled", (fb[4 * W + 120] = 0, all_within(112.0f)) && lit_in(0, 86, W, 101) > 0 && lit_in(0, 104, W, 119) > 0);
        /* the open notice closes with the page; the ledger shrinking under it is honest */
        page_hide("a tap"); page_tick(0.025f);
        check("closed with the page", page_note_open() == -1);
        page_show_which(PAGE_NOTICES_PAGE, "the test"); page_tick(0.025f); frame(&n);
        page_tap(120, 150); page_tick(0.025f);
        check("row 2 open", page_note_open() == 2);
        n.notices = 1; frame(&n);
        check("one notice left under it: the page shows the one it has (row 0)", page_note_open() == 0);
        page_show_which(PAGE_WEEK, "the test"); page_tick(0.025f);
        check("another page: nothing open", page_note_open() == -1);
        page_hide("the test"); page_tick(0.025f);
    }

    puts("-- THE LAMP (B17): a long press on any page = the glass white, the rings under it too, 20 s; the page's 8 s wait; a tap ends both");
    {
        page_in_t t = base();
        page_show_which(PAGE_WEEK, "a drag up"); page_tick(0.025f);
        for (int i = 0; i < 120; i++) page_tick(0.025f);        /* 3 s into the week page */
        page_lamp("a long press");
        l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("the lamp burns, on the page that was up, with its line", page_lamp_on() && page_up() && page_which() == PAGE_WEEK && l && !strncmp(l, "page: the lamp (a long press)", 29));
        frame(&t);
        ppm("/tmp/page_lamp.ppm");
        int white = 0, off_disc = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            float dx = x + 0.5f - 120, dy = y + 0.5f - 120;
            bool on = dx * dx + dy * dy < 120.0f * 120.0f;
            if (fb[y * W + x] == 0xFFFF) { white++; if (!on) off_disc++; }
        }
        printf("  %d white px, %d of them off the disc\n", white, off_disc);
        check("the whole disc white (~45,000 px), nothing off it", white > 44000 && white < 46000 && off_disc == 0);
        check("the ring pixel under the lamp is white too (a lamp is not a page)", fb[4 * W + 120] == 0xFFFF);
        check("the mark: the whole frame", page_mark(&box) && box.w >= 236 && box.h >= 236);
        float left = page_left_s();
        for (int i = 0; i < 400; i++) page_tick(0.025f);        /* 10 s */
        check("10 s in: still burning, the page's own seconds untouched", page_lamp_on() && page_left_s() == left);
        page_hide("a tap"); l = page_tick(0.025f);
        check("a tap: the lamp out, the page down, the line says so", !page_lamp_on() && !page_up() && l && strstr(l, "the lamp out"));
        frame(&t);
        check("nothing drawn after", lit() == 2);
        page_lamp("a long press"); page_tick(0.025f);
        check("the lamp on the dark glass: the tool page lit white", page_lamp_on() && page_which() == PAGE_TOOL);
        for (int i = 0; i < 799; i++) page_tick(0.025f);        /* 19.975 s */
        check("19.98 s: still burning", page_lamp_on());
        const char *out = NULL;
        for (int i = 0; i < 4 && !out; i++) { l = page_tick(0.025f); if (l) out = l; }
        printf("  %s\n", out ? out : "(no line)");
        check("20 s: out by itself, the page down", !page_lamp_on() && !page_up() && out && strstr(out, "the lamp's 20 s are up"));
    }

    puts("-- BATTERY MANNERS on the page: the cell in the shade under 10 %; on the dock the number breathes");
    {
        page_in_t c = base(); c.batt_pct = 8; c.cell_lean = true;
        page_show("a drag up"); page_tick(0.025f);
        frame(&c);
        /* the cell group is centred on 168 at row 134: the drop then the number; the number's first block is the drop's right edge + a gap */
        int found_shade = 0, found_bright = 0;
        for (int y = 134; y < 149; y++) for (int x = 150; x < 220; x++) { if (fb[y * W + x] == ink(sh, 1.0f)) found_shade++; if (fb[y * W + x] == ink(sn, 1.0f)) found_bright++; }
        check("lean: the cell's number in the shade, none of it in the shine", found_shade > 0 && found_bright == 0);
        c.cell_lean = false; c.charging = true; c.batt_pct = 40;
        frame(&c);
        uint16_t a = 0; for (int y = 134; y < 149 && !a; y++) for (int x = 186; x < 220 && !a; x++) if (fb[y * W + x] && fb[y * W + x] != ink(&GEL_PAL_BLUE.body, 1.0f)) a = fb[y * W + x];
        for (int i = 0; i < 40; i++) page_tick(0.025f);        /* a second on: half the breath */
        frame(&c);
        uint16_t b = 0; for (int y = 134; y < 149 && !b; y++) for (int x = 186; x < 220 && !b; x++) if (fb[y * W + x] && fb[y * W + x] != ink(&GEL_PAL_BLUE.body, 1.0f)) b = fb[y * W + x];
        check("on the dock: the number's ink a second later is another (it breathes)", a && b && a != b);
        c.charging = false; frame(&c);
        uint16_t d = 0; for (int y = 134; y < 149 && !d; y++) for (int x = 186; x < 220 && !d; x++) if (fb[y * W + x] && fb[y * W + x] != ink(&GEL_PAL_BLUE.body, 1.0f)) d = fb[y * W + x];
        check("off the dock: the shine, steady", d == ink(sn, 1.0f));
        page_hide("the test"); page_tick(0.025f);
    }

    puts("-- THE SETTINGS PAGE: the rows, the state at the right, a tap on a row is the row's (the 8 s again), off the rows the close; THE SLIME ROW first");
    {
        /* the font's own counts, blocks x 9 px at 3 px: S 11 E 10 T 7 I 9 N 11 G 11 '1' 8 -> "SETTINGS 1" 85; L 7 A 10 -> "LEASE" 48;
         * O 12 N 11 -> "ON" 23; F 8 -> "OFF" 28; Q 11 U 11 M 12 -> "QUIET TIME" 11+11+9+10+7+0+7+9+12+10 = 86; "07:30" 12+7+2+11+12 = 44;
         * R 10 -> "RESTING" 69; W 13 C 9 K 9 -> "WATER LOCK" 13+10+7+10+10+0+7+12+9+9 = 87; "LINK NOW" 7+9+11+9+0+11+12+13 = 72;
         * THE SLIME ROW: "SLIME" 11+7+9+12+10 = 49; H 11 -> "HERE" 11+10+10+10 = 41; W 12 Y 7 -> "AWAY" 10+12+10+7 = 39.
         * MORE SETTINGS: "WIFI" 12+9+8+9 = 38; D 10 -> "HOLD" 11+12+7+10 = 40; "AUTO" 10+11+7+12 = 40; "NETWORK" 11+10+7+12+12+10+10 = 72;
         * "CAMPUS-AIR" 9+10+12+10+11+11+3+10+9+10 = 95; "ADDRESS" 10+10+10+10+10+11+11 = 72; "10.0.0.5" 58; B 10 -> "BLUETOOTH" 10+7+11+10+7+12+12+7+11 = 87;
         * "--" 6; "BRIGHTNESS" 10+10+9+11+11+7+11+10+11+11 = 101; "MID" 12+9+10 = 31; "LOW" 7+12+12 = 31; "HIGH" 11+9+11+11 = 42;
         * P 10 -> "LIT SPELL" 7+9+7+0+11+10+10+7+7 = 68; "LONG" 7+12+11+11 = 41; "SHORT" 11+11+12+10+7 = 51; "LAMP" 7+10+12+10 = 39;
         * "CLOCK" 9+7+12+9+10 = 47; '2' 11 '4' 9 -> "24H" 11+9+11 = 31; "12H" 8+11+11 = 30; "QUIET FROM" 11+11+9+10+7+0+8+10+12+12 = 90;
         * "23:00" 11+11+2+12+12 = 48; "QUIET TO" 11+11+9+10+7+0+7+12 = 67; "07:00" 12+7+2+12+12 = 45; "11 PM" 8+8+0+10+12 = 38; "7 AM" 7+0+10+12 = 29;
         * ABOUT: "ABOUT" 10+10+12+11+7 = 50; "BUILD" 10+11+9+7+10 = 47; "a4c6ca6+" (lowercase = the uppercase set, + 5) 10+9+9+12+9+10+12+5 = 76;
         * "IP" 9+10 = 19; "HEAP" 11+10+10+10 = 41; "123K" 8+11+11+10 = 40; "CELL" 9+10+7+7 = 33; V 9 -> "4120MV" 9+8+11+12+12+9 = 61;
         * "UP" 11+10 = 21; '3' 11 -> "3H12M" 11+11+8+11+12 = 53; "STEPS" 11+7+10+10+11 = 49; "4213" 9+11+8+11 = 39 */
        const uint16_t BR = ink(sn, 1.0f), DM = ink(sn, 0.75f), SH = ink(sh, 1.0f), BD = ink(&GEL_PAL_BLUE.body, 1.0f);
        auto count = [&](int y0, int y1, uint16_t c) { int n = 0; for (int y = y0; y < y1; y++) for (int x = 0; x < W; x++) if (fb[y * W + x] == c) n++; return n; };
        page_in_t c = base();
        c.set_here = true; c.set_lease = true; c.set_quiet = true; c.set_face = true; c.set_lock = false;
        c.net_ssid = "CAMPUS-AIR"; c.net_ip = "10.0.0.5"; c.link_word = "RESTING"; c.radio_on = true;
        c.bt_word = "--"; c.bright = 1; c.spell_short = false; c.bumps_on = true; c.h12 = false; c.quiet_from = 23; c.quiet_to = 7; c.quiet_desk = false;
        c.build = "a4c6ca6+"; c.heap_free = 123 * 1024 + 5; c.up_s = 3 * 3600 + 12 * 60 + 9; c.cell_mv = 4120;
        frame_on(PAGE_SETTINGS, &c);
        ppm("/tmp/page_settings1.ppm");
        check("settings 1/6: its name", !strcmp(page_name(page_which()), "settings 1/6"));
        check("the title SETTINGS 1 at the top, dimmed (85 blocks x 9)", count(38, 53, DM) == 85 * 9 && count(38, 53, BR) == 0);
        check("row 0: THE SLIME ROW -- SLIME dimmed at the left (49 x 9), HERE in the shine at the right (41 x 9) while the house holds him",
              count(66, 81, DM) == 49 * 9 && count(66, 81, BR) == 41 * 9 && count(66, 81, SH) == 0);
        check("row 1: LEASE (48 x 9) ON (23 x 9), nothing in the shade", count(100, 115, DM) == 48 * 9 && count(100, 115, BR) == 23 * 9 && count(100, 115, SH) == 0);
        check("row 2: QUIET TIME (86 x 9) ON", count(134, 149, DM) == 86 * 9 && count(134, 149, BR) == 23 * 9);
        check("row 3: FACE ON; no fifth row (ALARM lives on page 2 now)", count(168, 183, BR) == 23 * 9 && lit_in(0, 194, W, H) == 0);
        check("every lit pixel inside the rings (the planted ring pixel aside)", (fb[4 * W + 120] = 0, all_within(112.0f)));
        c.set_here = false; c.set_lease = false; frame(&c);
        check("the house empty: AWAY in the shade (39 x 9) on the SLIME row, none in the shine", count(66, 81, SH) == 39 * 9 && count(66, 81, BR) == 0);
        check("LEASE off: OFF in the shade (28 x 9), none in the shine", count(100, 115, SH) == 28 * 9 && count(100, 115, BR) == 0);
        c.set_here = true; c.set_lease = true;

        /* the tap: on a row, the row's action; the row lit with a bar under it; the 8 s again */
        for (int i = 0; i < 200; i++) page_tick(0.025f);        /* 5 s in */
        check("5 s in: under 3 s left", page_left_s() < 3.1f);
        int r = page_tap(120, 70); const char *l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a tap on the SLIME row: SET_SLIME (main: the lease's road, then the page closes for the theatre), the line says so, the 8 s again",
              r == SET_SLIME && page_left_s() > 7.9f && l && strstr(l, "a tap on SLIME"));
        frame(&c);
        check("the tapped row lit: SLIME in the shine now (49 + 41 blocks), a bar under it in the body colour",
              count(66, 81, BR) == (49 + 41) * 9 && count(66, 81, DM) == 0 && count(84, 86, BD) > 100);
        check("the other rows as they were", count(100, 115, DM) == 48 * 9);
        for (int i = 0; i < 25; i++) page_tick(0.025f);         /* 0.625 s: the highlight's 0.6 s gone */
        frame(&c);
        check("0.6 s later: the row dimmed again, the bar gone", count(66, 81, DM) == 49 * 9 && count(84, 86, BD) == 0);
        r = page_tap(120, 104); l = page_tick(0.025f);
        check("a tap on the LEASE row: SET_LEASE, the line says so", r == SET_LEASE && l && strstr(l, "a tap on LEASE"));
        check("the bands' edges: 57 and 90 are row 0 (SLIME), 91 is row 1 (LEASE), 56 is nothing",
              page_tap(120, 57) == SET_SLIME && page_tap(120, 90) == SET_SLIME && page_tap(120, 91) == SET_LEASE && page_tap(120, 56) == PAGE_ACT_NONE);
        check("rows 2 and 3: QUIET TIME and FACE", page_tap(120, 134) == SET_QUIET && page_tap(120, 170) == SET_FACE);
        check("a tap on the title, or under the last row, or off the chord: none (main closes the page)",
              page_tap(120, 30) == PAGE_ACT_NONE && page_tap(120, 200) == PAGE_ACT_NONE && page_tap(20, 70) == PAGE_ACT_NONE);
        for (int i = 0; i < 25; i++) page_tick(0.025f);

        /* page 2: WATER LOCK, LINK NOW (the link's word beside it), ALARM */
        page_next("a drag up"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_settings2.ppm");
        check("settings 2/6: its name, the title SETTINGS 2", !strcmp(page_name(page_which()), "settings 2/6") && count(38, 53, DM) == 88 * 9);   /* '2' is 11 blocks, '1' 8: 85 - 8 + 11 */
        check("row 0: WATER LOCK (87 x 9) OFF in the shade", count(66, 81, DM) == 87 * 9 && count(66, 81, SH) == 28 * 9);
        check("row 1: LINK NOW (72 x 9) with the link's word RESTING (69 x 9) dimmed at the right, nothing in the shine", count(100, 115, DM) == (72 + 69) * 9 && count(100, 115, BR) == 0);
        check("row 2: ALARM's hour 07:30 in the shine (44 x 9)", count(134, 149, BR) == 44 * 9);
        check("no status lines under the rows any more (they are SETTINGS 3's rows)", lit_in(0, 156, W, H) == 0);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        check("a tap on WATER LOCK: SET_LOCK; on LINK NOW: SET_LINK; on ALARM: SET_ALARM; under them: none",
              page_tap(120, 70) == SET_LOCK && page_tap(120, 104) == SET_LINK && page_tap(120, 138) == SET_ALARM && page_tap(120, 165) == PAGE_ACT_NONE);
        for (int i = 0; i < 25; i++) page_tick(0.025f);         /* the tapped ALARM row's highlight gone */
        c.alarm_min = -1; frame(&c);
        check("no alarm: OFF in the shade on the alarm's row", count(134, 149, SH) == 28 * 9 && count(134, 149, BR) == 0);
        c.alarm_min = 7 * 60 + 30;
        c.set_lock = true; frame(&c);
        check("locked: ON in the shine on WATER LOCK's row", count(66, 81, BR) == 23 * 9);
        c.set_lock = false;

        /* page 3 (MORE SETTINGS): WIFI, NETWORK, ADDRESS, BLUETOOTH */
        page_next("a drag up"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_settings3.ppm");
        check("settings 3/6: its name, the title SETTINGS 3", !strcmp(page_name(page_which()), "settings 3/6") && count(38, 53, DM) == 88 * 9);   /* '3' is 11 blocks too */
        check("row 0: WIFI (38 x 9) AUTO dimmed (40 x 9) while the radio is on its budget", count(66, 81, DM) == (38 + 40) * 9 && count(66, 81, BR) == 0);
        check("row 1: NETWORK (72 x 9) and the SSID CAMPUS-AIR dimmed (95 x 9)", count(100, 115, DM) == (72 + 95) * 9);
        check("row 2: ADDRESS (72 x 9) and the ip 10.0.0.5 dimmed (58 x 9)", count(134, 149, DM) == (72 + 58) * 9);
        check("row 3: BLUETOOTH (87 x 9) and -- in the shade (6 x 9): no word given", count(168, 183, DM) == 87 * 9 && count(168, 183, SH) == 6 * 9);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        check("the taps: WIFI, NETWORK, ADDRESS, BLUETOOTH", page_tap(120, 70) == SET_WIFI && page_tap(120, 104) == SET_NETWORK && page_tap(120, 138) == SET_ADDRESS && page_tap(120, 172) == SET_BT);
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        c.wifi_hold = true; c.net_ssid = ""; c.net_ip = ""; c.radio_on = false; frame(&c);
        check("WIFI HOLD in the shine (40 x 9); the radio off: NETWORK reads OFF in the shade (28 x 9), ADDRESS -- (6 x 9)",
              count(66, 81, BR) == 40 * 9 && count(100, 115, SH) == 28 * 9 && count(134, 149, SH) == 6 * 9);
        c.radio_on = true; frame(&c);
        check("the radio up but not joined: NETWORK reads JOINING (dimmed)", count(100, 115, SH) == 0 && count(100, 115, DM) > 72 * 9);
        /* BLUETOOTH (ruling 58): OFF in the shade; `ON --` dimmed; PHONE in the shine -- and the five-glyph word is NOT cut by row 3's chord (six would be: `ON  --` lost its last dash) */
        c.bt_word = "OFF"; frame(&c);
        check("BLUETOOTH OFF: the word in the shade, the label alone dimmed", count(168, 183, SH) > 0 && count(168, 183, DM) == 87 * 9 && count(168, 183, BR) == 0);
        c.bt_word = "ON --"; frame(&c);
        int on_dash = count(168, 183, DM);
        check("BLUETOOTH ON --: dimmed beside the label, nothing in the shade or the shine", on_dash > 87 * 9 && count(168, 183, SH) == 0 && count(168, 183, BR) == 0);
        c.bt_word = "ON -"; frame(&c);
        check("the five glyphs of ON -- fit the chord beside BLUETOOTH (one more dash than ON - lights more)", count(168, 183, DM) < on_dash);
        c.bt_word = "ON  --"; frame(&c);
        check("six glyphs would be cut (ON  -- lights no more than ON --): the row keeps five", count(168, 183, DM) <= on_dash);
        c.bt_word = "PHONE"; frame(&c);
        check("BLUETOOTH PHONE: in the shine, the label alone dimmed", count(168, 183, BR) > 0 && count(168, 183, DM) == 87 * 9 && count(168, 183, SH) == 0);
        /* THE AIR DOOR and THE PAIRING WINDOW (2026-10-06): DESK / BOTH in the shine; the window's count `42 S` in the shine, as the PULSE row counts */
        c.bt_word = "DESK"; frame(&c);
        check("BLUETOOTH DESK: in the shine, the label alone dimmed", count(168, 183, BR) > 0 && count(168, 183, DM) == 87 * 9 && count(168, 183, SH) == 0);
        c.bt_word = "BOTH"; frame(&c);
        check("BLUETOOTH BOTH: in the shine, the label alone dimmed", count(168, 183, BR) > 0 && count(168, 183, DM) == 87 * 9 && count(168, 183, SH) == 0);
        c.bt_word = "42 S"; frame(&c);
        int pair_count = count(168, 183, BR);
        check("BLUETOOTH pairing: `42 S` in the shine, nothing in the shade", pair_count > 0 && count(168, 183, DM) == 87 * 9 && count(168, 183, SH) == 0);
        c.bt_word = "9 S"; frame(&c);
        check("the count shrinks with the number (9 S lights less than 42 S)", count(168, 183, BR) < pair_count && count(168, 183, BR) > 0);
        c.wifi_hold = false; c.net_ssid = "CAMPUS-AIR"; c.net_ip = "10.0.0.5"; c.bt_word = "--";
        {   /* a long SSID is cut to what the chord leaves beside the label */
            page_in_t lg = c; lg.net_ssid = "A-VERY-LONG-NETWORK-NAME-INDEED"; frame(&lg);
            check("a long SSID: cut, every pixel still inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)) && count(100, 115, DM) > 72 * 9);
        }

        /* page 4: BRIGHTNESS, LIT SPELL, LAMP */
        page_next("a drag up"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_settings4.ppm");
        check("settings 4/6: its name, the title SETTINGS 4", !strcmp(page_name(page_which()), "settings 4/6") && count(38, 53, DM) == 86 * 9);   /* '4' is 9 blocks */
        check("row 0: BRIGHTNESS (101 x 9) MID in the shine (31 x 9)", count(66, 81, DM) == 101 * 9 && count(66, 81, BR) == 31 * 9);
        check("row 1: LIT SPELL (68 x 9) LONG (41 x 9)", count(100, 115, DM) == 68 * 9 && count(100, 115, BR) == 41 * 9);
        check("row 2: LAMP (39 x 9), no state (an action)", count(134, 149, DM) == 39 * 9 && count(134, 149, BR) == 0);
        /* THE BUMPS (2026-09-23): the fourth row, BUMPS ON in the shine -- under LAMP, the three above untouched; nothing under it */
        check("row 3: BUMPS (54 x 9) ON in the shine (23 x 9); nothing under it", count(168, 183, DM) == 54 * 9 && count(168, 183, BR) == 23 * 9 && lit_in(0, 194, W, H) == 0);
        check("the taps: BRIGHTNESS, LIT SPELL, LAMP, BUMPS; under them none", page_tap(120, 70) == SET_BRIGHT && page_tap(120, 104) == SET_SPELL && page_tap(120, 138) == SET_LAMP && page_tap(120, 172) == SET_BUMPS && page_tap(120, 206) == PAGE_ACT_NONE);
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        c.bumps_on = false; frame(&c);
        check("BUMPS OFF in the shade (28 x 9), nothing in the shine on the row", count(168, 183, SH) == 28 * 9 && count(168, 183, BR) == 0);
        c.bumps_on = true;
        c.bright = 0; c.spell_short = true; frame(&c);
        check("LOW (31 x 9) and SHORT (51 x 9)", count(66, 81, BR) == 31 * 9 && count(100, 115, BR) == 51 * 9);
        c.bright = 2; frame(&c);
        check("HIGH (42 x 9)", count(66, 81, BR) == 42 * 9);
        c.bright = 1; c.spell_short = false;

        /* page 5: CLOCK, QUIET FROM, QUIET TO */
        page_next("a drag up"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_settings5.ppm");
        check("settings 5/6: its name, the title SETTINGS 5", !strcmp(page_name(page_which()), "settings 5/6") && count(38, 53, DM) == 88 * 9);   /* '5' is 11 blocks */
        check("row 0: CLOCK (47 x 9) 24H (31 x 9)", count(66, 81, DM) == 47 * 9 && count(66, 81, BR) == 31 * 9);
        check("row 1: QUIET FROM (90 x 9) 23:00 in the shine (48 x 9)", count(100, 115, DM) == 90 * 9 && count(100, 115, BR) == 48 * 9);
        check("row 2: QUIET TO (67 x 9) 07:00 (45 x 9)", count(134, 149, DM) == 67 * 9 && count(134, 149, BR) == 45 * 9);
        /* THE PULSE ROW (2026-10-03): the fourth row, under QUIET TO; never read = the label alone (an action, as LAMP); nothing under it */
        check("row 3: PULSE, its label dimmed, no value yet; nothing under it", count(168, 183, DM) > 0 && count(168, 183, BR) == 0 && count(168, 183, SH) == 0 && lit_in(0, 194, W, H) == 0);
        check("the taps: CLOCK, QUIET FROM, QUIET TO, PULSE; under them none", page_tap(120, 70) == SET_CLOCK && page_tap(120, 104) == SET_QFROM && page_tap(120, 138) == SET_QTO && page_tap(120, 172) == SET_PULSE && page_tap(120, 206) == PAGE_ACT_NONE);
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        {
            int label = count(168, 183, DM);
            c.pulse_left_s = 24; frame(&c);
            int reading = count(168, 183, BR);
            check("reading: `24 S` in the shine beside the label", reading > 0 && count(168, 183, DM) == label);
            /* THE STILL SPOT (2026-10-08): the LEDs wait for the wrist -- `STILL` in the shade, over the count (the count is the whole length while it waits) */
            c.pulse_still = true; frame(&c);
            check("waiting: `STILL` in the shade, no count", count(168, 183, SH) > 0 && count(168, 183, BR) == 0 && count(168, 183, DM) == label);
            c.pulse_still = false; frame(&c);
            check("the wrist still: the count is back in the shine", count(168, 183, BR) == reading && count(168, 183, SH) == 0);
            c.pulse_left_s = 0; c.pulse_bpm = 68; frame(&c);
            int bpm = count(168, 183, BR);
            check("landed: `68 BPM` in the shine (wider than the count)", bpm > reading);
            c.pulse_bpm = 0; c.pulse_fail = true; frame(&c);
            check("no clean pulse: `NONE` in the shade, nothing in the shine", count(168, 183, SH) > 0 && count(168, 183, BR) == 0);
            int none = count(168, 183, SH);
            /* THE PERFUSION INDEX (2026-10-14): the strap was the cause -- `THIN` in the shade where NONE was, one word, no number */
            c.pulse_thin = true; frame(&c);
            check("the strap: `THIN` in the shade (a different word, nothing in the shine)", count(168, 183, SH) > 0 && count(168, 183, SH) != none && count(168, 183, BR) == 0);
            c.pulse_thin = false; c.pulse_fail = false; c.pulse_absent = true; frame(&c);
            check("no sensor: `--` in the shade", count(168, 183, SH) > 0 && count(168, 183, BR) == 0);
            c.pulse_absent = false; frame(&c);
            check("every lit pixel inside the rings with the fourth row", (fb[4 * W + 120] = 0, all_within(112.0f)));
        }
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        c.quiet_desk = true; frame(&c);
        check("the desk's grain ruling: both hours in the shade (48 + 45), none in the shine", count(100, 115, SH) == 48 * 9 && count(134, 149, SH) == 45 * 9 && count(100, 115, BR) == 0 && count(134, 149, BR) == 0);
        c.quiet_desk = false;

        /* SETTINGS 6, THE HIDDEN SWITCHES (T15, 2026-10-12): PULSE AUTO, DOCK HAND, EAR GEAR, FORGET -- words only; FORGET's two taps.
         * The font's counts: "SETTINGS 6" 77 + 12 = 89; "PULSE AUTO" 10+11+7+11+10+0+10+11+7+12 = 89; "DOCK HAND" 10+12+9+10+0+11+10+11+10 = 83;
         * "EAR GEAR" 10+10+10+0+11+10+10+10 = 71; "FULL" 8+11+7+7 = 33; "FORGET" 8+12+10+11+10+7 = 58; "PHONE" 10+11+12+11+10 = 54;
         * "NONE" 11+12+11+10 = 44; "SURE?" 11+11+10+10+6 = 48 (the `?` glyph is this page's) */
        page_next("a drag up"); page_tick(0.025f);
        c.pauto_on = true; c.dockhand_on = false; c.ear_gear = 240; c.ble_bonded = false;
        frame(&c);
        ppm("/tmp/page_settings6.ppm");
        check("settings 6/6: its name, the title SETTINGS 6 (89 x 9)", !strcmp(page_name(page_which()), "settings 6/6") && count(38, 53, DM) == 89 * 9);
        check("row 0: PULSE AUTO (89) ON (23)", count(66, 81, DM) == 89 * 9 && count(66, 81, BR) == 23 * 9);
        check("row 1: DOCK HAND (83) OFF in the shade (28)", count(100, 115, DM) == 83 * 9 && count(100, 115, SH) == 28 * 9 && count(100, 115, BR) == 0);
        check("row 2: EAR GEAR (71) FULL dimmed (33): the gear off", count(134, 149, DM) == (71 + 33) * 9 && count(134, 149, BR) == 0);
        check("row 3: FORGET (58) NONE in the shade (44): no bond; nothing under it", count(168, 183, DM) == 58 * 9 && count(168, 183, SH) == 44 * 9 && lit_in(0, 194, W, H) == 0);
        check("no digits: words only", count(0, H, BR) == 23 * 9);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        c.ear_gear = 80; frame(&c);
        check("the low gear: LOW in the shine (31)", count(134, 149, BR) == 31 * 9 && count(134, 149, DM) == 71 * 9);
        c.ear_gear = 160; frame(&c);
        check("the middle gear: MID (31)", count(134, 149, BR) == 31 * 9);
        c.ble_bonded = true; frame(&c);
        check("a bond: FORGET PHONE dimmed (58 + 54), nothing in the shade", count(168, 183, DM) == (58 + 54) * 9 && count(168, 183, SH) == 0);
        check("the taps: PULSE AUTO, DOCK HAND, EAR GEAR; under the rows none", page_tap(120, 70) == SET_PAUTO && page_tap(120, 104) == SET_DOCKHAND && page_tap(120, 138) == SET_EARGEAR && page_tap(120, 206) == PAGE_ACT_NONE);
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        r = page_tap(120, 172); l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a tap on FORGET: armed -- PAGE_ACT_KEEP (nothing for main), the line says SURE?, the 8 s again", r == PAGE_ACT_KEEP && page_forget_armed() && page_left_s() > 7.9f && l && strstr(l, "SURE?"));
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_settings6_sure.ppm");
        check("armed: SURE? in the shine (48), the label dimmed (58)", count(168, 183, BR) == 48 * 9 && count(168, 183, DM) == 58 * 9);
        { char st6[160]; page_status(st6, sizeof st6); check("`page` says FORGET armed", strstr(st6, "FORGET armed") != NULL); }
        r = page_tap(120, 172); l = page_tick(0.025f);
        check("a second tap inside 3 s: SET_FORGET (main: `ble forget`), disarmed, the line says so", r == SET_FORGET && !page_forget_armed() && l && strstr(l, "a tap on FORGET"));
        for (int i = 0; i < 25; i++) page_tick(0.025f);
        frame(&c);
        check("after it: PHONE again (the bond is main's word, not the page's)", count(168, 183, BR) == 0 && count(168, 183, DM) == (58 + 54) * 9);
        page_tap(120, 172); page_tick(0.025f);
        {
            const char *dis = NULL;
            for (int i = 0; i < 130; i++) { const char *x = page_tick(0.025f); if (x) dis = x; }   /* 3.25 s */
            check("3 s without a second tap: disarmed, the line says so, the page still up", !page_forget_armed() && dis && strstr(dis, "disarmed") && page_up());
        }
        page_tap(120, 172); page_tick(0.025f);
        page_next("a drag up"); page_tick(0.025f);
        page_prev("a swipe right"); page_tick(0.025f);
        check("the page turning drops the arm", page_which() == PAGE_SETTINGS6 && !page_forget_armed());
        check("the rows' names: pauto, dockhand, eargear, forget", page_setting_by_name("pauto") == SET_PAUTO && page_setting_by_name("dockhand") == SET_DOCKHAND
              && page_setting_by_name("eargear") == SET_EARGEAR && page_setting_by_name("forget") == SET_FORGET && !strcmp(page_setting_label(SET_FORGET), "FORGET"));
        page_hide("the test"); page_tick(0.025f);

        /* THE BLUETOOTH PAGE (T16): after the notices; five lines in words, PAIRING the one row, a tap elsewhere keeps it.
         * The counts: "BLUETOOTH" 87; "PHONE" 54, "NEAR" 11+10+10+10 = 41, "AWAY" 39, "NONE" 44; "AIR DOOR" 10+9+10+0+10+12+12+10 = 73,
         * "OPEN" 12+10+10+11 = 43, "SHUT" 11+11+11+7 = 40; "EAR" 30, "ON" 23, "OFF" 28, "NO BOND" 11+12+0+10+12+11+10 = 66;
         * "PAIRING" 10+10+9+10+9+11+11 = 70; "NOTICES" 11+12+7+9+9+10+11 = 69, "SOME" 11+12+12+10 = 45; the rows at 62, 89, 116, 143, 170 */
        page_show_which(PAGE_BLE, "the test"); page_tick(0.025f);
        page_in_t b = c;
        b.ble_on = true; b.ble_bonded = false; b.ble_phone = false; b.ble_desk = false; b.ble_ear = false; b.ble_pairing = false; b.ble_notices = false;
        frame(&b);
        ppm("/tmp/page_ble.ppm");
        check("bluetooth: its name, the title BLUETOOTH (87 x 9)", !strcmp(page_name(page_which()), "bluetooth") && count(38, 53, DM) == 87 * 9);
        check("PHONE (54) NONE in the shade (44) at 62: no bond", count(62, 77, DM) == 54 * 9 && count(62, 77, SH) == 44 * 9);
        check("AIR DOOR (73) SHUT (40) at 89", count(89, 104, DM) == 73 * 9 && count(89, 104, SH) == 40 * 9);
        check("EAR (30) NO BOND in the shade (66) at 116: the switch on, nobody bonded", count(116, 131, DM) == 30 * 9 && count(116, 131, SH) == 66 * 9);
        check("PAIRING (70) SHUT (40) at 143", count(143, 158, DM) == 70 * 9 && count(143, 158, SH) == 40 * 9);
        check("NOTICES (69) NONE (44) at 170; nothing under it", count(170, 185, DM) == 69 * 9 && count(170, 185, SH) == 44 * 9 && lit_in(0, 186, W, H) == 0);
        check("all absent: nothing in the shine, no digits", count(0, H, BR) == 0);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        b.ble_bonded = true; b.ble_ear = true; frame(&b);
        check("a bond, the phone out of reach: AWAY dimmed (54 + 39); EAR ON in the shine (23)", count(62, 77, DM) == (54 + 39) * 9 && count(62, 77, SH) == 0 && count(116, 131, BR) == 23 * 9 && count(116, 131, SH) == 0);
        b.ble_phone = true; b.ble_desk = true; b.ble_pairing = true; b.ble_notices = true; frame(&b);
        ppm("/tmp/page_ble_all.ppm");
        check("everything there: NEAR (41), OPEN (43), OPEN (43), SOME (45) in the shine, nothing in the shade", count(62, 77, BR) == 41 * 9 && count(89, 104, BR) == 43 * 9 && count(143, 158, BR) == 43 * 9 && count(170, 185, BR) == 45 * 9 && count(0, H, SH) == 0);
        b.ble_on = false; b.ble_ear = false; frame(&b);
        check("the switch off: EAR OFF in the shade (28)", count(116, 131, SH) == 28 * 9 && count(116, 131, BR) == 0);
        for (int i = 0; i < 200; i++) page_tick(0.025f);
        r = page_tap(120, 150); l = page_tick(0.025f);
        printf("  %s\n", l ? l : "(no line)");
        check("a tap on PAIRING: SET_PAIRING (main: the window), the 8 s again, the line says so", r == SET_PAIRING && page_left_s() > 7.9f && l && strstr(l, "a tap on PAIRING"));
        frame(&b);
        check("the PAIRING row lit with its bar", count(143, 158, BR) == (70 + 43) * 9 && count(161, 163, BD) > 0);
        for (int i = 0; i < 200; i++) page_tick(0.025f);
        r = page_tap(120, 70); l = page_tick(0.025f);
        check("a tap on PHONE's line: SET_BLE (main logs the words), the page kept 8 s", r == SET_BLE && page_left_s() > 7.9f && l && strstr(l, "a tap on bluetooth"));
        check("a tap off the disc: none", page_tap(2, 120) == PAGE_ACT_NONE);
        check("the band's edges: 134 and 167 are PAIRING, 133 and 168 the disc", page_tap(120, 134) == SET_PAIRING && page_tap(120, 167) == SET_PAIRING && page_tap(120, 133) == SET_BLE && page_tap(120, 168) == SET_BLE);
        check("its names: pairing, ble", page_setting_by_name("pairing") == SET_PAIRING && page_setting_by_name("ble") == SET_BLE && !strcmp(page_setting_label(SET_PAIRING), "PAIRING"));
        page_show_which(PAGE_SETTINGS5, "the test"); page_tick(0.025f);   /* back on SETTINGS 5 for the 12H checks */
        c.h12 = true; frame(&c);
        check("12H: CLOCK reads 12H (30 x 9), the hours 11 PM (38 x 9) and 7 AM (29 x 9)", count(66, 81, BR) == 30 * 9 && count(100, 115, BR) == 38 * 9 && count(134, 149, BR) == 29 * 9);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));

        /* 12H on the tool page and the alarm's row: the page has the room for AM / PM */
        page_show("a drag up"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_up_12h.ppm");
        int hour12 = lit_in(40, 74, 200, 94);
        printf("  the 12H hour box lit %d (want 768)\n", hour12);
        check("the hour as 7:41 AM at 4 px (48 blocks x 16)", hour12 == 48 * 16);
        check("the alarm as 7:30 AM with the bell (more pixels than 07:30's row)", lit_in(40, 160, 200, 175) > alarm_px);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        page_show_which(PAGE_SETTINGS2, "the test"); page_tick(0.025f); frame(&c);
        check("the ALARM row in 12H: 7:30 AM in the shine (7+2+11+12+0+10+12 = 54 x 9)", count(134, 149, BR) == 54 * 9);
        c.h12 = false;

        /* ABOUT: read-only lines, a tap anywhere keeps it */
        page_show_which(PAGE_ABOUT, "the test"); page_tick(0.025f);
        frame(&c);
        ppm("/tmp/page_about.ppm");
        check("about: its name, the title ABOUT (50 x 9)", !strcmp(page_name(page_which()), "about") && count(38, 53, DM) == 50 * 9);
        check("BUILD (47 x 9) and a4c6ca6+ in the shine (76 x 9, the + a glyph: the tree was dirty) at 58", count(58, 73, DM) == 47 * 9 && count(58, 73, BR) == 76 * 9);
        check("IP (19) and 10.0.0.5 (58) at 80", count(80, 95, DM) == 19 * 9 && count(80, 95, BR) == 58 * 9);
        check("HEAP (41) and 123K (40) at 102", count(102, 117, DM) == 41 * 9 && count(102, 117, BR) == 40 * 9);
        check("CELL (33) and 4120MV (61) at 124", count(124, 139, DM) == 33 * 9 && count(124, 139, BR) == 61 * 9);
        check("UP (21) and 3H12M (53) at 146", count(146, 161, DM) == 21 * 9 && count(146, 161, BR) == 53 * 9);
        check("STEPS (49) and 4213 (39) at 168", count(168, 183, DM) == 49 * 9 && count(168, 183, BR) == 39 * 9);
        check("BOX at 190, not asked: -- in the shade (2026-09-17: THE BOX'S STATUS row)", count(190, 205, DM) > 0 && count(190, 205, BR) == 0 && count(190, 205, SH) > 0);
        check("every lit pixel inside the rings", (fb[4 * W + 120] = 0, all_within(112.0f)));
        c.box = "KAPTOP 2 CLEAR"; frame(&c);
        ppm("/tmp/page_about_box.ppm");
        check("BOX at 190, asked: KAPTOP 2 CLEAR in the shine, cut to the chord", count(190, 205, BR) > 0 && count(190, 205, SH) == 0);
        check("every lit pixel inside the rings, the box row too", (fb[4 * W + 120] = 0, all_within(112.0f)));
        c.box = NULL; frame(&c);
        for (int i = 0; i < 200; i++) page_tick(0.025f);
        r = page_tap(120, 120); l = page_tick(0.025f);
        check("a tap anywhere on about: SET_ABOUT, the 8 s again, the line says so", r == SET_ABOUT && page_left_s() > 7.9f && l && strstr(l, "a tap on about"));
        check("a tap off the disc: none", page_tap(2, 120) == PAGE_ACT_NONE);
        page_in_t ab = c; ab.net_ip = ""; ab.cell_mv = 0; ab.up_s = 45; ab.build = ""; frame(&ab);
        check("no ip, no cell, 45 s up, no build: -- in the shade for the ip and the cell, 45S, -- for the build", count(80, 95, SH) == 6 * 9 && count(124, 139, SH) == 6 * 9 && count(146, 161, BR) == (9 + 11 + 11) * 9 && count(58, 73, BR) == 6 * 9);
        ab.up_s = 12 * 60 + 9; frame(&ab);
        check("12 m 9 s up: 12M09S (8 + 11 + 12 + 12 + 12 + 11)", count(146, 161, BR) == (8 + 11 + 12 + 12 + 12 + 11) * 9);
        page_hide("the test"); page_tick(0.025f);

        /* not a settings page, or under the lamp: no row */
        page_show("a drag up"); page_tick(0.025f);
        check("a tap on the tool page is no row (main closes it)", page_tap(120, 70) == PAGE_ACT_NONE);
        page_show_which(PAGE_SETTINGS, "the test"); page_lamp("a long press"); page_tick(0.025f);
        check("under the lamp: no row (the tap puts the lamp out)", page_tap(120, 70) == PAGE_ACT_NONE);
        page_hide("the test"); page_tick(0.025f);
        check("the rows' names round-trip for `set <row>`", page_setting_by_name("slime") == SET_SLIME && page_setting_by_name("lease") == SET_LEASE
              && page_setting_by_name("link") == SET_LINK && page_setting_by_name("alarm") == SET_ALARM && page_setting_by_name("wifi") == SET_WIFI
              && page_setting_by_name("bright") == SET_BRIGHT && page_setting_by_name("qto") == SET_QTO && page_setting_by_name("about") == SET_ABOUT
              && page_setting_by_name("bumps") == SET_BUMPS && !strcmp(page_setting_label(SET_BUMPS), "BUMPS")
              && page_setting_by_name("pulse") == SET_PULSE && !strcmp(page_setting_label(SET_PULSE), "PULSE")
              && !strcmp(page_setting_name(SET_QUIET), "quiet") && !strcmp(page_setting_label(SET_QFROM), "QUIET FROM")
              && page_setting_by_name("dnd") == -1 && page_setting_name(SET_N) == NULL && page_setting_label(SET_N) == NULL);
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
