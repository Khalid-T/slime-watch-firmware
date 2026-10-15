/* THE LULL -- lull.h says why; this is the small machine.
 *
 * Three stamps (the slow strokes' lifts, oldest first), a wait (the
 * third stroke's content face, counting down), one bool (on) with its
 * seconds left, a level 0..1 and the last end's cause. The level climbs
 * over LULL_IN_S while on and falls over LULL_OUT_S after the time's
 * end; a pet, a jolt or a wall drop it to 0 in the frame they land
 * (the lids up at once: whoever ended it brings the open eyes). The
 * dark zeroes everything by hand. Nothing here reads a clock of its
 * own: the strokes are stamped by main, the frames bring dt. */
#include "lull.h"
#include <string.h>

static struct {
    uint32_t stamp[LULL_STROKES];   /* the slow strokes' lift stamps, oldest first; 0 = empty */
    int      n;                     /* how many stamps (0..LULL_STROKES - 1 between lulls) */
    bool     armed;                 /* the third landed: waiting out its face */
    float    wait_s;                /* the face's seconds left */
    bool     on;
    float    left_s;                /* the lull's seconds left */
    float    level;                 /* 0..1 */
    int      wall;                  /* the wall at the begin */
    bool     pet;                   /* a pet landed since the last tick */
    int      end;                   /* the last end's cause */
} L;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void lull_begin(void)
{
    memset(&L, 0, sizeof L);
}

int lull_stroke(uint32_t now_ms, uint32_t held_ms, float face_s)
{
    if (held_ms < LULL_STROKE_MS) return LULL_STROKE_QUICK;
    if (face_s < 0) face_s = 0;
    if (L.on) { L.left_s = LULL_S; return LULL_STROKE_RENEWED; }
    if (L.armed) { L.wait_s = face_s; return LULL_STROKE_WAITING; }
    if (!now_ms) now_ms = 1;
    if (L.n >= LULL_STROKES) L.n = LULL_STROKES - 1;                    /* never (the third arms); kept honest */
    L.stamp[L.n++] = now_ms;
    if (L.n < LULL_STROKES) return LULL_STROKE_COUNTED;
    if (now_ms - L.stamp[0] <= (uint32_t)LULL_WINDOW_MS) {
        L.n = 0;
        L.armed = true;
        L.wait_s = face_s;
        return LULL_STROKE_ARMED;
    }
    /* three, but the first too old: drop it, the newest two stand */
    for (int i = 1; i < LULL_STROKES; i++) L.stamp[i - 1] = L.stamp[i];
    L.n = LULL_STROKES - 1;
    return LULL_STROKE_COUNTED;
}

static void end_now(int cause)
{
    L.on = false;
    L.end = cause;
    if (cause != LULL_END_TIME) L.level = 0;                             /* at once; the time eases out in the tick */
}

int lull_tick(const lull_in_t *in)
{
    if (!in) return LULL_EV_NONE;
    float dt = clampf(in->dt, 0.0f, 0.25f);
    int ev = LULL_EV_NONE;
    bool here = in->lit && in->holding && !in->dozing;
    if (L.armed) {
        if (!here || L.pet) { L.armed = false; L.n = 0; }               /* the glass dark / empty / a sleeper / a pet: the strokes forgotten, no event */
        else {
            L.wait_s -= dt;
            if (L.wait_s <= 0) {
                L.armed = false;
                L.on = true;
                L.left_s = LULL_S;
                L.wall = in->wall;
                L.end = LULL_END_NONE;
                ev = LULL_EV_BEGIN;
            }
        }
    }
    if (L.on && ev == LULL_EV_NONE) {
        if (!in->lit)                     { end_now(LULL_END_DARK); }                     /* the dark: silent (go_dark told us by hand already) */
        else if (L.pet)                   { end_now(LULL_END_PET);  ev = LULL_EV_END; }
        else if (in->jerk > LULL_JOLT_G)  { end_now(LULL_END_JOLT); ev = LULL_EV_END; }
        else if (in->wall != L.wall)      { end_now(LULL_END_WALL); ev = LULL_EV_END; }
        else if (!here)                   { end_now(LULL_END_GONE); ev = LULL_EV_END; }
        else {
            L.left_s -= dt;
            if (L.left_s <= 0) { end_now(LULL_END_TIME); ev = LULL_EV_END; }
        }
    }
    L.pet = false;
    if (L.on) L.level = clampf(L.level + (LULL_IN_S > 0 ? dt / LULL_IN_S : 1.0f), 0.0f, 1.0f);
    else      L.level = clampf(L.level - (LULL_OUT_S > 0 ? dt / LULL_OUT_S : 1.0f), 0.0f, 1.0f);
    return ev;
}

void lull_pet(void)
{
    if (L.on || L.armed) L.pet = true;
}

void lull_dark(void)
{
    bool was = L.on;
    L.on = false; L.armed = false; L.n = 0; L.level = 0; L.wait_s = 0; L.left_s = 0; L.pet = false;
    if (was) L.end = LULL_END_DARK;
}

void lull_fx(lull_fx_t *out)
{
    if (!out) return;
    out->eye_kind = L.level > 0 ? GEL_EYE_DROOPY : -1;
    out->eye_alpha = L.level;
    out->level = L.level;
}

float lull_level(void) { return L.level; }
bool  lull_on(void)    { return L.on; }
bool  lull_armed(void) { return L.armed; }
int   lull_count(void) { return L.n; }
int   lull_end(void)   { return L.end; }

const char *lull_end_name(int cause)
{
    switch (cause) {
    case LULL_END_TIME: return "the time";
    case LULL_END_PET:  return "a pet";
    case LULL_END_JOLT: return "a jolt";
    case LULL_END_WALL: return "the wall";
    case LULL_END_DARK: return "the dark";
    case LULL_END_GONE: return "he left";
    default:            return "nothing";
    }
}
