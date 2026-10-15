/* THE STILL SPOT: the arbiter (still.h). Pure C: no Arduino, no clock. */
#include "still.h"
#include <stdio.h>
#include <string.h>

void still_begin(still_t *s, float read_s, float wait_s)
{
    memset(s, 0, sizeof *s);
    s->state = STILL_WAIT;
    s->read_s = read_s;
    s->wait_s = wait_s;
    s->budget_s = wait_s + 2.0f * read_s;
}

void still_end(still_t *s) { s->state = STILL_IDLE; }
bool still_waiting(const still_t *s) { return s->state == STILL_WAIT; }
bool still_reading(const still_t *s) { return s->state == STILL_READ; }
const char *still_why(const still_t *s) { return s->state == STILL_DONE ? s->why : ""; }

static int give_up(still_t *s, const char *why)
{
    s->state = STILL_DONE;
    snprintf(s->why, sizeof s->why, "%s", why);
    return STILL_GAVE_UP;
}

int still_tick(still_t *s, float dt, float jerk_g)
{
    if (s->state == STILL_IDLE || s->state == STILL_DONE) return STILL_NONE;
    if (dt < 0) dt = 0;
    s->t += dt;
    if (s->state == STILL_WAIT) {
        if (jerk_g < STILL_JERK_G) s->quiet_s += dt; else s->quiet_s = 0;
        if (s->quiet_s >= STILL_ARM_S) {
            s->state = STILL_READ;
            s->lit_s = 0;
            return STILL_LIGHT;
        }
        /* the first wait's deadline is the wait; after a restart the read must still fit the budget */
        float deadline = s->restarts ? s->budget_s - s->read_s : s->wait_s;
        if (s->t >= deadline) {
            char b[56];
            if (s->restarts) snprintf(b, sizeof b, "moved %.0f s in (%.2f g), then not still again", (double)s->spoil_at_s, (double)s->spoil_g);
            else snprintf(b, sizeof b, "not still in %.0f s (the wrist: %.2f g)", (double)s->wait_s, (double)jerk_g);
            return give_up(s, b);
        }
        return STILL_NONE;
    }
    /* STILL_READ */
    s->lit_s += dt;
    if (jerk_g < STILL_BURST_G) { s->quiet_s += dt; return STILL_NONE; }
    s->spoil_g = jerk_g; s->spoil_at_s = s->lit_s; s->quiet_s = 0;
    bool room = s->restarts < STILL_RESTART_MAX && (s->t + STILL_ARM_S + s->read_s) <= s->budget_s;
    if (!room) {
        char b[56];
        snprintf(b, sizeof b, "moved %.0f s in (%.2f g)%s", (double)s->spoil_at_s, (double)s->spoil_g,
                 s->restarts >= STILL_RESTART_MAX ? ", again" : ", no time left");
        return give_up(s, b);
    }
    s->restarts++;
    s->state = STILL_WAIT;
    return STILL_RESTART;
}
