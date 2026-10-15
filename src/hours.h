/* THE HOURS' LIGHT (Khalid, 2026-09-09: "make it a true home for the
 * slime and something I can use too, as a watch; fill the firmware with
 * RAM-light stuff"): the glass follows the day.
 *
 * By the material: the gel is translucent, lit by whatever light comes
 * through the glass. Dawn and dusk light is warm and low; midday light
 * is white; the night is dark and blue. So the PALETTE the gel is
 * rastered with is the desk's blue TINTED by the hour -- the body, the
 * shade and the shine lerped a little toward a warm tone at dawn and
 * dusk, the shine cooled and the shade darkened in the quiet hours; the
 * outline and the eyes are INK (the identity anchor, slime-sprite-locked)
 * and never move. And the BACKLIGHT follows: full by day, lower in the
 * warm hours, lowest at night -- never under the dim level, so a raise
 * at 3 am is still a glass, not a lamp (rest.cpp's BL_SLEEP is under it
 * and stays what it is).
 *
 * THE REAL SUN (design/watch-ideas.md E39, 2026-09-23): the dawn and
 * the dusk are three-hour windows CENTRED ON THE SUN -- the fixed hours
 * (05-08, 17-20) are the sun at 06:30 and 18:30, nothing else -- so
 * when the wire's `time` carries today's `sunrise=HH:MM sunset=HH:MM`
 * (link.cpp -> hours_sun) the amber peaks at the real dawn and the real
 * dusk, and the night (the quiet hours, his sleep, unchanged) yields to
 * the REAL dawn: a December morning at 06:30 is still the night, a
 * June one already the day. The pair is kept in NVS (store.sun_*) so a
 * cold boot with the RTC's clock lights the glass by the sun before
 * any link; a change eases over HOURS_SUN_EASE_S, never a jump. None
 * known = the fixed hours, byte for byte as before.
 *
 * No Arduino here: the local minute and the grain's quiet hours are
 * parameters; the palette is recomputed once a minute (the draw reads a
 * pointer, nothing per frame; each tick while the sun eases); the
 * backlight's factor eases over BL_EASE_S so a phase edge is never a
 * step. tools/hours_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gel.h"
#include "theme.h"      /* THE THEME: the base the hour tints (GEL_PAL_BLUE until the wardrobe sets one) */

enum { HOURS_NOCLOCK = 0, HOURS_NIGHT, HOURS_DAWN, HOURS_DAY, HOURS_DUSK, HOURS_EVENING };

void hours_begin(uint8_t bl_floor);          /* main's BL_DIM: the lit glass never goes under it by the hour */
/* one call a frame: the local minute (-1 = no clock: neutral), the
 * grain's quiet hours ("23-7"; NULL / "-" = 23:00-07:00, rest.cpp's
 * default). The tint is recomputed when the minute turns; the
 * backlight's factor eases toward the hour's over BL_EASE_S. */
void hours_tick(float dt, int minute, const char *quiet);
const gel_pal_t *hours_pal(void);            /* the palette the draw passes instead of GEL_PAL_BLUE */
void hours_retint(void);                     /* THE THEME (theme.h) changed under it: recompute the tint on the new base at the next tick */
/* a backlight level scaled by the hour's (eased) factor: base x k, never
 * under the floor; a base already under the floor (BL_SLEEP) is left as
 * it is */
uint8_t hours_level(uint8_t base);
float   hours_bl_k(void);                    /* the eased factor now */
int     hours_phase(void);
const char *hours_phase_name(void);
void    hours_status(char *out, int cap);    /* `hours` over serial */

/* THE REAL SUN: today's sunrise and sunset as local minutes (the wire's
 * `time`, or NVS at a boot); -1 / -1 = none known = the fixed hours.
 * Refused (false, nothing changed) when a value is outside the day or
 * the set is not after the rise. snap = at once (a boot: no ease from
 * the fixed hours); else the windows slide over HOURS_SUN_EASE_S. */
bool hours_sun(int rise_min, int set_min, bool snap);
void hours_sun_get(int *rise_min, int *set_min);      /* the pair the light is heading for; -1 = the fixed hours */
bool hours_sun_known(void);
#define HOURS_SUN_EASE_S     60.0f
#define HOURS_SUN_FIXED_RISE (6 * 60 + 30)            /* the fixed hours ARE the sun at 06:30 / 18:30 ... */
#define HOURS_SUN_FIXED_SET  (18 * 60 + 30)
#define HOURS_SUN_HALF_MIN   90                       /* ... with the amber's window 90 min either side (05-08, 17-20) */

/* the pure function, for the test: the tint and the backlight's target
 * factor at a minute, no easing, the fixed hours (= hours_compute_sun at
 * the fixed sun) */
void hours_compute(int minute, const char *quiet, gel_pal_t *pal, float *warm, float *cool, float *bl_k);
/* the same at a given sun (local minutes, fractional while easing) */
void hours_compute_sun(int minute, const char *quiet, float rise_min, float set_min,
                       gel_pal_t *pal, float *warm, float *cool, float *bl_k);
