/* THE BARE MOTOR (2026-09-12, Khalid: "i wired in the haptic motor ...
 * GPIO 33 and ground"): a coin vibration motor straight off GPIO 33,
 * no driver chip, no diode -- the pin itself is the driver (LEDC PWM,
 * the S3's strongest drive, 40 mA; a coin ERM wants more, so it runs
 * softer than it could). A DARK DRIVER like the DRV2605: compiled in,
 * inert until `bom motor on`. It plays the SAME three gestures as
 * haptic.h (bump / purr / hop) as duty envelopes, and haptic_play()
 * falls through to it when no DRV2605 answers -- the creature's bumps
 * reach the wrist either way. Never a buzz from the creature: `haptic
 * buzz <ms> [<pct>]` is the bench test only.
 *
 * The pin is the PPG's reserved INT (33): the two share it until the
 * PPG is soldered -- `bom` refuses both on at once. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void motor_begin(void);              /* the pin as LEDC output, off */
void motor_end(void);                /* off, the pin let go (input) */
void motor_tick(void);               /* the envelope's next step (a no-op when idle) */
bool motor_present(void);            /* on the BOM and the pin is ours */
bool motor_play(int kind);           /* HAPTIC_* from haptic.h; false = absent */
/* THE HEARTBEAT HANDED BACK (pulse.h, watch-ideas D32; 2026-10-08): his
 * pulse as a duty envelope -- one soft tap per beat (HEARTBEAT_KICK_MS
 * at 100 % to start the ERM, HEARTBEAT_TAP_MS at HEARTBEAT_TAP_PCT),
 * 60 / bpm s apart, for secs; false = absent or no plan (the pulse out
 * of the honest band). The purr under a pet plays this when a pulse is
 * known (main.cpp); nothing else may. */
bool motor_beats(float bpm, float secs);
bool motor_buzz(uint32_t ms, uint8_t pct, uint32_t hz);   /* the bench test: one flat pulse, capped at 2 s, at hz (0 = raw DC on the pin); the frequency STAYS for the gestures after */
void motor_off(void);                /* at once (before a sleep) */
bool motor_hold(uint32_t s);         /* the meter's test: the pin plain HIGH for s seconds (60 cap; the nap ends it) */
void motor_status(char *out, int cap);
