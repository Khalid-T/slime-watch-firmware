/* THE GRAIN ON THE WRIST (Khalid, 2026-09-09: "make it a true home for
 * the slime"). The desk's rule (grain.py): the same slime moves the same
 * in every house because its idiosyncrasy constants derive from ONE
 * seed -- rolled at birth, never again. The wrist gets that seed on the
 * link's `ok grain seed= regard= night= quiet= mood=` line (link.cpp
 * caches it in NVS), and here it becomes the wrist's own constants:
 *
 *   THE DESK'S SIX, ITS FORMULAS EXACTLY -- random.Random(f"{seed}:1")
 *   (the desk's slime id 1: the one soul), Python's own seeding (the
 *   string + its SHA-512, big-endian, into MT19937's init_by_array) and
 *   its uniform(), ported bit for bit so the numbers ARE the desk's:
 *   bored u(0.85,1.18) blink u(0.85,1.15) saccade u(0.85,1.15)
 *   dwell u(0.88,1.15) breath u(0.92,1.08) hop u(0.96,1.06).
 *   THE WRIST'S OWN, the same stream after them (so the desk's six are
 *   untouched): breath_amp u(0.90,1.10), favourite floor(u(0,5)),
 *   settle u(0.85,1.15), slide u(0.85,1.15), wake_hold u(0.85,1.15).
 *
 *   REGARD (-1..1, the desk's, decayed there on read) is WORN, never
 *   spoken: the eyes come up faster when it is high, content eyes
 *   linger, the idle life a touch livelier (positive only -- the desk's
 *   REGARD_DWELL_GAIN: grudges do not pace the body), and under
 *   REGARD_SHY_AT the glance's first face is one notch cooler.
 *   MOOD (the desk's carried mood) picks the glance's first eye kind
 *   when the tell has nothing to say. NIGHT (night=1) counts as the
 *   quiet hours while the wrist has no clock.
 *
 * "Expectation colours, never causes" (VISION): nothing here starts a
 * behaviour; every number is a tint on one that already happens.
 * Recomputed only when the grain line changes (link.cpp's parse_grain
 * -> grain_update); until then the knobs are the nominal 1.0 creature.
 * No Arduino: tools/grain_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* the desk's six, grain.knobs(1) exactly */
    float bored, blink, saccade, dwell, breath, hop;
    /* the wrist's own */
    float breath_amp;       /* x on the resting BREATH_AMP */
    int   favourite;        /* an IDLE_* kind (1..5) whose weight is tilted up; 0 = none (nominal) */
    float settle;           /* x on the idle settle's depth */
    float slide;            /* x on the residual's ease, TILT_EASE (walls.h; it was the bowl's SLIDE_EASE) */
    float wake_hold;        /* x on the wake's EYE_HOLD_WAKE_S */
    /* regard's colours */
    float eye_rise_s;       /* engage(): the eyes come up over this (0.35 s at regard 1, 0.9 at -1) */
    float linger;           /* x on the stroke's content-eyes hold (1 + 0.4 x max(0, regard)) */
    float idle_gap;         /* x on IDLE_EVERY_* (bored x (1 - 0.12 x max(0, regard)), the desk's dwell rule) */
    bool  cool;             /* regard < REGARD_SHY_AT -0.25: the glance's first face a notch cooler */
    /* the mood's colour */
    int   mood_eye;         /* GEL_EYE_* the glance wears first at a raise with nothing to tell; -1 = none */
} grain_knobs_t;

void grain_begin(void);                     /* the nominal creature until the link says (or the cached line is re-read) */
/* the grain line's numbers (link.cpp's parse_grain, at boot from NVS
 * and on every `ok grain`): recomputes the knobs when they changed */
void grain_update(int seed, float regard, int night, const char *mood);
const grain_knobs_t *grain_knobs(void);
bool  grain_told(void);                     /* a line has been seen (else the default seed) */
int   grain_seed(void);
float grain_regard(void);
const char *grain_mood(void);
bool  grain_night(void);                    /* the desk said night=1 */
void  grain_status(char *out, int cap);     /* `grain` over serial */
/* THE BELLY TRAVELS (design/watch-ideas.md C20, 2026-09-10): one body,
 * one stomach. The grain line may carry `belly=<0..1>` -- the desk's
 * stomach fullness as of the line (ignored when absent; a cached line
 * from NVS never sets it: an hour has passed) -- and the wrist wears it
 * as a RESTING LUMP on the dome, GEL's own term: BELLY_LUMP_X (0.4) x
 * belly at the line, sinking to nothing over BELLY_SINK_S (an hour).
 * main takes the larger of this, the feed's and the walk's lump. */
void  grain_belly(float belly);             /* a live grain line said belly= (0..1, clamped) */
void  grain_tick(float dt);                 /* the belly's sink, once a frame */
float grain_lump(void);                     /* the resting lump now, 0..0.4 */
float grain_belly_now(void);                /* the fullness as it stands (sinking), 0 = none */
/* the derivation itself, for the test: Python's random.Random(f"{seed}:{slime_id}")
 * and its eleven uniform() draws in the order above (doubles, as Python has them) */
void  grain_derive(int seed, int slime_id, double out[11]);
