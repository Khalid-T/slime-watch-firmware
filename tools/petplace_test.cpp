/* THE PET'S PLACE on the laptop (petplace.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/petplace_test tools/petplace_test.cpp -x c src/petplace.c -lm && /tmp/petplace_test
 * The frame (each wall's quarter turn is gesture.cpp's table); the
 * thirds on every wall against his painted box (the dome 156 x 96, its
 * base on the rim): the crown's third and above it the crown, the
 * middle the flank with a side, the last third and below it the base
 * with a side; the dead band on his centre line (no side); no box =
 * the crown; the press (the flank's shallower); the roads: the flank's
 * lean away from the finger up by 0.3 s, back to 0 by 1 s, the base's
 * slide 2 px away out by 0.08 s, home through one settle by 0.5 s, the
 * other road 0 on each, nothing with no side; done at 1 s. */
#include "petplace.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b) { return fabsf(a - b) < 0.005f; }

/* the box on each wall: the dome 156 wide along the wall, 96 tall, the base centre 106 px from the face's centre */
static void box_on(int wall, int *bx, int *by, int *bw, int *bh)
{
    switch (wall & 3) {
    default: *bx = 42;  *by = 130; *bw = 156; *bh = 96;  break;   /* bottom: y 130..225 */
    case 1:  *bx = 130; *by = 42;  *bw = 96;  *bh = 156; break;   /* right:  x 130..225 */
    case 2:  *bx = 42;  *by = 14;  *bw = 156; *bh = 96;  break;   /* top:    y 14..109 */
    case 3:  *bx = 14;  *by = 42;  *bw = 96;  *bh = 156; break;   /* left:   x 14..109 */
    }
}
static petplace_t at(int wall, int x, int y)
{
    int bx, by, bw, bh; box_on(wall, &bx, &by, &bw, &bh);
    return petplace_of(x, y, wall, bx, by, bw, bh);
}

int main(void)
{
    puts("-- the frame (gesture.cpp's own table)");
    int a, d;
    petplace_frame(0, 10, 20, &a, &d); CHECK(a == 10 && d == 20, "bottom: the screen's frame (%d, %d)", a, d);
    petplace_frame(1, 10, 20, &a, &d); CHECK(a == -20 && d == 10, "right: down is screen right, his right screen up (%d, %d)", a, d);
    petplace_frame(2, 10, 20, &a, &d); CHECK(a == -10 && d == -20, "top: down is screen up (%d, %d)", a, d);
    petplace_frame(3, 10, 20, &a, &d); CHECK(a == 20 && d == -10, "left: down is screen left, his right screen down (%d, %d)", a, d);

    puts("-- the thirds on the bottom wall");
    petplace_t p;
    p = at(0, 120, 135); CHECK(p.place == PETPLACE_CROWN && p.side == 0, "a tap on his crown: the crown (depth %d)", p.depth);
    p = at(0, 120, 161); CHECK(p.place == PETPLACE_CROWN, "the last px of the crown's third (depth %d) is the crown", p.depth);
    p = at(0, 120, 162); CHECK(p.place == PETPLACE_FLANK && p.side == 0, "the first px of the middle third (depth %d) is the flank; dead centre has no side", p.depth);
    p = at(0, 160, 170); CHECK(p.place == PETPLACE_FLANK && p.side == 1, "a tap in his right flank: the flank, his right (along %d)", p.along);
    p = at(0, 80, 170);  CHECK(p.place == PETPLACE_FLANK && p.side == -1, "a tap in his left flank: the flank, his left (along %d)", p.along);
    p = at(0, 128, 170); CHECK(p.place == PETPLACE_FLANK && p.side == 0, "8 px off the centre line: inside the dead band, no side");
    p = at(0, 129, 170); CHECK(p.place == PETPLACE_FLANK && p.side == 1, "9 px off: a side");
    p = at(0, 120, 215); CHECK(p.place == PETPLACE_BASE && p.side == 0, "a tap on his base, centre: the base, no side (depth %d)", p.depth);
    p = at(0, 160, 215); CHECK(p.place == PETPLACE_BASE && p.side == 1, "his base, right: the base, his right");
    p = at(0, 80, 215);  CHECK(p.place == PETPLACE_BASE && p.side == -1, "his base, left: the base, his left");
    p = at(0, 120, 100); CHECK(p.place == PETPLACE_CROWN, "above the box (within the far tap's 60 px): the crown");
    p = at(0, 120, 236); CHECK(p.place == PETPLACE_BASE, "below the box (the rim): the base");
    p = petplace_of(120, 170, 0, 0, 0, 0, 0); CHECK(p.place == PETPLACE_CROWN && p.side == 0, "no box painted: the crown, the pet as today");

    puts("-- the thirds on the other walls");
    p = at(1, 140, 120); CHECK(p.place == PETPLACE_CROWN, "right wall: a tap at his crown (screen x 140) is the crown");
    p = at(1, 175, 80);  CHECK(p.place == PETPLACE_FLANK && p.side == 1, "right wall: screen up is his right flank");
    p = at(1, 175, 160); CHECK(p.place == PETPLACE_FLANK && p.side == -1, "right wall: screen down is his left flank");
    p = at(1, 215, 80);  CHECK(p.place == PETPLACE_BASE && p.side == 1, "right wall: his base, his right");
    p = at(2, 120, 100); CHECK(p.place == PETPLACE_CROWN, "top wall: a tap near the centre of the glass is his crown");
    p = at(2, 80, 60);   CHECK(p.place == PETPLACE_FLANK && p.side == 1, "top wall: screen left is his right flank");
    p = at(2, 160, 60);  CHECK(p.place == PETPLACE_FLANK && p.side == -1, "top wall: screen right is his left flank");
    p = at(2, 120, 20);  CHECK(p.place == PETPLACE_BASE && p.side == 0, "top wall: the rim is his base");
    p = at(3, 100, 120); CHECK(p.place == PETPLACE_CROWN, "left wall: screen x 100 is his crown");
    p = at(3, 60, 160);  CHECK(p.place == PETPLACE_FLANK && p.side == 1, "left wall: screen down is his right flank");
    p = at(3, 60, 80);   CHECK(p.place == PETPLACE_FLANK && p.side == -1, "left wall: screen up is his left flank");
    p = at(3, 20, 80);   CHECK(p.place == PETPLACE_BASE && p.side == -1, "left wall: his base, his left");

    puts("-- the press");
    CHECK(near(petplace_press_k(PETPLACE_CROWN), 1.0f), "the crown's press is the pet's");
    CHECK(near(petplace_press_k(PETPLACE_FLANK), PETPLACE_FLANK_PRESS_K), "the flank's press is %.2f of it", PETPLACE_FLANK_PRESS_K);
    CHECK(near(petplace_press_k(PETPLACE_BASE), 1.0f), "the base's press is the pet's");

    puts("-- the flank's lean away");
    float a1, s;
    petplace_t fr = at(0, 160, 170);   /* the finger on his right flank */
    petplace_fx(&fr, 0.0f, &a1, &s);  CHECK(near(a1, 0) && near(s, 0), "at the tap: nothing yet");
    petplace_fx(&fr, 0.15f, &a1, &s); CHECK(near(a1, -0.06f), "0.15 s: halfway in, a1 %.3f (away from his right = negative)", a1);
    petplace_fx(&fr, 0.30f, &a1, &s); CHECK(near(a1, -PETPLACE_LEAN) && near(s, 0), "0.3 s: the full lean %.2f, no slide", a1);
    petplace_fx(&fr, 0.65f, &a1, &s); CHECK(near(a1, -0.06f), "0.65 s: easing back, a1 %.3f", a1);
    petplace_fx(&fr, 1.00f, &a1, &s); CHECK(near(a1, 0), "1 s: upright");
    petplace_t fl = at(0, 80, 170);    /* the finger on his left flank */
    petplace_fx(&fl, 0.30f, &a1, &s); CHECK(near(a1, PETPLACE_LEAN), "the left flank: the lean the other way, a1 %.2f", a1);
    petplace_t fc = at(0, 120, 170);
    petplace_fx(&fc, 0.30f, &a1, &s); CHECK(near(a1, 0) && near(s, 0), "dead centre on the flank: no lean, no slide");

    puts("-- the base's slide away");
    petplace_t br = at(0, 160, 215);   /* the finger on his base, his right */
    petplace_fx(&br, 0.0f, &a1, &s);  CHECK(near(s, 0), "at the tap: in place");
    petplace_fx(&br, 0.08f, &a1, &s); CHECK(near(s, -PETPLACE_SLIDE_PX) && near(a1, 0), "0.08 s: out %.1f px toward his left (away), no lean", s);
    petplace_fx(&br, 0.29f, &a1, &s); CHECK(fabsf(s) < 0.05f, "0.29 s: back through his place (%.2f)", s);
    petplace_fx(&br, 0.395f, &a1, &s); CHECK(s > 0.2f && s < 0.5f, "0.4 s: a little past it, the settle (%.2f)", s);
    petplace_fx(&br, 0.50f, &a1, &s); CHECK(near(s, 0), "0.5 s: home");
    petplace_t bl = at(0, 80, 215);
    petplace_fx(&bl, 0.08f, &a1, &s); CHECK(near(s, PETPLACE_SLIDE_PX), "his left base: the slide toward his right (%.1f)", s);
    petplace_t cr = at(0, 120, 135);
    petplace_fx(&cr, 0.30f, &a1, &s); CHECK(near(a1, 0) && near(s, 0), "the crown: no road at all");

    puts("-- done");
    CHECK(!petplace_done(0.99f) && petplace_done(1.0f), "done at %.0f s, not before", PETPLACE_S);
    CHECK(!strcmp(petplace_name(PETPLACE_CROWN), "crown") && !strcmp(petplace_name(PETPLACE_FLANK), "flank") && !strcmp(petplace_name(PETPLACE_BASE), "base"), "the names");

    printf("petplace_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
