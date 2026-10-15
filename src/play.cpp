/* PLAY: THE BEAD AND THE SPIN, THE TOSS AND THE RHYTHM -- the arbiter,
 * the bead's roll, the toss's flight, the rhythm's schedule. See play.h
 * for the four games, their numbers and the gate table.
 *
 * Reasoned from the material (design-zoom-out): the bowl is the glass
 * and gravity is already its floor, so a bead is nothing new -- one
 * more small thing in the bowl, rolling where the tilt sends it, lost
 * into the gel when it reaches him (he is a Predator: what touches him
 * is eaten). A spin is the bowl itself turned fast: the mass lags, the
 * eyes lag, the body squints until the world stops. Neither is ever
 * asked for by the watch: the wrist starts them, the wrist can leave
 * them, nothing is scored, nothing is counted where he can see.
 *
 * THE TOSS AND THE RHYTHM (2026-10-09, THE OVERHAUL WAVE): a flick of
 * the whole bowl throws what is in it -- the mass leaves the floor for
 * a moment and comes down on it (the fall's own splat, smaller: it was
 * thrown a few px, not dropped a wall). A beat drummed on the glass is
 * heard through the glass; a gel has no hands to drum back with, so it
 * drums with its body -- the squash, the one thing it moves at will --
 * and a second late, the way a creature repeats what it was shown.
 *
 * Plain C++ with no Arduino: time is a parameter, the output is a game
 * that starts (and the bead's place, the toss's lift, the pulses). */
#include "play.h"
#include "walls.h"      /* FLAT_G: level is the four walls' own word for it */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static_assert(TOSS_SPLAT == 0.5f * LAND_SQUASH, "the small splat is half the fall's (play.h TOSS_SPLAT, walls.h LAND_SQUASH)");
static_assert(TOSS_LAND_S == LAND_S, "the small splat eases off over the fall's own LAND_S");

/* --- state -------------------------------------------------------------- */

static play_gates_t gates;          /* the last tick's gates (play_tap reads them) */
static float    still_s = 0;        /* flat and still, so far */
static bool     bead_open = false;  /* the game is on */
static bool     bead_on = false;    /* a bead is on the glass */
static float    bx = 0, by = 0;     /* the bead's centre, from the glass's centre (face px, +x right, +y down) */
static float    bvx = 0, bvy = 0;   /* its speed, face px/s */
static float    over_s = 0;         /* flat with no bead, so far (the game's end) */
static float    lump_t = -1;        /* seconds since the bite, -1 = none */
static uint32_t bead_ms = 0;        /* the last bead game's start (0 = never) */
static uint32_t spin_ms = 0;        /* the last spin's */
static float    spin_span_ms = 0;   /* the yaw over SPIN_DPS, so far */
static float    spin_peak = 0;      /* its peak this spin */
static bool     spin_read = false;  /* this spin has been judged (once per spin) */
static float    dizzy_t = 0;        /* the dizzy spell's clock, counting down */
static int      dizzy_sign = 1;
static float    last_spin_dps = 0;
/* THE TOSS */
static bool     exc_on = false;     /* an in-plane excursion over TOSS_G is going on */
static float    exc_x = 0, exc_y = 0;   /* its direction (unit, the first frame's) */
static float    exc_span_ms = 0;    /* how long it has been over */
static bool     last_exc_set = false;   /* the last excursion that ended: its direction and end, for the reversal test */
static float    last_exc_x = 0, last_exc_y = 0;
static uint32_t last_exc_end_ms = 0;
static uint32_t last_rev_ms = 0;    /* the last reversal seen (0 = none) */
static bool     cand = false;       /* a flick ended with no reversal before it: the toss if none follows */
static uint32_t cand_ms = 0;        /* when it ended */
static float    cand_x = 0, cand_y = 0, cand_g = 0;
static float    toss_t = -1;        /* the toss's clock, -1 = none; the flight then the splat */
static bool     toss_air = false;   /* in the air (else splatting) */
static float    toss_x = 0, toss_y = 0; /* the last toss's direction */
static bool     toss_dir_set = false;
static bool     toss_landed = false;    /* the take */
static bool     toss_sense = false;     /* the take */
static uint32_t toss_sense_ms = 0;      /* the last "tossed" sense (0 = never) */
/* THE RHYTHM */
static int      rh_n = 0;           /* pulses scheduled */
static int      rh_i = 0;           /* the next pulse to play */
static uint32_t rh_at[RHYTHM_MAX];  /* each pulse's start (ms) */
static float    rh_t = -1;          /* the pulse now playing: seconds into it, -1 = none */
static bool     rh_done = false;    /* the take */
static bool     rh_sense = false;   /* the take */
static uint32_t rh_sense_ms = 0;    /* the last "rhythm" sense (0 = never) */
static int      refused = PLAY_OK;
static uint32_t counts[6] = { 0, 0, 0, 0, 0, 0 };
static bool     log_new = false;
static char     log_line[128];
/* F50 CATCH THE DROP */
static uint32_t bites = 0;          /* beads eaten in this game (every CATCH_EVERY_BITES-th brings a drop) */
static float    drop_due = -1;      /* seconds until the drop leaves the door, -1 = none owed */
static bool     drop_on = false;    /* a drop falling */
static float    dx = 0, dy = 0;     /* its centre, from the glass's centre (face px) */
static float    dspeed = 0;         /* its speed DOWN GRAVITY (a scalar: the way is re-read each frame, it does not roll) */
static bool     drop_inside = false;   /* it came in past the rim (the door is outside it): the landing is judged after */
static float    catch_t = -1;       /* seconds since the catch, -1 = none (the meal's lump) */
static float    mark_t = -1;        /* seconds since the miss, -1 = no mark */
static float    mx = 0, my = 0;     /* the wet mark's place, from the centre */
static uint32_t drops = 0, caught_n = 0, missed_n = 0;
/* F52 THE COURSE */
static int      cur_wall = -1;      /* the wall he was last seen landed on (-1 = not yet) */
static bool     course_on = false;
static int      gate_wall[COURSE_GATES] = { 0, 0, 0 };
static int      passed = 0;         /* gates lit, in order */
static float    course_idle = 0;    /* since the last gate (or the start) */
static uint32_t course_ms = 0;      /* the last course's start (0 = never) */
static uint32_t events = 0;         /* PLAY_EV_* this tick */
static const char *const WALL_NAMES[4] = { "bottom", "right", "top", "left" };   /* gel.h's GEL_WALL_NAMES, without gel.h (pure) */

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
    /* one line at a time; a second before main took the first wins (the
     * newer is the one worth reading) */
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_line, sizeof log_line, fmt, ap);
    va_end(ap);
    log_new = true;
}

const char *play_refused_name(int r)
{
    switch (r) {
    case PLAY_OK:                 return "ok";
    case PLAY_REFUSED_DARK:       return "the dark house";
    case PLAY_REFUSED_QUIET:      return "the quiet hours";
    case PLAY_REFUSED_SHELF:      return "on the shelf";
    case PLAY_REFUSED_EMPTY:      return "the empty glass";
    case PLAY_REFUSED_PAGE:       return "the page is up";
    case PLAY_REFUSED_WORD:       return "a word is up";
    case PLAY_REFUSED_GAP:        return "played inside 10 min";
    case PLAY_REFUSED_NOT_ARMED:  return "not flat and still for 3 s";
    case PLAY_REFUSED_ON_GEL:     return "the tap landed on him";
    case PLAY_REFUSED_ASLEEP:     return "he is asleep";
    case PLAY_REFUSED_FALLING:    return "a fall is on";
    default:                      return "?";
    }
}

/* THE GATE TABLE: the first closed gate, or PLAY_OK. The gap is the
 * game's own (last_ms 0 = never played). */
static int gate(uint32_t now_ms, uint32_t last_ms)
{
    if (!gates.lit)     return PLAY_REFUSED_DARK;
    if (gates.quiet)    return PLAY_REFUSED_QUIET;
    if (gates.shelf)    return PLAY_REFUSED_SHELF;
    if (!gates.holding) return PLAY_REFUSED_EMPTY;
    if (gates.page)     return PLAY_REFUSED_PAGE;
    if (gates.word)     return PLAY_REFUSED_WORD;
    if (gates.asleep)   return PLAY_REFUSED_ASLEEP;
    if (last_ms && now_ms - last_ms < PLAY_GAP_MS) return PLAY_REFUSED_GAP;
    return PLAY_OK;
}

static bool gates_open(void)
{
    return gates.lit && !gates.quiet && !gates.shelf && gates.holding && !gates.page && !gates.word && !gates.asleep;
}

/* the sense's own gap (the toss, the rhythm: every one plays, the word
 * goes up once per PLAY_GAP_MS) */
static bool sense_due(uint32_t now_ms, uint32_t *last_ms)
{
    if (*last_ms && now_ms - *last_ms < PLAY_GAP_MS) return false;
    *last_ms = now_ms ? now_ms : 1;
    return true;
}

void play_begin(void)
{
    memset(&gates, 0, sizeof gates);
    still_s = 0; bead_open = bead_on = false; bx = by = bvx = bvy = 0; over_s = 0; lump_t = -1;
    bead_ms = spin_ms = 0; spin_span_ms = 0; spin_peak = 0; spin_read = false; dizzy_t = 0; dizzy_sign = 1;
    last_spin_dps = 0; refused = PLAY_OK; memset(counts, 0, sizeof counts); log_new = false; log_line[0] = 0;
    bites = 0; drop_due = -1; drop_on = false; drop_inside = false; dx = dy = dspeed = 0; catch_t = -1; mark_t = -1; mx = my = 0;
    drops = caught_n = missed_n = 0;
    cur_wall = -1; course_on = false; gate_wall[0] = gate_wall[1] = gate_wall[2] = 0; passed = 0; course_idle = 0; course_ms = 0; events = 0;
    exc_on = false; exc_x = exc_y = 0; exc_span_ms = 0; last_exc_set = false; last_exc_x = last_exc_y = 0; last_exc_end_ms = 0; last_rev_ms = 0;
    cand = false; cand_ms = 0; cand_x = cand_y = cand_g = 0; toss_t = -1; toss_air = false; toss_x = toss_y = 0; toss_dir_set = false;
    toss_landed = false; toss_sense = false; toss_sense_ms = 0;
    rh_n = rh_i = 0; memset(rh_at, 0, sizeof rh_at); rh_t = -1; rh_done = false; rh_sense = false; rh_sense_ms = 0;
}

static void bead_over(const char *why)
{
    bead_open = false; bead_on = false; bvx = bvy = 0; over_s = 0; bites = 0;
    drop_due = -1;                           /* a drop owed to a game that ended is not owed: never unasked */
    say("play: the bead is over (%s)", why);
}

static void course_over(const char *why)
{
    course_on = false; passed = 0; course_idle = 0;
    events |= PLAY_EV_COURSE_GONE;
    say("play: the course left (%s): the gates gone", why);
}

/* THE DROP's fall: straight down gravity's in-plane direction, re-read
 * each frame (he tilts to steer where it lands); its speed is one
 * number along that way -- it does not roll, it has no sideways
 * momentum to lose. Level (no direction to read) it falls toward the
 * wall he sits on; the pull is never under DROP_SLIDE_G (a level glass
 * still drips). Into his painted box it is CAUGHT; at the bowl's side,
 * moving outward, it is MISSED and the wet mark is left there. */
static void drop_step(const play_in_t *in, float dt)
{
    float g = sqrtf(in->gx * in->gx + in->gy * in->gy);
    float ux, uy;
    if (g >= DROP_DIR_G) { ux = in->gx / g; uy = in->gy / g; }
    else { float phi = walls_phi(in->wall); ux = sinf(phi); uy = cosf(phi); }
    float pull = g > DROP_SLIDE_G ? g : DROP_SLIDE_G;
    dspeed += pull * DROP_G_PX * dt;
    dx += ux * dspeed * dt;
    dy += uy * dspeed * dt;
    float x = 120.0f + dx, y = 120.0f + dy;
    if (in->box_w > 0 && x >= in->box_x && x < in->box_x + in->box_w && y >= in->box_y && y < in->box_y + in->box_h) {
        drop_on = false; dspeed = 0;
        catch_t = 0; caught_n++;
        events |= PLAY_EV_CAUGHT;
        say("play: the drop fell into him: CAUGHT (the meal's lump)");
        return;
    }
    float r = sqrtf(dx * dx + dy * dy);
    if (r < DROP_RIM_R - 1.0f) drop_inside = true;
    /* the landing: at the bowl's side and heading out of it, once it came
     * in (the door is past the rim: at it the drop heads IN); never in,
     * and DROP_OUT_PX past the door, it fell out of the door instead */
    if (r >= DROP_RIM_R && (dx * ux + dy * uy) > 0 && (drop_inside || r >= DROP_DOOR_R + DROP_OUT_PX)) {
        float k = DROP_RIM_R / r;
        mx = dx * k; my = dy * k;
        drop_on = false; dspeed = 0;
        mark_t = 0; missed_n++;
        events |= PLAY_EV_MISSED;
        say("play: the drop met the glass at %.0f,%.0f: missed (a wet mark, a minute)", (double)(120.0f + mx), (double)(120.0f + my));
    }
}

/* the roll: gravity's in-plane part pulls it, the loss slows it, the
 * bowl's side keeps it in -- the part of its speed into the rim is
 * lost, the part along the rim mostly kept (a bead rolls round a bowl,
 * it does not bounce off it) */
static void bead_step(float gx, float gy, float dt)
{
    bvx += gx * BEAD_G_PX * dt;
    bvy += gy * BEAD_G_PX * dt;
    float k = 1.0f - BEAD_DAMP * dt;
    if (k < 0) k = 0;
    bvx *= k; bvy *= k;
    bx += bvx * dt;
    by += bvy * dt;
    float r = sqrtf(bx * bx + by * by);
    if (r > BEAD_RIM_R) {
        float nx = bx / r, ny = by / r;              /* outward */
        bx = nx * BEAD_RIM_R; by = ny * BEAD_RIM_R;
        float vn = bvx * nx + bvy * ny;              /* into the rim */
        if (vn > 0) {
            float tx = -ny, ty = nx;                 /* along it */
            float vt = (bvx * tx + bvy * ty) * BEAD_RIM_KEEP;
            bvx = tx * vt; bvy = ty * vt;
        }
    }
}

int play_tick(const play_in_t *in)
{
    int started = PLAY_NONE;
    gates = in->gates;
    events = 0;
    float dt = in->dt > 0 ? in->dt : 0;
    bool flat = sqrtf(in->gx * in->gx + in->gy * in->gy) < FLAT_G;
    bool still = in->jerk_g < PLAY_STILL_G;

    /* THE STILLNESS: flat AND still accumulate; a tilt or a jolt breaks
     * it and the 3 s start over (a wrist that fidgets is not held) */
    if (flat && still) still_s += dt;
    else still_s = 0;

    /* A GATE CLOSING ends everything of play's at once: the bead's game,
     * the drop (owed or falling, no mark), the wet mark, the course --
     * nothing visible from here (the dark house draws nothing, the page
     * covers the glass; a word has it) */
    if (!gates_open()) {
        int g = gate(in->now_ms, 0);
        if (bead_open) bead_over(play_refused_name(g));
        if (course_on) course_over(play_refused_name(g));
        drop_due = -1; drop_on = false; dspeed = 0; mark_t = -1;
    }

    /* THE BEAD's game */
    if (bead_open) {
        if (bead_on) {
            bead_step(in->gx, in->gy, dt);
            over_s = 0;
        } else {
            if (flat) over_s += dt; else over_s = 0;
            if (over_s >= BEAD_OVER_S) bead_over("flat 10 s, no bead");
        }
    }
    if (lump_t >= 0) { lump_t += dt; if (lump_t >= BEAD_LUMP_S) lump_t = -1; }

    /* CATCH THE DROP: the drop owed leaves the air door (the rim opposite
     * the wall he sits on) when its delay is spent; then it falls */
    if (drop_due >= 0) {
        drop_due -= dt;
        if (drop_due <= 0) {
            float phi = walls_phi(in->wall);
            dx = -sinf(phi) * DROP_DOOR_R; dy = -cosf(phi) * DROP_DOOR_R;
            dspeed = 0; drop_on = true; drop_inside = false; drop_due = -1; drops++;
            events |= PLAY_EV_DROP;
            say("play: a drop from the air door (over the %s wall): it falls down gravity", WALL_NAMES[in->wall & 3]);
        }
    }
    if (drop_on) drop_step(in, dt);
    if (catch_t >= 0) { catch_t += dt; if (catch_t >= CATCH_LUMP_S) catch_t = -1; }
    if (mark_t >= 0) { mark_t += dt; if (mark_t >= MARK_S) mark_t = -1; }

    /* THE COURSE: the wall he is on, read when landed; a change is a
     * fall taken -- the next gate passed, or the course's own start */
    if (in->on_wall && in->wall != cur_wall) {
        int from = cur_wall;
        cur_wall = in->wall & 3;
        if (from >= 0) {
            if (course_on) {
                if (cur_wall == gate_wall[passed]) {
                    passed++;
                    course_idle = 0;
                    events |= PLAY_EV_GATE;
                    if (passed >= COURSE_GATES) {
                        course_on = false; passed = 0;
                        events |= PLAY_EV_COURSE_DONE;
                        say("play: through the %s gate, the third: THE COURSE RUN, the gates gone", WALL_NAMES[cur_wall]);
                    } else {
                        say("play: through the %s gate (%d of %d; the %s next)", WALL_NAMES[cur_wall], passed, COURSE_GATES, WALL_NAMES[gate_wall[passed]]);
                    }
                } else {
                    say("play: landed on the %s wall, not a gate yet (the %s is next)", WALL_NAMES[cur_wall], WALL_NAMES[gate_wall[passed]]);
                }
            } else if (bead_open && !bead_on) {
                int g = gate(in->now_ms, course_ms);
                refused = g;
                if (g == PLAY_OK) {
                    int d = ((cur_wall - from) & 3) == 3 ? -1 : 1;   /* the way round the first fall went (a half turn: the bowl's own way, +1) */
                    for (int i = 0; i < COURSE_GATES; i++) gate_wall[i] = (cur_wall + d * (i + 1)) & 3;
                    course_on = true; passed = 0; course_idle = 0;
                    course_ms = in->now_ms ? in->now_ms : 1;
                    counts[PLAY_COURSE]++;
                    started = PLAY_COURSE;
                    say("play: a fall in the open game: THE COURSE -- the %s, the %s, then the %s wall", WALL_NAMES[gate_wall[0]], WALL_NAMES[gate_wall[1]], WALL_NAMES[gate_wall[2]]);
                } else {
                    say("play: a fall in the open game: no course (%s)", play_refused_name(g));
                }
            }
        }
    }
    if (course_on) {
        course_idle += dt;
        if (course_idle >= COURSE_IDLE_S) course_over("30 s at one wall");
    }

    /* THE SPIN: the yaw over the threshold, about the normal, for the
     * span -- judged once; the yaw back under half the threshold frees
     * the next */
    {
        float yaw = fabsf(in->yaw_dps);
        bool flat_spin = yaw >= SPIN_DPS && yaw >= SPIN_AXIS_K * in->rate_dps;
        if (flat_spin) {
            spin_span_ms += dt * 1000.0f;
            if (yaw > spin_peak) spin_peak = yaw;
            if (!spin_read && spin_span_ms >= (float)SPIN_SPAN_MS) {
                spin_read = true;
                last_spin_dps = spin_peak;
                int g = gate(in->now_ms, spin_ms);
                refused = g;
                if (g == PLAY_OK) {
                    spin_ms = in->now_ms ? in->now_ms : 1;
                    dizzy_t = SPIN_DIZZY_S;
                    dizzy_sign = in->yaw_dps >= 0 ? 1 : -1;
                    counts[PLAY_SPIN]++;
                    started = PLAY_SPIN;
                    say("play: a spin at %.0f deg/s (%s): dizzy for %.0f s", (double)spin_peak, dizzy_sign > 0 ? "clockwise" : "widdershins", (double)SPIN_DIZZY_S);
                } else {
                    say("play: a spin at %.0f deg/s: nothing (%s)", (double)spin_peak, play_refused_name(g));
                }
            }
        } else {
            spin_span_ms = 0;
            if (yaw < SPIN_STILL_DPS) { spin_read = false; spin_peak = 0; }
        }
    }
    if (dizzy_t > 0) { dizzy_t -= dt; if (dizzy_t < 0) dizzy_t = 0; }

    /* THE TOSS: an EXCURSION is the in-plane jerk over TOSS_G and mostly
     * in the plane; it keeps the direction of its first frame. A
     * REVERSAL is an excursion starting against the last one's direction
     * within TOSS_REVERSAL_MS of its end -- a shake's shape. A flick that
     * lasted TOSS_SPAN_MS, had no reversal just before it and gets none
     * for TOSS_REVERSAL_MS after it is the toss (read then, once). */
    {
        float jp = sqrtf(in->jx_g * in->jx_g + in->jy_g * in->jy_g);
        bool over = jp >= TOSS_G && jp >= TOSS_PLANE_K * in->jerk_g;
        if (over) {
            float ux = in->jx_g / jp, uy = in->jy_g / jp;
            if (!exc_on) {
                exc_on = true; exc_x = ux; exc_y = uy; exc_span_ms = 0;
                if (last_exc_set && in->now_ms - last_exc_end_ms <= TOSS_REVERSAL_MS && ux * last_exc_x + uy * last_exc_y < 0) {
                    last_rev_ms = in->now_ms ? in->now_ms : 1;       /* out, and back: the shake's shape */
                    if (cand) { cand = false; say("play: a flick of %.1f g, then back: a shake, not a toss", (double)cand_g); cand_g = 0; }
                }
            } else if (ux * exc_x + uy * exc_y < 0) {
                /* the jerk swung round inside the excursion: a reversal too */
                last_rev_ms = in->now_ms ? in->now_ms : 1;
                exc_x = ux; exc_y = uy; exc_span_ms = 0;
                if (cand) { cand = false; cand_g = 0; }
            }
            exc_span_ms += dt * 1000.0f;
            if (jp > cand_g && !cand) cand_g = jp;
        } else if (exc_on) {
            exc_on = false;
            last_exc_set = true; last_exc_x = exc_x; last_exc_y = exc_y; last_exc_end_ms = in->now_ms;
            bool rev_before = last_rev_ms && in->now_ms - last_rev_ms <= TOSS_REVERSAL_MS + (uint32_t)exc_span_ms;
            if (exc_span_ms >= (float)TOSS_SPAN_MS && !rev_before && !cand) {
                cand = true; cand_ms = in->now_ms; cand_x = exc_x; cand_y = exc_y;
            } else if (!cand) cand_g = 0;
        }
        if (cand && in->now_ms - cand_ms >= TOSS_REVERSAL_MS) {
            cand = false;
            int g = gate(in->now_ms, 0);
            if (g == PLAY_OK && gates.falling) g = PLAY_REFUSED_FALLING;
            refused = g;
            if (g == PLAY_OK) {
                toss_t = 0; toss_air = true; toss_x = cand_x; toss_y = cand_y; toss_dir_set = true;
                toss_sense = sense_due(in->now_ms, &toss_sense_ms);
                counts[PLAY_TOSS]++;
                started = PLAY_TOSS;
                say("play: a toss, %.1f g toward %+.1f,%+.1f: %.0f px off the rim", (double)cand_g, (double)cand_x, (double)cand_y, (double)TOSS_LIFT_PX);
            } else {
                say("play: a flick of %.1f g: no toss (%s)", (double)cand_g, play_refused_name(g));
            }
            cand_g = 0;
        }
    }
    /* the flight: up over TOSS_UP_S (eased), then down under the fall's
     * own FALL_G; the landing is one take, the splat eases off after */
    if (toss_t >= 0) {
        toss_t += dt;
        if (toss_air) {
            float t_down = sqrtf(2.0f * TOSS_LIFT_PX / FALL_G);
            if (toss_t >= TOSS_UP_S + t_down) { toss_air = false; toss_t = 0; toss_landed = true; say("play: the toss lands: the small splat, the squint"); }
        } else if (toss_t >= TOSS_LAND_S) toss_t = -1;
        if (!gates.holding && toss_t >= 0) { toss_t = -1; toss_air = false; }   /* the house went empty under it: nothing to land */
    }

    /* THE RHYTHM's pulses, on the clock: each starts at its time and
     * plays RHYTHM_PULSE_MS; the last one's end is the take */
    if (rh_n) {
        if (!gates_open()) { rh_n = rh_i = 0; rh_t = -1; say("play: the echo stops (%s)", play_refused_name(gate(in->now_ms, 0))); }
        else {
            if (rh_t >= 0) { rh_t += dt; if (rh_t * 1000.0f >= (float)RHYTHM_PULSE_MS) rh_t = -1; }
            if (rh_t < 0 && rh_i < rh_n && (int32_t)(in->now_ms - rh_at[rh_i]) >= 0) { rh_t = 0; rh_i++; }
            if (rh_t < 0 && rh_i >= rh_n) { rh_n = rh_i = 0; rh_done = true; say("play: the beat echoed: the happy eyes"); }
        }
    }
    return started;
}

int play_rhythm(uint32_t now_ms, const uint32_t *gaps_ms, int n_gaps, uint32_t since_last_ms)
{
    if (n_gaps < 0) n_gaps = 0;
    if (n_gaps > RHYTHM_MAX - 1) n_gaps = RHYTHM_MAX - 1;
    int g = gate(now_ms, 0);
    refused = g;
    if (g != PLAY_OK) { say("play: a rhythm of %d taps: no echo (%s)", n_gaps + 1, play_refused_name(g)); return 0; }
    uint32_t first = now_ms + (since_last_ms < RHYTHM_DELAY_MS ? RHYTHM_DELAY_MS - since_last_ms : 0);
    rh_n = n_gaps + 1; rh_i = 0; rh_t = -1;
    rh_at[0] = first;
    for (int i = 0; i < n_gaps; i++) rh_at[i + 1] = rh_at[i] + gaps_ms[i];
    rh_sense = sense_due(now_ms, &rh_sense_ms);
    counts[PLAY_RHYTHM]++;
    char line[96]; int o = 0;
    for (int i = 0; i < n_gaps && o < (int)sizeof line - 8; i++) o += snprintf(line + o, sizeof line - o, "%s%u", i ? " " : "", (unsigned)gaps_ms[i]);
    say("play: a rhythm of %d taps (%s ms): echoed %u ms after the last", rh_n, line, (unsigned)(first - now_ms + since_last_ms));
    return rh_n;
}

float play_toss_lift(void)
{
    if (toss_t < 0 || !toss_air) return 0;
    if (toss_t < TOSS_UP_S) { float u = toss_t / TOSS_UP_S; return TOSS_LIFT_PX * (1.0f - (1.0f - u) * (1.0f - u)); }   /* up, easing out at the top */
    float t = toss_t - TOSS_UP_S;
    float l = TOSS_LIFT_PX - 0.5f * FALL_G * t * t;
    return l > 0 ? l : 0;
}
float play_toss_squash(void)
{
    if (toss_t < 0 || toss_air) return 0;
    float u = toss_t / TOSS_LAND_S;
    if (u > 1) u = 1;
    float sm = u * u * (3.0f - 2.0f * u);
    return TOSS_SPLAT * (1.0f - sm);
}
bool  play_toss_dir(float *x, float *y) { if (x) *x = toss_x; if (y) *y = toss_y; return toss_dir_set; }
bool  play_toss_landed(void)    { bool b = toss_landed; toss_landed = false; return b; }
bool  play_toss_sense_due(void) { bool b = toss_sense; toss_sense = false; return b; }
float play_rhythm_squash(void)
{
    if (rh_t < 0) return 0;
    float u = rh_t * 1000.0f / (float)RHYTHM_PULSE_MS;
    if (u > 1) u = 1;
    return RHYTHM_PULSE * sinf((float)M_PI * u);
}
int   play_rhythm_pending(void)   { return rh_n ? rh_n - rh_i + (rh_t >= 0 ? 1 : 0) : 0; }
bool  play_rhythm_done(void)      { bool b = rh_done; rh_done = false; return b; }
bool  play_rhythm_sense_due(void) { bool b = rh_sense; rh_sense = false; return b; }

int play_tap(uint32_t now_ms, int x, int y, bool on_gel)
{
    if (!bead_open && still_s < BEAD_STILL_S) { refused = PLAY_REFUSED_NOT_ARMED; return 0; }   /* not the bead's: the far tap or the pet as ever */
    if (on_gel) { refused = PLAY_REFUSED_ON_GEL; return 0; }
    if (bead_open) {
        if (bead_on) return 0;                    /* one bead in the bowl at a time: the tap is what it always was */
        bead_on = true;
        bx = (float)(x - 120); by = (float)(y - 120); bvx = bvy = 0;
        over_s = 0;
        say("play: another bead at %d,%d", x, y);
        return 1;
    }
    int g = gate(now_ms, bead_ms);
    refused = g;
    if (g != PLAY_OK) {
        say("play: a tap after %.1f s flat and still: no bead (%s)", (double)still_s, play_refused_name(g));
        return 0;
    }
    bead_ms = now_ms ? now_ms : 1;
    bead_open = true; bead_on = true;
    bx = (float)(x - 120); by = (float)(y - 120); bvx = bvy = 0;
    over_s = 0; bites = 0;
    counts[PLAY_BEAD]++;
    say("play: the bead at %d,%d (flat and still %.1f s): it rolls with the tilt", x, y, (double)still_s);
    return 2;
}

void play_bead_eat(uint32_t now_ms)
{
    (void)now_ms;
    if (!bead_on) return;
    bead_on = false; bvx = bvy = 0; over_s = 0;
    lump_t = 0;
    bites++;
    if (bites % CATCH_EVERY_BITES == 0 && !drop_on) {   /* every second bite: a drop is owed (one in the air at a time) */
        drop_due = CATCH_DELAY_S;
        say("play: the bead rolled into him: eaten (a small lump) -- the %luth bite: a drop is coming", (unsigned long)bites);
    } else {
        say("play: the bead rolled into him: eaten (a small lump)");
    }
}

/* --- CATCH THE DROP / THE COURSE: the readings ---------------------------- */

uint32_t play_events(void) { return events; }
bool  play_drop_on(float *x, float *y)
{
    if (x) *x = 120.0f + dx;
    if (y) *y = 120.0f + dy;
    return drop_on;
}
float play_drop_due_s(void) { return drop_due > 0 ? drop_due : 0; }
bool  play_mark_on(float *x, float *y, float *k)
{
    if (x) *x = 120.0f + mx;
    if (y) *y = 120.0f + my;
    if (k) *k = mark_t >= 0 ? 1.0f - mark_t / MARK_S : 0;
    return mark_t >= 0;
}
void  play_catch_tally(uint32_t *d, uint32_t *c, uint32_t *m)
{
    if (d) *d = drops;
    if (c) *c = caught_n;
    if (m) *m = missed_n;
}
bool  play_course_on(void) { return course_on; }
int   play_course_gates(int walls[COURSE_GATES], bool lit[COURSE_GATES])
{
    for (int i = 0; i < COURSE_GATES; i++) {
        if (walls) walls[i] = course_on ? gate_wall[i] : -1;
        if (lit) lit[i] = course_on && i < passed;
    }
    return course_on ? passed : 0;
}
int   play_course_next(void)   { return course_on ? gate_wall[passed] : -1; }
float play_course_idle_s(void) { return course_on ? course_idle : 0; }

void  play_status(char *out, int cap)
{
    float x, y, k;
    int n = snprintf(out, cap, "play: %s (flat and still %.1f s); the bead game %s; %lu bead games, %lu spins, %lu courses, %lu tosses, %lu rhythms; %lu drops (%lu caught, %lu missed)%s",
                     still_s >= BEAD_STILL_S ? "ARMED" : "not armed", (double)still_s,
                     bead_open ? (bead_on ? "OPEN, a bead rolling" : "OPEN, no bead") : "closed",
                     (unsigned long)counts[PLAY_BEAD], (unsigned long)counts[PLAY_SPIN], (unsigned long)counts[PLAY_COURSE],
                     (unsigned long)counts[PLAY_TOSS], (unsigned long)counts[PLAY_RHYTHM],
                     (unsigned long)drops, (unsigned long)caught_n, (unsigned long)missed_n,
                     toss_t >= 0 ? (toss_air ? "; a toss in the air" : "; the toss's splat") : rh_n ? "; echoing a beat" : dizzy_t > 0 ? "; dizzy" : "");
    if (n < 0 || n >= cap) return;
    if (drop_due > 0) n += snprintf(out + n, cap - n, "; a drop owed in %.1f s", (double)drop_due);
    else if (play_drop_on(&x, &y)) n += snprintf(out + n, cap - n, "; a drop falling at %.0f,%.0f", (double)x, (double)y);
    if (n < 0 || n >= cap) return;
    if (play_mark_on(&x, &y, &k)) n += snprintf(out + n, cap - n, "; a wet mark at %.0f,%.0f (%.0f %% left)", (double)x, (double)y, (double)(k * 100.0f));
    if (n < 0 || n >= cap) return;
    if (course_on) n += snprintf(out + n, cap - n, "; the course: %s %s %s (%d lit, the %s next, %.0f s at this wall)",
                                 WALL_NAMES[gate_wall[0]], WALL_NAMES[gate_wall[1]], WALL_NAMES[gate_wall[2]], passed, WALL_NAMES[gate_wall[passed]], (double)course_idle);
    else n += snprintf(out + n, cap - n, "; no course");
    if (n < 0 || n >= cap) return;
    snprintf(out + n, cap - n, "; lump %.2f; the last refusal: %s", (double)play_lump(), play_refused_name(refused));
}

bool play_bead_on(float *x, float *y)
{
    if (x) *x = 120.0f + bx;
    if (y) *y = 120.0f + by;
    return bead_on;
}
bool  play_bead_armed(void) { return still_s >= BEAD_STILL_S; }
bool  play_bead_open(void)  { return bead_open; }
float play_still_s(void)    { return still_s; }
float play_lump(void)
{
    float bead = 0, meal = 0;
    if (lump_t >= 0) {
        float t = lump_t / BEAD_LUMP_S;
        if (t > 1) t = 1;
        bead = BEAD_LUMP * (1.0f - t) * (1.0f - t);   /* the meal's own ease-out: fast at first, the last of it lingering */
    }
    if (catch_t >= 0) {                                /* the caught drop: the meal's lump, the meal's minute */
        float t = catch_t / CATCH_LUMP_S;
        if (t > 1) t = 1;
        meal = CATCH_LUMP * (1.0f - t) * (1.0f - t);
    }
    return meal > bead ? meal : bead;                  /* one belly: the larger */
}
float play_dizzy(void)      { return dizzy_t > 0 ? dizzy_t / SPIN_DIZZY_S : 0; }
int   play_dizzy_sign(void) { return dizzy_sign; }
float play_spin_dps(void)   { return last_spin_dps; }
bool  play_busy(void)       { return bead_on || dizzy_t > 0 || drop_on || course_on || toss_t >= 0 || rh_n > 0; }
int   play_refused(void)    { return refused; }
uint32_t play_count(int game) { return (game >= PLAY_BEAD && game <= PLAY_RHYTHM) ? counts[game] : 0; }
bool  play_log_new(void)    { return log_new; }
const char *play_log(void)  { return log_line; }
void  play_log_taken(void)  { log_new = false; }
