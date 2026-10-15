/* THE DOCK HANDS HIM OVER (design/watch-ideas.md C24, 2026-10-09; ONE
 * BODY ACROSS THE HOUSES): "Docking while HOLDING is an honest release:
 * the cable is the desk's hand, and a watch on the charger is not on
 * the wrist; the desk claims him at once and the plug is the wrist's
 * whole face. Undocking worn claims him back on the next dial."
 *
 * Until today THE CABLE NEVER CLAIMS (radio.h) kept a docked wrist from
 * TAKING him, but a wrist that HELD him when the cable went in kept
 * him -- the slime on a watch lying on the desk beside the laptop he
 * was typing at, the beat keeping the lease honest and the desk body
 * dormant. This is the other half: the cable's SETTLED rising edge
 * (cable.h: 10 s past CABLE_ON_MV, or the desk on the serial port)
 * while HOLDING is the release -- `release docked` by the hold's own
 * road (the goodbye, the sink, the wrist's word first: house_release_now)
 * -- and the cable's falling edge, if the wrist is WORN within the
 * minute (motion: the day log's WORN flag is a minute wide), is the
 * honest claim on the next dial (the rising's shape: a reason the
 * radio comes up for, as the hold's `come` is). An undock onto the
 * shelf claims nothing: THE SHELF NEVER CLAIMS, and the ordinary claim
 * lands when he puts it on.
 *
 * ONE PURE DECISION, C, no Arduino: edges in, a verb out, nothing
 * else. The handover is remembered (`handed`) so only an undock that
 * UNDOES THIS release claims back -- a house emptied by the desk's
 * own `lost` knock while docked is the desk's doing, and the undock
 * leaves it to the ordinary rules. Behind his switch (store.dockhand_off,
 * NVS `dockoff`, default ON: `dock hand on|off` over serial); off, the
 * dock is a charger and nothing here speaks. tools/dockhand_test.cpp
 * pins the table; link.cpp applies it at its cable edge. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the undock's worn test: motion within this of the cable's falling
 * edge -- the day log's WORN flag is a minute wide, and a wrist picked
 * up off the charger and put on moves inside it */
#define DOCKHAND_WORN_MS 60000u

enum { DOCKHAND_NONE = 0, DOCKHAND_RELEASE = 1, DOCKHAND_CLAIM = 2 };

typedef struct {
    bool cable_last;        /* the cable as last seen (the edge is the difference) */
    bool handed;            /* this module released him at the last dock: the undock may claim back */
} dockhand_t;

typedef struct {
    bool     on;            /* his switch (!store.dockhand_off) */
    bool     cable;         /* on the cable now (link.cpp's on_cable: charging() or the serial dock), settled */
    bool     holding;       /* house_holding() */
    uint32_t still_ms;      /* since the body last moved (house_still_ms) */
} dockhand_in_t;

void dockhand_begin(dockhand_t *s, bool cable);     /* the boot's state is no edge */
/* one call a tick: DOCKHAND_RELEASE on the cable's rising edge while
 * holding (and on), DOCKHAND_CLAIM on its falling edge after such a
 * release while worn (and on), else NONE -- ``why`` says in words what
 * an edge did or why it was kept back ("" when there was no edge) */
int  dockhand_tick(dockhand_t *s, const dockhand_in_t *in, char *why, int cap);
const char *dockhand_name(int d);                   /* "nothing" / "the dock's release" / "the undock's claim" */

#ifdef __cplusplus
}
#endif
