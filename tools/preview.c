/* Host preview of the gel port: renders frames of the watch face to
 * PPM files so the shape can be checked on the laptop before a flash.
 * Build: cc -O2 -std=gnu99 -o preview tools/preview.c src/gel.c -lm
 * Run:   ./preview outdir
 * Then:  python3 tools/ppm2png.py --cols 6 outdir/strip.png outdir/'*.ppm'
 *
 * THE FOUR WALLS (walls.h): the frames are the rest at each of the four
 * walls square and with the pull 30 deg past the wall (the residual
 * worn as the lean, the slosh and the slide -- the sprite itself never
 * turns), the pull at the 60 deg edge, then a quarter-turn fall from
 * the bottom wall to the right (the peel, the drop at the swap, the
 * arc mid-flight, the impact, the splat, the slump with the residual
 * easing in), the half-turn fall's drop and mid-flight (through the
 * centre, as before), a few moods on the other walls, and the meal
 * (THE LUMP IS A BELLY: 19-21, against 01).
 *
 * The residual's terms come from walls.h's own walls_rest() -- the
 * numbers main.cpp eases toward -- so the preview and the body agree.
 */
#include "../src/gel.h"
#include "../src/walls.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 240
#define H 240

static uint16_t fb[W * H];
static uint16_t cell[GEL_CELL_W * GEL_CELL_H];

static void save(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t v = fb[i];
        uint16_t d = (uint16_t)((v >> 8) | (v << 8));
        int r = (d >> 11) & 0x1f, g = (d >> 5) & 0x3f, b = d & 0x1f;
        unsigned char px[3] = { (unsigned char)((r << 3) | (r >> 2)),
                                (unsigned char)((g << 2) | (g >> 4)),
                                (unsigned char)((b << 3) | (b >> 2)) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

#define DEG(d) ((float)(d) * (float)M_PI / 180.0f)
#define B GEL_WALL_BOTTOM
#define R GEL_WALL_RIGHT
#define T GEL_WALL_TOP
#define L GEL_WALL_LEFT

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[512];
    gel_cell_params_t p;

    /* wall: where it sits; res: the pull past the wall (deg, + toward
     * the body's right; the residual worn as lean / slosh / slide at 1 g,
     * added to the frame's own a1 / a2 / slide); lift / slide in face px */
    struct { const char *name; int wall; float res, squash, a1, a2, eye, gel; int kind; float lift, slide; int gaze; float lump; } frames[] = {
        { "01-rest-bottom",        B,   0, -0.15f, 0.0f,  0.0f, 1.0f, 0.10f, GEL_EYE_OPEN,   0,  0, 0 },
        { "02-rest-bottom-tilt30", B,  30, -0.15f, 0.0f,  0.0f, 1.0f, 0.35f, GEL_EYE_OPEN,   0,  0, 2 },
        { "03-rest-right-tilt30",  R,  30, -0.15f, 0.0f,  0.0f, 1.0f, 0.60f, GEL_EYE_OPEN,   0,  0, 2 },
        { "04-rest-top-tilt30",    T,  30, -0.15f, 0.0f,  0.0f, 1.0f, 0.80f, GEL_EYE_OPEN,   0,  0, 2 },
        { "05-rest-left-tilt30",   L,  30, -0.15f, 0.0f,  0.0f, 1.0f, 0.20f, GEL_EYE_OPEN,   0,  0, 2 },
        { "06-rest-bottom-tilt-58",B, -58, -0.15f, 0.0f,  0.0f, 1.0f, 0.50f, GEL_EYE_OPEN,   0,  0, -2 },
        /* the quarter-turn fall, bottom -> right: the peel on the bottom
         * wall (the creep along it, PEEL_SQUASH over the wrist's posture),
         * the drop hung at the swap (lift 32, 71 across as the centroid
         * gives it -- gel_fit_inside brings it to 49, inside the rim), the
         * arc mid-flight, the impact
         * on the right wall, the splat, the slump (the residual easing in) */
        { "07-fall-peel",          B,   0,  0.35f, 0.30f, 0.0f, 1.0f, 0.70f, GEL_EYE_WIDE,   0,   7, 2 },
        { "08-fall-drop",          R,   0, -0.50f, 0.0f,  0.0f, 1.0f, 0.72f, GEL_EYE_WIDE,   32, -71, 0 },
        { "09-fall-arc",           R,   0, -0.22f, 0.0f,  0.0f, 1.0f, 0.74f, GEL_EYE_WIDE,   16, -38, 0 },
        { "10-fall-impact",        R,   0, -0.25f, 0.0f,  0.0f, 1.0f, 0.75f, GEL_EYE_WIDE,    0,   0, 0 },
        { "11-fall-splat",         R,   0,  0.48f, 0.0f,  0.0f, 1.0f, 0.78f, GEL_EYE_SQUINT,  0,   0, 0 },
        { "12-fall-slump",         R,  20,  0.12f, 0.0f,  0.0f, 1.0f, 0.82f, GEL_EYE_OPEN,    0,   0, 0 },
        /* the half-turn fall, bottom -> top: the drop starts past the
         * centre on the axis and goes straight through it */
        { "13-flip-drop",          T,   0, -0.50f, 0.0f,  0.0f, 1.0f, 0.72f, GEL_EYE_WIDE,   118, -15, 0 },
        { "14-flip-flight",        T,   0, -0.22f, 0.0f,  0.0f, 1.0f, 0.74f, GEL_EYE_WIDE,   55,  -5, 0 },
        /* a few more moods, each on its wall */
        { "15-pet-right",          R,   0,  0.32f, 0.0f,  0.0f, 1.0f, 0.20f, GEL_EYE_HAPPY,  0,  0, 0 },
        { "16-sleep-top",          T, -10,  0.30f, 0.0f,  0.0f, 1.0f, 0.00f, GEL_EYE_SLEEP,  0,  0, 0 },
        { "17-noeyes-left",        L,   0,  0.00f, 0.0f,  0.0f, 0.0f, 0.90f, GEL_EYE_OPEN,   0,  0, 0 },
        { "18-stretch-bottom",     B,   0, -0.45f, 0.0f,  0.10f, 1.0f, 0.50f, GEL_EYE_WIDE,   0,  0, 0 },
        /* THE LUMP IS A BELLY (Khalid 2026-09-10 18:55): the meal at the
         * rest's own pose (01 is the empty one), half dissolved and just
         * fed, and just fed on the right wall -- the lower flank fuller,
         * the crown where it was; meal-preview.png puts the four side by side */
        { "19-meal-bottom-half",   B,   0, -0.15f, 0.0f,  0.0f, 1.0f, 0.10f, GEL_EYE_OPEN,   0,  0, 0, 0.5f },
        { "20-meal-bottom-full",   B,   0, -0.15f, 0.0f,  0.0f, 1.0f, 0.10f, GEL_EYE_HAPPY,  0,  0, 0, 1.0f },
        { "21-meal-right-full",    R,   0, -0.15f, 0.0f,  0.0f, 1.0f, 0.60f, GEL_EYE_HAPPY,  0,  0, 0, 1.0f },
    };
    for (unsigned i = 0; i < sizeof frames / sizeof frames[0]; i++) {
        float phi = walls_phi(frames[i].wall);
        float a1 = 0, a2 = 0, sl = 0;
        walls_rest(DEG(frames[i].res), 1.0f, &a1, &a2, &sl);
        memset(&p, 0, sizeof p);
        p.angle = gel_snap_angle(phi);
        p.squash = frames[i].squash;
        p.a1 = frames[i].a1 + a1;
        p.a2 = frames[i].a2 + a2;
        p.phase = (float)M_PI / 2.0f;
        p.eye_alpha = frames[i].eye;
        p.gel = frames[i].gel;
        p.eye_kind = frames[i].kind;
        p.gaze_x = frames[i].gaze;
        p.lump = frames[i].lump;
        gel_raster_cell(cell, &GEL_PAL_BLUE, &p);
        /* THE DISC IS THE GLASS: the drop hangs no lower than the rim allows, the
         * puddle's creep and the slide keep every block inside (gel_fit_inside,
         * the body's own call): a frame the wrist could never show is not shown */
        float lift = frames[i].lift, slide = frames[i].slide + sl;
        if (gel_fit_inside(cell, phi, &lift, &slide))
            printf("  %s: fitted inside the disc (lift %.0f -> %.0f, slide %.0f -> %.0f)\n", frames[i].name,
                   frames[i].lift, lift, frames[i].slide + sl, slide);
        gel_compose(fb, cell, phi, lift, slide);
        /* the glass: show the round face as a dark grey disc where the gel is not */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float dx = x + 0.5f - 120.0f, dy = y + 0.5f - 120.0f;
                if (fb[y * W + x] == 0 && dx * dx + dy * dy <= 120.0f * 120.0f)
                    fb[y * W + x] = (uint16_t)((0x18E3 >> 8) | (0x18E3 << 8));
            }
        snprintf(path, sizeof path, "%s/%s.ppm", dir, frames[i].name);
        save(path);
        printf("%s\n", path);
    }
    return 0;
}
