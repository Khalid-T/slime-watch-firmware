/* THE PET'S PLACE (design/watch-night-2026-10-12.md C1, THE OPERATOR
 * NIGHT): a tap is the pet wherever it lands on him, but a gel pressed
 * on its crown is not a gel poked in the flank. The tap's point is read
 * in HIS frame (the feed's own quarter turn: along his wall, + his
 * right; down onto him, + toward his base) against his painted box,
 * and his height is cut in thirds:
 *   the crown's third -- the pet as today (the flat press, the bounce);
 *   a flank            -- the press shallower (PETPLACE_FLANK_PRESS_K of
 *                         it) and a LEAN AWAY from the finger: a1
 *                         PETPLACE_LEAN the other way, in over
 *                         PETPLACE_LEAN_IN_S (the stroke's own in),
 *                         easing back by PETPLACE_S -- the stroke's lean
 *                         mechanism reversed (it leaned INTO the travel);
 *   his base's third   -- the press plus one tiny slide along the wall
 *                         away from the finger: PETPLACE_SLIDE_PX on the
 *                         shove's road at a fifth (out fast, back with
 *                         the shove's one settle).
 * A tap within PETPLACE_DEAD_PX of his centre line on the flank or the
 * base has no side: the press alone (nothing leans or slides from a
 * finger dead on the middle). The eyes glance at the finger first (the
 * far tap's gaze math, main's look_at, for PETPLACE_GLANCE_S) and then
 * the pet's face as ever. Never on the empty glass, never under a
 * costume (the act is the costume's), never a new shape: squash, a1,
 * slide and gaze are the whole vocabulary.
 *
 * Pure C, no Arduino: the tap, the wall and the box in; the place and
 * its side out; the two roads as functions of the seconds since the
 * pet. tools/petplace_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/petplace_test tools/petplace_test.cpp -x c src/petplace.c -lm && /tmp/petplace_test
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PETPLACE_LEAN           0.12f   /* a1 away from the finger on a flank (the stroke's 0.15 was INTO the travel; a poke is less than a stroke) */
#define PETPLACE_LEAN_IN_S      0.30f   /* the lean's way in: the stroke's own 0.3 (the press's own 0.3 s) */
#define PETPLACE_S              1.00f   /* the whole road: the lean eased back, the slide home, by a second */
#define PETPLACE_FLANK_PRESS_K  0.60f   /* the flank's press, of the crown's PET_SQUASH: a finger in the side sinks less than one on top */
#define PETPLACE_SLIDE_PX       2.0f    /* the base's slide along the wall away from the finger: the shove's 10 px at a fifth */
#define PETPLACE_SLIDE_OUT_S    0.08f   /* the shove's road: out this fast ... */
#define PETPLACE_SLIDE_S        0.50f   /* ... then home with its one settle by here */
#define PETPLACE_DEAD_PX        8       /* a tap this close to his centre line has no side: the press alone */
#define PETPLACE_GLANCE_S       0.40f   /* the glance at the finger before the pet's face (main's look_at; the peekaboo's own 0.4) */

typedef enum { PETPLACE_CROWN = 0, PETPLACE_FLANK = 1, PETPLACE_BASE = 2 } petplace_place_t;

typedef struct {
    int place;      /* petplace_place_t */
    int side;       /* +1 the finger on his right, -1 on his left, 0 on his centre line (crown: always 0) */
    int depth;      /* the tap's depth in his frame, px from the box's crown edge (the test's witness) */
    int along;      /* the tap's offset from his centre line in his frame, + his right (the test's witness) */
} petplace_t;

/* THE GEL'S FRAME (the feed's own, gesture.cpp's gel_frame): a screen
 * offset (dx, dy) as his (along, down); walls.h's order 0 bottom / 1
 * right / 2 top / 3 left */
void petplace_frame(int wall, int dx, int dy, int *along, int *down);

/* where on him the tap at (x, y) screen px landed, given the wall he
 * sits on and his painted box (gel_face_box: screen px; w 0 = no box,
 * the crown as today). Above the box is the crown, below it the base. */
petplace_t petplace_of(int x, int y, int wall, int bx, int by, int bw, int bh);

/* the press, of the crown's: 1 on the crown and the base, PETPLACE_FLANK_PRESS_K on a flank */
float petplace_press_k(int place);

/* the roads, `since_s` after the pet: the lean's a1 (signed: away from
 * the finger, 0 off the flank) and the slide's px along the wall
 * (signed: away from the finger, 0 off the base). Both 0 once
 * petplace_done. */
void petplace_fx(const petplace_t *p, float since_s, float *a1, float *slide_px);
bool petplace_done(float since_s);

/* "crown" / "flank" / "base" */
const char *petplace_name(int place);

#ifdef __cplusplus
}
#endif
