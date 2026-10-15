/* THE DOOR'S OWN PROOF (2026-10-13, the operator night T10), pure: the
 * wrist proves the key to every door (`hi <nonce>` -> `hello watch
 * <mac>`), but until tonight NO door proved itself back -- any host that
 * said `hi` on a door's port was answered, and could then `wifi add` a
 * network into NVS, push a `look`, knock `lost`, set the alarm. Now the
 * hello carries the wrist's own nonce -- `hello watch <mac> <nonce2>` --
 * and a desk with the key answers `ok hello <desk> <kind> <epoch>
 * <mac2>`, mac2 = HMAC(key, "<nonce2> desk") over the one primitive both
 * doors share. A desk that cannot (an old watchlink: a bare `ok hello`,
 * or a host with no key answering `ok hello` to anything) gets a
 * READ-ONLY session: the hands that change the wrist or the house --
 * `wifi add`, `look`, `knock`, `alarm` -- are refused `no proof`, and
 * `time`'s answer is not taken while the clock is already set (a cold
 * boot still gets its hour: better a stranger's hour than none). The day
 * sync, the lease verbs, grain, word, sky, status all flow as before:
 * nothing the morning needs is refused. A bare board (no key) has no
 * session at all, as today.
 *
 * THE OLD DESK'S FIRST WORD: watchlink's `_hello` of old counts the
 * words (`len(args) != 2`) and answers `no hello` WITHOUT hanging up,
 * the door nonce kept -- so a `no hello` to the nonce'd line is read as
 * "an old desk" and the bare hello of old goes out once more on the same
 * door nonce (proof_refused); that one's bare `ok hello` lands as
 * UNPROVEN. Over the dock the desk also answers the wrist's own log
 * lines `no hello`: the SECOND `no hello` (the echo of the first hello's
 * log line) arrives while the retry is out and is filtered as before.
 *
 * No Arduino, no mbedtls: the caller hands in two random words for the
 * nonce (esp_random on the board) and the HMAC behind a seam
 * (link.cpp's hmac_msg; the test's own). Proven by tools/proof_test.cpp. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define PROOF_NONCE_HEX 16   /* the wrist's nonce: two random words as 16 hex (the desk's is 16 hex too) */
#define PROOF_MAC_HEX   32   /* the first 16 bytes of HMAC-SHA256, as the hello's own mac */

enum {
    PROOF_NONE = 0,      /* no hello out (a bare board has no session: as today) */
    PROOF_ASKED,         /* the nonce'd hello is out */
    PROOF_RETRY,         /* the door said `no hello` to it (an old desk): the bare hello of old is out */
    PROOF_PROVEN,        /* the door's mac2 is the key's: every ask is taken */
    PROOF_UNPROVEN,      /* a bare `ok hello`, or a wrong mac2: this session is READ-ONLY */
};

typedef void (*proof_hmac_fn)(const char *msg, char *out33);   /* the seam: HMAC-SHA256(key, msg), the first 16 bytes as 32 hex */

typedef struct {
    uint8_t state;                       /* PROOF_* */
    bool    wrong;                       /* UNPROVEN because a mac2 was there and was not the key's (a sharper log line than an old desk's) */
    char    nonce[PROOF_NONCE_HEX + 1];  /* the wrist's own nonce for this hello */
    char    mac[PROOF_MAC_HEX + 1];      /* the wrist's answer to the door nonce, kept for the one retry */
} proof_t;

static inline void proof_reset(proof_t *p) { memset(p, 0, sizeof *p); }

/* a new hello: the wrist's nonce from two random words, the mac the
 * caller computed over the door's nonce; the state ASKED */
static inline void proof_ask(proof_t *p, const char *mac, uint32_t r1, uint32_t r2)
{
    memset(p, 0, sizeof *p);
    snprintf(p->nonce, sizeof p->nonce, "%08lx%08lx", (unsigned long)r1, (unsigned long)r2);
    strncpy(p->mac, mac, PROOF_MAC_HEX); p->mac[PROOF_MAC_HEX] = 0;
    p->state = PROOF_ASKED;
}

/* the hello line: `hello watch <mac> <nonce2>` while ASKED, the bare
 * `hello watch <mac>` of old on the RETRY (and in any other state).
 * Returns the length; 62 chars (a 64-byte buffer holds it). */
static inline int proof_hello_line(const proof_t *p, char *out, int cap)
{
    if (p->state == PROOF_ASKED) return snprintf(out, cap, "hello watch %s %s\n", p->mac, p->nonce);
    return snprintf(out, cap, "hello watch %s\n", p->mac);
}

/* the door's word to the nonce'd hello was `no hello`: an old desk. true
 * = send the bare hello once (the caller's request); the state RETRY.
 * Any other word, or a second `no hello`, is false: the caller's own
 * handling (the refusal, or the dock's filter) as before. */
static inline bool proof_refused(proof_t *p, const char *line)
{
    if (p->state != PROOF_ASKED || strcmp(line, "no hello")) return false;
    p->state = PROOF_RETRY;
    return true;
}

/* the door's `ok hello <desk> <kind> <epoch> [<mac2>]`: the sixth word
 * judged against HMAC(key, "<nonce2> desk"). No sixth word = UNPROVEN
 * (an old desk's bare answer); a sixth word that is not the key's =
 * UNPROVEN with `wrong` set; on the RETRY (no nonce went out) any answer
 * is UNPROVEN. Returns the state landed on. */
static inline int proof_judge(proof_t *p, const char *line, proof_hmac_fn hmac)
{
    if (p->state != PROOF_ASKED && p->state != PROOF_RETRY) return p->state;
    const char *w6 = NULL;
    const char *s = line; int word = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (++word == 6) { w6 = s; break; }
        while (*s && *s != ' ') s++;
    }
    p->wrong = false;
    if (p->state == PROOF_RETRY || !w6) { p->state = PROOF_UNPROVEN; return p->state; }
    char msg[PROOF_NONCE_HEX + 6];
    snprintf(msg, sizeof msg, "%s desk", p->nonce);
    char want[PROOF_MAC_HEX + 1];
    hmac(msg, want);
    size_t n = 0; while (w6[n] && w6[n] != ' ' && w6[n] != '\r' && w6[n] != '\n') n++;
    unsigned diff = (n != PROOF_MAC_HEX);
    for (size_t i = 0; i < PROOF_MAC_HEX && i < n; i++) diff |= (unsigned)(w6[i] ^ want[i]);   /* the whole word read, as the knock's judge does */
    if (diff) { p->wrong = true; p->state = PROOF_UNPROVEN; }
    else p->state = PROOF_PROVEN;
    return p->state;
}

/* the session is read-only: an unproven door's hands are refused */
static inline bool proof_read_only(const proof_t *p) { return p->state == PROOF_UNPROVEN; }

/* the asks a read-only session refuses `no proof` -- the hands that
 * change the wrist or the house: `wifi add` (NVS), `look` (the wear),
 * `knock` (the lease), `alarm` (the row, and its look). `wisp` and
 * `ble pair` are not gated tonight (ruling 149 says why). */
static inline bool proof_gated_ask(const char *line)
{
    return !strncmp(line, "wifi add ", 9) || !strncmp(line, "look ", 5) || !strncmp(line, "knock ", 6)
        || !strcmp(line, "alarm") || !strncmp(line, "alarm ", 6)
        || !strncmp(line, "ble pair", 8);   /* the operator's ruling (2026-10-13 00:50): the pairing window is a hand too -- an unproven door never opens it */
}

/* `time`'s answer (the clock, the tz, the sun) is taken from an unproven
 * door only while the clock is unset */
static inline bool proof_takes_time(const proof_t *p, bool clock_set) { return !proof_read_only(p) || !clock_set; }

static inline const char *proof_name(const proof_t *p)
{
    static const char *const N[] = { "none", "asked", "retry", "proven", "unproven" };
    return p->state <= PROOF_UNPROVEN ? N[p->state] : "?";
}
