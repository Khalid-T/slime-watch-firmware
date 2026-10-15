/* THE AIR DOOR's line gathering -- bleline.h says why; Arduino-free. */
#include "bleline.h"
#include <string.h>

void bleline_init(bleline_t *l) { memset(l, 0, sizeof *l); }
void bleline_reset(bleline_t *l) { l->n = 0; l->overlong = false; }

int bleline_feed(bleline_t *l, const uint8_t *d, int n, bleline_fn fn, void *arg)
{
    int out = 0;
    for (int i = 0; i < n; i++) {
        char c = (char)d[i];
        if (c == '\n') {
            if (l->overlong) { l->overlong = false; l->n = 0; continue; }   /* the dropped line's end: gathering resumes */
            l->buf[l->n] = 0;
            if (l->n) { l->lines++; out++; if (fn) fn(l->buf, arg); }      /* a bare newline is nothing */
            l->n = 0;
            continue;
        }
        if (c == '\r' || l->overlong) continue;
        if (l->n >= BLELINE_MAX) { l->overlong = true; l->dropped++; l->n = 0; continue; }
        l->buf[l->n++] = c;
    }
    return out;
}
