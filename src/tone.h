/* THE WORD'S TONE (design/watch-night-2026-10-12.md C5, the operator
 * night). A sentence has three tones besides the plain stop, and the
 * bubble carries them in its own punctuation: a `?` is THE WORD'S
 * QUESTION (question.c, its vote -- nothing of it here); a `!` is the
 * box saying something GLAD; a `...` is the box TRAILING OFF. Until
 * tonight the creature drew every line with the same face. Now, at the
 * word's SHOW (the raise, the tap, `glance` -- not the fetch: a held
 * word has not been read), once per shown word:
 *
 *   `!`    one bright settle (TONE_SETTLE_SQUASH over TONE_SETTLE_S:
 *          idle's settle's depth at less than half its length) and
 *          HAPPY eyes for TONE_HAPPY_S;
 *   `...`  one slow blink (the lids CLOSED for TONE_BLINK_S, two and a
 *          half of idle's) and then the lids half-down -- DROOPY -- for
 *          TONE_DROOP_S; no squash: nothing in the body moves.
 *
 * A plain stop, no punctuation, a `?`: nothing. The bubble wears NO
 * MARK (the punctuation is the mark, ruling 100's rule for the
 * question holds for the other two). Never a sound, never a bump,
 * never a sense: a tone is a face, not news. A beat begun runs to its
 * end (the word tapped away in its first tenth was still read); the
 * word staying on the glass plays nothing more; a new word is a new
 * show. The eyes come in for the beat and go out over TONE_FADE_S after
 * it when main's own are down (the max rule in draw(): the larger
 * presence wins, so engaged eyes are untouched).
 *
 * Pure C, no Arduino: an edge detector main feeds word_showing() /
 * word_text() each frame beside question_tick (the same shape), and a
 * look draw() folds in beside the drowse's. State: an int, a bool, a
 * stamp. tools/tone_test.cpp runs it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gel.h"            /* GEL_EYE_HAPPY, GEL_EYE_CLOSED, GEL_EYE_DROOPY */

#ifdef __cplusplus
extern "C" {
#endif

#define TONE_HAPPY_S        1.2f    /* `!`: the happy eyes, seconds (the peekaboo's and the shove's are 1.0; a glad word a hair longer) */
#define TONE_SETTLE_SQUASH  0.06f   /* `!`: the settle's depth (idle's own SETTLE_SQUASH) ... */
#define TONE_SETTLE_S       0.6f    /* `!`: ... over this (idle's is 1.5 s: this one is bright) */
#define TONE_BLINK_S        0.30f   /* `...`: the slow blink's close (idle's BLINK_S is 0.12) */
#define TONE_DROOP_S        1.0f    /* `...`: the lids half-down for this after the blink (the item's "a second") */
#define TONE_FADE_S         0.4f    /* the eyes' presence going out after the beat (the drowse's OUT) */

enum { TONE_NONE = 0, TONE_GLAD = 1, TONE_TRAIL = 2 };

/* what draw() folds in */
typedef struct {
    int   eye_kind;         /* GEL_EYE_* for the beat, -1 = leave the draw's */
    float eye_alpha;        /* the eyes' presence for the beat, 0 when none */
    float squash;           /* the settle, added (0 but for the glad word's) */
} tone_fx_t;

/* the tone a word's trimmed text ends in: `!` = TONE_GLAD, `...` (or the
 * one-glyph ellipsis) = TONE_TRAIL, anything else -- a stop, nothing, a
 * `?` (question.c's) -- TONE_NONE */
int  tone_of(const char *text);
/* boot: nothing playing (the statics are zero at boot; the test resets) */
void tone_begin(void);
/* one call a frame with the word's state: a word that just came to
 * show starts its tone's beat (once per shown word). Returns the beat
 * started this frame (TONE_GLAD / TONE_TRAIL) or TONE_NONE */
int  tone_tick(bool showing, const char *text, uint32_t now_ms);
/* this frame's look */
void tone_fx(tone_fx_t *out, uint32_t now_ms);
/* a beat is playing now (its fade included) */
bool tone_on(uint32_t now_ms);
/* the log's word for a tone */
const char *tone_name(int tone);

#ifdef __cplusplus
}
#endif
