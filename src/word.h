/* THE WORD: the mind's rare line, fetched once per link (`word` ->
 * `ok word <text>` | `ok none`, box door only), HELD until the next
 * glance -- a raise, or a tap -- and then drawn as ONE BUBBLE in a
 * pixel font on the wall OPPOSITE the gel (THE PLACE, place.h: never
 * over him; inward of the hour when they share a wall) while the
 * screen is lit. A tap dismisses it (that tap is not a pet). Never
 * re-shown, never persisted: a reboot forgets it. It never lights the
 * screen. Words only: the bubble is the mind's line, no number is
 * ever formatted into it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "place.h"

void word_set(const char *text);        /* the link: a word came (replaces one still held) */
void word_glance(void);                 /* the body raised to look: a held word shows */
bool word_tap(void);                    /* the tap path: true = the tap took the word (not a pet) */
void word_draw(uint16_t *fb, int at, int inset);   /* over the composed frame, after the gel: AT the wall THE PLACE chose, ``inset`` px inward */
void word_box(int at, int inset, place_box_t *out);   /* the bubble's box at a wall (THE PLACE tests it; empty when nothing shows) */
#ifdef __cplusplus
#include "dirty.h"
bool word_mark(dirty_rect_t *box);      /* THE MARK: did this frame's bubble differ from last frame's; the box of both */
#endif
bool word_showing(void);
bool word_held(void);
const char *word_text(void);            /* "" when none */
