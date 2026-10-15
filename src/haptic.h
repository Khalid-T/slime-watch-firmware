/* DRV2605 + a coin LRA (0x5A on the second bus): the watch touches him
 * back. A DARK DRIVER -- compiled in, inert until `bom haptic on`. The
 * gel's gestures only: a bump, a purr, a triple hop -- never a buzz
 * (the DRV2605's alert/buzz effects are not in the table). Raw
 * registers, the chip's own LRA effect library (library 6), no library
 * code. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum { HAPTIC_BUMP = 0, HAPTIC_PURR = 1, HAPTIC_HOP3 = 2, HAPTIC_KINDS = 3 };

void haptic_begin(void);            /* probe 0x5A (device id in STATUS), LRA mode, the stored calibration */
void haptic_end(void);
void haptic_tick(void);             /* standby 2 s after the last play (a no-op when absent) */
bool haptic_present(void);
bool haptic_play(int kind);         /* the ONE call: the DRV2605, else the bare motor (motor.h); false = neither (the creature carries on) */
/* THE HEARTBEAT HANDED BACK (pulse.h, watch-ideas D32; 2026-10-08): his
 * pulse as beats for secs -- the DRV2605's Soft Bump 60 % per beat with
 * the chip's own waits between (four beats at most: the sequencer's
 * eight slots), else the bare motor's envelope (motor_beats); false =
 * neither, or no plan (a pulse out of the honest band) */
bool haptic_beats(float bpm, float secs);
void haptic_calibrate(void);        /* `haptic cal`: the LRA's auto-calibration, once, stored in NVS */
void haptic_status(char *out, int cap);
