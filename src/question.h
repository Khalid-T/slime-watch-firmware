/* THE WORD'S QUESTION (design/watch-ideas.md E45, 2026-10-08): a word
 * from the box whose trimmed text ENDS IN `?` is a question. From the
 * moment it SHOWS on the glass (not from the fetch: a held word is not
 * yet asked) a window of QUESTION_S opens; a PET on him inside it (the
 * tap on the gel, not on the bubble) is the vote `wrist:yes`, the tap
 * on the bubble (the dismissal) is `wrist:no` -- each with the
 * question's first words, so the box knows which question it answers.
 * One vote a question: the first closes the window. The window closing
 * with neither is SILENCE: nothing goes up the wire (silence is not a
 * no -- the box asked and heard nothing, which is what happened); one
 * log line says so. The pet still plays as a pet (and goes up as
 * `wrist:pet` as ever); the bubble shows no mark of being a question
 * (it is the mind's words only: the `?` is the mark). A plain word
 * (no `?`) opens no window; a new word replacing a shown one closes
 * it; a reboot forgets.
 *
 * Pure C, no Arduino: the clock comes in as milliseconds, the word's
 * state as parameters (question_tick is an edge detector on "showing":
 * main.cpp feeds it word_showing() / word_text() each frame), the two
 * votes are asked by main's pet path and its dismissal path; what goes
 * out is a yes / no and the echo. tools/question_test.cpp runs it on
 * the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QUESTION_S        10        /* the window after the show, seconds (the item's own "within ten seconds": a glance, a read, a pet) */
#define QUESTION_ECHO_MAX 48        /* the question's first words carried with the vote (link.cpp's SENSE_LINE is 80: `sense wrist:yes ` + 48 + the newline = 65) */

/* the word's trimmed text ends in `?` */
bool question_is(const char *text);
/* boot: nothing asked */
void question_begin(void);
/* one call a frame with the word's state: a question that just came to
 * show opens the window; the word going (dismissed, replaced) closes
 * it; the window running out with no vote is logged. Returns a log
 * line to print, or NULL */
const char *question_tick(bool showing, const char *text, uint32_t now_ms);
/* the two votes, asked by main at the pet and at the dismissal: true =
 * the vote counts (the window was open; it closes with it) and `out`
 * holds the question's echo; false = no question is open (a pet is a
 * pet, a dismissal a dismissal: nothing goes) */
bool question_yes(uint32_t now_ms, char *out, int cap);
bool question_no(uint32_t now_ms, char *out, int cap);
/* the window is open now (a vote would count) */
bool question_open(uint32_t now_ms);
/* ms left in the window, 0 when none is open */
uint32_t question_left_ms(uint32_t now_ms);
/* the echo: the question's first words, whole, under QUESTION_ECHO_MAX */
void question_echo(const char *text, char *out, int cap);
/* `question` over serial: the last question, its answer, the window */
void question_status(char *out, int cap, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
