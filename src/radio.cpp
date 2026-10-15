/* THE RADIO BUDGET -- radio.h says what; this is the rule, Arduino-free. */
#include "radio.h"
#include "house.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* --- the tunables, with their cause ------------------------------------------ */
/* the honest claim / beat while he is WITH the watch: ten minutes of lit
 * glass since the last ok (awake time, kept across naps -- a glance is
 * 10-20 s of it, so a glance within the same ten minutes of use costs
 * nothing). Khalid's number to rule on: README "THE RADIO BUDGET". */
const uint32_t LINK_EVERY_S  = 600;
/* the courtesy dial: the box's watch lease lapses at 30 min of silence,
 * so a wrist heard from every half hour of WALL time (naps included)
 * keeps it -- and it is how the honest claim lands within the half hour
 * of walking away from the desk, and how a closed day gets home */
const uint32_t LINK_STALE_S  = 1800;
/* linked and nothing to say (no verb waiting, no day in flight) for
 * this long: hang up, radio off. The session's work -- the claim, the
 * senses, the day, the word, the sky, the time -- takes 2-4 s in
 * lockstep; five more seconds catches the desk's own ask (a wisp) */
const uint32_t LINK_LINGER_S = 5;
/* a due dial that has no link after this long (no network in the air,
 * every door skipped, a handshake that will not close) gives the radio
 * up: the scan every 30 s and the door backoff would otherwise run for
 * the whole lit spell... */
const uint32_t LINK_TRY_S    = 45;
/* ...and it is not tried again for this long (wall; a nap in between
 * ends it early, since the next wake may be on a different network) */
const uint32_t LINK_RETRY_S  = 120;
/* THE LIT GLASS POLLS: the minute was the only cadence (BEAT_MS 60000 in
 * link.cpp until the snappy pass); over a session that is up it is now
 * the dim glass's only. Lit and EMPTY the honest claim goes every 5 s
 * -- one line each way, a few hundred bytes, the box's answer the cost
 * -- so a `slimectl go` is felt on the wrist within the 5 (over the
 * desk's door each claim is one ssh from the desk to the box, ~1 s: the
 * 5 s is that door's floor too); lit and HOLDING the beat every 5 s
 * as well (Khalid, 2026-09-10 evening, timing the flashed build: a
 * `slimectl come` left the wrist gel standing ~57 s, "both places,
 * that's buns" -- a beat over an open session is one line; the cost is
 * nothing), so a desk `come` or his sitting down sinks the wrist gel
 * within the 5 */
const uint32_t LINK_BEAT_S   = 60;
const uint32_t CLAIM_LIT_S   = 5;
const uint32_t BEAT_LIT_S    = 5;

static const char *NAMES[] = { "resting", "the dock", "nothing to dial with", "forced", "the house never told",
                               "something to send", "the claim is due", "the box has heard nothing",
                               "spent: the radio waits for the cable" };

/* BATTERY MANNERS: the cell under 10 % (in->lean) doubles the EVERY
 * clock -- the honest claim every twenty minutes of lit glass instead
 * of ten; the courtesy (STALE) and everything with news to send stay as
 * they are (the box's lease and his night are not the cell's to trade) */
uint32_t radio_every_s(const radio_in_t *in) { return in->lean ? 2u * LINK_EVERY_S : LINK_EVERY_S; }

int radio_reason(const radio_in_t *in)
{
    if (in->docked) return RADIO_DOCKED;
    if (in->spent && !in->cable) return RADIO_SPENT;      /* THE LAST DROP'S MANNERS: no WiFi dial off the cable -- not his hand's, not the day's, not the claim's (the burst's sag browns the body out mid-dial and loses the minute); the cable feeds the rail, so there the rule below stands */
    if (in->forced) return RADIO_FORCED;                 /* his hand (a hold, `link now`) outranks a missing door or key: provisioning joins first */
    if (!in->can) return RADIO_NOWAY;
    if (in->house_state == HOUSE_UNKNOWN) return RADIO_UNKNOWN;
    if (in->pending) return RADIO_PENDING;
    if (!in->have_ok) return RADIO_EVERY;                 /* never linked since boot: the first claim, at once */
    if (in->awake_since_ok_s >= radio_every_s(in)) return RADIO_EVERY;
    if ((uint32_t)(in->now_ms - in->last_ok_ms) >= LINK_STALE_S * 1000u) return RADIO_STALE;
    return RADIO_REST;
}

const char *radio_reason_name(int r)
{
    return (r >= 0 && r <= RADIO_SPENT) ? NAMES[r] : "?";
}

uint32_t radio_next_due_s(const radio_in_t *in)
{
    if (!in->have_ok) return 0;
    uint32_t every_s = radio_every_s(in);
    uint32_t every = in->awake_since_ok_s >= every_s ? 0 : every_s - in->awake_since_ok_s;
    uint32_t age_s = (uint32_t)(in->now_ms - in->last_ok_ms) / 1000u;
    uint32_t stale = age_s >= LINK_STALE_S ? 0 : LINK_STALE_S - age_s;
    return every < stale ? every : stale;
}

bool radio_wanted(int r)
{
    return r != RADIO_REST && r != RADIO_DOCKED && r != RADIO_SPENT;
}

uint32_t radio_lease_every_s(bool lit, int house_state)
{
    if (!lit) return LINK_BEAT_S;
    return house_state == HOUSE_HOLDING ? BEAT_LIT_S : CLAIM_LIT_S;   /* EMPTY and UNKNOWN alike: the claim decides */
}

bool radio_hold_session(bool lit, bool heap_low, bool verb)
{
    return lit && !heap_low && verb;
}

/* --- THE CABLE NEVER CLAIMS (radio.h says why) ------------------------------------ */

bool radio_may_claim(bool cable, int house_state, bool worn)
{
    if (house_state == HOUSE_UNKNOWN) return true;   /* the one first question, cable or shelf */
    if (cable) return false;                     /* EMPTY and HOLDING on the cable never claim on their own */
    return worn;                                 /* THE SHELF NEVER CLAIMS: still (no motion 5 min) = no claim -- the box's rule would let a `claim still` land on an empty lease */
}

int radio_lease_verb(bool cable, int house_state, bool worn)
{
    if (house_state == HOUSE_HOLDING) return LEASE_BEAT;      /* a held lease stays honest: the beat, cable or not, worn or still */
    return radio_may_claim(cable, house_state, worn) ? LEASE_CLAIM : LEASE_NONE;
}

/* --- THE LAN KNOCK's line ------------------------------------------------------- */

bool radio_knock_parse(const char *body, knock_t *out)
{
    memset(out, 0, sizeof *out);
    if (!body) return false;
    while (*body == ' ' || *body == '\t' || *body == '\r' || *body == '\n') body++;
    const char *e = body;
    while (*e && *e != ' ' && *e != '\r' && *e != '\n') e++;
    size_t n = (size_t)(e - body);
    if (n != 4 || (strncmp(body, "lost", 4) && strncmp(body, "free", 4))) return false;
    memcpy(out->what, body, 4); out->what[4] = 0;
    while (*e == ' ') e++;
    if (!*e || *e == '\r' || *e == '\n') return true;              /* bare: unsigned */
    /* `<stamp> <mac>`: the stamp all digits, the mac 32 hex */
    char *end = NULL;
    unsigned long st = strtoul(e, &end, 10);
    if (end == e || *end != ' ') return false;
    e = end;
    while (*e == ' ') e++;
    int k = 0;
    while (k < KNOCK_MAC_HEX && isxdigit((unsigned char)e[k])) { out->mac[k] = (char)tolower((unsigned char)e[k]); k++; }
    if (k != KNOCK_MAC_HEX) return false;
    const char *tail = e + k;
    while (*tail == ' ' || *tail == '\r' || *tail == '\n') tail++;
    if (*tail) return false;
    out->mac[k] = 0;
    out->stamp = (uint32_t)st;
    out->signed_ = true;
    return true;
}

bool radio_knock_fresh(uint32_t stamp, uint32_t now_min)
{
    uint32_t d = stamp > now_min ? stamp - now_min : now_min - stamp;
    return d <= KNOCK_STALE_MIN;
}

int radio_knock_judge(const knock_t *k, bool have_key, const char *mac_ours,
                      bool clock_set, uint32_t now_min, char *why, int cap)
{
    if (!have_key) { snprintf(why, cap, "unsigned"); return 0; }         /* a bare board: a bare knock is taken, said as such */
    if (!k->signed_ || !mac_ours || strcmp(mac_ours, k->mac)) { snprintf(why, cap, "no key"); return 1; }
    if (clock_set && !radio_knock_fresh(k->stamp, now_min)) { snprintf(why, cap, "stale"); return 1; }
    snprintf(why, cap, "ok");
    return 0;
}
