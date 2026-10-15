/* THE WISP (watch-body.md section 1: "the box calls ... the wisp flies
 * in through a small air door with the glyph, hovers until glanced").
 *
 * A 3x3 mote of the pet's shine colour with a 1-px halo. It comes in
 * from the rim at the AIR DOOR -- the point of the glass opposite the
 * floor, on the bowl's angle -- along a slow bow to a post beside the
 * gel and bobs there; a glance (a touch, a wake, the hour showing) is
 * it being SEEN: it holds a moment and fades over a second, rising a
 * little. One wisp per app, three at most (a fourth sends the oldest
 * off); a second notice from an app already hovering swells its halo
 * once.
 *
 * Two doors bring one: the DESK's ask over the link (`wisp <app>
 * [<title>]` -- the desk's own doorbell passed on, link.cpp) and the
 * iPhone over BLE (ancs.cpp; BLE only while the WiFi radio is down, ble.cpp). Built in the default
 * env: no radio of its own.
 *
 * THE HOUSE'S RULES: a wisp is for him, so it needs him HOME -- and it
 * never lights the screen (the calm rule): arriving on a dark glass it
 * WAITS, queued (three at most, the oldest dropped), and flies in when
 * the glass is next lit -- a raise, a touch -- which is not yet its
 * glance: the glance that sends it off must begin AFTER it has landed.
 * The slime leaving takes every hovering wisp with him; what still
 * waited goes to the air door (below).
 *
 * THE WISP'S MANNERS (2026-10-09, the overhaul wave; watch-ideas E41-E43):
 *   THE WISP AT THE DOOR (E43): a notice arriving while the house is
 *   EMPTY is no longer `no house` -- it waits AT THE AIR DOOR as a dim
 *   1-px mote (no halo, no bump: nothing for a creature nobody is home
 *   for) for WISP_DOOR_S; the ARRIVAL (holding turning true, the gel's
 *   rise) flies it in with him as an ordinary landing, its reaction
 *   played then through THE BUMPS' gate; an hour unclaimed, it fades
 *   (counted, main logs it). The queue of three holds the door's and the
 *   dark glass's together. The ledger keeps it either way (it was for
 *   him), so wisp_ledger_keep is the phone's belt-and-braces only.
 *   THE WISP'S INK (E42): the mote and its halo wear the ink of what it
 *   carries -- the theme's SHINE for the desk's doorbell, its SHADE for
 *   the phone, its BODY colour when the box itself speaks (the app id
 *   WISP_BOX_APP, "jarvas": the mind in his own flesh's colour; the
 *   "presence gold" of the item is no theme ink -- wardrobe.cpp's rule:
 *   the Sage's gold is not a theme). The fourth ink of the item is the
 *   plain shine: the default. The caller names the source (ancs.cpp the
 *   phone; the link's ask the desk, or the box by its id).
 *   THE WISP EATEN (E41): a FEED whose end lands within WISP_EAT_PX of a
 *   hovering wisp eats it -- it flies the short way into him (to the
 *   finger's landing, shrinking over WISP_EAT_S), a small lump WISP_LUMP
 *   rides the belly's own term for WISP_LUMP_S (the bead's shape), the
 *   ledger keeps the notice (eaten is read, not lost). A glance still
 *   sends a wisp off as ever.
 *
 * THE NOTICE'S OWN REACTION (2026-10-06; Khalid 09-30: "each
 * notification should get its own reaction"): a wisp carries a
 * notice_react_t (notice.h) -- the haptic kind and its plays for the
 * landing, and the halo's style: the plain ring, the WIDE ring (one px
 * out, the phone said important), or the PULSING ring (a call ringing:
 * it swells and ebbs until the call ends -- wisp_leave -- or he looks).
 * wisp_arrive_react takes it with the notice's message; wisp_arrive is
 * that with the default (one bump, the plain halo) and no message --
 * the desk's ask and every caller from before. The landing's take
 * (wisp_take_landed_react) hands main the STRONGEST reaction that
 * landed since the last take; main plays it through THE BUMPS' gate.
 *
 * No Arduino in here: tools/wisp_test.cpp walks the queue and the
 * glance on the laptop (the draw is pixels over the frame, excluded). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "dirty.h"
#include "notice.h"     /* THE NOTICE'S OWN REACTION: the table's shape, not the table */
#include "gel.h"        /* gel_rgba_t: THE WISP'S INK is one of the theme's (E42) */

#define WISP_MAX       3
#define WISP_APP_MAX   48
#define WISP_TITLE_MAX 32
#define WISP_MSG_MAX   96               /* THE NOTICE OPENED: the message kept in the ledger (five rows of the page's chord) */
#define WISP_NAME_MAX  24               /* the app's display name ("Messages"), looked up after the notice; "" = unknown (the page shows the id's last word) */

enum { WISP_OK = 0, WISP_NO_HOUSE = 1 };    /* WISP_NO_HOUSE: only an app-less ask now (E43: an EMPTY house keeps it at the door) */

/* THE WISP'S INK (E42): who sent it, and so what it wears */
enum { WISP_SRC_DESK = 0, WISP_SRC_PHONE = 1, WISP_SRC_BOX = 2 };
#define WISP_BOX_APP   "jarvas"           /* the box's own voice over the desk's `wisp` ask: `wisp jarvas <title>` (presence.py's speaker name; case folded) */
#define WISP_DOOR_S    3600.0f            /* THE WISP AT THE DOOR (E43): an hour unclaimed at the air door, it fades -- the item's own hour */
#define WISP_EAT_PX    36.0f              /* THE WISP EATEN (E41): the feed's end this close to a hovering wisp eats it -- the posts sit 22 px shy of the feed's base third (gesture.cpp: a feed ends past 160 of 240), plus a fingertip's half-width (~14 px) */
#define WISP_EAT_S     0.35f              /* the eaten wisp's short flight into him, shrinking: a swallow, not a fade (FADE_S 1.0) */
#define WISP_LUMP      0.20f              /* the eaten notice's small lump on the belly's term (the bead's BEAD_LUMP is 0.30, the meal's 1.0: a notice is less than a bead) */
#define WISP_LUMP_S    30.0f              /* ...sinking away over this, the bead's own spell (BEAD_LUMP_S 30), the meal's ease-out */
#define WISP_EAT_EYE_S 1.0f               /* main's happy eyes at the swallow: a second (the feed's FEED_EYE_S is 1.5) */

/* a notice for him: title may be NULL; holding = house_holding() at the
 * caller. WISP_OK = it flew in, waits for the light, or (EMPTY) waits at
 * the air door; WISP_NO_HOUSE = no app id (nothing kept). The source is
 * the desk's, or the box's when the app id is WISP_BOX_APP */
int  wisp_arrive(const char *app, const char *title, bool holding);
/* the same with the notice's own reaction (NULL = the default) and its
 * message (NULL / "" = none; kept in the ledger for THE NOTICE OPENED,
 * cut to WISP_MSG_MAX - 1); the same app already hovering takes the
 * NEWER reaction (a ringing that ends in a missed call stops pulsing) */
int  wisp_arrive_react(const char *app, const char *title, const char *msg, const notice_react_t *react, bool holding);
/* the whole of it with the SOURCE named (WISP_SRC_*): ancs.cpp's door says WISP_SRC_PHONE; the two above resolve it from the app id */
int  wisp_arrive_from(int source, const char *app, const char *title, const char *msg, const notice_react_t *react, bool holding);
int  wisp_source_of(const char *app);      /* WISP_SRC_BOX when the app id is WISP_BOX_APP (case folded), else WISP_SRC_DESK */
const gel_rgba_t *wisp_ink(int source);    /* the theme's shine / shade / body for desk / phone / box (anything else: the shine) */
const char *wisp_source_name(int source);  /* "desk" / "phone" / "box" */
/* THE WISP EATEN (E41): the feed's END point (screen px, main: e.x + e.dx,
 * e.y + e.dy); true = a hovering wisp within WISP_EAT_PX was eaten (the
 * nearest; main then shows the happy eyes and drops the meal's lump) */
bool wisp_feed_at(int x, int y);
/* THE NOTICE CLEARED FROM THE WRIST (T17): that app's hovering wisp eaten -- the same swallow as the feed's, into the
 * point given (main: his painted box's centre; the page is up, the gel unseen under it); true = one was there to eat */
bool wisp_eat_app(const char *app, int x, int y);
float wisp_lump(void);                     /* the eaten notice's small lump, 0..WISP_LUMP, sinking; main's belly term takes the max */
uint32_t wisp_eaten(void);                 /* ever eaten */
void wisp_leave(const char *app);       /* the notice cleared at its source: its wisp goes (ANCS); a pulsing halo stops here -- the call ended */
/* one call a frame, before the draw: now_ms = the wall clock (the
 * ledger's stamps), phi = the bowl's angle (rad, as main.cpp keeps it),
 * holding = house_holding(), lit = the glass is not DARK, glanced = a
 * touch, a wake, the hour showing */
void wisp_tick(uint32_t now_ms, float dt, float phi, bool holding, bool lit, bool glanced);
/* THE LEDGER (THE WATCH PAGE's notices, 2026-09-10 evening): the last
 * WISP_LEDGER notices that arrived for him -- app, title, when -- kept
 * in RAM only (a reboot forgets), newest first; since E43 a notice the
 * house was empty for is kept too (it waits at the door: it is his) */
#define WISP_LEDGER 3
typedef struct {
    char     app[WISP_APP_MAX];
    char     name[WISP_NAME_MAX];   /* the app's display name, "" until wisp_ledger_name says it */
    char     title[WISP_TITLE_MAX];
    char     msg[WISP_MSG_MAX];     /* the notice's message, "" when none came */
    uint32_t at_ms;
} wisp_note_t;
int  wisp_ledger(const wisp_note_t **out);   /* the count; *out = the entries, newest first */
/* the app's display name, learned after the notice (ANCS asks the phone
 * for it separately): every ledger line of that app takes it; "" or
 * NULL clears; an app not in the ledger is nothing */
void wisp_ledger_name(const char *app, const char *name);
/* THE PHONE'S NOTICE IS HIS (2026-10-06 15:19, the first WhatsApp notice felt on the dark wrist while the slime was on the desk):
 * kept on the ledger even when the house is EMPTY -- only the mote needs him home. ancs.cpp calls it on a `no house`
 * (since E43 only an app-less ask is one; the EMPTY path keeps the ledger itself, so this is the phone's belt-and-braces). */
void wisp_ledger_keep(const char *app, const char *title, const char *msg);
void wisp_draw(uint16_t *fb, float phi);   /* the wisps, over the composed frame, before the push */
bool wisp_mark(dirty_rect_t *box);         /* THE MARK: did this frame's wisps differ from last frame's; the box of both */
int  wisp_count(void);                     /* on the glass now (flying, hovering, fading) */
int  wisp_take_landed(void);               /* THE BUMPS (bumps.h): how many wisps LANDED at their post since the last take (the flight in done) -- main bumps once for any number */
/* the same take with the STRONGEST reaction among them (notice_react_rank:
 * none < purr < bump < hop, two plays over one): *haptic_kind = HAPTIC_*
 * or -1 (every landing was the mote alone), *repeats = its plays; the
 * count returned as wisp_take_landed does (one take empties both) */
int  wisp_take_landed_react(int *haptic_kind, int *repeats);
int  wisp_waiting(void);                   /* queued: for the next light, or at the air door (E43) -- the one queue of three */
int  wisp_at_door(void);                   /* ...of which at the air door (the house EMPTY when they came) */
int  wisp_take_door_faded(void);           /* how many faded at the door an hour unclaimed since the last take (main logs it: nothing fails quietly) */
uint32_t wisp_sent(void);                  /* ever flown in */
/* THE WISP WATCHED (THE OPERATOR NIGHT C3; wispeye.h): the NEWEST mote
 * still on its bow in or hovering -- never one fading, being eaten or
 * waiting at the door -- its glass point (the bob aside), false when
 * none; its born number (0 = none: a newer number is a new watch) and
 * whether it is still flying (the gaze follows the flight, the dwell
 * starts at the landing); the halos that swelled (a second notice from
 * an app already here) since the last take: one blink for any number */
bool     wisp_eye_point(int *x, int *y);
uint32_t wisp_eye_seq(bool *flying);
int      wisp_take_swelled(void);
/* THE DESK NEXT DOOR (near.h; C9): the air door's glass point -- the rim
 * opposite the floor at the bowl's angle, where the desk's wisps come in
 * (main's glance toward the door on the rise into AT HAND) */
void     wisp_door_point(float phi, float *x, float *y);
void wisp_status(char *out, int cap);      /* one line for `wisp` / `status` */
