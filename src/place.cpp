/* THE PLACE: see place.h. */
#include "place.h"
#include "gel.h"
#include <stdio.h>
#include <string.h>

static int last_gel = -2;               /* the gel's wall at the last decision (-2 = none yet) */
static int last_hour = -1, last_word = -1, last_inset = 0;   /* the last places (-1 = the text was off) */

void place_reset(void) { last_gel = -2; last_hour = last_word = -1; last_inset = 0; }

bool place_box_clear(const uint16_t *fb, int fb_w, int fb_h, const place_box_t *b)
{
    int x0 = b->x < 0 ? 0 : b->x, y0 = b->y < 0 ? 0 : b->y;
    int x1 = b->x + b->w > fb_w ? fb_w : b->x + b->w, y1 = b->y + b->h > fb_h ? fb_h : b->y + b->h;
    for (int y = y0; y < y1; y += 2)
        for (int x = x0; x < x1; x += 2)
            if (fb[y * fb_w + x]) return false;
    return true;
}

static bool boxes_meet(const place_box_t *a, const place_box_t *b)
{
    if (a->w <= 0 || a->h <= 0 || b->w <= 0 || b->h <= 0) return false;
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* the walls' outward direction on the glass: bottom +y, right +x, top -y, left -x */
static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { 1, 0, -1, 0 };

/* the inset that puts ``word`` inward of ``hour`` on wall ``at`` with
 * PLACE_GAP between (0 when they do not meet) */
static int inset_past(int at, const place_box_t *hour, const place_box_t *word)
{
    if (!boxes_meet(hour, word)) return 0;
    /* the outward edge of the word must pass the inward edge of the hour */
    int need;
    if (DY[at] < 0)      need = (hour->y + hour->h + PLACE_GAP) - word->y;             /* top: the word moves down */
    else if (DY[at] > 0) need = (word->y + word->h) - (hour->y - PLACE_GAP);           /* bottom: up */
    else if (DX[at] < 0) need = (hour->x + hour->w + PLACE_GAP) - word->x;             /* left: right */
    else                 need = (word->x + word->w) - (hour->x - PLACE_GAP);           /* right: left */
    return need > 0 ? need : 0;
}

/* the candidate order for a text whose preferred wall is ``pref`` and
 * whose last resort is ``last`` (the gel's own wall, or the fourth) */
static void order(int pref, int gel_wall, int out[4])
{
    out[0] = pref;
    out[1] = (pref + 1) & 3;
    out[2] = (pref + 3) & 3;
    out[3] = (pref + 2) & 3;
    /* the gel's own wall last of all */
    if (gel_wall >= 0)
        for (int i = 1; i < 3; i++)
            if (out[i] == gel_wall) { int t = out[i]; out[i] = out[3]; out[3] = t; }
}

/* the word's place at a wall: its inset past the hour (if the hour is
 * there) and whether the box is then clear of the gel */
static bool word_fits(int at, const uint16_t *fb, int fb_w, int fb_h, int gel_wall,
                      bool hour_on, int hour_at, const place_box_t *hour_box,
                      place_box_fn word_box, int *inset_out)
{
    place_box_t wb;
    word_box(at, 0, &wb);
    int inset = (hour_on && hour_at == at) ? inset_past(at, hour_box, &wb) : 0;
    if (inset) word_box(at, inset, &wb);
    *inset_out = inset;
    if (gel_wall < 0) return true;
    return place_box_clear(fb, fb_w, fb_h, &wb);
}

void place_decide(place_t *out, const uint16_t *fb, int fb_w, int fb_h, int gel_wall,
                  bool hour_on, place_box_fn hour_box, bool word_on, place_box_fn word_box)
{
    out->log[0] = 0;
    const bool gel_moved = gel_wall != last_gel;
    /* THE HOUR: the top, or the bottom while the gel is on the top wall;
     * the backstop past that */
    int hour_pref = (gel_wall == GEL_WALL_TOP) ? GEL_WALL_BOTTOM : GEL_WALL_TOP;
    int hour_at = hour_pref;
    place_box_t hb;
    if (hour_on) {
        int cand[4];
        order(hour_pref, gel_wall, cand);
        bool placed = false;
        /* sticky: the last place, if the gel kept its wall and it is still clear */
        if (!gel_moved && last_hour >= 0) {
            hour_box(last_hour, 0, &hb);
            if (gel_wall < 0 || place_box_clear(fb, fb_w, fb_h, &hb)) { hour_at = last_hour; placed = true; }
        }
        for (int i = 0; i < 4 && !placed; i++) {
            hour_box(cand[i], 0, &hb);
            if (gel_wall < 0 || place_box_clear(fb, fb_w, fb_h, &hb)) { hour_at = cand[i]; placed = true; }
        }
        if (!placed) hour_at = hour_pref;   /* none clear: the pinned spot after all */
        hour_box(hour_at, 0, &hb);
    }
    /* THE WORD: opposite the gel (the top with no gel); the backstop:
     * the sides, then the gel's wall; inward of the hour when they share */
    int word_pref = gel_wall < 0 ? GEL_WALL_TOP : (gel_wall + 2) & 3;
    int word_at = word_pref, word_inset = 0;
    if (word_on) {
        int cand[4], inset = 0;
        order(word_pref, gel_wall, cand);
        bool placed = false;
        if (!gel_moved && last_word >= 0
            && word_fits(last_word, fb, fb_w, fb_h, gel_wall, hour_on, hour_at, &hb, word_box, &inset)) {
            word_at = last_word; word_inset = inset; placed = true;
        }
        for (int i = 0; i < 4 && !placed; i++)
            if (word_fits(cand[i], fb, fb_w, fb_h, gel_wall, hour_on, hour_at, &hb, word_box, &inset)) {
                word_at = cand[i]; word_inset = inset; placed = true;
            }
        if (!placed) {                      /* none clear: opposite the gel, inward of the hour if it is there */
            word_at = word_pref;
            word_fits(word_pref, fb, fb_w, fb_h, GEL_WALL_NONE, hour_on, hour_at, &hb, word_box, &word_inset);
        }
    }
    /* the log: a place that changed while its text is on */
    int n = 0;
    if (hour_on && hour_at != last_hour) {
        if (hour_at != GEL_WALL_TOP)
            n += snprintf(out->log + n, sizeof out->log - (size_t)n, "place: the hour at the %s (the gel on the %s wall)",
                          GEL_WALL_NAMES[hour_at], gel_wall >= 0 ? GEL_WALL_NAMES[gel_wall] : "no");
        else if (last_hour >= 0)
            n += snprintf(out->log + n, sizeof out->log - (size_t)n, "place: the hour back at the top");
    }
    if (word_on && (word_at != last_word || word_inset != last_inset)) {
        char gel[24];
        if (gel_wall >= 0) snprintf(gel, sizeof gel, "the gel on the %s", GEL_WALL_NAMES[gel_wall]);
        else snprintf(gel, sizeof gel, "no gel");
        n += snprintf(out->log + n, sizeof out->log - (size_t)n, "%sthe word at the %s%s%s (%s)",
                      n ? "; " : "place: ", GEL_WALL_NAMES[word_at],
                      word_at != word_pref ? " -- opposite the gel would meet it" : "",
                      word_inset ? ", under the hour" : "", gel);
    }
    (void)n;
    last_gel = gel_wall;
    last_hour = hour_on ? hour_at : -1;
    last_word = word_on ? word_at : -1;
    last_inset = word_on ? word_inset : 0;
    out->hour_at = hour_at;
    out->word_at = word_at;
    out->word_inset = word_inset;
}
