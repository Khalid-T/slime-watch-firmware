/* The glance (design/watch-body.md section 1): on a wake by a raise the
 * TIME appears for ~6 s -- a small pixel font, HH:MM, in the pet's
 * shine colour, pinned to the top of the watch (Khalid 23:15, 09-09:
 * "the time shouldn't move, just the slime"; THE PLACE, place.h: the
 * bottom for the spell the gel is on the top wall) -- and the gel keeps
 * drawing. Only when the clock is set. THE SECOND LOOK (watch-ideas
 * B15): a raise within 30 s of the last glance holds it twice as long
 * -- he looked and missed it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "place.h"

void glance_show(void);                 /* a raise: show the hour if it is known */
void glance_tick(float dt);
void glance_draw(uint16_t *fb, int at);     /* over the composed frame, before the push: AT the wall THE PLACE chose */
void glance_box(int at, int inset, place_box_t *out);   /* the hour's box at a wall (THE PLACE tests it; the clockwork's number shares it) */
#ifdef __cplusplus
#include "dirty.h"
bool glance_mark(dirty_rect_t *box);    /* THE MARK: did this frame's hour differ from last frame's; the box of both */
#endif
bool glance_on(void);
/* THE NIGHT LIGHT (tell.cpp): the hour in the SHADE colour only -- a
 * raise at 3 am is not a lamp; main sets it for the light's few seconds */
void glance_set_shade(bool on);
/* BATTERY MANNERS (vigor.cpp): under 20 % the glance holds GLANCE_SHORT_S
 * instead of GLANCE_S -- a thrifty glass shows the hour and goes; main
 * sets it once a frame from vigor_thrift() */
void glance_set_short(bool on);
/* MORE SETTINGS' CLOCK row (hourfmt.h): 12H shows `7:41`, 24H `07:41`;
 * main sets it from NVS at boot and on the row's tap */
void glance_set_h12(bool on);
