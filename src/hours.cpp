/* THE HOURS' LIGHT -- hours.h says what; this is how.
 *
 * Two weights from the local minute, each 0..1 and smooth in time:
 *   WARM  a triangle over the dawn window and another over the dusk
 *         window, peaking in their middles (the low sun);
 *   COOL  1 inside the quiet hours, ramping in over the hour before
 *         they begin and out over the hour after they end -- and
 *         yielding to WARM (cool x (1 - warm)): at 06:30 it is dawn
 *         that lights the glass, not the night; at 05:00 the night.
 * The tint: body, shade and shine lerped toward WARM_TONE by WARM_MAX x
 * warm; the shine lerped toward COOL_TONE and the shade / body toward
 * black by COOL_MAX x cool. Midday: both 0, the palette IS
 * GEL_PAL_BLUE. The outline and the eye are copied, never touched.
 * The backlight's factor: 1 - WARM_DIP x warm - COOL_DIP x cool.
 *
 * No Arduino. */
#include "hours.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* --- the tunables, with their cause ----------------------------------------- */
/* the low sun: the dawn and dusk windows are HOURS_SUN_HALF_MIN either
 * side of the sun (hours.h): 05-08 and 17-20 at the fixed 06:30 / 18:30,
 * the real sun's own when the wire has told it (THE REAL SUN) */
/* the night is the rest's quiet hours (the grain's, else 23:00-07:00);
 * the light does not switch at a minute, so it ramps in and out over
 * this long at each edge */
static const int   NIGHT_FROM_MIN = 23 * 60;
static const int   NIGHT_TO_MIN   = 7 * 60;
static const int   NIGHT_EDGE_MIN = 60;
/* the warm tone (amber: a low sun through glass) and how far toward it
 * at the peak -- a little: the gel must stay itself, this is light on
 * it, not a new palette */
static const gel_rgba_t WARM_TONE = { 1.00f, 0.72f, 0.45f, 0 };
static const float WARM_MAX      = 0.12f;
/* the night: the shine cooled toward a deep blue, the shade and the body
 * darkened (the room is dark; the gel is a shade darker in it) */
static const gel_rgba_t COOL_TONE = { 0.35f, 0.55f, 0.95f, 0 };
static const float COOL_MAX      = 0.10f;
static const float COOL_BODY_X   = 0.5f;     /* the body darkens half as much as the shade */
/* the backlight: awake = full by day, ~70% in the warm hours, ~45% at
 * night; a phase edge eases over BL_EASE_S (95% done), never a step */
static const float WARM_DIP      = 0.30f;
static const float COOL_DIP      = 0.55f;
static const float BL_EASE_S     = 2.0f;

/* --- the state ------------------------------------------------------------- */
static struct {
    gel_pal_t pal;
    int       minute;           /* the minute the tint was computed for; -2 = never */
    char      quiet[12];
    float     warm, cool;
    float     bl_k, bl_target;  /* the eased factor and the hour's */
    bool      bl_init;
    uint8_t   floor;
    int       phase;
    /* THE REAL SUN: the pair the light is heading for (-1 = fixed), and
     * the windows' centres now -- easing from where they were to the
     * target over HOURS_SUN_EASE_S (k 0 -> 1), fractional minutes */
    int       sun_rise, sun_set;
    float     rise_from, set_from, rise_now, set_now, ease_k;
} H;

static float sun_target_rise(void) { return H.sun_rise >= 0 ? (float)H.sun_rise : (float)HOURS_SUN_FIXED_RISE; }
static float sun_target_set(void)  { return H.sun_set >= 0 ? (float)H.sun_set : (float)HOURS_SUN_FIXED_SET; }
static float lerp_(float a, float b, float k) { return a + (b - a) * k; }

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float lerp(float a, float b, float k) { return a + (b - a) * k; }
static void tint_toward(gel_rgba_t *c, const gel_rgba_t *to, float k)
{
    c->r = lerp(c->r, to->r, k);
    c->g = lerp(c->g, to->g, k);
    c->b = lerp(c->b, to->b, k);
}

/* "23-7" / "23:30-7" as rest.cpp reads it; "-" or nothing = the default */
static void parse_quiet(const char *q, int *from, int *to)
{
    *from = NIGHT_FROM_MIN;
    *to = NIGHT_TO_MIN;
    if (!q || !q[0] || !strcmp(q, "-")) return;
    int fh = 0, fm = 0, th = 0, tm = 0;
    const char *dash = strchr(q, '-');
    if (!dash) return;
    if (sscanf(q, "%d:%d", &fh, &fm) < 1) return;
    if (sscanf(dash + 1, "%d:%d", &th, &tm) < 1) return;
    if (fh < 0 || fh > 23 || th < 0 || th > 23) return;
    *from = fh * 60 + fm;
    *to = th * 60 + tm;
}

/* a triangle 0 -> 1 -> 0 over [from, to] (fractional: the sun eases) */
static float triangle(int m, float from, float to)
{
    if (m <= from || m >= to) return 0;
    float half = (to - from) * 0.5f;
    float d = fabsf((float)m - (from + half));
    return clamp01(1.0f - d / half);
}

/* minutes from a to b going forward round the day */
static int fwd(int a, int b) { int d = b - a; while (d < 0) d += 1440; return d % 1440; }

/* 1 inside [from, to) round the day, ramping in over the hour before and out over the hour after */
static float night_weight(int m, int from, int to)
{
    if (from == to) return 0;
    int len = fwd(from, to);
    int in = fwd(from, m);
    if (in < len) return 1.0f;                      /* inside: full */
    int before = fwd(m, from);                      /* minutes until it begins */
    if (before > 0 && before <= NIGHT_EDGE_MIN) return 1.0f - (float)before / NIGHT_EDGE_MIN;
    int after = fwd(to, m);                         /* minutes since it ended */
    if (after >= 0 && after < NIGHT_EDGE_MIN) return 1.0f - (float)after / NIGHT_EDGE_MIN;
    return 0;
}

void hours_compute_sun(int minute, const char *quiet, float rise_min, float set_min,
                       gel_pal_t *pal, float *warm_out, float *cool_out, float *bl_k)
{
    float warm = 0, cool = 0;
    if (minute >= 0) {
        int from, to;
        parse_quiet(quiet, &from, &to);
        warm = triangle(minute, rise_min - HOURS_SUN_HALF_MIN, rise_min + HOURS_SUN_HALF_MIN);
        float dusk = triangle(minute, set_min - HOURS_SUN_HALF_MIN, set_min + HOURS_SUN_HALF_MIN);
        if (dusk > warm) warm = dusk;
        cool = night_weight(minute, from, to) * (1.0f - warm);   /* dawn outshines the night -- the REAL dawn, when known */
    }
    *pal = *theme_pal();                         /* THE THEME (wardrobe): the base is the palette the slime wears; GEL_PAL_BLUE until one is set */
    if (warm > 0) {
        tint_toward(&pal->body, &WARM_TONE, WARM_MAX * warm);
        tint_toward(&pal->shade, &WARM_TONE, WARM_MAX * warm);
        tint_toward(&pal->shine, &WARM_TONE, WARM_MAX * warm);
    }
    if (cool > 0) {
        static const gel_rgba_t BLACK = { 0, 0, 0, 0 };
        tint_toward(&pal->shine, &COOL_TONE, COOL_MAX * cool);
        tint_toward(&pal->shade, &BLACK, COOL_MAX * cool);
        tint_toward(&pal->body, &BLACK, COOL_MAX * COOL_BODY_X * cool);
    }
    /* the outline and the eye: GEL_PAL_BLUE's, untouched (the ink) */
    if (warm_out) *warm_out = warm;
    if (cool_out) *cool_out = cool;
    if (bl_k) *bl_k = clamp01(1.0f - WARM_DIP * warm - COOL_DIP * cool);
}

void hours_compute(int minute, const char *quiet, gel_pal_t *pal, float *warm_out, float *cool_out, float *bl_k)
{
    hours_compute_sun(minute, quiet, (float)HOURS_SUN_FIXED_RISE, (float)HOURS_SUN_FIXED_SET, pal, warm_out, cool_out, bl_k);
}

static int phase_of(int minute, float warm, float cool, float rise_min, float set_min)
{
    if (minute < 0) return HOURS_NOCLOCK;
    if (cool >= 0.5f) return HOURS_NIGHT;
    if (warm > 0 && minute < (rise_min + set_min) * 0.5f) return HOURS_DAWN;   /* the dawn's side of the day's middle */
    if (warm > 0) return HOURS_DUSK;
    if (minute >= set_min + HOURS_SUN_HALF_MIN || cool > 0) return HOURS_EVENING;
    if (minute < rise_min - HOURS_SUN_HALF_MIN) return HOURS_NIGHT;
    return HOURS_DAY;
}

void hours_begin(uint8_t bl_floor)
{
    memset(&H, 0, sizeof H);
    H.pal = *theme_pal();
    H.minute = -2;
    H.bl_k = H.bl_target = 1.0f;
    H.bl_init = false;
    H.floor = bl_floor;
    H.phase = HOURS_NOCLOCK;
    H.sun_rise = H.sun_set = -1;
    H.rise_from = H.rise_now = (float)HOURS_SUN_FIXED_RISE;
    H.set_from = H.set_now = (float)HOURS_SUN_FIXED_SET;
    H.ease_k = 1.0f;
}

bool hours_sun(int rise_min, int set_min, bool snap)
{
    if (!(rise_min < 0 && set_min < 0)) {
        if (rise_min < 0 || rise_min > 1439 || set_min < 0 || set_min > 1439 || rise_min >= set_min) return false;
    } else rise_min = set_min = -1;
    if (rise_min == H.sun_rise && set_min == H.sun_set && (H.ease_k >= 1.0f || !snap)) return true;   /* the same pair: nothing to move */
    H.sun_rise = rise_min;
    H.sun_set = set_min;
    if (snap) {
        H.rise_from = H.rise_now = sun_target_rise();
        H.set_from = H.set_now = sun_target_set();
        H.ease_k = 1.0f;
    } else {
        H.rise_from = H.rise_now;                     /* from wherever the windows are now: mid-ease is fine */
        H.set_from = H.set_now;
        H.ease_k = 0.0f;
    }
    H.minute = -2;                                    /* the next tick recomputes the tint */
    return true;
}

void hours_sun_get(int *rise_min, int *set_min)
{
    if (rise_min) *rise_min = H.sun_rise;
    if (set_min) *set_min = H.sun_set;
}
bool hours_sun_known(void) { return H.sun_rise >= 0; }

void hours_tick(float dt, int minute, const char *quiet)
{
    const char *q = quiet ? quiet : "";
    if (H.ease_k < 1.0f) {
        /* THE REAL SUN easing: the windows slide toward the pair over
         * HOURS_SUN_EASE_S; the tint follows each tick meanwhile */
        H.ease_k += (dt < 0 ? 0 : dt) / HOURS_SUN_EASE_S;
        if (H.ease_k > 1.0f) H.ease_k = 1.0f;
        H.rise_now = lerp_(H.rise_from, sun_target_rise(), H.ease_k);
        H.set_now = lerp_(H.set_from, sun_target_set(), H.ease_k);
        H.minute = -2;
    }
    if (minute != H.minute || strncmp(q, H.quiet, sizeof H.quiet - 1) != 0) {
        /* once a minute: the tint and the factor's target */
        H.minute = minute;
        strncpy(H.quiet, q, sizeof H.quiet - 1);
        H.quiet[sizeof H.quiet - 1] = 0;
        hours_compute_sun(minute, quiet, H.rise_now, H.set_now, &H.pal, &H.warm, &H.cool, &H.bl_target);
        H.phase = phase_of(minute, H.warm, H.cool, H.rise_now, H.set_now);
        if (!H.bl_init) { H.bl_k = H.bl_target; H.bl_init = true; }   /* the first word: no fade-in from nowhere */
    }
    /* the ease: 95% of the way in BL_EASE_S */
    float k = 3.0f * dt / BL_EASE_S;
    if (k > 1.0f) k = 1.0f;
    H.bl_k += (H.bl_target - H.bl_k) * k;
    if (fabsf(H.bl_target - H.bl_k) < 0.002f) H.bl_k = H.bl_target;
}

const gel_pal_t *hours_pal(void) { return &H.pal; }
void hours_retint(void) { H.minute = -2; }          /* THE THEME changed: the next tick recomputes the tint on the new base */
float hours_bl_k(void) { return H.bl_k; }
int hours_phase(void) { return H.phase; }

uint8_t hours_level(uint8_t base)
{
    if (base < H.floor) return base;             /* BL_SLEEP: rest.cpp's word, under the floor already */
    float v = base * H.bl_k;
    if (v < H.floor) v = H.floor;
    if (v > base) v = base;
    return (uint8_t)(v + 0.5f);
}

static const char *const PHASE_NAMES[] = { "no clock", "night", "dawn", "day", "dusk", "evening" };
const char *hours_phase_name(void) { return PHASE_NAMES[H.phase]; }

void hours_status(char *out, int cap)
{
    char sun[48];
    if (H.sun_rise >= 0) snprintf(sun, sizeof sun, "sun %02d:%02d-%02d:%02d%s", H.sun_rise / 60, H.sun_rise % 60, H.sun_set / 60, H.sun_set % 60, H.ease_k < 1.0f ? " (easing in)" : "");
    else snprintf(sun, sizeof sun, "sun fixed 06:30 / 18:30%s", H.ease_k < 1.0f ? " (easing back)" : "");
    if (H.minute < 0)
        snprintf(out, cap, "hours: %s | tint none (GEL_PAL_BLUE) | %s | backlight x%.2f", hours_phase_name(), sun, H.bl_k);
    else
        snprintf(out, cap, "hours: %s at %02d:%02d | tint warm %.2f cool %.2f | body (%.2f,%.2f,%.2f) shade (%.2f,%.2f,%.2f) shine (%.2f,%.2f,%.2f) | night %s | %s | backlight x%.2f -> x%.2f (awake %d, floor %d)",
                 hours_phase_name(), H.minute / 60, H.minute % 60, H.warm, H.cool,
                 H.pal.body.r, H.pal.body.g, H.pal.body.b, H.pal.shade.r, H.pal.shade.g, H.pal.shade.b,
                 H.pal.shine.r, H.pal.shine.g, H.pal.shine.b, H.quiet[0] ? H.quiet : "23-7 (default)", sun,
                 H.bl_k, H.bl_target, (int)hours_level(200), (int)H.floor);
}
