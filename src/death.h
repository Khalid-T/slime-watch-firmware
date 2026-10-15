/* THE LAST DEATH (the operator night, 2026-10-12; T2): how the body
 * last died, pure. The hardware half (lastdeath.cpp) reads the SoC's
 * reset reason once at boot and maps it to a DEATH_* code; keeps THE
 * BREADCRUMB -- a small struct in RTC slow memory (RTC_NOINIT_ATTR:
 * kept across every reset but a power-on) ticked once a second from
 * loop with the uptime, the last console verb, the last nap's kind and
 * the link's state, sealed with a sum so a cold boot's garbage is never
 * read as a life; and the NVS counters `boots` / `crashes`. This header
 * holds the words, the validity, the counters' rule (ruling 112: a
 * crash is any death but power-on, software and a deep-sleep wake),
 * the hour inside which the verdict names it, the doctor's lines, and
 * the day header's reserved bytes: [0] boots today, [1] crashes today,
 * [2..3] the build id's crc16 (CCITT-FALSE, little-endian) -- reserved
 * bytes only, a file older than tonight reads as 0 / 0 / 0.
 * No Arduino: tools/death_test.cpp proves it on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

enum {
    DEATH_POWERON = 0, DEATH_SOFTWARE, DEATH_TASK_WDT, DEATH_INT_WDT, DEATH_OTHER_WDT,
    DEATH_BROWNOUT, DEATH_PANIC, DEATH_DEEPSLEEP, DEATH_UNKNOWN, DEATH_N
};
static const char *const DEATH_WORDS[DEATH_N] = {
    "power-on", "software", "the task watchdog", "the interrupt watchdog", "another watchdog",
    "brownout", "a panic", "deep sleep", "unknown"
};
static inline const char *death_word(int code)
{
    return (code >= 0 && code < DEATH_N) ? DEATH_WORDS[code] : DEATH_WORDS[DEATH_UNKNOWN];
}

/* ruling 112: a crash is any death the body did not choose and the cell did not cause by absence */
static inline bool death_is_crash(int code)
{
    return !(code == DEATH_POWERON || code == DEATH_SOFTWARE || code == DEATH_DEEPSLEEP);
}

/* the counters' rule: one boot each life, one crash when the death was one */
static inline void death_count(int code, uint32_t *boots, uint32_t *crashes)
{
    if (*boots != 0xFFFFFFFFu) (*boots)++;
    if (death_is_crash(code) && *crashes != 0xFFFFFFFFu) (*crashes)++;
}

#define DEATH_RECENT_S 3600        /* the verdict names a crash inside the last hour; older is the morning's history, not its first thing wrong */
static inline bool death_recent(int code, uint32_t up_s) { return death_is_crash(code) && up_s < DEATH_RECENT_S; }

/* --- the breadcrumb ---------------------------------------------------------- */

#define DEATH_MAGIC 0x54414544u    /* "DEAT" little-endian */
#define DEATH_VERB_CAP 12
#define DEATH_LINK_CAP 8
enum { DEATH_NAP_NONE = 0, DEATH_NAP_DEEP, DEATH_NAP_EAR, DEATH_NAP_N };
static const char *const DEATH_NAP_WORDS[DEATH_NAP_N] = { "none", "deep", "the ear" };

typedef struct {
    uint32_t magic;
    uint32_t uptime_s;                 /* the life's length when the crumb was last ticked */
    char     verb[DEATH_VERB_CAP];     /* the last console verb's first 11 characters, NUL */
    uint8_t  nap;                      /* DEATH_NAP_* */
    uint8_t  pad[3];
    char     link[DEATH_LINK_CAP];     /* the link's state word, first 7 characters */
    uint32_t sum;                      /* FNV-1a over everything before it */
} death_crumb_t;                       /* 36 bytes in RTC slow memory */

static inline uint32_t death_sum(const death_crumb_t *c)
{
    const uint8_t *p = (const uint8_t *)c;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(death_crumb_t, sum); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static inline void death_crumb_seal(death_crumb_t *c) { c->magic = DEATH_MAGIC; c->sum = death_sum(c); }
static inline bool death_crumb_valid(const death_crumb_t *c) { return c->magic == DEATH_MAGIC && c->sum == death_sum(c); }
static inline void death_crumb_clear(death_crumb_t *c) { memset(c, 0, sizeof *c); strncpy(c->link, "idle", DEATH_LINK_CAP - 1); death_crumb_seal(c); }
static inline void death_crumb_verb(death_crumb_t *c, const char *verb)
{
    strncpy(c->verb, verb ? verb : "", DEATH_VERB_CAP - 1); c->verb[DEATH_VERB_CAP - 1] = 0; death_crumb_seal(c);
}
static inline void death_crumb_link(death_crumb_t *c, const char *word)
{
    strncpy(c->link, word ? word : "", DEATH_LINK_CAP - 1); c->link[DEATH_LINK_CAP - 1] = 0; death_crumb_seal(c);
}
static inline void death_crumb_nap(death_crumb_t *c, int kind)
{
    c->nap = (uint8_t)((kind >= 0 && kind < DEATH_NAP_N) ? kind : DEATH_NAP_NONE); death_crumb_seal(c);
}
static inline void death_crumb_tick(death_crumb_t *c, uint32_t uptime_s) { c->uptime_s = uptime_s; death_crumb_seal(c); }
static inline const char *death_nap_word(int kind) { return (kind >= 0 && kind < DEATH_NAP_N) ? DEATH_NAP_WORDS[kind] : DEATH_NAP_WORDS[0]; }

/* --- the doctor's words -------------------------------------------------------- */

/* the first line: how the body last died, with the crumb when it survived (NULL = none) */
static inline int death_line(char *out, size_t cap, int code, const death_crumb_t *c, uint32_t boots, uint32_t crashes)
{
    int n;
    if (c) n = snprintf(out, cap, "died: %s, %lu s up before it (verb `%s`, nap %s, link %s)",
                        death_word(code), (unsigned long)c->uptime_s, c->verb[0] ? c->verb : "none", death_nap_word(c->nap), c->link);
    else   n = snprintf(out, cap, "died: %s (no breadcrumb)", death_word(code));
    if (n < 0 || (size_t)n >= cap) return n;
    n += snprintf(out + n, cap - (size_t)n, " | boots %lu, crashes %lu", (unsigned long)boots, (unsigned long)crashes);
    return n;
}

/* the verdict's words when a crash is inside the hour; 0 = nothing to say */
static inline int death_verdict(char *out, size_t cap, int code, uint32_t up_s, const death_crumb_t *c)
{
    if (!death_recent(code, up_s)) { if (cap) out[0] = 0; return 0; }
    if (c) return snprintf(out, cap, "died of %s %lu s ago (%lu s up before it, during `%s`, nap %s)",
                           death_word(code), (unsigned long)up_s, (unsigned long)c->uptime_s, c->verb[0] ? c->verb : "no verb", death_nap_word(c->nap));
    return snprintf(out, cap, "died of %s %lu s ago (no breadcrumb)", death_word(code), (unsigned long)up_s);
}

/* --- the day header's reserved bytes ------------------------------------------- */

/* CRC-16/CCITT-FALSE of a string (poly 0x1021, init 0xFFFF): "123456789" -> 0x29B1 */
static inline uint16_t death_crc16(const char *s)
{
    uint16_t crc = 0xFFFF;
    for (; s && *s; s++) {
        crc ^= (uint16_t)((uint8_t)*s) << 8;
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}
enum { DEATH_HDR_BOOTS = 0, DEATH_HDR_CRASHES = 1, DEATH_HDR_CRC_LO = 2, DEATH_HDR_CRC_HI = 3, DEATH_HDR_USED = 4 };
/* one more boot (and crash) on the day's header, saturating; the build's crc overwritten with this build's */
static inline void death_hdr_mark(uint8_t *res, bool boot, bool crash, uint16_t build_crc)
{
    if (boot && res[DEATH_HDR_BOOTS] != 255) res[DEATH_HDR_BOOTS]++;
    if (crash && res[DEATH_HDR_CRASHES] != 255) res[DEATH_HDR_CRASHES]++;
    res[DEATH_HDR_CRC_LO] = (uint8_t)(build_crc & 0xFF);
    res[DEATH_HDR_CRC_HI] = (uint8_t)(build_crc >> 8);
}
