/* THE DOCK HANDS HIM OVER -- dockhand.h says what; this is how. */
#include "dockhand.h"
#include <stdio.h>

static const char *const NAMES[] = { "nothing", "the dock's release", "the undock's claim" };

void dockhand_begin(dockhand_t *s, bool cable)
{
    s->cable_last = cable;
    s->handed = false;
}

static void put(char *why, int cap, const char *s)
{
    if (why && cap > 0) snprintf(why, cap, "%s", s);
}

int dockhand_tick(dockhand_t *s, const dockhand_in_t *in, char *why, int cap)
{
    put(why, cap, "");
    if (in->cable == s->cable_last) return DOCKHAND_NONE;      /* no edge: nothing to decide */
    s->cable_last = in->cable;
    if (in->cable) {
        /* the cable went in */
        if (!in->on) { put(why, cap, "docked: the dock hand is off -- the cable is a charger, he stays"); return DOCKHAND_NONE; }
        if (!in->holding) { put(why, cap, "docked: the house is not holding -- nothing to hand over"); return DOCKHAND_NONE; }
        s->handed = true;
        put(why, cap, "docked while holding: the cable is the desk's hand -- `release docked`");
        return DOCKHAND_RELEASE;
    }
    /* the cable came out */
    if (!s->handed) { put(why, cap, "undocked: nothing was handed over at the dock -- the ordinary claim"); return DOCKHAND_NONE; }
    s->handed = false;
    if (!in->on) { put(why, cap, "undocked: the dock hand is off -- the ordinary claim"); return DOCKHAND_NONE; }
    if (in->still_ms >= DOCKHAND_WORN_MS) { put(why, cap, "undocked still (the shelf): the claim waits until it is worn"); return DOCKHAND_NONE; }
    put(why, cap, "undocked worn: the claim back on the next dial");
    return DOCKHAND_CLAIM;
}

const char *dockhand_name(int d)
{
    return d >= 0 && d <= 2 ? NAMES[d] : "?";
}
