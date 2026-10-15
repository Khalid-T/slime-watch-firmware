/* The wake (stage 1+2, Khalid 2026-09-09): the nap is kept by the IMU's
 * own Wake-on-Motion engine on INT1 instead of a 4 Hz poll; a motion
 * is read as a short burst and judged the way InfiniTime judges a
 * raise (the pose turned toward the face and HELD) -- a bump goes back
 * to sleep without lighting the glass; awake, the inverse (the face
 * turned from the eyes and held) darkens within the second. main.cpp
 * calls three things: wake_nap_begin / wake_nap_once from nap(),
 * wake_woke from wake_up(), wake_lowered from loop(). The judgement
 * itself -- THE EPISODE POSE, the poll's triggers, the verdict -- is
 * raise.h's, pure, since 2026-09-10 evening. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "imu.h"

/* the nap: arm the engine on the pose it sleeps in */
void wake_nap_begin(const vec3_t *pose);
/* one light sleep: returns NULL to keep napping (a timer's minute duty
 * done, or a bump / a turn judged and re-armed), else the cause --
 * "a raise", "a shake", "a touch", "a word on the serial port" -- with
 * ``pose`` updated to the pose it woke in */
void wake_set_ear(bool (*wanted)(void), void (*tick)(void));   /* THE DARK EAR (wake.cpp): the nap idles awake, ticking BLE, while wanted() */
void wake_nap_last(bool *awake_ear, uint32_t *ms);           /* THE EAR'S LEDGER (ear.h): the last spell's kind (awake for the ear / light sleep) and its length, for the tally */
const char *wake_nap_once(vec3_t *pose);
/* awake again: the engine off, the cause noted; a raise sets the pose
 * he looks from */
void wake_woke(const char *why, const vec3_t *pose);
/* awake: LOWER-TO-SLEEP -- true when the glass should go dark now */
bool wake_lowered(float dt, const vec3_t *pose_lp, uint32_t last_motion_ms,
                  uint32_t last_touch_ms, bool touch_down, bool busy);   /* busy: a fall or a cling in progress */
/* THE HANG SLUMP (design/watch-ideas.md A3): the raise that lit the
 * glass came from an arm that hung (the polled posture hanging for
 * HANG_MIN_S or more before it) -- main draws the slump's stand-up */
bool wake_raise_from_hang(void);
/* SHOWING SOMEONE (design/watch-ideas.md B18): awake, the glass turned
 * away from him (raise_show against the look pose) and held steady
 * SHOW_HOLD_S -- true once per look, the moment it is read; the lowering
 * clock rests while the pose is company's */
bool wake_take_shown(void);
bool wake_showing(void);                 /* the pose is company's right now (the face dims his rings while it is) */
/* gravity out of the glass, +1 = face up toward the eyes */
float wake_face_of(const vec3_t *a);
/* serial: `wake`, `wake-cal`, `wom-test [mg] [s] [sel] [blank]` */
void wake_cmd(int argc, char **argv);
/* THE SETTINGS PAGE: QUIET TIME's rule on or off (off: a raise in the
 * quiet hours lights the glass as by day); main sets it from NVS at
 * boot and from the page's row */
void wake_quiet_set(bool on);
bool wake_quiet_on(void);
