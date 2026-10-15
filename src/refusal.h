/* THE REFUSAL SAID ONCE (2026-10-12, the operator night T5), pure: the
 * ledger of the doors' repeated words. The lit glass beats every 5 s; a
 * box that answers `no gate` for an evening wrote the same log line 12
 * times a minute (the ledger's F3), and `no box` / `no soul` / `no word`
 * / `no sky` the same way. Now each VERB keeps one row -- the word it
 * last got, when that word was first heard, how many times since -- and
 * `refusal_note` says whether the line is worth logging: the first time
 * a verb is refused, and again each time its word CHANGES. Everything
 * between is counted, never said; `link` and doctor carry the count
 * (`refusals: no gate x 212 since 09:12 (beat)`). Nothing lit, nothing
 * on the glass. The table is reset at a new session (the next `ok
 * hello`): a refusal belongs to the door that gave it.
 *
 * No Arduino, no clock: the caller hands in millis() and, for the
 * status line, the minute of the local day (-1 = no clock). Proven by
 * tools/refusal_test.cpp. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REFUSAL_N     6     /* verbs kept at once (beat / claim / come / release / word / sky / status / look: six is more than a session ever needs) */
#define REFUSAL_WORD  12    /* the word kept per row (11 chars: `no gate`, `no verb`, `none` whole; a longer line is cut, the log's first line said it whole) */

typedef struct {
    const char *verb;              /* a static string (REQ_NAMES / a literal); NULL = the row is free */
    char        word[REFUSAL_WORD];
    uint32_t    first_ms;          /* when this word was first heard for this verb */
    uint16_t    count;             /* how many times since, this one included */
} refusal_row_t;

typedef struct {
    refusal_row_t row[REFUSAL_N];
    uint16_t      unkept;          /* lines that found the table full: said as before, not counted */
} refusal_t;

static inline void refusal_reset(refusal_t *t) { memset(t, 0, sizeof *t); }

/* true = log the line (the first time for this verb, or its word changed);
 * false = the same word again: counted, say nothing */
static inline bool refusal_note(refusal_t *t, const char *verb, const char *word, uint32_t now_ms)
{
    refusal_row_t *free_row = NULL;
    for (int i = 0; i < REFUSAL_N; i++) {
        refusal_row_t *r = &t->row[i];
        if (!r->verb) { if (!free_row) free_row = r; continue; }
        if (strcmp(r->verb, verb)) continue;
        if (!strncmp(r->word, word, REFUSAL_WORD - 1)) {   /* the same word again */
            if (r->count < 0xFFFF) r->count++;
            return false;
        }
        strncpy(r->word, word, REFUSAL_WORD - 1); r->word[REFUSAL_WORD - 1] = 0;   /* the word changed: said again, counted afresh */
        r->first_ms = now_ms; r->count = 1;
        return true;
    }
    if (!free_row) { if (t->unkept < 0xFFFF) t->unkept++; return true; }   /* full: said every time as it always was, uncounted */
    free_row->verb = verb;
    strncpy(free_row->word, word, REFUSAL_WORD - 1); free_row->word[REFUSAL_WORD - 1] = 0;
    free_row->first_ms = now_ms; free_row->count = 1;
    return true;
}

/* rows in use (0 = nothing to say) */
static inline int refusal_rows(const refusal_t *t)
{
    int n = 0;
    for (int i = 0; i < REFUSAL_N; i++) if (t->row[i].verb) n++;
    return n;
}

/* "refusals: no gate x 212 since 09:12 (beat), none x 2 since 09:14 (sky)";
 * with no clock (now_min < 0) `since 4 min ago`; "refusals: none" when
 * the table is empty; a full table's overflow named at the end. Returns
 * the rows in use. */
static inline int refusal_status(const refusal_t *t, char *buf, int cap, uint32_t now_ms, int now_min)
{
    int n = 0, rows = 0;
    n += snprintf(buf + n, n < cap ? cap - n : 0, "refusals:");
    for (int i = 0; i < REFUSAL_N; i++) {
        const refusal_row_t *r = &t->row[i];
        if (!r->verb) continue;
        uint32_t ago_s = (now_ms - r->first_ms) / 1000u;
        char since[16];
        if (now_min >= 0) {
            int m = ((now_min - (int)(ago_s / 60u)) % 1440 + 1440) % 1440;
            snprintf(since, sizeof since, "%02d:%02d", m / 60, m % 60);
        } else if (ago_s < 120) snprintf(since, sizeof since, "%lu s ago", (unsigned long)ago_s);
        else snprintf(since, sizeof since, "%lu min ago", (unsigned long)(ago_s / 60u));
        n += snprintf(buf + n, n < cap ? cap - n : 0, "%s %s x %u since %s (%s)", rows ? "," : "", r->word, (unsigned)r->count, since, r->verb);
        rows++;
    }
    if (!rows) n += snprintf(buf + n, n < cap ? cap - n : 0, " none");
    if (t->unkept) n += snprintf(buf + n, n < cap ? cap - n : 0, " (+%u unkept: the table was full)", (unsigned)t->unkept);
    if (cap > 0 && n >= cap) buf[cap - 1] = 0;
    return rows;
}
