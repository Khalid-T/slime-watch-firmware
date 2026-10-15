/* THE LULL (design/watch-night-2026-10-12.md C11, the operator night).
 * A stroke is the long pet: a finger drawn across him, the content eyes
 * for their 4 s, the lean into the travel. Three SLOW strokes inside a
 * minute are something else -- a hand petting a creature toward rest.
 * Each must have taken LULL_STROKE_MS 600 ms or more (the detector says
 * how long the finger was down: gesture_stroke_ms); a quick stroke is
 * a stroke as ever and counts for nothing here. The third inside
 * LULL_WINDOW_MS of the first ARMS the lull; it BEGINS only when the
 * third stroke's own content face has ended (main hands over that
 * face's seconds: the lull never fights it, and a fourth slow stroke
 * while it waits moves the wait to its own face). Lulled: the lids
 * drift to DROOPY over LULL_IN_S, the breath's target eases toward the
 * sleeper's 0.30 Hz (rest.cpp reads lull_level in rest_in_t.lulled),
 * idle life goes quiet (lull_on in main's busy line) -- for LULL_S 45 s
 * from the begin, renewed by any slow stroke inside it (the hand is
 * still there), or until a PET (main's pet door), a JOLT (a jerk over
 * LULL_JOLT_G: twice the springs' 0.06, under the shake's 0.18), a WALL
 * change (he fell to another wall) or the DARK (main's go_dark, by
 * hand, as the drowse is told). The time's end eases the lids away over
 * LULL_OUT_S; a pet, a jolt or a wall end it AT ONCE (the lids up: the
 * pet's own engage, or main's for the jolt and the wall). Never the Zs,
 * never `asleep` (the night is the day log's; this is a look), never on
 * the empty glass, never dozing, never a sense up the wire.
 *
 * Pure C, no Arduino: the strokes come in stamped, the frame's facts
 * come in, a level and an eye kind go out; tools/lull_test.cpp runs
 * the edges on the laptop. State: a few words. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gel.h"            /* GEL_EYE_DROOPY */

#ifdef __cplusplus
extern "C" {
#endif

#define LULL_STROKES    3           /* the slow strokes it takes */
#define LULL_STROKE_MS  600         /* a stroke's finger down this long or longer is slow */
#define LULL_WINDOW_MS  60000       /* the three inside this of the first */
#define LULL_S          45.0f       /* the lull's length from its begin; a slow stroke inside it renews it */
#define LULL_IN_S       3.0f        /* the lids drift down over this */
#define LULL_OUT_S      1.5f        /* ... and away over this at the time's end (a pet / jolt / wall: at once) */
#define LULL_JOLT_G     0.12f       /* a jerk over this ends it (the springs' jolt is 0.06 g, the shake 0.18) */

/* what main hands over each frame */
typedef struct {
    float dt;               /* seconds since the last tick */
    bool  lit;              /* the glass is on (awake or dim) */
    bool  holding;          /* the pet is here (house_holding) */
    bool  dozing;           /* rest's asleep or stirring */
    int   wall;             /* the wall he sits on (GEL_WALL_*) */
    float jerk;             /* this frame's jerk, g (main's last_jerk) */
} lull_in_t;

/* what draw() folds in */
typedef struct {
    int   eye_kind;         /* GEL_EYE_DROOPY while the lull shows, -1 = leave the draw's */
    float eye_alpha;        /* the lids' presence 0..1 (0 when not lulled) */
    float level;            /* 0..1 the lull's depth (rest's field, `lull` and the test) */
} lull_fx_t;

/* lull_stroke's word on a stroke */
enum { LULL_STROKE_QUICK = 0,       /* under LULL_STROKE_MS: a stroke as ever, nothing here */
       LULL_STROKE_COUNTED,         /* a slow one, counted (lull_count says how many of the three) */
       LULL_STROKE_ARMED,           /* the third inside the minute: the lull begins after this stroke's face */
       LULL_STROKE_WAITING,         /* a slow one while armed: the wait moves to this stroke's face */
       LULL_STROKE_RENEWED };       /* a slow one inside the lull: its 45 s begin again */

/* lull_tick's event, and the end's cause */
enum { LULL_EV_NONE = 0, LULL_EV_BEGIN = 1, LULL_EV_END = 2 };
enum { LULL_END_NONE = 0, LULL_END_TIME, LULL_END_PET, LULL_END_JOLT, LULL_END_WALL, LULL_END_DARK, LULL_END_GONE };

void  lull_begin(void);                                      /* a clean state (the statics are zero at boot; the test resets) */
int   lull_stroke(uint32_t now_ms, uint32_t held_ms, float face_s);   /* a stroke lifted, HOLDING: how long the finger was down, the content face's seconds it brings; LULL_STROKE_* */
int   lull_tick(const lull_in_t *in);                        /* once a frame: BEGIN when the wait runs out, END when a cause lands (lull_end says which) */
void  lull_pet(void);                                        /* a pet (main's pet door): the lull ends on the next tick, the wait is dropped */
void  lull_dark(void);                                       /* the glass went dark (main's go_dark): taken silently, nothing left */
void  lull_fx(lull_fx_t *out);                               /* this frame's look */
float lull_level(void);                                      /* 0..1 (rest_in_t.lulled) */
bool  lull_on(void);                                         /* lulled now (the 45 s running) */
bool  lull_armed(void);                                      /* the third stroke landed, the face not yet over */
int   lull_count(void);                                      /* slow strokes counted toward the three (0..2) */
int   lull_end(void);                                        /* the last end's cause (LULL_END_*) */
const char *lull_end_name(int cause);                        /* "the time" / "a pet" / "a jolt" / "the wall" / "the dark" / "he left" */

#ifdef __cplusplus
}
#endif
