/* THE BORED ROLL: the wrist's own morphs (design/watch-wardrobe.md rule
 * 4's second half, tick 6; Khalid 2026-09-11: "the watch can only
 * MORPH"). Until 2026-09-23 the wrist only mirrored the desk's look.
 * Now, as the desk's gel tries on a borrowed shape after a stretch of
 * nothing happening (Config.BORED_MIN_S..BORED_MAX_S, the calm preset's
 * pace), the wrist's gel does the same from what is baked: a costume
 * out of the mounted table through the veil, worn for a spell of lit
 * minutes, the veil home.
 *
 * THE CLOCKS, in LIT seconds (the glass is lit for spells of 12-90 s: a
 * bored spell spans many raises, as the wear does):
 *   BORED  counts while the roll's gate holds (wardrobe_may_roll: lit,
 *          holding, the wardrobe mounted, the bare glass -- no page, no
 *          word bubble, no Sage, no veil, the gel standing, the body free,
 *          awake); the dark does not reset it, a pet does (real attention
 *          is boredom's antidote: the desk's rule). At BORED_MIN_S..MAX_S
 *          (the desk's 300-660 x the grain's `bored` knob x vigor's idle
 *          factor) the urge: WEAR_CHANCE of a costume (the desk's own odds
 *          -- `costume` weighs 1.0 in a bored table that weighs ~3.4), else
 *          it passes; either way the clock starts over.
 *   WEAR   the roll's own costume stays WEAR_MIN_S..MAX_S of lit, holding
 *          wear (the desk's BORED_HOLD_MS x WEAR_MULT x u(0.7, 1.5) = 210-450
 *          s), then BORED_OFF: the veil home. The desk's pushed `look`, a
 *          `wardrobe off` over serial, the house emptying end it sooner
 *          (bored_end: the clocks start over).
 *
 * THE PICK is the die's, over the mounted table: never a SAGE-flagged form
 * (rule 5: under a mind line only), never a HUMAN-flagged one (the desk's
 * bored door is the costume rack, `s.mode == "slime"`: the human home is
 * never a bored wear there either), never the last one twice; uniform
 * among the rest (the desk leans the rack by the room's theme; the wrist
 * has no room). Nothing is persisted: a reboot wakes in the gel (or the
 * desk's remembered look), never in a roll's costume.
 *
 * No Arduino: the clock and the state come in as parameters,
 * tools/wardrobe_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float dt;
    bool  lit;          /* the glass is on */
    bool  holding;      /* house_holding(): a gel to be bored at all */
    bool  mounted;      /* the wardrobe is mounted with a form to roll */
    bool  bare;         /* the bare glass, the gel standing, the body free, awake (wardrobe.cpp composes it) */
    bool  own_worn;     /* the roll's own costume is on now (the veil lifted) */
} bored_in_t;

enum { BORED_NONE = 0, BORED_WEAR, BORED_OFF };

/* the flags a table entry carries (wardrobe.cpp's ENTRY_FLAG_*) */
#define BORED_FLAG_SAGE  2
#define BORED_FLAG_HUMAN 4

void bored_begin(uint32_t seed);
/* THE GATE, pure: may the bored clock run / the urge land this frame */
bool wardrobe_may_roll(const bored_in_t *in);
/* one call a frame: BORED_WEAR = put a costume on now (bored_pick says
 * which), BORED_OFF = the wear's spell is over, the veil home */
int  bored_tick(const bored_in_t *in);
/* the die's pick over a table of `count` entries whose flags `flags_at`
 * reads: never SAGE, never HUMAN, never `last` twice (unless it is the
 * only one); -1 = nothing to wear */
int  bored_pick(int count, uint8_t (*flags_at)(int), int last);
void bored_pet(void);            /* attention: the bored clock back to zero */
void bored_end(void);            /* the own wear ended by another hand: the clocks start over */
void bored_now(bool sure);       /* a test aid (`wardrobe roll`): the urge now; sure = the costume for certain */
float bored_wear_s(void);        /* the rolled length of the current / last own wear (lit s) */
int  bored_count(void);          /* costumes rolled since boot */
void bored_status(char *out, int cap);
