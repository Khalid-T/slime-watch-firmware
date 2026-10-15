/* THE NOTICE'S OWN REACTION -- notice.h says why; this is the table.
 *
 * The creature's touch vocabulary is three gestures (haptic.h: a bump,
 * a purr, a triple hop) and a count, so twelve categories cannot each
 * have a stranger of their own; what they have is a SHAPE each, picked
 * by what the notice IS to him:
 *
 *   a call, ringing       the triple hop (the phone is RINGING: the
 *                         liveliest thing the motor does), the halo
 *                         pulsing until the call ends or he looks
 *   a missed call,        two bumps (the knock's "you were wanted and
 *   a voicemail           were not there"), the plain halo
 *   social, email,        one bump -- a person, a letter, a bill, a
 *   business, location,   place, a thing: the one touch every wisp
 *   other                 had until today
 *   schedule              the triple hop, once (a reminder is the
 *                         alarm's kin: clockwork's alarm hops too)
 *   health                the purr (the body's own good news -- a ring
 *                         closed, a stand -- is a contentment, not a
 *                         knock)
 *   news, entertainment   NOTHING felt: the mote alone. A headline, a
 *                         show, a score is nobody's call on his wrist
 *                         (ruling: a touch from the creature is for
 *                         HIM; the world's noise flies in silently)
 *
 * Then the flags, in their weight: PRE-EXISTING silences and flattens
 * (a replay is old news); SILENT silences; IMPORTANT widens the halo
 * unless it pulses. */
#include "notice.h"
#include <string.h>

typedef struct { int haptic, repeats, halo; } row_t;
static const row_t TABLE[NOTICE_CAT_N] = {
    /* other         */ { HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL },
    /* call          */ { HAPTIC_HOP3, 1, NOTICE_HALO_PULSING },
    /* missed call   */ { HAPTIC_BUMP, 2, NOTICE_HALO_NORMAL },
    /* voicemail     */ { HAPTIC_BUMP, 2, NOTICE_HALO_NORMAL },
    /* social        */ { HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL },
    /* schedule      */ { HAPTIC_HOP3, 1, NOTICE_HALO_NORMAL },
    /* email         */ { HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL },
    /* news          */ { -1,          0, NOTICE_HALO_NORMAL },
    /* health        */ { HAPTIC_PURR, 1, NOTICE_HALO_NORMAL },
    /* business      */ { HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL },
    /* location      */ { HAPTIC_BUMP, 1, NOTICE_HALO_NORMAL },
    /* entertainment */ { -1,          0, NOTICE_HALO_NORMAL },
};
static const char *const CAT_NAMES[NOTICE_CAT_N] = { "other", "call", "missed call", "voicemail", "social", "schedule",
                                                     "email", "news", "health", "business", "location", "entertainment" };

void notice_react_default(notice_react_t *out)
{
    if (!out) return;
    out->haptic = HAPTIC_BUMP; out->repeats = 1; out->halo = NOTICE_HALO_NORMAL;
}

void notice_react(int category, int flags, notice_react_t *out)
{
    if (!out) return;
    if (category < 0 || category >= NOTICE_CAT_N) { notice_react_default(out); }
    else { out->haptic = TABLE[category].haptic; out->repeats = TABLE[category].repeats; out->halo = TABLE[category].halo; }
    /* the flags, the weightiest last so it has the final word */
    if ((flags & NOTICE_FLAG_IMPORTANT) && out->halo == NOTICE_HALO_NORMAL) out->halo = NOTICE_HALO_WIDE;          /* wider -- unless it already pulses */
    if (flags & NOTICE_FLAG_SILENT) { out->haptic = -1; out->repeats = 0; }                                       /* the phone made no sound of it: neither do we; the halo keeps its shape (a silent ring still rings on the glass) */
    if (flags & NOTICE_FLAG_PREEXISTING) { out->haptic = -1; out->repeats = 0; out->halo = NOTICE_HALO_NORMAL; }   /* old news: nothing felt, nothing pulses, nothing wide */
    if (out->haptic < 0) out->repeats = 0;
}

int notice_react_rank(const notice_react_t *r)
{
    if (!r || r->haptic < 0 || r->repeats <= 0) return 0;
    int base = r->haptic == HAPTIC_PURR ? 1 : r->haptic == HAPTIC_BUMP ? 2 : r->haptic == HAPTIC_HOP3 ? 3 : 0;
    if (!base) return 0;
    return base * 2 + (r->repeats > 1 ? 1 : 0);
}

const char *notice_cat_name(int category)
{
    if (category == NOTICE_CAT_NONE) return "none";
    return (category >= 0 && category < NOTICE_CAT_N) ? CAT_NAMES[category] : "?";
}

const char *notice_halo_name(int halo)
{
    switch (halo) {
    case NOTICE_HALO_NORMAL:  return "normal";
    case NOTICE_HALO_WIDE:    return "wide";
    case NOTICE_HALO_PULSING: return "pulsing";
    default:                  return "?";
    }
}
