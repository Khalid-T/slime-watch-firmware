/* THE CABLE SENSE, SETTLED (src/cable.h) on the laptop: the band and the
 * settle, fed synthetic readings twice a second.
 * Build: c++ -std=gnu++17 -I src -o cable_test tools/cable_test.cpp
 * Run:   ./cable_test
 * The flap of 2026-09-10 21:28:46-47 (VBAT at the 4050 threshold: off,
 * then on one second later) settles nowhere; a sag to 4040 on the cable
 * is still the cable; a cell falling under 3980 is off the cable after
 * 10 s and not before; a reading that crosses back inside the settle
 * starts the settle over; the first sample snaps either way.
 * THE RISING CELL (2026-10-12): a cell under the float that rose 40 mV
 * over 4 closed minutes, steadily, is the cable (held 10 s like the
 * rest); a one-minute rebound after a load is not; the witness lapses
 * when the climb stops and the band lets go as ever. */
#include "cable.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
static void check(const char *what, bool ok)
{
    checks++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* feed one reading every 500 ms for ``ms``; returns the settled sense at the end, counts the edges */
static uint32_t now = 1000;
static int edges = 0;
static bool feed(cable_t *c, uint16_t mv, uint32_t ms)
{
    bool was = c->on;
    uint32_t end = now + ms;
    while (now < end) {
        now += 500;
        bool on = cable_sense(c, mv, now);
        if (on != was) { edges++; was = on; }
    }
    return c->on;
}

int main(void)
{
    cable_t c;

    /* a boot on the cable: on from the first sample (no settle for the first line) */
    memset(&c, 0, sizeof c);
    check("a boot at 4130 mV: on the cable at once", cable_sense(&c, 4130, now) == true);
    /* a sag on a weak port: 4040 is under the on-threshold but inside the band -- still the cable */
    check("a sag to 4040 for a minute: still the cable (the band)", feed(&c, 4040, 60000) == true && edges == 0);
    check("... even 3985, just over the far edge, for a minute", feed(&c, 3985, 60000) == true && edges == 0);
    /* under the far edge: off, after the settle and not before */
    feed(&c, 3970, 9500);
    check("3970 for 9.5 s: not yet off", c.on == true && edges == 0);
    feed(&c, 3970, 1000);
    check("3970 for 10.5 s: off the cable, one edge", c.on == false && edges == 1);
    /* the way back needs the on-threshold, held */
    check("4000 (inside the band) for a minute: still off", feed(&c, 4000, 60000) == false && edges == 1);
    feed(&c, 4060, 6000);
    check("4060 for 6 s: not yet on", c.on == false);
    feed(&c, 4000, 1000);
    check("... a dip to 4000 for 1 s inside the settle: the settle starts over", c.on == false && !c.cand);
    feed(&c, 4060, 9500);
    check("4060 for another 9.5 s: still not on (the settle restarted)", c.on == false && edges == 1);
    feed(&c, 4060, 1000);
    check("4060 held 10 s: on, the second edge", c.on == true && edges == 2);

    /* THE FLAP of 21:28:46-47: a reading alternating each second across 4050 */
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 4049, now);
    check("a boot at 4049: off (under the on-threshold)", c.on == false);
    for (int i = 0; i < 120; i++) feed(&c, i % 2 ? 4051 : 4049, 1000);
    check("4049 / 4051 alternating each second for 2 min: never on (no edge settles)", c.on == false && edges == 0);
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 4051, now);
    check("a boot at 4051: on", c.on == true);
    for (int i = 0; i < 120; i++) feed(&c, i % 2 ? 4049 : 4051, 1000);
    check("... the same flap from on: never off (4049 is inside the band)", c.on == true && edges == 0);
    for (int i = 0; i < 120; i++) feed(&c, i % 2 ? 3970 : 4051, 1000);
    check("3970 / 4051 alternating each second from on: never off (each dip is shorter than the settle)", c.on == true && edges == 0);

    /* a cell in, later: a full cell freshly unplugged rests above the far edge -- read as the cable until it drains */
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 4130, now);
    check("a full cell unplugged, resting at 4120: still read as the cable (the honesty gap, cable.h)", feed(&c, 4120, 60000) == true);
    check("... drained to 3975 for 10 s: off", feed(&c, 3975, 10500) == false && edges == 1);
    /* a boot off the cable */
    memset(&c, 0, sizeof c);
    check("a boot at 3900: off from the first sample", cable_sense(&c, 3900, now) == false);
    /* the same now twice: the same answer (charging() is asked several times a frame) */
    check("asked twice at the same instant: the same answer, no edge counted twice",
          cable_sense(&c, 4060, now + 500) == false && cable_sense(&c, 4060, now + 500) == false && c.cand);

    /* THE RISING CELL: the second witness, fed the closed minutes' mV */
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 3500, now);
    check("a boot at 3500 (an empty cell on the charger, the float blind): off", c.on == false);
    static const uint16_t climb[] = { 3500, 3515, 3530, 3545, 3560 };   /* ~15 mV a minute: the charger's first hour */
    for (int i = 0; i < 4; i++) { cable_minute(&c, climb[i]); feed(&c, climb[i], 60000); }
    check("four closed minutes of the climb (3500 -> 3545): not yet (the window needs five readings)", c.on == false && !c.rising);
    cable_minute(&c, climb[4]);
    check("the fifth, +60 over the window: the witness is up", c.rising == true);
    feed(&c, 3560, 9500);
    check("... held 9.5 s: not yet on (the settle, like the rest)", c.on == false && c.cand);
    feed(&c, 3560, 1000);
    check("... held 10.5 s: on the cable, one edge", c.on == true && edges == 1);
    for (int i = 0; i < 10; i++) { cable_minute(&c, (uint16_t)(3575 + 15 * i)); feed(&c, (uint16_t)(3575 + 15 * i), 60000); }
    check("ten more climbing minutes to 3710: still on (the witness holds under 3980)", c.on == true && c.rising && edges == 1);
    /* unplugged mid-charge: the climb stops, the witness lapses, the band lets go */
    static const uint16_t rest[] = { 3690, 3685, 3683, 3682, 3682 };
    cable_minute(&c, rest[0]); feed(&c, rest[0], 60000);
    check("unplugged at 3710, one resting minute (3690): still on (the window still shows +25)", c.on == true && c.rising);
    cable_minute(&c, rest[1]); feed(&c, rest[1], 60000);
    check("a second resting minute (3685): the window is flat (+5), the witness lapsed, 3685 is under the far edge -> off, the second edge", c.on == false && !c.rising && edges == 2);
    for (int i = 2; i < 5; i++) { cable_minute(&c, rest[i]); feed(&c, rest[i], 60000); }
    check("three more resting minutes: still off, no more edges", c.on == false && !c.rising && edges == 2);
    /* a rebound after a load (the radio's sag letting go) is one minute's rise: not a cable */
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 3500, now);
    static const uint16_t rebound[] = { 3500, 3555, 3560, 3561, 3561, 3562 };
    for (int i = 0; i < 6; i++) { cable_minute(&c, rebound[i]); feed(&c, rebound[i], 60000); }
    check("a 55 mV rebound in one minute then flat (+62 over the window): not the cable (the second half did not climb)", c.on == false && !c.rising);
    /* a session's sag then its release: a step, not a climb */
    memset(&c, 0, sizeof c);
    cable_sense(&c, 3540, now);
    static const uint16_t step[] = { 3540, 3540, 3540, 3560, 3560, 3560 };
    for (int i = 0; i < 6; i++) { cable_minute(&c, step[i]); feed(&c, step[i], 60000); }
    check("three sagged minutes then three released (+20): not the cable", c.on == false && !c.rising);
    /* at the float the float is the witness: a climb that reaches 4050 */
    memset(&c, 0, sizeof c); edges = 0;
    cable_sense(&c, 3990, now);
    static const uint16_t top[] = { 3990, 4005, 4020, 4035, 4050 };
    for (int i = 0; i < 5; i++) { cable_minute(&c, top[i]); feed(&c, top[i], 60000); }
    check("a climb that ends at 4050: on by the float (the witness is not raised at the float)", c.on == true && !c.rising && edges == 1);
    check("the rise is 40 mV over 4 minutes", CABLE_RISE_MV == 40 && CABLE_RISE_MIN == 4);

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
