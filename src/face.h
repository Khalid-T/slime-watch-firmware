/* THE WORDLESS WATCH FACE (Khalid, 2026-09-09 23:25: "keep on filling
 * the firmware -- make it not just a black screen").
 *
 * The empty glass is a WATCH, useful whether or not the slime is home;
 * when he IS home the same marks sit quietly behind him. Wordless, in
 * the pet's own palette, no numbers in the creature's language -- the
 * hour is the one exception (design-zoom-out). Everything here is
 * PINNED TO THE WATCH like the hour (rotation-free); only the gel lives
 * in gravity's frame.
 *
 *   THE HOUR      the resting face while the house is EMPTY: the glance's
 *                 clock, centred, in the shade colour, lit as long as the
 *                 screen is; a raise brightens it to the shine for the
 *                 glance's spell. HOLDING keeps the glance's own 6 s at the
 *                 top -- the slime is the face.
 *   THE DAY RING  a 2 px arc along the rim from 12 o'clock CLOCKWISE:
 *                 today's steps against his NORM (the median of the last
 *                 7 days' totals; NO RING until 3 days exist -- a norm
 *                 from fewer is a guess, and nothing is drawn against a
 *                 guess). Full = the norm; past it the ring turns to the
 *                 shine, never doubles. The body colour, dimmed. On a
 *                 raise both rings SWEEP IN from 12 o'clock over 0.6 s.
 *   THE NIGHT RING a fainter arc (the shade) just inside it, from 12
 *                 o'clock COUNTER-CLOCKWISE: the last night's hours
 *                 against 8; nothing while no night is known. THE
 *                 NIGHT RING KNOWS HIS HEART (D33 + D36): the SHINE
 *                 for a night whose nocturnal dip was there (the
 *                 ring's one brightening, the day ring's rule at the
 *                 norm); its tip breathing a pixel at 0.1 Hz when the
 *                 week's sleep midpoints wander over an hour.
 *   THE DROP      the water lock's 5x7 drop at 6 o'clock inside the rings,
 *                 filled from the bottom by the cell's level; a full drop
 *                 pulsing slowly while it drinks from the cable; nothing at
 *                 all at the charger's float (no cell, or a full one on the
 *                 cable: nothing to say).
 *   THE TRACE     when the house goes EMPTY a dim residue of the gel's last
 *                 resting silhouette stays on the glass where it was and
 *                 fades over an hour; the arrival takes it back; a reboot
 *                 forgets it. THE TRACE STIRRED (watch-night C14): a tap
 *                 on the empty glass near it makes the residue shiver --
 *                 its tint once from the quarter toward TRACE_STIR_K and
 *                 back over TRACE_STIR_S; nothing else (no gel, no rise).
 *   THE HOUR TICK  a 2 px radial tick on the rim (r 107-112, just inside
 *                 the night ring) at the hour hand's angle, 12 o'clock up,
 *                 the shade colour. There whenever the glass is lit and
 *                 the clock is known; no digits, no hands (design/
 *                 watch-ideas.md B11).
 *   THE MINUTE TICK (B12, 2026-10-09) a 1 px tick in the SHINE at the
 *                 minute hand's angle, a px longer than the hour's (r
 *                 106-112): the hands' own grammar -- the hour short and
 *                 stout and dim, the minute long and thin and bright --
 *                 so the two read apart at arm's length in sun where the
 *                 3x5 digits do not. THE HANDS STACK: drawn first, the
 *                 hour's over it, the alarm's over both, each only on
 *                 black, so where two coincide the thinner, brighter
 *                 minute shows as the core of the hour's tick, and under
 *                 the alarm's (the same shine) its one px of extra length
 *                 is the trace of it passing. Hidden under the gel where
 *                 he rests, as every mark is.
 *   THE REACHING CUE (the snappy pass, 2026-09-10 evening; Khalid timed
 *                 three knocks -> 12-15 s before the slime rose, the
 *                 whole dial, and nothing on the glass said it heard):
 *                 from the knock / the hold / the rising until the box
 *                 answers, the RIM RIPPLES once a second at the wall
 *                 where he will rise -- a short arc of the rim band at
 *                 that wall, widening from REACH_HALF0 to REACH_HALF1 and
 *                 fading over REACH_RIPPLE_S, then quiet until the next
 *                 second; the shine, only on black. No gel: a mark, not
 *                 a creature. Gone on the answer (the rise, or a refusal).
 *   THE ALARM'S TICK  while an alarm is set, the same tick at ITS hour on
 *                 the 12-hour dial, in the SHINE (the hour's is the shade):
 *                 he sees where his morning is. The dial's blink on the
 *                 lift is this tick being born, the clear's fade is it
 *                 going; gone with `alarm off` (design/watch-ideas.md B14).
 *   THE DOCK FACE while a day is being carried up (any door) the DAY RING
 *                 doubles as the courier's progress -- the shine along the
 *                 ring for bytes sent / the day's bytes, over the steps --
 *                 and blinks once (300 ms, the shine) when the box says
 *                 `ok day`; on the USB DOCK a 5x5 plug glyph (the shade) at
 *                 6 o'clock just inside the rings, only while the serial
 *                 link is the held door. Nothing when idle.
 *
 * Drawn AFTER the gel and only on pixels the gel left black (behind
 * him); the rings live in the outermost band the gel's base never
 * reaches anyway. No Arduino in here: the time, the numbers and the
 * framebuffer are parameters, so tools/face_test.cpp renders it on the
 * laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define FACE_W 240
#define FACE_H 240
#define FACE_TRACE_BYTES (FACE_W * FACE_H / 8)     /* the silhouette mask: 7,200 B (PSRAM on the board) */

/* what main hands over each frame */
typedef struct {
    float    dt;            /* seconds since the last tick */
    bool     lit;           /* the screen is on */
    bool     holding;       /* house_holding(): the slime is the face */
    bool     gel_drawn;     /* the gel or its theatre is on the glass this frame */
    int      house_event;   /* HOUSE_EV_* taken this frame (0 none): the sink arms the trace, the arrival clears it */
    int      minute;        /* the local minute of the day, -1 = no clock */
    bool     glance;        /* glance_on(): a raise's brightening of the resting hour */
    uint32_t steps;         /* wrist_steps_today() */
    int      night_min;     /* rest_night_min(): the last night's minutes, 0 = none known */
    uint16_t batt_mv;
    bool     charging;      /* charging(): at the float or above */
    /* THE DOCK FACE (link.cpp's numbers): a day sync in flight over any door */
    bool     sync_active;   /* link_sync_progress(): the day ring doubles as the courier's progress */
    uint32_t sync_sent, sync_total;
    uint32_t sync_done_seq; /* link_sync_done_seq(): a change = the box said `ok day`: the ring blinks once */
    bool     docked;        /* link_docked(): the serial link is the held door -- the plug at 6 o'clock */
    /* THE REACHING CUE: link_reaching() -- a come / a claim by hand is on its way to the box; the wall he will rise at (walls.h GEL_WALL_*) */
    bool     reaching;
    int      reach_wall;
    int      alarm_min;     /* THE ALARM'S TICK (watch-ideas B14): clockwork_alarm(), the minute of the day; -1 = no alarm set */
    bool     h12;           /* MORE SETTINGS' CLOCK row: the resting hour as `7:41` (12H) or `07:41` (24H); hourfmt.h */
    bool     company;       /* SHOWING SOMEONE (watch-ideas B18, 2026-09-23): wake_showing() -- the glass held toward someone: the rings and the ticks dim to COMPANY_K (his day is his), eased over COMPANY_EASE_S */
    /* THE NIGHT RING KNOWS HIS HEART (nightsum.h; watch-ideas D33 + D36, 2026-10-08) */
    bool     night_deep;    /* nightsum_deep(rest_night_id()) and not dozing: the nocturnal dip was there -- the night ring in the SHINE, its one brightening */
    bool     night_wander;  /* nightsum_wander(): the week's sleep midpoints wander over an hour -- the ring's tip breathes 1 px */
    bool     stir;          /* THE TRACE STIRRED (watch-night C14): a tap landed on the EMPTY glass near the trace this frame (face_trace_near said so) -- the residue shivers once */
} face_in_t;

void face_begin(uint8_t *trace_bits, bool on);      /* the mask's memory (FACE_TRACE_BYTES; NULL = no trace), the NVS switch */
/* the closed days' step totals, newest first: the norm is their median */
void face_set_history(const uint32_t *totals, int n);
/* one call a frame; returns a log line to print, or NULL */
const char *face_tick(const face_in_t *in);
/* over the composed frame, after the gel (and the Zs), before the
 * glance and the word: writes only where the frame is black */
void face_draw(uint16_t *fb);
void face_set_on(bool on);
bool face_on(void);
uint32_t face_norm(void);                            /* 0 = no norm known yet (under 3 days of files) */
int  face_norm_days(void);                           /* the days it stands on */
void face_raised(void);                              /* THE SWEEP: a raise -- the rings arrive from 12 o'clock */
void face_status(char *out, int cap);               /* `face`: what is shown and the numbers */
/* THE RIM AS THE DIAL (clockwork.cpp drives it): the hour tick follows
 * the finger -- drawn at ``minute``'s hour instead of the clock's (-1 =
 * the clock's again); on the lift face_dial_set() blinks it twice at
 * the dialled hour then lets it go back to the clock's, face_dial_clear()
 * fades it out over DIAL_FADE_S then the same. The minute tick stays the
 * clock's throughout. */
void face_dial(int minute);
void face_dial_set(void);
void face_dial_clear(void);
int  face_dial_minute(void);                         /* the dial's minute while the tick is not the clock's, -1 = the clock's */
/* THE HOUR'S GLANCE (hourlook.c, watch-night C15): the hour tick's angle
 * as drawn this frame -- degrees clockwise from 12 o'clock (0..359; the
 * dial's hour while the finger drags it, else the clock's), -1 without
 * a clock. The stacked hands' rim point is at it. */
int  face_hour_tick_deg(void);
/* THE TRACE STIRRED (watch-night C14): is the glass point inside the
 * trace's box plus TRACE_STIR_MARGIN -- false while no trace is on the
 * glass. main asks at a tap on the empty glass and hands the answer in
 * as face_in_t.stir. */
bool face_trace_near(int x, int y);
/* CLOCKWORK (clockwork.cpp) borrows the ring's arc: frac of the turn
 * from 12 o'clock on the DAY RING's own radius, written OVER it (the
 * tool moment takes the ring, as the dock face does) */
void face_arc(uint16_t *fb, float frac, bool cw, uint16_t c);
/* ... and the tick (LAPS ON THE ARC, the timer's tick): a radial mark at
 * ``turn`` of the dial from 12 o'clock clockwise (0..1), from r_in out
 * to r_out, w px wide; over = written over whatever is there (a lap on
 * the arc), else only on black as the face's own marks. Marked in the
 * face's mark, as the arc is. face_ring_r() is the day ring's outer
 * edge, so a caller can sit a tick on the ring. */
void face_tick(uint16_t *fb, float turn, float r_in, float r_out, int w, uint16_t c, bool over);
float face_ring_r(void);
#include <stddef.h>
#include "dirty.h"
bool face_mark(dirty_rect_t *box);     /* THE MARK (mark.h): did this frame's face differ from last frame's; the box of both */
void *face_mem(size_t n);              /* the ring tables' memory (weak: malloc; main.cpp: PSRAM) */
