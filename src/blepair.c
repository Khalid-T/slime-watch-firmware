/* THE PAIRING WINDOW -- blepair.h says why; this is the rule, Arduino-free. */
#include "blepair.h"

void blepair_init(blepair_t *p) { p->open_ms = 0; p->secs = 0; p->open = false; }

void blepair_open(blepair_t *p, uint32_t now_ms, uint32_t secs)
{
    p->open_ms = now_ms;
    p->secs = secs ? secs : BLEPAIR_S;
    p->open = true;
}

void blepair_close(blepair_t *p) { p->open = false; }

bool blepair_is_open(const blepair_t *p, uint32_t now_ms)
{
    if (!p->open) return false;
    return (uint32_t)(now_ms - p->open_ms) < p->secs * 1000u;
}

uint32_t blepair_left_s(const blepair_t *p, uint32_t now_ms)
{
    if (!blepair_is_open(p, now_ms)) return 0;
    uint32_t gone = (uint32_t)(now_ms - p->open_ms);
    uint32_t left_ms = p->secs * 1000u - gone;
    uint32_t s = (left_ms + 999u) / 1000u;        /* rounded up: the row counts 60, 59 .. 1, never 0 while open */
    return s ? s : 1;
}

int blepair_admit(const blepair_t *p, uint32_t now_ms, bool bonded)
{
    if (bonded) return BLEPAIR_BONDED;
    if (blepair_is_open(p, now_ms)) return BLEPAIR_WINDOW;
    return BLEPAIR_REFUSED;
}

const char *blepair_why(int verdict)
{
    switch (verdict) {
    case BLEPAIR_BONDED:  return "bonded";
    case BLEPAIR_WINDOW:  return "a stranger inside the pairing window";
    case BLEPAIR_REFUSED: return "a stranger, no pairing window (the row to PAIR, or `ble pair`)";
    default: return "?";
    }
}
