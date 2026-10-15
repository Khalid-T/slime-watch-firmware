/* The gel, ported from render.py (dome_points / blob_radii / draw_slime)
 * by way of watchface.py, the laptop reference (see README: where the
 * two have parted).
 *
 * PIXEL ART (Khalid's ruling 2026-09-09: "it needs to read a bit more
 * pixelated"): the dome is rastered on the 1x ART CELL -- integer
 * scanline masks (a pixel is in when its centre is, the rule cairo's
 * ANTIALIAS_NONE follows), premultiplied source-over of the desk's own
 * alphas -- and only THEN blitted nearest at GEL_SCALE onto the face:
 * hard 5x5 blocks everywhere, no anti-aliasing, no sub-pixel edge.
 *
 * THE FOUR WALLS (Khalid 2026-09-10, reopening the continuous bowl of
 * 09-09: "I don't like how the slime shape is when it's between angles
 * like top-left" -- pixel art does not rotate): the geometry can still
 * be turned about the BASE CENTRE before the scanline raster (the
 * turn is one frame_t, kept -- one renderer, walls.h pins the angle),
 * but the drawn angle is SNAPPED to GEL_ANGLE_STEP_DEG = 90, and at
 * the exact quarters the frame's sin / cos snap to 0 / 1: the raster IS
 * the quarter blit, every rectangle a rectangle, the outline the
 * sprite's own -- crisp at 6, 3, 12 and 9 o'clock, and it is never
 * drawn anywhere else. The tilt between walls is worn as a lean, a
 * slosh and a slide (walls.h), never as a turned body. The cell is
 * square with the base centre at its middle, so a quarter turn fits
 * without a second layout.
 *
 * THE BASE BENT TO THE BOWL ("the floor is flat but the edge is
 * circular"): the dome's flat base is an arc of the glass's own
 * curvature (GEL_BOWL_R = the rim's radius in art px) -- each point of
 * the geometry rises by that circle's sagitta at its x, so the corners
 * sit up the bowl's sides (~2.5 art px at the chamfered corners) and the
 * edge closing the polygon IS the arc; the shade band under the body,
 * the meniscus and the outline follow it. The chamfer stays.
 *
 * The SHAPE is sacred (sprite-v1): every constant here is render.py's.
 * Plain C, no Arduino: tools/preview.c renders it on the laptop.
 */
#ifndef GEL_H
#define GEL_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the face and the stature (watchface.py: FACE, WATCH_SCALE, FLOOR_DROP) */
#define GEL_FACE        240
#define GEL_SCALE       6         /* art px -> face px: the dome 156 px wide, 96 tall -- 65% of the glass (Khalid, 23:00: "make the slime have more screen space"; was 5 = 54%) */
#define GEL_FLOOR_DROP  106       /* the base centre, px from the face centre along gravity: ON THE GLASS (the bent base meets the rim; 78 left it 42 px inside -- "always centered") */

/* the art cell: square, the BASE CENTRE (the pivot the geometry turns
 * about) at its middle; 32 art px of room every way holds the dome at
 * any angle, squash and lean (the outline reaches ~28 at the widest) */
#define GEL_CELL_W      64
#define GEL_CELL_H      64
#define GEL_CELL_CX     32
#define GEL_CELL_CY     32

/* the bowl the base is bent to: the glass's rim radius, in art px */
#define GEL_BOWL_R      (120.0f / GEL_SCALE)
/* the drawn angle's step (deg): 90 = THE FOUR WALLS, the sprite always
 * axis-aligned (Khalid 2026-09-10). It was 15 (24 directions) for the
 * one night of the continuous bowl; -DGEL_ANGLE_STEP_DEG=15f on a
 * preview build shows what that looked like, nothing on the wrist
 * reads any other value (walls.h pins the place to the wall too) */
#ifndef GEL_ANGLE_STEP_DEG
#define GEL_ANGLE_STEP_DEG 90.0f
#endif

/* render.py: the blob shape */
#define GEL_N_POINTS    34
#define GEL_BASE_RX     14.5f     /* half-width (pre-chamfer) */
#define GEL_BASE_RY     16.0f     /* FULL height */
#define GEL_DOME_N      2.2f      /* >2 = boxier than an ellipse */
#define GEL_CHAMFER     0.26f
#define GEL_CHAMFER_POW 4.0f
/* THE LUMP IS A BELLY (Khalid 2026-09-10 18:55: "when I swipe the slime
 * from top to bottom its head enlarges" -- the desk's crown lump, sin^3
 * at LUMP_AMP 0.60, read as a swollen head on the wrist). The meal
 * WIDENS THE LOWER DOME instead: the flank at th is pushed out (x only,
 * the crown never rises) by GEL_BELLY_AMP x lump x w(th) / w_peak, the
 * weight w = sin(th) (1 - sy)^GEL_BELLY_POW -- zero at the base's ends
 * (the foot stays on the bowl, the chamfer's corners where they were),
 * zero at the crown, its peak ~23 deg up the flank (0.44 of the height,
 * ~7 art px up), a fifth of it by 60 deg, under the eyes. GEL_BELLY_NORM
 * = 1 / w_peak (0.1701 for DOME_N 2.2 at POW 1.5), so GEL_BELLY_AMP IS
 * the flank's widening at its fullest: 0.25 = 25 % wider there at lump
 * 1.0 (+5 art px across the belly, +0 at the crown): fuller, never
 * taller. POW 2 (peak ~19 deg) cornered into a tent, POW 1 (~29 deg)
 * widened the face at eye level -- the head again; 1.5 at 0.25 was the
 * one (meal-preview.png). The silhouette is still the progress bar: it
 * sinks over gesture.cpp's LUMP_S. The desk's own crown term is kept
 * under -DGEL_CROWN_LUMP (a preview build; the desk's renderer is
 * render.py's and is not touched by this). */
#ifndef GEL_BELLY_AMP
#define GEL_BELLY_AMP   0.25f
#endif
#ifndef GEL_BELLY_POW              /* the weight's power on (1 - sy): higher = lower and sharper (a preview build may try another; NORM goes with it) */
#define GEL_BELLY_POW   1.5f
#define GEL_BELLY_NORM  5.88f
#endif
#define GEL_LUMP_AMP    0.60f     /* render.LUMP_AMP: only under GEL_CROWN_LUMP (the head that grew) */
/* Config.GEL_RIPPLE x RIPPLE_CURRENTS[0] ("gentle slosh"): the resting
 * breath of the rim -- "animated, not more movement" */
#define GEL_RIPPLE      0.45f
#define GEL_GEL_HZ      0.12f     /* laps of the resting breath per second */

typedef struct { float r, g, b, a; } gel_rgba_t;
typedef struct { gel_rgba_t body, shade, outline, shine, eye; } gel_pal_t;

/* render.PALETTES["blue"] -- the default, the homage's hair tones */
extern const gel_pal_t GEL_PAL_BLUE;

enum {
    GEL_EYE_OPEN = 0, GEL_EYE_CLOSED, GEL_EYE_SLEEP, GEL_EYE_WIDE,
    GEL_EYE_HAPPY, GEL_EYE_CONTENT, GEL_EYE_DROOPY, GEL_EYE_SQUINT,
};

/* THE FOUR WALLS: the four rim places the gel sits at -- 6, 3, 12, 9
 * o'clock. main.cpp holds the wall itself (walls.h: the hysteresis,
 * the fall); the glance and the word take the wall opposite (place.h);
 * the status page names it */
enum { GEL_WALL_BOTTOM = 0, GEL_WALL_RIGHT = 1, GEL_WALL_TOP = 2, GEL_WALL_LEFT = 3, GEL_WALL_NONE = -1 };
extern const char *const GEL_WALL_NAMES[4];
int gel_wall_of(float phi);            /* the nearest wall to a gravity angle (rad from straight down, + toward the right edge) */

typedef struct {
    float angle;       /* gravity's direction, rad from straight down the cell, positive toward the right (snap it: gel_snap_angle) */
    float squash;      /* authored + spring: clamp(-0.55, 0.70) */
    float lump;        /* 0..1, the engulfed meal under the surface (the feed's, the walk's): THE BELLY, GEL_BELLY_AMP x lump wider at the lower flank, the crown flat */
    float a2, a1;      /* the 2-lobe and 1-lobe harmonics */
    float phase;       /* the a2 lobe's phase */
    float gel;         /* 0..1, the resting-breath lap */
    float eye_alpha;   /* the face's presence, 0 = not there */
    int   eye_kind;    /* GEL_EYE_* */
    int   gaze_x, gaze_y;
} gel_cell_params_t;

/* The art cell: GEL_CELL_W x GEL_CELL_H words of RGB565 in the panel's
 * byte order, the gel composited over the black glass (transparent =
 * black), its base centre at (GEL_CELL_CX, GEL_CELL_CY), the geometry
 * turned by p->angle. The inputs are quantised the way the desk's cell
 * cache quantises them (32 steps of squash/a1/a2/lump, 8 of presence and
 * gel). */
/* Returns 1 when the cell was rastered, 0 when the cache answered (the
 * optimization pass: the quantised inputs, the palette and the buffer
 * are the last call's, so the cell already holds the picture). */
int gel_raster_cell(uint16_t *cell, const gel_pal_t *pal, const gel_cell_params_t *p);
void gel_raster_stats(uint32_t *rastered, uint32_t *cached);   /* the counts since boot */
void gel_raster_forget(void);                                  /* the next call rasters whatever the inputs (the buffer was touched) */
/* THE PAINTED BOX (the optimization pass, reiterated): the cell's box of
 * painted pixels after the last raster (art px; the whole cell before
 * any), and the rect of the face the last gel_compose painted (face px,
 * clipped to the glass; w 0 = nothing) -- the gel's own dirty hint */
void gel_cell_box(int *x, int *y, int *w, int *h);
void gel_face_box(int *x, int *y, int *w, int *h);
/* THE DISC IS THE GLASS (Khalid: "the slime goes outside the screen"):
 * nothing of the gel may be drawn outside it. Given the cell's painted
 * box (the last raster into ``cell``) and the place it would be
 * composed at (``phi`` the wall's angle, ``lift`` / ``slide`` as
 * gel_compose takes them), moves the base centre by the LEAST that puts
 * every painted block inside the rim -- less lift (higher, toward the
 * wall) for a crown past the far rim, less slide (toward the axis) for
 * a flank or a base corner past the side; a fall's drop hangs from the
 * puddle's centroid but no lower than the rim allows. A negative lift
 * (the house's sink, the peekaboo: under the rim on purpose) is left
 * alone. Returns 1 when it moved anything. */
int gel_fit_inside(const uint16_t *cell, float phi, float *lift, float *slide);

/* the drawn angle: the nearest multiple of GEL_ANGLE_STEP_DEG (a wall's
 * own angle passes through unchanged) */
float gel_snap_angle(float angle);

/* The face: the cell blitted nearest at GEL_SCALE onto a GEL_FACE
 * square framebuffer, its base centre GEL_FLOOR_DROP - lift face px from
 * the face centre along ``phi`` (the wall's angle: the place on the
 * rim; a fall's drop is the same call at the new wall with a lift) and
 * ``slide`` face px across it (positive toward the body's right: the
 * residual's slide along the wall, the slosh's few px of sway, the
 * drop's sideways part in a fall). Black outside the round glass. The
 * base centre lands on a whole pixel: the blocks stay blocks. */
void gel_compose(uint16_t *fb, const uint16_t *cell, float phi, float lift, float slide);

/* render.blob_radii: authored squash -> (rx, ry) */
void gel_blob_radii(float squash, float *rx, float *ry);
/* THE STATURE WEEK (daykeep.h, 2026-09-23): the squash that stands the
 * dome ``taller_px`` art px taller (negative = as much shorter and
 * rounder) -- blob_radii's own line read backwards, ry = BASE_RY (1 -
 * 0.45 squash), so a quarter art px is -0.0347 of squash: the drift
 * flows into the one value the raster already takes; 0 -> 0 exactly */
float gel_stature_squash(float taller_px);

#ifdef __cplusplus
}
#endif
#endif
