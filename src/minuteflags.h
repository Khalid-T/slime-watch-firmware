/* THE HOUSE IN THE ROW (2026-10-12, the operator night, T12).
 * The day log's minute row had three reserved flag bits (5-7) and the
 * desk's week could say how he slept and walked but not WHERE the slime
 * lived: on his wrist or on the desk, with a door open or alone, and
 * whether anything came for him. Now the three bits say it, one row a
 * minute, no byte added, the format's version unchanged (the bits were
 * reserved for exactly this):
 *
 *   HELD    0x20  the house was on the wrist (house_holding) at any
 *                 second of the minute -- the lease toggled mid-minute
 *                 counts that minute for both houses;
 *   LINKED  0x40  a door answered (link_linked: the LAN, the air door,
 *                 the box, the dock) at any second of the minute;
 *   NOTICE  0x80  a wisp flew in to him this minute (wisp_sent moved):
 *                 the desk's, the phone's or the box's, one bit however
 *                 many came -- a wisp held at the air door while the
 *                 house was EMPTY counts in the minute it finally flies.
 *
 * Sticky over the minute: daylog.cpp samples the three once a second
 * (wrist_tick) and takes the bits when the minute files (finish_minute);
 * a take clears them for the next minute and rebases the wisp count.
 * Pure: no Arduino, no clock -- the three states are parameters;
 * tools/minuteflags_test.cpp runs the table on the laptop. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_HELD    0x20   /* daylog.h DAYLOG_HELD: the same bit */
#define MF_LINKED  0x40   /* DAYLOG_LINKED */
#define MF_NOTICE  0x80   /* DAYLOG_NOTICE */
#define MF_ALL     (MF_HELD | MF_LINKED | MF_NOTICE)

typedef struct {
    uint8_t  bits;        /* the minute's bits so far */
    uint32_t wisp_base;   /* wisp_sent as the minute began: a move past it is a notice */
} mf_t;

/* the minute begins (boot, or a minute discarded with no file to take it): nothing yet */
static inline void mf_begin(mf_t *f, uint32_t wisp_sent)
{
    f->bits = 0;
    f->wisp_base = wisp_sent;
}

/* one sample of the three states; once a second is plenty */
static inline void mf_sample(mf_t *f, bool holding, bool linked, uint32_t wisp_sent)
{
    if (holding) f->bits |= MF_HELD;
    if (linked) f->bits |= MF_LINKED;
    if (wisp_sent != f->wisp_base) f->bits |= MF_NOTICE;   /* unsigned: a count that wrapped still moved */
}

/* the minute files: its bits, and the next minute begins from here */
static inline uint8_t mf_take(mf_t *f, uint32_t wisp_sent)
{
    uint8_t b = f->bits & MF_ALL;
    mf_begin(f, wisp_sent);
    return b;
}

#ifdef __cplusplus
}
#endif
