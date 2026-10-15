/* THE PLACE: text never over the gel (Khalid 2026-09-10, with the four
 * walls: the word bubble drew straight across the gel's body when the
 * gel sat on the right wall and the bubble was nailed to the top).
 *
 * ONE rule, in one place, for every text the glass shows over the
 * creature -- the glance's hour (and the clockwork's number, which has
 * the hour's spot), and the word bubble:
 *
 *   - the WORD takes the wall OPPOSITE the gel (the top when there is
 *     no gel);
 *   - the HOUR stays pinned to the TOP (Khalid 23:15, 09-09: "the time
 *     shouldn't move, just the slime") -- EXCEPT when the gel is on the
 *     top wall, where it takes the bottom for that spell (with four
 *     walls this is the only case; logged);
 *   - the BACKSTOP: a text box that would still meet the gel (its real
 *     pixels, not its box -- the gel is 65 % of the glass and its box
 *     would evict the hour whenever the gel sat on a side wall) takes
 *     the next free wall: the two sides, then the gel's own; none free
 *     = the preferred wall after all (the bubble's plate cuts on the
 *     gel);
 *   - the hour and the word share the band opposite the gel: when both
 *     show on one wall the word sits INWARD of the hour (an inset along
 *     the wall's axis, PLACE_GAP between them), never over it;
 *   - a choice is STICKY: while the gel keeps its wall and the chosen
 *     place is still clear, it stays (the lean and the slide move the
 *     gel a few px a frame; the text must not flicker between walls).
 *
 * Plain C++ (no Arduino): the framebuffer the gel was composed into
 * (and nothing else yet), the gel's wall, which texts are on, and each
 * drawer's own box for a wall (a callback: the drawer knows its font
 * and its wrap) -> where each goes. tools/place_test.cpp proves it
 * against the real gel and the real drawers. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { int x, y, w, h; } place_box_t;
/* a drawer's box at a wall: ``at`` GEL_WALL_*, ``inset`` px toward the
 * centre from the drawer's own radius (0 = its usual spot) */
typedef void (*place_box_fn)(int at, int inset, place_box_t *out);

typedef struct {
    int  hour_at;       /* GEL_WALL_*: where the hour (and the clockwork's number) goes */
    int  word_at;       /* GEL_WALL_*: where the word bubble goes */
    int  word_inset;    /* px inward of the bubble's usual radius (stacked under the hour), else 0 */
    char log[112];      /* a line when a place changed this call, else "" */
} place_t;

#define PLACE_GAP 4     /* px between the hour's box and the word's when they share a wall */

/* the decision, once a frame after the gel is composed (fb holds the
 * gel alone; gel_wall GEL_WALL_NONE when none is drawn) */
void place_decide(place_t *out, const uint16_t *fb, int fb_w, int fb_h, int gel_wall,
                  bool hour_on, place_box_fn hour_box, bool word_on, place_box_fn word_box);
/* no lit pixel of fb inside the box (sampled every other pixel each
 * way: a block is 2 px or more) */
bool place_box_clear(const uint16_t *fb, int fb_w, int fb_h, const place_box_t *b);
/* the sticky memory forgotten (a test's, the dark house's) */
void place_reset(void);
