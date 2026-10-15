/* THE WORD'S QUESTION -- question.h says what; this is how. */
#include "question.h"
#include <string.h>
#include <stdio.h>

enum { Q_NONE = 0, Q_OPEN, Q_YES, Q_NO, Q_SILENT, Q_GONE };
static const char *const ANSWER_NAMES[] = { "none asked", "open", "yes", "no", "silence (nothing went)", "gone before the window closed" };

static struct {
    int      state;
    bool     was_showing;             /* the edge detector's memory */
    uint32_t shown_ms;                /* when the question came to show */
    char     echo[QUESTION_ECHO_MAX + 1];
    char     logline[QUESTION_ECHO_MAX + 96];
} Q;

bool question_is(const char *text)
{
    if (!text) return false;
    int n = (int)strlen(text);
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
    return n > 0 && text[n - 1] == '?';
}

void question_begin(void) { memset(&Q, 0, sizeof Q); }

void question_echo(const char *text, char *out, int cap)
{
    if (cap <= 0) return;
    out[0] = 0;
    if (!text) return;
    while (*text == ' ') text++;
    int lim = cap - 1 < QUESTION_ECHO_MAX ? cap - 1 : QUESTION_ECHO_MAX;
    int n = (int)strlen(text);
    while (n > 0 && text[n - 1] == ' ') n--;
    if (n <= lim) { memcpy(out, text, n); out[n] = 0; return; }
    /* too long: cut at the last space inside the limit (whole words); a
     * first word longer than the limit is cut outright */
    int cut = lim;
    while (cut > 0 && text[cut] != ' ') cut--;
    if (cut == 0) cut = lim;
    while (cut > 0 && text[cut - 1] == ' ') cut--;
    memcpy(out, text, cut); out[cut] = 0;
}

static bool open_now(uint32_t now_ms)
{
    return Q.state == Q_OPEN && (now_ms - Q.shown_ms) < (uint32_t)QUESTION_S * 1000u;
}

const char *question_tick(bool showing, const char *text, uint32_t now_ms)
{
    const char *line = NULL;
    if (showing && !Q.was_showing) {
        /* the word came to show: a question opens its window, a plain word nothing */
        if (question_is(text)) {
            Q.state = Q_OPEN;
            Q.shown_ms = now_ms;
            question_echo(text, Q.echo, sizeof Q.echo);
            snprintf(Q.logline, sizeof Q.logline, "question: '%s' -- a pet in %d s is yes, the tap on it is no, silence is silence", Q.echo, QUESTION_S);
            line = Q.logline;
        }
    } else if (!showing && Q.was_showing && Q.state == Q_OPEN) {
        /* the word went without a vote through these hands (replaced, or
         * dismissed by a path that did not ask): the window closes */
        Q.state = Q_GONE;
        snprintf(Q.logline, sizeof Q.logline, "question: '%s' gone unanswered (the word left the glass): nothing goes", Q.echo);
        line = Q.logline;
    }
    Q.was_showing = showing;
    if (Q.state == Q_OPEN && !open_now(now_ms)) {
        Q.state = Q_SILENT;
        snprintf(Q.logline, sizeof Q.logline, "question: '%s' unanswered in %d s: silence, nothing goes up (silence is not a no)", Q.echo, QUESTION_S);
        line = Q.logline;
    }
    return line;
}

static bool vote(uint32_t now_ms, int answer, char *out, int cap)
{
    if (!open_now(now_ms)) return false;
    Q.state = answer;
    if (out && cap > 0) { strncpy(out, Q.echo, cap - 1); out[cap - 1] = 0; }
    return true;
}

bool question_yes(uint32_t now_ms, char *out, int cap) { return vote(now_ms, Q_YES, out, cap); }
bool question_no(uint32_t now_ms, char *out, int cap)  { return vote(now_ms, Q_NO, out, cap); }
bool question_open(uint32_t now_ms) { return open_now(now_ms); }

uint32_t question_left_ms(uint32_t now_ms)
{
    if (!open_now(now_ms)) return 0;
    return (uint32_t)QUESTION_S * 1000u - (now_ms - Q.shown_ms);
}

void question_status(char *out, int cap, uint32_t now_ms)
{
    if (Q.state == Q_NONE) { snprintf(out, cap, "question: none asked (a word ending in ? opens a %d s window at its show)", QUESTION_S); return; }
    if (open_now(now_ms)) snprintf(out, cap, "question: '%s' open, %lu ms left (a pet = wrist:yes, the tap on it = wrist:no)", Q.echo, (unsigned long)question_left_ms(now_ms));
    else snprintf(out, cap, "question: '%s' -- %s", Q.echo, ANSWER_NAMES[Q.state]);
}
