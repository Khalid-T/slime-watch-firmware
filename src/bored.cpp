/* THE BORED ROLL -- bored.h says what; this is how. Two clocks in lit
 * seconds (the bored spell, the wear's spell), one die (the same LCG the
 * veil and idle life roll), the desk's numbers. No Arduino. */
#include "bored.h"
#include "grain.h"          /* THE GRAIN: the `bored` knob (u(0.85, 1.18), the desk's own) */
#include "vigor.h"          /* VIGOR: a body running down bores slower; spent, never */
#include <string.h>
#include <stdio.h>

/* --- the tunables, with their cause ----------------------------------------- */
/* the desk's Config.BORED_MIN_S / BORED_MAX_S: a stretch of nothing
 * happening before the body tries on a shape by itself -- LIT seconds
 * here, the glass being lit a minute at a time */
static const float BORED_MIN_S   = 300.0f;
static const float BORED_MAX_S   = 660.0f;
/* the desk's odds at the urge: `costume` weighs 1.0 in its bored table
 * (a wiggle 0.4, a sigh 0.4, a stretch 0.5, a wander 0.5, the wisp
 * 0.25, nothing 0.3: ~3.4 in all) -- about one urge in three; the wrist
 * has no wiggle or wander, so the rest is the urge passing */
static const float WEAR_CHANCE   = 0.3f;
/* the wear: the desk's BORED_HOLD_MS 20 s x WEAR_MULT 15 x u(0.7, 1.5)
 * = 210-450 s; a few lit minutes across spells */
static const float WEAR_MIN_S    = 180.0f;
static const float WEAR_MAX_S    = 420.0f;

/* --- the state ------------------------------------------------------------- */
static struct {
    uint32_t die;
    float    bored_s, bored_at;
    float    wear_s, wear_left_s;
    bool     sure;              /* bored_now(true): the next urge wears */
    int      count;
    int      last_event;
} B;

static uint32_t roll(void) { B.die = B.die * 1664525u + 1013904223u; return B.die >> 8; }
static float rollf(void) { return (roll() & 0xFFFF) / 65535.0f; }

/* the next bored spell: the desk's range x the grain's `bored` knob x
 * vigor's idle factor (a low cell, a slower creature) */
static float next_bored(void)
{
    return (BORED_MIN_S + rollf() * (BORED_MAX_S - BORED_MIN_S)) * grain_knobs()->bored * vigor_idle_k();
}

void bored_begin(uint32_t seed)
{
    memset(&B, 0, sizeof B);
    B.die = seed ? seed : 1;
    B.bored_at = next_bored();
}

bool wardrobe_may_roll(const bored_in_t *in)
{
    return in->lit && in->holding && in->mounted && in->bare && !in->own_worn;
}

int bored_tick(const bored_in_t *in)
{
    B.last_event = BORED_NONE;
    if (in->own_worn) {
        /* the wear's spell: lit, holding seconds only (the dark and the
         * empty house keep the costume, not the clock) */
        if (in->lit && in->holding) {
            B.wear_left_s -= in->dt;
            if (B.wear_left_s <= 0) { B.wear_left_s = 0; B.last_event = BORED_OFF; }
        }
        return B.last_event;
    }
    if (!wardrobe_may_roll(in)) return BORED_NONE;
    B.bored_s += in->dt;
    if (B.bored_s < B.bored_at) return BORED_NONE;
    /* THE URGE (never spent: VIGOR under VIGOR_EMPTY does nothing of its own) */
    bool wear = !vigor_spent() && (B.sure || rollf() < WEAR_CHANCE);
    B.sure = false;
    B.bored_s = 0;
    B.bored_at = next_bored();
    if (!wear) return BORED_NONE;
    B.wear_s = WEAR_MIN_S + rollf() * (WEAR_MAX_S - WEAR_MIN_S);
    B.wear_left_s = B.wear_s;
    B.count++;
    B.last_event = BORED_WEAR;
    return BORED_WEAR;
}

int bored_pick(int count, uint8_t (*flags_at)(int), int last)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        uint8_t f = flags_at(i);
        if ((f & (BORED_FLAG_SAGE | BORED_FLAG_HUMAN)) || i == last) continue;
        n++;
    }
    if (n == 0) {
        /* one form on the rack and it was the last: it is what there is */
        if (last >= 0 && last < count && !(flags_at(last) & (BORED_FLAG_SAGE | BORED_FLAG_HUMAN))) return last;
        return -1;
    }
    int r = (int)(roll() % (uint32_t)n);
    for (int i = 0; i < count; i++) {
        uint8_t f = flags_at(i);
        if ((f & (BORED_FLAG_SAGE | BORED_FLAG_HUMAN)) || i == last) continue;
        if (r-- == 0) return i;
    }
    return -1;
}

void bored_pet(void) { B.bored_s = 0; }
void bored_end(void) { B.wear_left_s = 0; B.bored_s = 0; B.bored_at = next_bored(); }
void bored_now(bool sure) { B.bored_s = B.bored_at; B.sure = sure; }
float bored_wear_s(void) { return B.wear_s; }
int bored_count(void) { return B.count; }

void bored_status(char *out, int cap)
{
    if (B.wear_left_s > 0)
        snprintf(out, cap, "roll: worn, %.0f s of %.0f lit left | %d rolled", B.wear_left_s, B.wear_s, B.count);
    else
        snprintf(out, cap, "roll: bored %.0f s of %.0f lit | %d rolled", B.bored_s, B.bored_at, B.count);
}
