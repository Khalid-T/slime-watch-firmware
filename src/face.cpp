/* THE WORDLESS WATCH FACE -- face.h says what; this is how. */
#include "face.h"
#include "hourfmt.h"
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */
#include "gauge.h"
#include "mark.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* THE MARK (mark.h, the optimization pass reiterated): what this frame
 * drew, against last frame's; main.cpp takes it after the draws */
static mark_t mk;
bool face_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

/* the ring tables' memory: PSRAM on the board (main.cpp's strong
 * definition), malloc on the laptop */
void * __attribute__((weak)) face_mem(size_t n) { return malloc(n); }

/* HARDWARE ROOM: the drop prefers the MAX17048's millivolts when the
 * gauge is soldered and answers (gauge.cpp's strong definition wins on
 * the board); on the laptop, where no gauge.cpp is linked, this weak one
 * hands the ADC's number straight back. */
uint16_t __attribute__((weak)) gauge_prefer(uint16_t adc_mv) { return adc_mv; }

/* --- the tunables, with their cause ------------------------------------------ */
/* the glass: 240 px across, the rim at radius 120 (gel_compose's round mask) */
static const float RIM_R        = 120.0f;
/* the rings live in the band the gel's base never reaches: its base
 * centre rests GEL_FLOOR_DROP (106) px from the centre and the outline
 * bends up the bowl from there, so nothing of the gel is past ~112.
 * THE RADIUS IS ONE KNOB: FACE_DAY_R_OUT is the day ring's outer edge
 * (r in [DAY_R_OUT - RING_W, DAY_R_OUT)); the night ring sits RING_GAP
 * inside it, so both move together. 118 leaves 2 px to the rim -- the
 * bezel may eat the last pixel, and Pebble Round's guide (design/
 * watch-inspiration.md, "two pixels of bezel, never a thin ring at the
 * edge; manufacturing variations may cause them to be visibly
 * off-center") says a thin edge ring is the first thing a bezel ruins.
 * If it does on this glass, the honest fix is THICKER AND INSET, never
 * brighter: -DFACE_DAY_R_OUT=114 -DFACE_RING_W=4 on the build line (or
 * here), and the night ring follows. Khalid's to read on the wrist. */
#ifndef FACE_DAY_R_OUT
#define FACE_DAY_R_OUT 118.0f
#endif
#ifndef FACE_RING_W
#define FACE_RING_W 2.0f
#endif
static const float DAY_R_OUT    = FACE_DAY_R_OUT;
static const float RING_W       = FACE_RING_W;
static const float RING_GAP     = 1.0f;
/* the norm: the median of the last 7 days' totals. NOTHING until 3 days
 * exist (the optimization pass, from design/watch-inspiration.md: a norm
 * from fewer days is a guess, and the ring against a guess is a mark
 * about nothing -- so no day ring at all, and face_norm() is 0, which
 * the tell reads as "no norm known"); a day under NORM_MIN_STEPS was the
 * watch on a shelf, not his day, and is left out */
static const int      NORM_DAYS       = 7;
static const int      NORM_MIN_DAYS   = 3;
static const uint32_t NORM_MIN_STEPS  = 200;
/* THE SWEEP (the optimization pass; watch-inspiration.md, Garmin's rule
 * inverted: "the ten seconds after a raise are the whole show -- the
 * rings should ARRIVE on the raise rather than be there, the day ring
 * sweeping in from twelve over the first half second, the night ring
 * the other way"): on a raise both arcs grow from 12 o'clock to their
 * length over SWEEP_S, eased out (quick to start, settling at the end),
 * so a glance reads the day as a motion, not a diagram. The dock's
 * progress arc and the blink are not swept (the courier is not a fact
 * about the day). */
static const float SWEEP_S      = 0.6f;
/* SHOWING SOMEONE (design/watch-ideas.md B18, 2026-09-23): while the
 * glass is held toward someone (face_in_t.company: wake.cpp's showing
 * pose) the day ring, the night ring, the hour and minute ticks and the
 * alarm's tick are at COMPANY_K of their ink -- his day is his; the
 * hour text is not touched (company may read the time). Eased over
 * COMPANY_EASE_S both ways. */
static const float COMPANY_K      = 0.4f;
static const float COMPANY_EASE_S = 0.4f;
/* the day ring's ink: the body colour dimmed (it sits behind him); the
 * shine once the norm is met -- brighter, never a second lap */
static const float DAY_K        = 0.55f;
/* the night ring against a full night */
static const float NIGHT_H      = 8.0f;
static const float NIGHT_K      = 0.85f;    /* the shade colour, most of it */
/* THE NIGHT RING KNOWS HIS HEART (watch-ideas D33 + D36, 2026-10-08;
 * nightsum.c judges, main hands the two verdicts over): a DEEP night
 * (the nocturnal dip was there) draws the night ring in the SHINE at
 * NIGHT_DEEP_K -- the day ring's own rule at the norm, the night ring's
 * one brightening, the same arc; a week whose sleep midpoints WANDER
 * breathes the ring's TIP: its last STEADY_TIP_PX of arc fades between
 * the ring's ink and dark at STEADY_BREATH_HZ (a ten-second breath --
 * the slowest thing on the glass; the gel's breath is 0.3-0.8 Hz), the
 * ink in STEADY_TIP_STEPS steps so the mark moves 16 times a period
 * (every ~0.6 s) and not every frame: each move is one scan under the
 * face's box (the glass, as for any ring) and a push of the tile or two
 * the diff finds -- under the once-a-second safety scan's cost.
 * Rulings 86-87. */
static const float NIGHT_DEEP_K     = 1.0f;
static const float STEADY_BREATH_HZ = 0.1f;
static const float STEADY_TIP_PX    = 1.0f;
static const int   STEADY_TIP_STEPS = 8;
/* the resting hour while EMPTY: the glance's 3x5 font at the glance's
 * 3 px (2 px is 1.4 mm tall on this glass -- unreadable at arm's
 * length), so it is DIMMER instead of smaller: the shade colour;
 * a raise lifts it to the shine for the glance's spell, easing back */
static const int   HOUR_PX      = 3;
static const float HOUR_EASE_S  = 0.5f;
/* the drop: the water lock's 5x7 glyph at 3 px, its centre DROP_R
 * below the middle -- inside the rings, over the gel's floor at 6
 * o'clock (he covers it when he rests there: the marks are behind him) */
static const int   DROP_PX      = 3;
static const int   DROP_R       = 96;
/* the cell: empty at DROP_EMPTY_MV, full at DROP_FULL_MV; at the
 * charger's float or above (CABLE_MV: the board with no cell reads
 * ~4130 mV on USB, a full cell on the cable the same) there is nothing
 * to say, so nothing is drawn; between main's CHARGE_MV and that it is
 * a full drop pulsing slowly -- the cell drinking */
static const int   DROP_EMPTY_MV = 3300;
static const int   DROP_FULL_MV  = 4100;
static const int   CABLE_MV      = 4100;
static const float PULSE_S       = 2.0f;
static const float PULSE_LOW     = 0.55f;
static const float DROP_EDGE_K   = 0.22f;   /* the empty part: the glyph's edge only, dim */
/* the trace: a film of gel left on the glass -- the BODY's tint thinned
 * (the outline colour at a quarter is black on this panel), fading over
 * an hour. It stays where it was on the GLASS (a residue turns with the
 * glass, not with gravity) and the arrival takes it back. */
static const float TRACE_S      = 3600.0f;
static const float TRACE_K      = 0.25f;
/* THE TRACE STIRRED (watch-night C14): a tap on the empty glass within
 * TRACE_STIR_MARGIN of the trace's box makes the residue shiver -- its
 * tint once from TRACE_K toward TRACE_STIR_K and back on a half sine
 * over TRACE_STIR_S, scaled by the hour's fade as the trace is. Nothing
 * else: no gel, no rise; the knocks still mean come. */
static const float TRACE_STIR_S      = 0.6f;
static const float TRACE_STIR_K      = 0.45f;
static const int   TRACE_STIR_MARGIN = 12;
/* THE DOCK FACE: while a day is carried up, the day ring is the
 * courier's progress (the shine, bytes sent / the day's bytes, over the
 * steps); the box's `ok day` is one BLINK_S brightening of the ring;
 * on the USB dock a 5x5 plug glyph at DOCK_PX blocks, its centre DOCK_R
 * below the middle -- just inside the rings (the night ring's inner
 * edge is at 113; 104 + 7.5 = 111.5), the shade colour. The drop's spot
 * (r 96, rows up to 106.5) would overlap it, so while docked the drop
 * yields: the dock feeds the cell -- the plug tells that story once. */
static const float BLINK_S      = 0.3f;
static const int   DOCK_PX      = 3;
static const int   DOCK_R       = 104;
/* THE HOUR TICK (design/watch-ideas.md B11, built 2026-09-10): the
 * round face's oldest trick and the one thing the glass lacks once the
 * digits fade -- and not a number. A radial tick on the rim at the
 * hour hand's angle (12 o'clock = up, the WATCH frame: the marks are
 * pinned like the hour), HOUR_TICK_W 2 px wide and HOUR_TICK_LEN long,
 * in the shade colour, there whenever the glass is lit and the clock is
 * known (holding or empty). Both ticks end at TICK_R_OUT 112, just
 * inside the night ring's inner edge (113): the band the rings leave
 * free and the gel only covers when it rests there (behind him, the
 * face's rule). Nothing else: no digits, no hands.
 *
 * THE MINUTE TICK (watch-ideas B12, built 2026-10-09; it was a fainter
 * shade tick of 3 px, half the hour's ink -- a hint, not a hand): the
 * SHINE, MINUTE_TICK_W 1 px wide, MINUTE_TICK_LEN 6 long -- one px
 * longer than the hour's, from r 106. The hands' own grammar does the
 * telling apart: the hour short, stout and dim; the minute long, thin
 * and bright. One px, not two, because the shine already carries the
 * ink two px of the shade would (the alarm's tick is the shine AND 2
 * px wide so it reads as a MARK, a thing set, not a hand), and a 2 px
 * shine hand would outweigh the hour's and make the alarm's tick a
 * third of the same kind. Six and not more: r 106 is the last radius
 * clear of the drop's top row (y 206-225 at 6 o'clock) and the dock's
 * plug, so no hand ever lands on a glyph. The analog face with no
 * hands, readable at arm's length in sun where the 3x5 digits are not;
 * whether the digits should then go dimmer or sooner is ruling 90's. */
static const float TICK_R_OUT    = 112.0f;
/* THE REACHING CUE: a ripple a second at the wall he will rise at -- a
 * short arc of the rim band widening from REACH_HALF0 to REACH_HALF1
 * (half-widths, rad) and fading over REACH_RIPPLE_S, quiet for the rest
 * of the second; the shine, dimmed to REACH_K at its brightest */
static const float REACH_PERIOD_S = 1.0f;
static const float REACH_RIPPLE_S = 0.6f;
static const float REACH_HALF0    = 0.07f;      /* ~4 deg */
static const float REACH_HALF1    = 0.35f;      /* ~20 deg */
static const float REACH_K        = 0.9f;
static const int   HOUR_TICK_LEN   = 5;
static const int   HOUR_TICK_W     = 2;
static const int   MINUTE_TICK_LEN = 6;      /* a px longer than the hour's: the longer hand (B12) */
static const int   MINUTE_TICK_W   = 1;      /* the thinner hand; the shine carries the ink (ruling 90) */
/* THE RIM AS THE DIAL (watch-ideas B13): the hour tick under the
 * finger; on the lift two blinks (off-on-off-on, DIAL_BLINK_S a half)
 * mark the alarm set, a fade over DIAL_FADE_S marks it cleared */
static const int   DIAL_BLINKS   = 2;
static const float DIAL_BLINK_S  = 0.15f;
static const float DIAL_FADE_S   = 0.6f;
/* THE ALARM'S TICK (watch-ideas B14, 2026-09-10): while an alarm is set,
 * the hour tick's own shape at the alarm's hour on the 12-hour dial, in
 * the SHINE -- the hour's tick is the shade, so the two read apart at a
 * glance: where the hand is, where his morning is. It is the tick the
 * dial's lift blinks into being (the blink is in the shine now) and the
 * clear's fade takes away; `alarm off` takes it too. Nothing else: not a
 * number, not a bell. */
static const int   ALARM_TICK_LEN = HOUR_TICK_LEN;
static const int   ALARM_TICK_W   = HOUR_TICK_W;

/* --- the state ----------------------------------------------------------------- */
static bool     on = true;
static float    t_s = 0;                    /* the pulse's clock */
static float    hour_k = 0;                 /* 0 = the shade, 1 = the shine (eased) */
static uint32_t norm = 0;                   /* 0 = no norm known (under NORM_MIN_DAYS days): no day ring */
static int      norm_days = 0;              /* days the norm stands on */
static float    sweep_t = 1e9f;             /* the sweep's clock, seconds since the raise (past SWEEP_S = the rings at rest) */
static float    company_k = 0;              /* SHOWING SOMEONE: 0 = his rings at their ink, 1 = dimmed to COMPANY_K (eased) */
static float ck(float k) { return k * (1.0f - (1.0f - COMPANY_K) * company_k); }   /* an ink factor under company */
static float    reach_t = 0;                /* THE REACHING CUE's clock: seconds since the reach began (0 at each start) */
static bool     reach_was = false;
static uint8_t *trace = NULL;
static bool     trace_on = false, trace_pending = false;
static float    trace_age = 0;
static uint32_t trace_px = 0;
static face_in_t in;                        /* the last tick's numbers, for the draw and the status */
static char     logline[96];
static char     hour_text[12] = "";
static float    blink_t = 0;                /* > 0: the ring's one brightening after `ok day` */
static uint32_t done_seq_seen = 0;
static int      trace_x0 = 0, trace_y0 = 0, trace_x1 = 0, trace_y1 = 0;   /* the trace's box (x1/y1 exclusive) */
static uint32_t trace_gen = 0;              /* counts the traces taken: the mark's identity for one */
static float    stir_t = 1e9f;              /* THE TRACE STIRRED: seconds since the tap (past TRACE_STIR_S = the residue at rest) */
/* THE RIM AS THE DIAL */
enum { DIAL_NONE = 0, DIAL_FOLLOW, DIAL_BLINK, DIAL_FADE };
static int      dial_mode = DIAL_NONE;
static int      dial_minute = -1;           /* the hour tick's minute while not the clock's */
static float    dial_t = 0;                 /* the blink's / the fade's clock */

/* --- the font: the glance's 3x5 digits and colon (glance.cpp keeps its
 * own; this module has no Arduino and cannot borrow them) ---------------------- */
static const uint8_t DIGITS[10][5] = {
    { 0b111, 0b101, 0b101, 0b101, 0b111 }, { 0b010, 0b110, 0b010, 0b010, 0b111 },
    { 0b111, 0b001, 0b111, 0b100, 0b111 }, { 0b111, 0b001, 0b111, 0b001, 0b111 },
    { 0b101, 0b101, 0b111, 0b001, 0b001 }, { 0b111, 0b100, 0b111, 0b001, 0b111 },
    { 0b111, 0b100, 0b111, 0b101, 0b111 }, { 0b111, 0b001, 0b010, 0b010, 0b010 },
    { 0b111, 0b101, 0b111, 0b101, 0b111 }, { 0b111, 0b101, 0b111, 0b001, 0b111 },
};
static const uint8_t COLON[5] = { 0b000, 0b010, 0b000, 0b010, 0b000 };
/* the water lock's drop (main.cpp's GLYPH_DROP), the same family */
static const uint8_t GLYPH_DROP[7] = { 0b00100, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110 };
/* THE DOCK FACE's plug: two prongs up, the body, the cable down */
static const uint8_t GLYPH_PLUG[5] = { 0b01010, 0b11111, 0b11111, 0b01110, 0b00100 };

/* --- the ink ----------------------------------------------------------------- */
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline uint16_t ink(const gel_rgba_t *c, float k)
{
    int R = (int)(c->r * k * 31 + 0.5f), G = (int)(c->g * k * 63 + 0.5f), B = (int)(c->b * k * 31 + 0.5f);
    uint16_t v = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((v >> 8) | (v << 8));            /* the panel's byte order */
}

/* a mark goes only where the glass is black: behind him */
static inline void put(uint16_t *fb, int x, int y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= FACE_W || y >= FACE_H) return;
    uint16_t *p = fb + y * FACE_W + x;
    if (*p == 0) *p = c;
}

static void block(uint16_t *fb, int x, int y, int s, uint16_t c)
{
    mark_rect(&mk, x, y, s, s, c);
    for (int yy = y; yy < y + s; yy++)
        for (int xx = x; xx < x + s; xx++) put(fb, xx, yy, c);
}

/* --- the norm ---------------------------------------------------------------- */
void face_set_history(const uint32_t *totals, int n)
{
    uint32_t v[NORM_DAYS];
    int m = 0;
    for (int i = 0; i < n && m < NORM_DAYS; i++)
        if (totals[i] >= NORM_MIN_STEPS) v[m++] = totals[i];
    norm_days = m;
    if (m < NORM_MIN_DAYS) { norm = 0; return; }        /* not a norm yet: nothing drawn, nothing told */
    for (int i = 1; i < m; i++) {                      /* a sort of seven */
        uint32_t x = v[i]; int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    norm = (m & 1) ? v[m / 2] : (v[m / 2 - 1] + v[m / 2]) / 2;
    if (norm < NORM_MIN_STEPS) norm = NORM_MIN_STEPS;
}

uint32_t face_norm(void) { return norm; }
int face_norm_days(void) { return norm_days; }

/* THE SWEEP: a raise starts the rings from 12 o'clock */
void face_raised(void) { sweep_t = 0; }
/* the sweep's fraction of the arcs' length: 1 - (1 - t)^3, quick to
 * start and settling at the end; 1 once it is over */
static float sweep_k(void)
{
    if (sweep_t >= SWEEP_S) return 1.0f;
    float u = 1.0f - sweep_t / SWEEP_S;
    return 1.0f - u * u * u;
}

/* --- the frame ------------------------------------------------------------------ */
void face_begin(uint8_t *trace_bits, bool on_)
{
    trace = trace_bits;
    on = on_;
    trace_on = trace_pending = false;
    t_s = 0; hour_k = 0;
    sweep_t = 1e9f;                             /* a boot is not a raise: the rings are simply there */
    blink_t = 0; done_seq_seen = 0;
    dial_mode = DIAL_NONE; dial_minute = -1; dial_t = 0;
    memset(&in, 0, sizeof in);
    in.minute = -1;
    in.alarm_min = -1;
    logline[0] = 0;
}

void face_set_on(bool o) { on = o; }
bool face_on(void) { return on; }

const char *face_tick(const face_in_t *i)
{
    float dt = i->dt > 0 ? (i->dt < 0.25f ? i->dt : 0.25f) : 0;
    in = *i;
    in.batt_mv = gauge_prefer(in.batt_mv);      /* the fuel gauge's mV when it is there (HARDWARE ROOM) */
    t_s += dt;
    if (t_s > 3600.0f) t_s -= 3600.0f;
    if (sweep_t < SWEEP_S) sweep_t += dt;      /* THE SWEEP's clock (only lit frames tick: the raise is a lit moment) */
    /* THE REACHING CUE's clock: from the reach's first frame; a period a ripple */
    if (in.reaching) { reach_t = reach_was ? reach_t + dt : 0; if (reach_t >= REACH_PERIOD_S) reach_t -= REACH_PERIOD_S; }
    reach_was = in.reaching;
    float want = (!in.holding && in.glance) ? 1.0f : 0.0f;
    hour_k += (want - hour_k) * clampf(dt / (HOUR_EASE_S / 3.0f), 0, 1);
    company_k += ((in.company ? 1.0f : 0.0f) - company_k) * clampf(dt / (COMPANY_EASE_S / 3.0f), 0, 1);   /* SHOWING SOMEONE */
    if (company_k < 0.005f && !in.company) company_k = 0;
    if (in.minute >= 0) hour_fmt(hour_text, sizeof hour_text, in.minute, in.h12, false);   /* MORE SETTINGS' CLOCK row: `7:41` or `07:41` (hourfmt.h) */
    else hour_text[0] = 0;
    const char *say = NULL;
    /* THE DOCK FACE: the box's `ok day` -- the ring's one blink */
    if (in.sync_done_seq != done_seq_seen) {
        done_seq_seen = in.sync_done_seq;
        blink_t = BLINK_S;
        snprintf(logline, sizeof logline, "face: the day came home: the ring blinks once");
        say = logline;
    }
    if (blink_t > 0) { blink_t -= dt; if (blink_t < 0) blink_t = 0; }
    /* THE RIM AS THE DIAL: the blink's and the fade's clocks; over, the tick is the clock's again */
    if (dial_mode == DIAL_BLINK || dial_mode == DIAL_FADE) {
        dial_t += dt;
        float over = dial_mode == DIAL_BLINK ? DIAL_BLINK_S * 2.0f * DIAL_BLINKS : DIAL_FADE_S;
        if (dial_t >= over) { dial_mode = DIAL_NONE; dial_minute = -1; }
    }
    /* the trace: the departure arms it (the mask is read off the next
     * frame, the gel still at rest as the sink begins); the arrival
     * takes it back */
    if (in.house_event == 2 /* HOUSE_EV_LEFT */ && trace) { trace_pending = true; }
    if (in.house_event == 1 /* HOUSE_EV_ARRIVED */ && (trace_on || trace_pending)) {
        trace_on = trace_pending = false;
        snprintf(logline, sizeof logline, "face: the trace taken back (the arrival)");
        say = logline;
    }
    if (trace_on) {
        trace_age += dt;
        if (trace_age >= TRACE_S) {
            trace_on = false;
            snprintf(logline, sizeof logline, "face: the trace has faded");
            say = logline;
        }
    }
    /* THE TRACE STIRRED: the tap's frame starts the pulse's clock; a second tap starts it over */
    if (in.stir && trace_on) stir_t = 0;
    else if (stir_t < TRACE_STIR_S) stir_t += dt;
    return say;
}

/* the silhouette: every pixel the gel left lit, as bits */
static void trace_take(const uint16_t *fb)
{
    memset(trace, 0, FACE_TRACE_BYTES);
    trace_px = 0;
    trace_x0 = FACE_W; trace_y0 = FACE_H; trace_x1 = 0; trace_y1 = 0;
    for (int i = 0; i < FACE_W * FACE_H; i++)
        if (fb[i]) {
            trace[i >> 3] |= (uint8_t)(1 << (i & 7)); trace_px++;
            int x = i % FACE_W, y = i / FACE_W;
            if (x < trace_x0) trace_x0 = x;
            if (x + 1 > trace_x1) trace_x1 = x + 1;
            if (y < trace_y0) trace_y0 = y;
            if (y + 1 > trace_y1) trace_y1 = y + 1;
        }
    trace_on = trace_px > 0;
    trace_gen++;
    trace_age = 0;
    trace_pending = false;
    stir_t = 1e9f;
    snprintf(logline, sizeof logline, "face: the trace left on the glass (%lu px, an hour to fade)", (unsigned long)trace_px);
}

static void draw_trace(uint16_t *fb)
{
    float k = TRACE_K * (1.0f - trace_age / TRACE_S);
    /* THE TRACE STIRRED: the shiver, a half sine from the quarter toward TRACE_STIR_K and back, faded as the trace is */
    if (stir_t < TRACE_STIR_S) k += (TRACE_STIR_K - TRACE_K) * (1.0f - trace_age / TRACE_S) * sinf((float)M_PI * stir_t / TRACE_STIR_S);
    if (k <= 0.01f) return;
    uint16_t c = ink(&theme_pal()->body, k);
    if (c == 0) return;
    /* the mark: the trace's pixels are one bitmap (its generation) in one ink, over its box */
    mark_val(&mk, trace_gen); mark_val(&mk, c);
    mark_box(&mk, trace_x0, trace_y0, trace_x1 - trace_x0, trace_y1 - trace_y0);
    for (int b = 0; b < FACE_TRACE_BYTES; b++) {
        uint8_t bits = trace[b];
        if (!bits) continue;
        for (int k8 = 0; k8 < 8; k8++)
            if (bits & (1 << k8)) { int i = b * 8 + k8; if (fb[i] == 0) fb[i] = c; }
    }
}

/* an arc of the annulus [r_out - RING_W, r_out): frac of the full turn
 * from 12 o'clock, clockwise or counter-clockwise on the glass (screen
 * y down: clockwise as seen is up -> right -> down -> left). Only the
 * annulus's own pixels are visited, by the chords of each row. Pixel
 * centres at half-pixels, as gel_compose's round mask. */
/* THE RING TABLES (the optimization pass, reiterated): the annulus's
 * pixels and each one's angle, computed ONCE per radius by the very
 * loops below (the same floats: atan2f of the same half-pixel centres),
 * so a frame's arc is a walk of the table with one compare a pixel
 * instead of ~1,500 atan2f calls a ring (two rings a frame, ~3 ms of
 * the S3's time, and the icache's). ~12 KB a ring (face_mem: PSRAM on
 * the board); a radius whose table cannot be had draws the slow way. */
typedef struct { float ang; uint16_t off; } ring_px_t;
typedef struct { float r_out; ring_px_t *px; int n; bool tried; } ring_t;
static ring_t rings[2];

/* the annulus's pixels in scan order, each with its angle from 12
 * o'clock clockwise; fill = NULL counts them */
static int ring_walk(float r_out, ring_px_t *fill)
{
    float r_in = r_out - RING_W;
    const float C = FACE_W / 2.0f;
    int n = 0;
    for (int y = 0; y < FACE_H; y++) {
        float dy = y + 0.5f - C;
        float wo2 = r_out * r_out - dy * dy;
        if (wo2 <= 0) continue;
        float wo = sqrtf(wo2);
        float wi2 = r_in * r_in - dy * dy;
        float wi = wi2 > 0 ? sqrtf(wi2) : 0;
        for (int side = 0; side < 2; side++) {
            int xa = side ? (int)ceilf(C + wi - 0.5f) : (int)ceilf(C - wo - 0.5f);
            int xb = side ? (int)ceilf(C + wo - 0.5f) : (int)ceilf(C - wi - 0.5f);
            for (int x = xa; x < xb; x++) {
                float dx = x + 0.5f - C;
                float r2 = dx * dx + dy * dy;
                if (r2 >= r_out * r_out || r2 < r_in * r_in) continue;
                if (x < 0 || y < 0 || x >= FACE_W || y >= FACE_H) continue;   /* put()'s own bound: never inside the glass */
                if (fill) {
                    float ang = atan2f(dx, -dy);              /* 0 at 12 o'clock, + clockwise */
                    if (ang < 0) ang += 2.0f * (float)M_PI;
                    fill[n].ang = ang;
                    fill[n].off = (uint16_t)(y * FACE_W + x);
                }
                n++;
            }
        }
    }
    return n;
}

static ring_t *ring_get(float r_out)
{
    for (int i = 0; i < 2; i++) {
        ring_t *r = &rings[i];
        if (r->tried && r->r_out == r_out) return r;
        if (!r->tried) {
            r->tried = true; r->r_out = r_out; r->px = NULL;
            r->n = ring_walk(r_out, NULL);
            if (r->n > 0) r->px = (ring_px_t *)face_mem(sizeof(ring_px_t) * (size_t)r->n);
            if (r->px) ring_walk(r_out, r->px);
            return r;
        }
    }
    return NULL;
}

static void arc(uint16_t *fb, float r_out, float frac, bool cw, uint16_t c, bool over)
{
    if (frac <= 0) return;
    if (frac > 1) frac = 1;
    float r_in = r_out - RING_W;
    float limit = frac * 2.0f * (float)M_PI;
    const float C = FACE_W / 2.0f;
    /* the mark: an arc's pixels are a function of these alone; its box the annulus's */
    mark_f(&mk, r_out); mark_f(&mk, limit); mark_val(&mk, (uint32_t)cw | ((uint32_t)over << 1)); mark_val(&mk, c);
    mark_box(&mk, (int)floorf(C - r_out), (int)floorf(C - r_out), (int)ceilf(2.0f * r_out) + 1, (int)ceilf(2.0f * r_out) + 1);
    ring_t *rg = ring_get(r_out);
    if (rg && rg->px) {
        const ring_px_t *e = rg->px;
        const float full = 2.0f * (float)M_PI;
        for (int i = 0, n = rg->n; i < n; i++) {
            float ang = e[i].ang;
            if (!cw) ang = full - ang;
            if (ang > limit) continue;
            uint16_t *p = fb + e[i].off;
            if (over || *p == 0) *p = c;
        }
        return;
    }
    for (int y = 0; y < FACE_H; y++) {
        float dy = y + 0.5f - C;
        float wo2 = r_out * r_out - dy * dy;
        if (wo2 <= 0) continue;
        float wo = sqrtf(wo2);
        float wi2 = r_in * r_in - dy * dy;
        float wi = wi2 > 0 ? sqrtf(wi2) : 0;
        for (int side = 0; side < 2; side++) {
            /* the left chord piece [C - wo, C - wi), the right [C + wi, C + wo) */
            int xa = side ? (int)ceilf(C + wi - 0.5f) : (int)ceilf(C - wo - 0.5f);
            int xb = side ? (int)ceilf(C + wo - 0.5f) : (int)ceilf(C - wi - 0.5f);
            for (int x = xa; x < xb; x++) {
                float dx = x + 0.5f - C;
                float r2 = dx * dx + dy * dy;
                if (r2 >= r_out * r_out || r2 < r_in * r_in) continue;
                float ang = atan2f(dx, -dy);              /* 0 at 12 o'clock, + clockwise */
                if (ang < 0) ang += 2.0f * (float)M_PI;
                if (!cw) ang = 2.0f * (float)M_PI - ang;
                if (ang > limit) continue;
                if (over) { if (x >= 0 && y >= 0 && x < FACE_W && y < FACE_H) fb[y * FACE_W + x] = c; }
                else put(fb, x, y, c);
            }
        }
    }
}
static void draw_arc(uint16_t *fb, float r_out, float frac, bool cw, uint16_t c) { arc(fb, r_out, frac, cw, c, false); }

/* THE NIGHT'S STEADINESS: the arc's TIP -- its last STEADY_TIP_PX of
 * arc (the ring pixels within that of ``frac``'s end) re-inked from
 * ``c`` (what the arc just painted: only those pixels, never the gel's)
 * to ``c_tip``; marked by the pixels' own box and the ink (the face's
 * box is the glass once a ring is drawn; the diff finds the tile or
 * two). The ring table's path only: with no table (no PSRAM) the tip
 * stands still, and nothing else changes. */
static void arc_tip(uint16_t *fb, float r_out, float frac, bool cw, uint16_t c, uint16_t c_tip)
{
    if (frac <= 0) return;
    if (frac > 1) frac = 1;
    ring_t *rg = ring_get(r_out);
    if (!rg || !rg->px) return;
    const float full = 2.0f * (float)M_PI;
    float limit = frac * full;
    float tip = STEADY_TIP_PX / (r_out - RING_W * 0.5f);
    mark_f(&mk, limit); mark_val(&mk, c_tip); mark_val(&mk, 0x7191u);
    int bx0 = FACE_W, by0 = FACE_H, bx1 = 0, by1 = 0;
    const ring_px_t *e = rg->px;
    for (int i = 0, n = rg->n; i < n; i++) {
        float ang = e[i].ang;
        if (!cw) ang = full - ang;
        if (ang > limit || ang <= limit - tip) continue;
        uint16_t *p = fb + e[i].off;
        if (*p != c) continue;
        *p = c_tip;
        int x = e[i].off % FACE_W, y = e[i].off / FACE_W;
        if (x < bx0) bx0 = x;
        if (y < by0) by0 = y;
        if (x + 1 > bx1) bx1 = x + 1;
        if (y + 1 > by1) by1 = y + 1;
    }
    mark_box(&mk, bx0, by0, bx1 - bx0, by1 - by0);
}

/* an arc of the rim band CENTRED at ``centre`` (0 at 12 o'clock, +
 * clockwise), ``half`` rad to each side; only on black (THE REACHING
 * CUE); the slow path when the ring table cannot be had */
static void arc_at(uint16_t *fb, float r_out, float centre, float half, uint16_t c)
{
    const float full = 2.0f * (float)M_PI;
    const float C = FACE_W / 2.0f;
    mark_f(&mk, r_out); mark_f(&mk, centre); mark_f(&mk, half); mark_val(&mk, c);
    mark_box(&mk, (int)floorf(C - r_out), (int)floorf(C - r_out), (int)ceilf(2.0f * r_out) + 1, (int)ceilf(2.0f * r_out) + 1);
    ring_t *rg = ring_get(r_out);
    if (!rg || !rg->px) return;
    for (int i = 0, n = rg->n; i < n; i++) {
        float d = fabsf(rg->px[i].ang - centre);
        if (d > (float)M_PI) d = full - d;
        if (d > half) continue;
        uint16_t *p = fb + rg->px[i].off;
        if (*p == 0) *p = c;
    }
}

/* THE REACHING CUE: the ripple at the wall he will rise at -- the
 * wall's angle on the glass (walls.h: 0 bottom, pi/2 right, pi top,
 * -pi/2 left, from straight down) as the ring's (0 at 12 o'clock,
 * clockwise): bottom = pi, right = pi/2, top = 0, left = 3pi/2 */
static void draw_reach(uint16_t *fb)
{
    if (!in.reaching || reach_t >= REACH_RIPPLE_S) return;
    static const float RING_ANG[4] = { (float)M_PI, (float)M_PI / 2.0f, 0.0f, 3.0f * (float)M_PI / 2.0f };
    float u = reach_t / REACH_RIPPLE_S;                     /* 0 -> 1 over the ripple */
    float half = REACH_HALF0 + (REACH_HALF1 - REACH_HALF0) * u;
    float k = REACH_K * (1.0f - u);
    if (k < 0.03f) return;
    arc_at(fb, DAY_R_OUT, RING_ANG[in.reach_wall & 3], half, ink(&theme_pal()->shine, k));
}
/* CLOCKWORK's arc (clockwork.cpp): the same routine, drawn OVER the day
 * ring -- the tool moment takes the ring, as the dock face does */
void face_arc(uint16_t *fb, float frac, bool cw, uint16_t c) { arc(fb, DAY_R_OUT, frac, cw, c, true); }

static void draw_hour(uint16_t *fb)
{
    if (!hour_text[0]) return;
    const gel_rgba_t *sh = &theme_pal()->shade, *sn = &theme_pal()->shine;
    gel_rgba_t c = { sh->r + (sn->r - sh->r) * hour_k, sh->g + (sn->g - sh->g) * hour_k,
                     sh->b + (sn->b - sh->b) * hour_k, 1.0f };
    uint16_t col = ink(&c, 1.0f);
    int n = (int)strlen(hour_text);
    int w = n * 4 - 1;
    int x0 = FACE_W / 2 - (w * HOUR_PX) / 2, y0 = FACE_H / 2 - (5 * HOUR_PX) / 2;
    for (int i = 0; i < n; i++) {
        const uint8_t *g = hour_text[i] == ':' ? COLON : DIGITS[(hour_text[i] - '0') % 10];
        for (int row = 0; row < 5; row++)
            for (int cl = 0; cl < 3; cl++)
                if (g[row] & (1 << (2 - cl)))
                    block(fb, x0 + (i * 4 + cl) * HOUR_PX, y0 + row * HOUR_PX, HOUR_PX, col);
    }
}

/* the drop's level: rows lit from the bottom; -1 = nothing to draw */
static int drop_rows(float *pulse_k)
{
    *pulse_k = 1.0f;
    if (in.batt_mv >= CABLE_MV) return -1;
    if (in.charging) {
        *pulse_k = PULSE_LOW + (1.0f - PULSE_LOW) * (0.5f + 0.5f * sinf(2.0f * (float)M_PI * t_s / PULSE_S));
        return 7;
    }
    float level = clampf(((float)in.batt_mv - DROP_EMPTY_MV) / (float)(DROP_FULL_MV - DROP_EMPTY_MV), 0, 1);
    int rows = (int)ceilf(level * 7.0f - 1e-4f);
    return rows < 0 ? 0 : rows > 7 ? 7 : rows;
}

static void draw_drop(uint16_t *fb)
{
    float pk;
    int rows = drop_rows(&pk);
    if (rows < 0) return;
    const gel_rgba_t *sh = &theme_pal()->shine;
    uint16_t full = ink(sh, pk), edge = ink(sh, DROP_EDGE_K);
    int cx = FACE_W / 2, cy = FACE_H / 2 + DROP_R;
    int x0 = cx - (5 * DROP_PX) / 2, y0 = cy - (7 * DROP_PX) / 2;
    for (int row = 0; row < 7; row++) {
        bool lit = row >= 7 - rows;
        for (int col = 0; col < 5; col++) {
            if (!(GLYPH_DROP[row] & (1 << (4 - col)))) continue;
            bool on_edge = true;
            if (!lit) {
                /* the empty part: the glyph's edge only (a pixel with a
                 * neighbour outside the shape) */
                bool up = row > 0 && (GLYPH_DROP[row - 1] & (1 << (4 - col)));
                bool dn = row < 6 && (GLYPH_DROP[row + 1] & (1 << (4 - col)));
                bool lf = col > 0 && (GLYPH_DROP[row] & (1 << (4 - col + 1)));
                bool rt = col < 4 && (GLYPH_DROP[row] & (1 << (4 - col - 1)));
                on_edge = !(up && dn && lf && rt);
                if (!on_edge) continue;
            }
            block(fb, x0 + col * DROP_PX, y0 + row * DROP_PX, DROP_PX, lit ? full : edge);
        }
    }
}

/* THE HOUR TICK: a radial tick at angle ``ang`` (0 at 12 o'clock, +
 * clockwise), from r0 out to r1, ``w`` px wide across the radius;
 * pixel centres at half-pixels, as the rings */
static void tick_at(uint16_t *fb, float ang, float r0, float r1, int w, uint16_t c, bool over)
{
    const float C = FACE_W / 2.0f;
    float ox = sinf(ang), oy = -cosf(ang);          /* outward */
    float tx = cosf(ang), ty = sinf(ang);           /* along the rim, clockwise */
    mark_f(&mk, ang); mark_f(&mk, r0); mark_f(&mk, r1); mark_val(&mk, (uint32_t)w | ((uint32_t)over << 8)); mark_val(&mk, c);
    int bx0 = FACE_W, by0 = FACE_H, bx1 = 0, by1 = 0;
    for (float r = r0; r < r1; r += 0.5f)
        for (int s = 0; s < w; s++) {
            float off = (float)s - (float)(w - 1) * 0.5f;
            int x = (int)floorf(C + ox * r + tx * off), y = (int)floorf(C + oy * r + ty * off);
            if (over) { if (x >= 0 && y >= 0 && x < FACE_W && y < FACE_H) fb[y * FACE_W + x] = c; }
            else put(fb, x, y, c);
            if (x < bx0) bx0 = x;
            if (y < by0) by0 = y;
            if (x + 1 > bx1) bx1 = x + 1;
            if (y + 1 > by1) by1 = y + 1;
        }
    mark_box(&mk, bx0, by0, bx1 - bx0, by1 - by0);
}
static void tick_mark(uint16_t *fb, float ang, float r0, float r1, int w, uint16_t c) { tick_at(fb, ang, r0, r1, w, c, false); }
/* CLOCKWORK's ticks (clockwork.cpp): the laps on the arc, the timer's end */
void face_tick(uint16_t *fb, float turn, float r_in, float r_out, int w, uint16_t c, bool over)
{
    tick_at(fb, turn * 2.0f * (float)M_PI, r_in, r_out, w, c, over);
}
float face_ring_r(void) { return DAY_R_OUT; }

static void draw_ticks(uint16_t *fb)
{
    if (in.minute < 0) return;
    const float full = 2.0f * (float)M_PI;
    /* THE RIM AS THE DIAL: the hour tick under the finger while it drags */
    int hour_min = dial_mode == DIAL_FOLLOW && dial_minute >= 0 ? dial_minute : in.minute;
    float hour_ang = (float)(hour_min % 720) / 720.0f * full;
    float min_ang = (float)(in.minute % 60) / 60.0f * full;
    /* THE HANDS STACK (B12's coincidence rule): the minute first, the
     * hour over it, the alarm over both -- each only on black (put), so
     * at 12:00 the thin shine minute is the CORE of the stout shade hour
     * tick (two hands lying over each other, as hands do), and under the
     * alarm's tick (the same shine, 2 px) the minute's one px of extra
     * length at r 106 is all that shows of it passing: the mark he set
     * stands, the hand goes under it for its minute and is out the next.
     * COMPANY_K dims all three alike. */
    tick_mark(fb, min_ang, TICK_R_OUT - MINUTE_TICK_LEN, TICK_R_OUT, MINUTE_TICK_W, ink(&theme_pal()->shine, ck(1.0f)));
    tick_mark(fb, hour_ang, TICK_R_OUT - HOUR_TICK_LEN, TICK_R_OUT, HOUR_TICK_W, ink(&theme_pal()->shade, ck(1.0f)));
    /* THE ALARM'S TICK, in the shine: the lift's blink is it being born
     * (off-on-off-on at the dialled hour), the clear's fade it going;
     * between, wherever the alarm stands */
    int alarm_min = -1; float alarm_k = 1.0f;
    if (dial_mode == DIAL_BLINK && dial_minute >= 0) { alarm_min = dial_minute; if (!(((int)(dial_t / DIAL_BLINK_S)) & 1)) alarm_k = 0; }
    else if (dial_mode == DIAL_FADE && dial_minute >= 0) { alarm_min = dial_minute; alarm_k = clampf(1.0f - dial_t / DIAL_FADE_S, 0, 1); }
    else if (in.alarm_min >= 0) alarm_min = in.alarm_min;
    if (alarm_min >= 0 && alarm_k > 0.02f)
        tick_mark(fb, (float)(alarm_min % 720) / 720.0f * full, TICK_R_OUT - ALARM_TICK_LEN, TICK_R_OUT, ALARM_TICK_W,
                  ink(&theme_pal()->shine, ck(alarm_k)));
}

void face_dial(int minute)
{
    if (minute < 0) { dial_mode = DIAL_NONE; dial_minute = -1; return; }
    dial_mode = DIAL_FOLLOW; dial_minute = minute % 1440; dial_t = 0;
}
void face_dial_set(void)   { if (dial_minute >= 0) { dial_mode = DIAL_BLINK; dial_t = 0; } }
void face_dial_clear(void) { if (dial_minute >= 0) { dial_mode = DIAL_FADE; dial_t = 0; } }
int  face_dial_minute(void) { return dial_mode != DIAL_NONE ? dial_minute : -1; }
/* THE HOUR'S GLANCE: the hour tick's angle as draw_ticks draws it, in whole degrees */
int  face_hour_tick_deg(void)
{
    if (in.minute < 0) return -1;
    int hour_min = dial_mode == DIAL_FOLLOW && dial_minute >= 0 ? dial_minute : in.minute;
    return (hour_min % 720) / 2;
}

/* the plug: 5x5 at DOCK_PX, centred DOCK_R below the middle, the shade */
static void draw_plug(uint16_t *fb)
{
    uint16_t c = ink(&theme_pal()->shade, 1.0f);
    int cx = FACE_W / 2, cy = FACE_H / 2 + DOCK_R;
    int x0 = cx - (5 * DOCK_PX) / 2, y0 = cy - (5 * DOCK_PX) / 2;
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 5; col++)
            if (GLYPH_PLUG[row] & (1 << (4 - col)))
                block(fb, x0 + col * DOCK_PX, y0 + row * DOCK_PX, DOCK_PX, c);
}

bool face_trace_near(int x, int y)
{
    if (!trace_on) return false;
    return x >= trace_x0 - TRACE_STIR_MARGIN && x < trace_x1 + TRACE_STIR_MARGIN
        && y >= trace_y0 - TRACE_STIR_MARGIN && y < trace_y1 + TRACE_STIR_MARGIN;
}

void face_draw(uint16_t *fb)
{
    if (trace_pending && trace) trace_take(fb);          /* the sink's first frame: the gel still at rest */
    if (!on) return;
    /* the trace first: the rest sits over it where they meet */
    if (trace_on && !in.holding) draw_trace(fb);
    /* THE DOCK FACE: a day being carried -- the shine along the ring for
     * the bytes gone, drawn first (the steps show past it: a mark only
     * lands on black) */
    if (in.sync_active && in.sync_total)
        draw_arc(fb, DAY_R_OUT, (float)in.sync_sent / (float)in.sync_total, true, ink(&theme_pal()->shine, 1.0f));
    /* the day ring: steps against the norm (NO ring while no norm is
     * known: under 3 days it would be a mark against a guess); the shine
     * once met; the whole of it the shine for the blink after `ok day`;
     * swept in from 12 o'clock on a raise */
    float sk = sweep_k();
    float day = norm ? (float)in.steps / (float)norm : 0;
    if (day > 0) {
        uint16_t c = (day >= 1.0f || blink_t > 0) ? ink(&theme_pal()->shine, ck(1.0f)) : ink(&theme_pal()->body, ck(DAY_K));
        draw_arc(fb, DAY_R_OUT, (day > 1.0f ? 1.0f : day) * sk, true, c);
    }
    /* the night ring, counter-clockwise, just inside, swept the other way;
     * THE NIGHT RING KNOWS HIS HEART: the shine for a deep night, the tip
     * breathing when the week's nights wander */
    if (in.night_min > 0) {
        float nfrac = (float)in.night_min / 60.0f / NIGHT_H * sk;
        const gel_rgba_t *ncol = in.night_deep ? &theme_pal()->shine : &theme_pal()->shade;
        float nk = ck(in.night_deep ? NIGHT_DEEP_K : NIGHT_K);
        uint16_t nc = ink(ncol, nk);
        draw_arc(fb, DAY_R_OUT - RING_W - RING_GAP, nfrac, false, nc);
        if (in.night_wander) {
            float b = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * STEADY_BREATH_HZ * t_s);      /* 1 = the ring's ink, 0 = dark */
            b = floorf(b * STEADY_TIP_STEPS + 0.5f) / STEADY_TIP_STEPS;
            arc_tip(fb, DAY_R_OUT - RING_W - RING_GAP, nfrac, false, nc, ink(ncol, nk * b));
        }
    }
    if (in.docked) draw_plug(fb);                        /* THE DOCK FACE: the plug at 6 o'clock; the drop yields to it */
    else draw_drop(fb);
    draw_reach(fb);                                      /* THE REACHING CUE: the ripple at the wall, only while a come / a claim by hand is out */
    draw_ticks(fb);                                      /* THE HOUR TICK: the hour and the minute on the rim, whenever the clock is known */
    if (!in.holding) draw_hour(fb);
}

void face_status(char *out, int cap)
{
    float pk; int rows = drop_rows(&pk);
    int level = (int)(clampf(((float)in.batt_mv - DROP_EMPTY_MV) / (float)(DROP_FULL_MV - DROP_EMPTY_MV), 0, 1) * 100 + 0.5f);
    char tr[48];
    if (trace_on) snprintf(tr, sizeof tr, "the trace %lu px, %.0f s left%s", (unsigned long)trace_px, TRACE_S - trace_age, stir_t < TRACE_STIR_S ? ", stirred" : "");
    else snprintf(tr, sizeof tr, "%s", trace_pending ? "the trace pending" : trace ? "no trace" : "no trace memory");
    char dr[32];
    if (rows < 0) snprintf(dr, sizeof dr, "at the float: no drop");
    else if (in.charging) snprintf(dr, sizeof dr, "a full drop, pulsing");
    else snprintf(dr, sizeof dr, "the drop, %d/7 rows", rows);
    char dk[64] = "";
    if (in.sync_active) snprintf(dk, sizeof dk, " | carrying a day: %lu/%lu B on the ring", (unsigned long)in.sync_sent, (unsigned long)in.sync_total);
    else if (blink_t > 0) snprintf(dk, sizeof dk, " | the ring blinking (a day came home)");
    if (in.docked) strncat(dk, " | docked: the plug", sizeof dk - strlen(dk) - 1);
    char dayk[72];
    if (norm) snprintf(dayk, sizeof dayk, "day %lu/%lu steps (%d%%, norm from %d days)", (unsigned long)in.steps, (unsigned long)norm,
                       (int)(100.0f * in.steps / norm), norm_days);
    else snprintf(dayk, sizeof dayk, "day %lu steps (no ring: the norm needs %d days, %d so far)", (unsigned long)in.steps, NORM_MIN_DAYS, norm_days);
    char tk[72];                                /* THE HOUR TICK + THE MINUTE TICK: where the two hands stand (and the alarm's mark) */
    if (in.minute < 0) snprintf(tk, sizeof tk, "no ticks (no clock)");
    else if (in.alarm_min >= 0) snprintf(tk, sizeof tk, "ticks: hour at %d deg, minute at %d deg, the alarm's at %d deg", (in.minute % 720) / 2, (in.minute % 60) * 6, (in.alarm_min % 720) / 2);
    else snprintf(tk, sizeof tk, "ticks: hour at %d deg, minute at %d deg", (in.minute % 720) / 2, (in.minute % 60) * 6);
    snprintf(out, cap, "face %s | hour %s (%s) | %s | %s | night %.1f/%.0f h%s%s%s | batt %u mV %d%% (%s) | %s%s%s",
             on ? "on" : "off", hour_text[0] ? hour_text : "--:--",
             in.holding ? "holding: the glance's" : "empty: the resting face", tk, dayk,
             in.night_min / 60.0f, NIGHT_H, in.night_min > 0 ? "" : " (none known)",
             in.night_min > 0 && in.night_deep ? " (deep: the shine)" : "", in.night_min > 0 && in.night_wander ? " (the nights wander: the tip breathes)" : "",
             in.batt_mv, level, dr, tr, dk, sweep_t < SWEEP_S ? " | the rings sweeping in" : "");
}
