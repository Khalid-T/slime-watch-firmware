/* THE STILL SPOT (2026-10-08, THE PULSE, THREE WAYS BETTER): the pure
 * arbiter between the wrist's motion and a pulse read. The 10-03 wrist
 * read came back NONE "irregular (cv 0.32): moved?" -- the read judged
 * movement only by the intervals' spread after the fact, though the IMU
 * knew the wrist moved the whole time. Now every spot (the row's 30 s,
 * the console's, the creature's own in the day) goes through this:
 *
 *   WAIT   the LEDs stay dark until the wrist has been still (the jerk
 *          under STILL_JERK_G) for STILL_ARM_S; not still within the
 *          wait (PPG_WAIT_STILL_S) = the read gives up, the IMU the
 *          witness ("not still in 10 s").
 *   READ   the LEDs light, the samples gather; a burst over
 *          STILL_BURST_G DISCARDS them -- a restart when the budget
 *          (the wait + two reads' worth) still has a whole read in it,
 *          the wrist waited for again; else the verdict is "moved",
 *          said with the jerk that spoiled it and how far in.
 *
 * No Arduino, no clock of its own: main's jerk (the lit loop's
 * per-frame jerk, the nap's jerk between two polls) and dt come in,
 * one word comes out (light / restart / gave up / nothing);
 * tools/still_test.cpp pins it on the laptop. ppg.cpp owns one of
 * these and does what the word says on the bus. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STILL_JERK_G      0.05f   /* the jerk under this is still: play.h's PLAY_STILL_G, main's MOTION_G -- the one line the whole watch uses for "not moving" */
#define STILL_BURST_G     0.08f   /* mid-read, a jerk over this spoils the samples: wake.cpp's still_jerk_g (the nap's own stillness line) -- a breath or a tremor sits under it, a hand reaching for a cup over it (his wrist to tune) */
#define STILL_ARM_S       2.0f    /* still this long before the LEDs light (the bead asks 3 s flat AND still; a read needs only still, and a 10 s wait must have room for it) */
#define PPG_WAIT_STILL_S  10.0f   /* the longest a read waits for stillness before it gives up (the row would show STILL this long at most) */
#define STILL_RESTART_MAX 2       /* reads thrown away at most: a third burst is the verdict (the budget below runs out at about the same time) */

enum { STILL_IDLE = 0, STILL_WAIT, STILL_READ, STILL_DONE };
enum { STILL_NONE = 0, STILL_LIGHT, STILL_RESTART, STILL_GAVE_UP };

typedef struct {
    int   state;         /* STILL_* */
    float read_s;        /* the read's own length */
    float wait_s;        /* the first wait's deadline */
    float budget_s;      /* wait_s + 2 x read_s: the whole affair's clock */
    float t;             /* seconds since still_begin */
    float quiet_s;       /* still so far (0 at every jerk over the line) */
    float lit_s;         /* seconds since the LEDs last lit (the read in progress) */
    int   restarts;      /* reads discarded so far */
    float spoil_g;       /* the jerk that spoiled the last read (0 = none) */
    float spoil_at_s;    /* how far into that read it came */
    char  why[56];       /* the verdict in words once STILL_DONE (gave up) */
} still_t;

void still_begin(still_t *s, float read_s, float wait_s);   /* a read of read_s asked: WAIT */
/* one call per frame / per poll with the wrist's jerk (g) over the dt (s)
 * that passed. Returns STILL_LIGHT on the frame the LEDs may light (the
 * read starts, or starts over after a restart), STILL_RESTART on the
 * frame a burst spoils the samples (discard; WAIT again), STILL_GAVE_UP
 * once when the read is refused (still_why says why), else STILL_NONE. */
int  still_tick(still_t *s, float dt, float jerk_g);
void still_end(still_t *s);                                  /* the read finished (or was abandoned): IDLE */
bool still_waiting(const still_t *s);                        /* WAIT: the LEDs dark, the row shows STILL */
bool still_reading(const still_t *s);                        /* READ: the LEDs lit */
const char *still_why(const still_t *s);                     /* the verdict (STILL_DONE), else "" */

#ifdef __cplusplus
}
#endif
