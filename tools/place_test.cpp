/* THE PLACE on the laptop (place.cpp, word.cpp and glance.cpp have no
 * board in them; the stub Arduino.h stands in for millis / Serial and
 * the clock is faked here):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/place_test tools/place_test.cpp src/place.cpp src/word.cpp src/glance.cpp src/theme.cpp -x c src/gel.c -lm && /tmp/place_test
 * The real gel (gel.c, the wrist's posture) composed on each of the
 * four walls, the real hour and the real bubble ("the desk waves from
 * across the room"): the hour at the top for three walls and at the
 * bottom while the gel is on the top wall (logged, and back); the word
 * opposite the gel, inward of the hour when they share the band; every
 * text pixel on black glass, never over the gel; no gel = the top; the
 * word alone = opposite, no inset; the same frame twice = no log; the
 * backstop against a fake gel filling half the glass (the next free
 * wall, the gel's own last) and the whole glass (the preferred after
 * all); a dismissed word has no box. Writes place_<wall>.ppm beside
 * itself (python3 tools/ppm2png.py). */
#include "../src/place.h"
#include "../src/word.h"
#include "../src/glance.h"
#include "../src/gel.h"
#include "../src/walls.h"
#include "../src/board.h"
#include "../src/app.h"
#include <Arduino.h>                     /* the stub: millis, Serial */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

uint32_t fake_ms = 5000; FakeSerial Serial;
bool clock_is_set(void) { return true; }
void clock_local(struct tm *out, uint32_t *epoch_out)
{
    memset(out, 0, sizeof *out);
    out->tm_hour = 7; out->tm_min = 41;
    if (epoch_out) *epoch_out = 1700000000u;
}

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t fb[SCREEN_W * SCREEN_H], gel_only[SCREEN_W * SCREEN_H];
static uint16_t cell[GEL_CELL_W * GEL_CELL_H];

static void gel_on(int wall)
{
    gel_cell_params_t p;
    memset(&p, 0, sizeof p);
    p.angle = gel_snap_angle(walls_phi(wall));
    p.squash = -0.15f;                     /* the wrist's posture */
    p.phase = (float)M_PI / 2.0f;
    p.eye_alpha = 1.0f;
    gel_raster_cell(cell, &GEL_PAL_BLUE, &p);
    gel_compose(fb, cell, walls_phi(wall), 0, 0);
    memcpy(gel_only, fb, sizeof fb);
}

/* text pixels = what the overlays changed; over the gel = changed where the gel was lit */
static void count_text(int *text_px, int *over_gel)
{
    *text_px = *over_gel = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        if (fb[i] != gel_only[i]) { (*text_px)++; if (gel_only[i]) (*over_gel)++; }
}
static bool all_on_disc(void)
{
    for (int y = 0; y < SCREEN_H; y++) for (int x = 0; x < SCREEN_W; x++) {
        if (fb[y * SCREEN_W + x] == gel_only[y * SCREEN_W + x]) continue;
        int dx2 = 2 * x + 1 - SCREEN_W, dy2 = 2 * y + 1 - SCREEN_H;
        if (dx2 * dx2 + dy2 * dy2 > SCREEN_W * SCREEN_W) return false;
    }
    return true;
}
static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint16_t v = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        unsigned char px[3] = { (unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63),
                                (unsigned char)((v & 31) * 255 / 31) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}
static bool meet(const place_box_t *a, const place_box_t *b)
{
    return a->w > 0 && b->w > 0 && a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

int main(void)
{
    place_t pl;
    word_set("the desk waves from across the room");
    word_glance();                                  /* a raise: the held word shows */
    glance_show();                                  /* ... and the hour */
    CHECK(word_showing() && glance_on(), "the word and the hour are on");

    puts("-- the real gel on each wall, the hour and the word both on");
    static const int opp[4] = { GEL_WALL_TOP, GEL_WALL_LEFT, GEL_WALL_BOTTOM, GEL_WALL_RIGHT };
    for (int w = 0; w < 4; w++) {
        place_reset();
        gel_on(w);
        place_decide(&pl, fb, SCREEN_W, SCREEN_H, w, true, glance_box, true, word_box);
        int want_hour = (w == GEL_WALL_TOP) ? GEL_WALL_BOTTOM : GEL_WALL_TOP;
        CHECK(pl.hour_at == want_hour, "gel on the %s: the hour at the %s", GEL_WALL_NAMES[w], GEL_WALL_NAMES[pl.hour_at]);
        CHECK(pl.word_at == opp[w], "gel on the %s: the word at the %s (opposite)", GEL_WALL_NAMES[w], GEL_WALL_NAMES[pl.word_at]);
        place_box_t hb, wb;
        glance_box(pl.hour_at, 0, &hb);
        word_box(pl.word_at, pl.word_inset, &wb);
        if (pl.word_at == pl.hour_at) CHECK(pl.word_inset > 0 && !meet(&hb, &wb), "  they share the %s: the word inset %d px under the hour, the boxes apart", GEL_WALL_NAMES[pl.word_at], pl.word_inset);
        else CHECK(pl.word_inset == 0, "  the word on its own wall: no inset");
        CHECK(place_box_clear(fb, SCREEN_W, SCREEN_H, &hb), "  the hour's box is clear of the gel");
        CHECK(place_box_clear(fb, SCREEN_W, SCREEN_H, &wb), "  the word's box is clear of the gel");
        CHECK(pl.log[0] != 0, "  logged: %s", pl.log);
        if (w == GEL_WALL_TOP) CHECK(strstr(pl.log, "the hour at the bottom") != NULL, "  the hour's one exception is said");
        glance_draw(fb, pl.hour_at);
        word_draw(fb, pl.word_at, pl.word_inset);
        int text_px, over;
        count_text(&text_px, &over);
        CHECK(text_px > 300 && over == 0, "  %d text px drawn, %d over the gel", text_px, over);
        CHECK(all_on_disc(), "  every text pixel on the disc");
        char path[64];
        snprintf(path, sizeof path, "place_%s.ppm", GEL_WALL_NAMES[w]);
        ppm(path);
        /* the same frame again: nothing changed, nothing logged */
        memcpy(fb, gel_only, sizeof fb);
        place_decide(&pl, fb, SCREEN_W, SCREEN_H, w, true, glance_box, true, word_box);
        CHECK(pl.log[0] == 0 && pl.hour_at == want_hour && pl.word_at == opp[w], "  the same frame again: the same places, no log");
    }

    puts("-- the hour's exception, and back");
    place_reset();
    gel_on(GEL_WALL_BOTTOM);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_BOTTOM, true, glance_box, false, word_box);
    CHECK(pl.hour_at == GEL_WALL_TOP && pl.log[0] == 0, "the gel home: the hour at the top, nothing to say");
    gel_on(GEL_WALL_TOP);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_TOP, true, glance_box, false, word_box);
    CHECK(pl.hour_at == GEL_WALL_BOTTOM && strstr(pl.log, "the hour at the bottom (the gel on the top wall)"), "the gel on the top wall: %s", pl.log);
    gel_on(GEL_WALL_RIGHT);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_RIGHT, true, glance_box, false, word_box);
    CHECK(pl.hour_at == GEL_WALL_TOP && strstr(pl.log, "the hour back at the top"), "the gel off it: %s", pl.log);

    puts("-- no gel; the word alone");
    place_reset();
    memset(fb, 0, sizeof fb); memset(gel_only, 0, sizeof gel_only);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_NONE, true, glance_box, true, word_box);
    CHECK(pl.hour_at == GEL_WALL_TOP && pl.word_at == GEL_WALL_TOP && pl.word_inset > 0, "no gel: both at the top, the word under the hour (inset %d)", pl.word_inset);
    place_reset();
    gel_on(GEL_WALL_BOTTOM);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_BOTTOM, false, glance_box, true, word_box);
    CHECK(pl.word_at == GEL_WALL_TOP && pl.word_inset == 0, "the word alone, the gel home: the top at its own radius");
    word_draw(fb, pl.word_at, pl.word_inset);
    { int t, o; count_text(&t, &o); CHECK(t > 300 && o == 0, "  drawn, none over the gel"); }

    puts("-- the backstop against a fake gel");
    place_reset();
    memset(fb, 0, sizeof fb);
    for (int y = 0; y < SCREEN_H / 2 + 30; y++) for (int x = 0; x < SCREEN_W; x++) fb[y * SCREEN_W + x] = 0x1234;   /* the top half and a bit: the top and both sides' rows are taken */
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_BOTTOM, false, glance_box, true, word_box);
    CHECK(pl.word_at == GEL_WALL_BOTTOM && strstr(pl.log, "opposite the gel would meet it"), "the top and the sides taken: the word at the %s (%s)", GEL_WALL_NAMES[pl.word_at], pl.log);
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_BOTTOM, true, glance_box, false, word_box);
    CHECK(pl.hour_at == GEL_WALL_BOTTOM, "the hour, the same: at the %s", GEL_WALL_NAMES[pl.hour_at]);
    place_reset();
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x1234;
    place_decide(&pl, fb, SCREEN_W, SCREEN_H, GEL_WALL_RIGHT, true, glance_box, true, word_box);
    CHECK(pl.hour_at == GEL_WALL_TOP && pl.word_at == GEL_WALL_LEFT, "nothing clear anywhere: the preferred after all (the hour at the %s, the word at the %s)",
          GEL_WALL_NAMES[pl.hour_at], GEL_WALL_NAMES[pl.word_at]);

    puts("-- a dismissed word has no box");
    CHECK(word_tap(), "the tap took the word");
    { place_box_t b; word_box(GEL_WALL_TOP, 0, &b); CHECK(b.w == 0 && b.h == 0, "the box is empty"); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
