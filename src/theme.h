/* THE THEME (design/watch-wardrobe.md rule 7; Khalid 2026-09-11: "the
 * settings page should change from the colour of the slime ... evil
 * colour and what not should change the THEME of the watch, not always
 * blue"): the palette the slime wears IS the wrist's theme.
 *
 * One table -- render.PALETTES transcribed as the floats gel.c takes
 * (the desk's four ordinary hues and the dark band's ten; the "popular"
 * pick is his to trim, the rows cost 80 B each) -- and one current row.
 * theme_pal() replaces every GEL_PAL_BLUE read on the glass: the gel's
 * raster (through hours.cpp, whose tint warms and cools THIS base), the
 * hour, the day and night rings, the ticks, the drop, the reaching cue,
 * the plug, the word bubble's ink / plate / rim, the wisp's mote, the
 * clockwork's arc, the Zs, and the page's four inks (the settings page
 * included). GEL_PAL_BLUE itself stays in gel.c as row 0 and the
 * preview's palette: with no theme set, byte for byte today's glass.
 *
 * THE EVIL SLIME = A PALETTE, NOTHING HEAVY (rule 6): a reserved row
 * (umbra, abyss, ...) is the same gel rastered dark, lit wrong, hot
 * eyes -- theme_reserved() is the one branch anyone may take (the
 * page's bright ink is the shine either way: umbra's sick green on
 * black, gilt's coin). The Sage form keeps its own colours (its .slmf
 * palette; wardrobe.cpp never tints a costume): the one thing on the
 * glass that does not follow the theme.
 *
 * Plain C++, no Arduino: the host tests build the drawers with it; the
 * NVS key is wardrobe.cpp's (store.theme). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THEME_NAME_MAX 15

const gel_pal_t *theme_pal(void);           /* the current row (GEL_PAL_BLUE until set) */
const char      *theme_name(void);          /* "blue", "umbra", ... */
int              theme_index(void);         /* the row's index (0 = blue) */
bool             theme_reserved(void);      /* the dark band: the evil slime's theme */
/* set by name, or by a decimal index into the table (the wire may say
 * either: `pal=green` / `pal=1`); "-" / "" = blue. false = no such row
 * (nothing changes) */
bool             theme_set(const char *name_or_index);
int              theme_count(void);
const char      *theme_name_at(int i);      /* NULL past the end */
bool             theme_reserved_at(int i);
const gel_pal_t *theme_pal_at(int i);

#ifdef __cplusplus
}
#endif
