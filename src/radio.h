/* THE RADIO BUDGET (2026-09-10): when the radio may come up at all.
 *
 * Until tonight every wake -- a raise for the hour, a bump that lit the
 * glass -- turned the radio on, joined WiFi (1-2 s), dialled the box
 * over TLS (4-5 s of handshake, ~55 KB of PSRAM) and held the session
 * until the dark house dropped it 20 s later: a glance cost a full
 * connection. The radio and the handshake are the biggest draws after
 * the screen (slime-watch-hardware-notes); on the 800 mAh cell that is
 * the difference between two days and half a day.
 *
 * So the link dials only for a REASON, and this file is the reason,
 * pure (no Arduino; tools/radio_test.cpp walks it on the laptop):
 *
 *   DOCKED   the desk is on the serial port: the link is the cable,
 *            always up, no radio needed -- never a dial
 *   FORCED   `link now` over serial (a test aid), or a hold (`link
 *            hold`, `wifi-join`): his hand, over a missing door or key
 *            (provisioning joins first) -- but never without a network
 *   NOWAY    nothing to dial with (no network, no door, or no key
 *            remembered): the radio stays off, and THE HOUSE's offline
 *            clock runs (a watch with no desk is its own home)
 *   UNKNOWN  THE HOUSE was never told (a first boot): dial at once
 *   PENDING  something to send: a whistle (come / release), a sense
 *            worth the radio (the night's, a gesture's), a closed day
 *            to sync. A word / a wisp to FETCH is not a reason: it
 *            rides on the next dial.
 *   EVERY    the last successful link is older than LINK_EVERY_S of
 *            AWAKE time (600 s; kept across naps, the house's own clock
 *            shape): the honest claim / beat cadence while he is with
 *            the watch -- ten minutes of lit glass, one claim
 *   STALE    the box has been told nothing for LINK_STALE_S of WALL time
 *            (1800 s, naps included): the box's watch lease lapses at
 *            30 min, so a wrist heard from every half hour keeps it,
 *            lands the honest claim when he walked away from the desk,
 *            and carries the day home -- the courtesy dial
 *   REST     none of those: the glass lights, the hour shows, the gel is
 *            there, no WiFi. Resting by choice is NOT offline: THE
 *            HOUSE's offline-worn clock counts only when a dial was due
 *            (any reason above but DOCKED and REST) and no link is up.
 *
 * Every number is a tunable with its cause; the day's cost and the open
 * ruling on LINK_EVERY_S are in the README's "THE RADIO BUDGET".
 *
 * THE LIT GLASS POLLS (2026-09-10, the snappy pass -- Khalid: "the slime
 * isn't snappy enough ... between the watch and the screen"): the rule
 * above says WHEN a session comes up; this second, smaller rule says
 * how a session that IS up is used while he is looking. Lit (the glass
 * awake, not dim) the session is HELD -- no linger, no `bye` -- and the
 * lease verb goes every few seconds: the honest `claim` every
 * CLAIM_LIT_S while the house is EMPTY (a `slimectl go` on the desk
 * lands within it), the `beat` every BEAT_LIT_S while HOLDING (a desk
 * `come` sinks the wrist gel within it). The moment the glass dims the
 * linger applies as before and the session ends; the dark house drops
 * it as ever. It adds no dial: a session held is a session that was
 * already up for a reason. The heap floor is kept: a TLS session that
 * left under HEAP_FLOOR is not held past the linger.
 *
 * THE CABLE NEVER CLAIMS (2026-09-10 evening; the log 20:25-20:29: the
 * watch on the USB dock beside the laptop, the glass lit, the house
 * EMPTY -- a `claim` every 5 s over the held session, each one an exec
 * on the box, and when the desk's lease lapsed the docked, unworn
 * watch took the slime away from the laptop he was typing at: the
 * "both places" tug-of-war). The narrow, certain half of the grasp
 * doc's ruling 50 (the wrist claims only while WORN -- still his to
 * give): ON THE CABLE (the dock / charging, the same evidence THE
 * SHELF RULE reads: main's charging(), VBAT at the charger's float; or
 * the desk on the serial port) THE WATCH NEVER SENDS THE HONEST
 * `claim` -- not the link-up's, not the cadence's, not the re-claim a
 * beat's `no lapsed` or a knock's `lost` asks for. It still beats while
 * HOLDING (a held lease stays honest; the desk's force and a lapse let
 * it go), releases, answers knocks, syncs the day; the 1-s hold and
 * the three knocks (`come`), the desk's `free` knock and the rising's
 * claim still go -- his hand, or the desk's, outranks the rule -- and
 * the house never told (UNKNOWN) asks its one first question (a first
 * boot at the console is on the cable; unanswered, "the house never
 * told" would dial in a loop). With no claim to poll, a lit session on
 * the cable while EMPTY is not held: the linger's `bye` as for the dim
 * glass. Off the cable, worn, the honest claim resumes on its ordinary
 * schedule (the next cadence over a session, else the next dial). The
 * predicate is radio_may_claim(); radio_lease_verb() is the cadence's
 * choice under it; link.cpp logs each edge once. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum {
    RADIO_REST = 0,
    RADIO_DOCKED,
    RADIO_NOWAY,
    RADIO_FORCED,
    RADIO_UNKNOWN,
    RADIO_PENDING,
    RADIO_EVERY,
    RADIO_STALE,
    RADIO_SPENT     /* THE LAST DROP'S MANNERS (spent.h): under the spent line, off the cable -- no WiFi dial at all */
};

/* the tunables (link.cpp reads them for its logs and status lines) */
extern const uint32_t LINK_EVERY_S;     /* awake seconds since the last ok before the next claim / beat is due */
extern const uint32_t LINK_STALE_S;     /* wall seconds since the last ok before the courtesy dial */
extern const uint32_t LINK_LINGER_S;    /* linked with nothing to say for this long: hang up, radio off */
extern const uint32_t LINK_TRY_S;       /* a due dial that has found no link in this long gives the radio up... */
extern const uint32_t LINK_RETRY_S;     /* ...and is not tried again for this long (wall) */
extern const uint32_t LINK_BEAT_S;      /* the minute: the beat / the honest claim over a session with the glass dim (the dock, a hold) */
extern const uint32_t CLAIM_LIT_S;      /* THE LIT GLASS POLLS: the honest claim every this while lit and EMPTY (or never told) */
extern const uint32_t BEAT_LIT_S;       /* ...the beat every this while lit and HOLDING (5 too: a line over an open session costs nothing) */

typedef struct {
    uint32_t now_ms;        /* the wall clock (millis) */
    bool     have_ok;       /* a link has succeeded since boot */
    uint32_t last_ok_ms;    /* when (wall), if have_ok */
    uint32_t awake_since_ok_s;  /* awake seconds since the last ok (all of the uptime when never) */
    bool     pending;       /* something to send */
    int      house_state;   /* HOUSE_UNKNOWN / HOLDING / EMPTY (house.h) */
    bool     docked;        /* the desk is on the serial port */
    bool     can;           /* a network, a door and a key are remembered */
    bool     forced;        /* `link now`, or a hold -- never without a network (link.cpp clears it: there is nothing to bring up) */
    bool     lean;          /* BATTERY MANNERS (vigor_lean): the cell under 10 % -- LINK_EVERY_S doubles (the claim every 20 min of lit glass); STALE and the rest as they are */
    bool     spent;         /* THE LAST DROP'S MANNERS (spent.h, 2026-10-12): under the spent line (DROP_MV 3550, or vigor's spent band) -- a WiFi burst would brown the body out mid-dial */
    bool     cable;         /* ... charging(): on the cable the rail is fed, the spent line refuses nothing */
} radio_in_t;

/* the reason the radio should be up now -- RADIO_REST = none */
int radio_reason(const radio_in_t *in);
uint32_t radio_every_s(const radio_in_t *in);   /* the EVERY clock in force: LINK_EVERY_S, doubled while lean */
const char *radio_reason_name(int r);   /* "resting", "the dock", ... */
/* seconds until EVERY or STALE would fall due (0 = now); a resting
 * line's "next due in N min" -- awake seconds for EVERY, so it is the
 * sooner of the two by its own clock */
uint32_t radio_next_due_s(const radio_in_t *in);
/* true for the reasons that mean "a link is wanted": the house's
 * offline clock runs while one of these stands and no link is up
 * (RADIO_SPENT is not wanted: a body that may not dial is not offline,
 * it waits for the cable) */
bool radio_wanted(int r);

/* THE LIT GLASS POLLS: the lease verb's cadence over an OPEN session, in
 * seconds -- CLAIM_LIT_S lit and EMPTY / UNKNOWN, BEAT_LIT_S lit and
 * HOLDING, LINK_BEAT_S with the glass dim (lit = awake, not dim) */
uint32_t radio_lease_every_s(bool lit, int house_state);
/* true = keep the session open (no linger, no `bye`): lit, the heap
 * not under the floor (heap_low = a TLS session that left less than
 * HEAP_FLOOR free), and a verb to poll with (verb = radio_lease_verb()
 * is not LEASE_NONE: on the cable while EMPTY there is nothing to poll,
 * so nothing to hold the session for -- THE CABLE NEVER CLAIMS) */
bool radio_hold_session(bool lit, bool heap_low, bool verb);

/* THE CABLE NEVER CLAIMS, and THE SHELF NEVER CLAIMS (2026-09-17, the
 * other half of the grasp doc's 0.4 / ruling 50 -- "the wrist claims
 * only while WORN": the box arbitrates on the worn word since
 * SAGE-WEARABLE-CLAW-REPLY section 1, so a `claim worn` from the arm
 * takes him from a desk whose beat is older than 60 s WITHOUT the
 * whistle, and a wrist on the shelf must therefore never claim at all
 * -- a `claim still` that found no holder would take him to a shelf):
 * may the wrist send the honest `claim` now -- false on the cable, and
 * false when NOT WORN (no motion on the arm within house.cpp's
 * WORN_WINDOW_MIN), unless the house was never told (UNKNOWN asks its
 * one first question, cable or shelf: unanswered it would dial in a
 * loop). Only the honest claim: `come`, `release`, the `beat` and the
 * knocks are not asked here */
bool radio_may_claim(bool cable, int house_state, bool worn);
/* the cadence's verb under that rule: the `beat` while HOLDING (cable
 * or not, worn or still), the honest `claim` while EMPTY / UNKNOWN when
 * it may go, else nothing (the session has nothing to poll with) */
enum { LEASE_NONE = 0, LEASE_BEAT, LEASE_CLAIM };
int  radio_lease_verb(bool cable, int house_state, bool worn);

/* THE LAN KNOCK (the snappy pass, item 3): the desk's `POST /lease` to
 * the wrist's own HTTP page, body `<what> [<stamp> <mac>]` -- what =
 * `lost` (the desk has him: the wrist sinks on its own belief, the next
 * claim confirms) or `free` (the desk let him go: the wrist claims at
 * once). The signature is the hello's primitive: mac = HMAC-SHA256(the
 * watch key, "<stamp> lease <what>")[:32] with stamp = the UTC minute
 * (epoch / 60); link.cpp checks it, this only reads the line. A knock
 * never changes the lease by itself -- it makes the wrist ASK the box. */
#define KNOCK_MAC_HEX 32
#define KNOCK_STALE_MIN 1           /* a stamp this many minutes off the wrist's clock is stale */
typedef struct {
    char     what[8];               /* "lost" / "free" */
    bool     signed_;               /* a stamp and a mac were given */
    uint32_t stamp;                 /* the UTC minute */
    char     mac[KNOCK_MAC_HEX + 1];
} knock_t;
bool radio_knock_parse(const char *body, knock_t *out);      /* false = not a knock (bad word, bad shape) */
bool radio_knock_fresh(uint32_t stamp, uint32_t now_min);    /* |stamp - now| <= KNOCK_STALE_MIN */
/* THE KNOCK (LAN or the dock), 2026-09-10 night: the verdict, pure --
 * the same for `POST /lease` and the dock's `knock ...` line (link_knock
 * is the one caller; net.cpp and the serial ask both go through it).
 * link.cpp computes mac_ours = HMAC(key, "<stamp> lease <what>") when it
 * has a key and the knock is signed (NULL otherwise) and hands it in
 * with whether a key is remembered and what the clock says. 0 = taken
 * (why: "ok", or "unsigned" for a bare knock on a bare board), 1 =
 * refused (why: "no key" -- unsigned or mis-signed with a key here;
 * "stale" -- the stamp more than KNOCK_STALE_MIN off a set clock). */
int  radio_knock_judge(const knock_t *k, bool have_key, const char *mac_ours,
                       bool clock_set, uint32_t now_min, char *why, int cap);
