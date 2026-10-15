/* THE GENTLE WAKE: the rule (gentle.h has the whole account). Pure C:
 * the clock, the sleep and the stir come in as parameters, the pulse
 * is fed; tools/gentle_test.cpp runs it on the laptop. */
#include "gentle.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    float    bpm;
    bool     ok;
    bool     dozing;
    uint32_t ms;
} reading_t;

static reading_t hist[GENTLE_HIST];     /* newest first */
static int       n = 0;
static float     floor_bpm = 0;         /* the night's floor, 0 = none */
static int       floor_n = 0;           /* clean dozing readings folded into it */
static bool      spent = false;
static uint32_t  seen_seq = 0;

void gentle_begin(void)
{
    memset(hist, 0, sizeof hist);
    n = 0; floor_bpm = 0; floor_n = 0; spent = false; seen_seq = 0;
}

void gentle_pulse(float bpm, bool ok, uint32_t now_ms, bool dozing)
{
    for (int i = GENTLE_HIST - 1; i > 0; i--) hist[i] = hist[i - 1];
    hist[0].bpm = bpm; hist[0].ok = ok; hist[0].dozing = dozing; hist[0].ms = now_ms;
    if (n < GENTLE_HIST) n++;
    if (!ok) return;                                   /* a failed spot is remembered, never read */
    if (!dozing) { floor_bpm = 0; floor_n = 0; return; }   /* a reading awake: the night the floor belonged to is over */
    if (floor_n == 0 || bpm < floor_bpm) floor_bpm = bpm;
    floor_n++;
}

void gentle_pulse_seen(uint32_t seq, float bpm, bool ok, uint32_t now_ms, bool dozing)
{
    if (seq == 0 || seq == seen_seq) return;
    seen_seq = seq;
    gentle_pulse(bpm, ok, now_ms, dozing);
}

void gentle_spent(void) { spent = true; }
void gentle_rearm(void) { spent = false; }
bool gentle_is_spent(void) { return spent; }
int  gentle_count(void) { return n; }

/* the last two clean readings, newest first; false = fewer than two */
static bool last_two(const reading_t **a, const reading_t **b)
{
    const reading_t *r[2]; int k = 0;
    for (int i = 0; i < n && k < 2; i++) if (hist[i].ok) r[k++] = &hist[i];
    if (k < 2) return false;
    *a = r[0]; *b = r[1];
    return true;
}

bool gentle_check(const gentle_in_t *in, gentle_out_t *out)
{
    gentle_out_t o;
    memset(&o, 0, sizeof o);
    o.from_minute = -1;
    o.floor_bpm = floor_n ? (int)(floor_bpm + 0.5f) : 0;
    const reading_t *a = NULL, *b = NULL;
    bool pair = last_two(&a, &b);
    if (pair) { o.last_bpm = (int)(a->bpm + 0.5f); o.prev_bpm = (int)(b->bpm + 0.5f); }
    if (in->alarm_minute < 0) o.state = GENTLE_NO_ALARM;
    else if (in->minute < 0) o.state = GENTLE_NO_CLOCK;
    else {
        o.from_minute = (in->alarm_minute - GENTLE_WINDOW_MIN + 1440) % 1440;
        int ahead = (in->alarm_minute - in->minute + 1440) % 1440;   /* minutes until the alarm; 0 = its own minute */
        bool inside = ahead >= 1 && ahead <= GENTLE_WINDOW_MIN;
        if (spent) o.state = GENTLE_SPENT;
        else if (!inside) o.state = GENTLE_ARMED;
        else if (!in->dozing) o.state = GENTLE_AWAKE;
        else if (in->stir_age_s < 0 || in->stir_age_s > GENTLE_STIR_S) o.state = GENTLE_NO_STIR;
        else if (!pair || floor_n < GENTLE_FLOOR_N || (uint32_t)(in->now_ms - a->ms) > (uint32_t)GENTLE_PULSE_AGE_S * 1000u)
            o.state = GENTLE_NO_PULSE;
        else if (a->bpm >= floor_bpm + GENTLE_RISE_BPM && b->bpm >= floor_bpm + GENTLE_RISE_BPM && a->bpm >= b->bpm)
            o.state = GENTLE_WAKE;
        else o.state = GENTLE_FLAT;
    }
    o.wake = o.state == GENTLE_WAKE;
    if (out) *out = o;
    return o.wake;
}

const char *gentle_state_name(int state)
{
    static const char *const NAMES[] = {
        "no alarm", "no clock", "spent", "armed", "awake", "not stirring", "no pulse known", "pulse flat", "wake"
    };
    return state >= 0 && state <= GENTLE_WAKE ? NAMES[state] : "?";
}

void gentle_status(const gentle_in_t *in, char *out, int cap)
{
    gentle_out_t o;
    gentle_check(in, &o);
    char pulse[40] = "";
    if (o.floor_bpm) snprintf(pulse, sizeof pulse, " (floor %d, last %d <- %d)", o.floor_bpm, o.last_bpm, o.prev_bpm);
    else if (o.last_bpm) snprintf(pulse, sizeof pulse, " (no floor yet, last %d)", o.last_bpm);
    switch (o.state) {
    case GENTLE_NO_ALARM: snprintf(out, cap, "gentle: no alarm"); break;
    case GENTLE_NO_CLOCK: snprintf(out, cap, "gentle: no clock"); break;
    case GENTLE_SPENT:    snprintf(out, cap, "gentle: spent for %02d:%02d%s", in->alarm_minute / 60, in->alarm_minute % 60, pulse); break;
    case GENTLE_ARMED:    snprintf(out, cap, "gentle: armed %02d:%02d-%02d:%02d%s", o.from_minute / 60, o.from_minute % 60,
                                   in->alarm_minute / 60, in->alarm_minute % 60, pulse); break;
    case GENTLE_AWAKE:    snprintf(out, cap, "gentle: in the window, awake%s", pulse); break;
    case GENTLE_NO_STIR:  snprintf(out, cap, "gentle: in the window, not stirring%s", pulse); break;
    case GENTLE_NO_PULSE: snprintf(out, cap, "gentle: in the window, no pulse known%s", pulse); break;
    case GENTLE_FLAT:     snprintf(out, cap, "gentle: in the window, pulse flat%s", pulse); break;
    case GENTLE_WAKE:     snprintf(out, cap, "gentle: in the window, wake%s", pulse); break;
    default:              snprintf(out, cap, "gentle: ?"); break;
    }
}
