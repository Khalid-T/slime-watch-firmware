/* THE THEME: render.PALETTES as gel_pal_t rows (body, shade, outline,
 * shine, eye -- the same order gel.c's GEL_PAL_BLUE carries), and the
 * one current row. See theme.h. */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "theme.h"

typedef struct { const char *name; bool reserved; gel_pal_t pal; } theme_row_t;

/* the desk's PALETTES, 2026-09-17 (render.py): the four ordinary hues,
 * then THE RESERVED BAND in its order (each ordinary hue's dark twin
 * first, then the twin-less members). The alphas are the desk's: the
 * translucency is what makes gel gel. */
static const theme_row_t ROWS[] = {
    { "blue",   false, { {0.129f, 0.588f, 0.878f, 0.78f}, {0.106f, 0.310f, 0.659f, 0.84f}, {0.086f, 0.149f, 0.361f, 0.96f}, {0.486f, 0.847f, 0.941f, 0.90f}, {0.086f, 0.149f, 0.361f, 1.00f} } },
    { "green",  false, { {0.30f, 0.72f, 0.34f, 0.82f}, {0.17f, 0.50f, 0.20f, 0.88f}, {0.05f, 0.22f, 0.08f, 0.96f}, {0.62f, 0.93f, 0.60f, 0.85f}, {0.05f, 0.22f, 0.08f, 1.00f} } },
    { "red",    false, { {0.85f, 0.28f, 0.26f, 0.82f}, {0.60f, 0.14f, 0.13f, 0.88f}, {0.28f, 0.06f, 0.06f, 0.96f}, {0.98f, 0.62f, 0.58f, 0.85f}, {0.28f, 0.06f, 0.06f, 1.00f} } },
    { "purple", false, { {0.62f, 0.33f, 0.83f, 0.82f}, {0.42f, 0.17f, 0.60f, 0.88f}, {0.20f, 0.07f, 0.30f, 0.96f}, {0.85f, 0.65f, 0.98f, 0.85f}, {0.20f, 0.07f, 0.30f, 1.00f} } },
    /* the dark band: the body gone dark, a shine of the wrong colour, eyes never its own */
    { "umbra",     true, { {0.16f, 0.08f, 0.22f, 0.86f}, {0.09f, 0.04f, 0.14f, 0.90f}, {0.03f, 0.00f, 0.06f, 0.97f}, {0.55f, 0.95f, 0.45f, 0.85f}, {0.85f, 0.10f, 0.12f, 1.00f} } },
    { "abyss",     true, { {0.05f, 0.10f, 0.24f, 0.86f}, {0.03f, 0.05f, 0.15f, 0.90f}, {0.01f, 0.02f, 0.07f, 0.97f}, {0.35f, 0.95f, 0.90f, 0.85f}, {0.95f, 0.35f, 0.10f, 1.00f} } },
    { "blight",    true, { {0.10f, 0.20f, 0.08f, 0.86f}, {0.05f, 0.12f, 0.04f, 0.90f}, {0.02f, 0.05f, 0.01f, 0.97f}, {0.85f, 0.95f, 0.30f, 0.85f}, {0.95f, 0.15f, 0.55f, 1.00f} } },
    { "gore",      true, { {0.30f, 0.04f, 0.06f, 0.86f}, {0.18f, 0.02f, 0.03f, 0.90f}, {0.07f, 0.00f, 0.01f, 0.97f}, {0.80f, 0.95f, 0.40f, 0.85f}, {0.92f, 0.96f, 1.00f, 1.00f} } },
    { "bruise",    true, { {0.22f, 0.06f, 0.28f, 0.86f}, {0.13f, 0.03f, 0.17f, 0.90f}, {0.05f, 0.00f, 0.07f, 0.97f}, {0.95f, 0.40f, 0.75f, 0.85f}, {0.60f, 1.00f, 0.20f, 1.00f} } },
    { "gilt",      true, { {0.07f, 0.06f, 0.05f, 0.86f}, {0.03f, 0.03f, 0.02f, 0.90f}, {0.01f, 0.01f, 0.00f, 0.97f}, {1.00f, 0.80f, 0.25f, 0.85f}, {0.98f, 0.98f, 0.95f, 1.00f} } },
    { "onyx",      true, { {0.06f, 0.06f, 0.08f, 0.86f}, {0.03f, 0.03f, 0.05f, 0.90f}, {0.00f, 0.00f, 0.02f, 0.97f}, {0.85f, 0.88f, 0.92f, 0.85f}, {0.65f, 0.85f, 1.00f, 1.00f} } },
    { "ember",     true, { {0.12f, 0.08f, 0.07f, 0.86f}, {0.06f, 0.04f, 0.03f, 0.90f}, {0.02f, 0.01f, 0.01f, 0.97f}, {1.00f, 0.45f, 0.10f, 0.85f}, {1.00f, 0.85f, 0.20f, 1.00f} } },
    { "verdigris", true, { {0.04f, 0.14f, 0.12f, 0.86f}, {0.02f, 0.08f, 0.07f, 0.90f}, {0.00f, 0.03f, 0.02f, 0.97f}, {0.55f, 1.00f, 0.35f, 0.85f}, {0.95f, 1.00f, 0.95f, 1.00f} } },
    { "wine",      true, { {0.20f, 0.03f, 0.08f, 0.86f}, {0.12f, 0.01f, 0.05f, 0.90f}, {0.05f, 0.00f, 0.02f, 0.97f}, {1.00f, 0.50f, 0.70f, 0.85f}, {1.00f, 0.92f, 0.80f, 1.00f} } },
};
static const int NROWS = (int)(sizeof ROWS / sizeof ROWS[0]);

static int cur = 0;

/* row 0 is GEL_PAL_BLUE byte for byte: the pointer handed out for it is
 * gel.c's own, so "no theme" and "blue" are the same glass */
const gel_pal_t *theme_pal(void) { return cur == 0 ? &GEL_PAL_BLUE : &ROWS[cur].pal; }
const char *theme_name(void) { return ROWS[cur].name; }
int theme_index(void) { return cur; }
bool theme_reserved(void) { return ROWS[cur].reserved; }
int theme_count(void) { return NROWS; }
const char *theme_name_at(int i) { return (i >= 0 && i < NROWS) ? ROWS[i].name : NULL; }
bool theme_reserved_at(int i) { return (i >= 0 && i < NROWS) ? ROWS[i].reserved : false; }
const gel_pal_t *theme_pal_at(int i) { return (i >= 0 && i < NROWS) ? (i == 0 ? &GEL_PAL_BLUE : &ROWS[i].pal) : NULL; }

bool theme_set(const char *s)
{
    if (!s || !s[0] || !strcmp(s, "-")) { cur = 0; return true; }
    if (isdigit((unsigned char)s[0])) {
        int i = atoi(s);
        if (i < 0 || i >= NROWS) return false;
        cur = i; return true;
    }
    for (int i = 0; i < NROWS; i++)
        if (!strcmp(ROWS[i].name, s)) { cur = i; return true; }
    return false;
}
