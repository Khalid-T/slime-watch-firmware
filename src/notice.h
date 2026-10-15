/* THE NOTICE'S OWN REACTION (Khalid, 2026-09-30: "the same canned line
 * per notification sucks -- each notification should get its own
 * reaction"; memory/notification-reactions-2026-09-30.md). Until today
 * every notice that landed as a wisp was ONE BUMP, the mail and the
 * ringing phone alike. Now the phone's word about a notice -- its
 * CATEGORY and its FLAGS, the two bytes ANCS puts first in every
 * notification (ancs.cpp) -- picks the reaction from ONE TABLE:
 *
 *   the haptic  the creature's gesture when the mote lands (haptic.h's
 *               kinds: a bump, a purr, a triple hop -- never a buzz),
 *               or -1 = none (the mote alone: it flew in, nothing felt)
 *   the plays   how many times it plays: 1, or 2 for a thing he missed
 *               (a missed call, a voicemail: two bumps ~250 ms apart,
 *               the knock's "you were wanted"); 0 with no haptic
 *   the halo    how the wisp wears its halo: NORMAL (the 1-px ring),
 *               WIDE (one px wider: the phone said IMPORTANT), or
 *               PULSING (a call RINGING: the halo swells and ebbs until
 *               the call ends -- wisp_leave -- or the glance)
 *
 * The flags rule over the category: SILENT (the phone made no sound of
 * it) = no haptic, whatever the kind; PRE-EXISTING (a replay of what
 * was already in the Center when the phone connected) = no haptic and
 * a plain halo -- old news is not a knock; IMPORTANT widens the halo
 * unless it already pulses (a ringing call is the strongest thing on
 * the glass; wider than pulsing there is not). The desk's `wisp` ask
 * carries no category (NOTICE_CAT_NONE): the default, one bump, the
 * plain halo -- what every wisp was until today. A category the table
 * does not know is the default too.
 *
 * ONE PURE TABLE, C, no Arduino: tools/notice_test.cpp pins the whole
 * of it (every category, every flag, their order). THE BUMPS' gate
 * (bumps.h) still says whether anything plays AT ALL -- the dark
 * glass, the quiet hours, the sleep, the cable, his switch; this table
 * only says WHAT, once the gate opens. */
#pragma once
#include "haptic.h"     /* HAPTIC_BUMP / HAPTIC_PURR / HAPTIC_HOP3: the kinds, not the driver */

#ifdef __cplusplus
extern "C" {
#endif

/* ANCS's CategoryID, as the phone numbers them (Apple's table; ancs.cpp passes the byte through) */
enum { NOTICE_CAT_OTHER = 0, NOTICE_CAT_CALL = 1, NOTICE_CAT_MISSED_CALL = 2, NOTICE_CAT_VOICEMAIL = 3, NOTICE_CAT_SOCIAL = 4,
       NOTICE_CAT_SCHEDULE = 5, NOTICE_CAT_EMAIL = 6, NOTICE_CAT_NEWS = 7, NOTICE_CAT_HEALTH = 8, NOTICE_CAT_BUSINESS = 9,
       NOTICE_CAT_LOCATION = 10, NOTICE_CAT_ENTERTAINMENT = 11, NOTICE_CAT_N = 12 };
#define NOTICE_CAT_NONE (-1)            /* no category: the desk's `wisp` ask (link.cpp) -- the default reaction */
/* ANCS's EventFlags (the same bits ancs.cpp reads) */
enum { NOTICE_FLAG_SILENT = 1, NOTICE_FLAG_IMPORTANT = 2, NOTICE_FLAG_PREEXISTING = 4 };
/* the halo's style */
enum { NOTICE_HALO_NORMAL = 0, NOTICE_HALO_WIDE = 1, NOTICE_HALO_PULSING = 2 };

typedef struct {
    int haptic;         /* HAPTIC_* to play when the mote lands, -1 = none */
    int repeats;        /* the plays: 1 = once, 2 = twice (~250 ms apart), 0 = none (haptic -1) */
    int halo;           /* NOTICE_HALO_* */
} notice_react_t;

/* the table: category (NOTICE_CAT_*, or NOTICE_CAT_NONE / unknown = the default) + flags -> the reaction */
void notice_react(int category, int flags, notice_react_t *out);
/* the default (one bump, once, the plain halo): what the desk's wisp gets, and what every wisp was before the table */
void notice_react_default(notice_react_t *out);
/* which of two reactions is the stronger (the landed take keeps the strongest of a frame): none < purr < bump < hop,
 * two plays over one of the same kind; returns the rank (0 = none), for the compare */
int  notice_react_rank(const notice_react_t *r);
const char *notice_cat_name(int category);     /* "call", "missed call", ... ; "none" for NOTICE_CAT_NONE; "?" past the table */
const char *notice_halo_name(int halo);        /* "normal" / "wide" / "pulsing"; "?" */

#ifdef __cplusplus
}
#endif
