/* QMI8658: the accelerometer, raw over I2C (no library -- the registers
 * are few and the wake logic wants the raw vector anyway), and its
 * Wake-on-Motion engine, which watches the wrist while the chip sleeps
 * (stage 1, Khalid 2026-09-09: the nap's 4 Hz polling replaced by the
 * IMU's own motion engine on an INT pin). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { float x, y, z; } vec3_t;   /* in g */

/* probes 0x6B then 0x6A; returns the address found or 0. Re-entrant since
 * THE BUS UNSTUCK (2026-10-12): a second call (bus.cpp's recovery) probes
 * afresh and puts the gyro / the motion engine back as they were */
uint8_t imu_begin(void);
/* THE BUS UNSTUCK: the last transaction on bus 0 was ACKed by the chip --
 * imu_read's false is "no new sample" when this is true, the bus (or the
 * chip) silent when it is false; false too while no address is known */
bool imu_answered(void);
/* the latest accelerometer sample; false if nothing new (with the gyro
 * on, the same read takes its sample too: imu_gyro) */
bool imu_read(vec3_t *a);
/* PLAY: THE SPIN (design/watch-ideas.md F53, 2026-09-23): the gyro,
 * on only while the glass is lit (main's wake_up / go_dark) -- 224 Hz
 * with the accelerometer, +-1024 dps (a turn a second is 360; the
 * range keeps a hard spin unclipped), read in the same 12-byte burst
 * as the accelerometer so a frame costs one more transaction of
 * nothing. Off in the dark house and under the motion engine (both
 * write CTRL7 accel-only); the wake turns it back on. */
bool imu_gyro_set(bool on);
bool imu_gyro_on(void);
/* the gyro sample the last imu_read carried, in deg/s (the chip's own
 * axes: z is the glass's normal); false while the gyro is off */
bool imu_gyro(vec3_t *g);
uint8_t imu_addr(void);

/* Wake-on-Motion: the accelerometer drops to its low-power ODR and
 * raises INT1 (active HIGH, from an initial LOW) when |delta a| on any
 * axis exceeds ``mg`` (1..255 milli-g) after ``blank`` samples of
 * settling. Data keeps flowing at the low-power rate. */
bool imu_wom_enable(uint8_t mg, uint8_t blank);
/* the same with the pin/initial-level select by hand (CAL1_H[7:6]): the tuning aid */
bool imu_wom_enable_sel(uint8_t mg, uint8_t blank, uint8_t sel);
uint8_t imu_reg(uint8_t reg);                /* any register, for the tuning aid */
bool imu_read_raw(vec3_t *a);                /* the data registers, data-ready or not */
/* back to the ordinary 224 Hz stream, WoM off */
bool imu_wom_disable(void);
bool imu_wom_on(void);
/* STATUS1: the event flags (bit2 WoM); reading it clears the interrupt */
uint8_t imu_status1(void);
bool imu_int1(void);                         /* the INT1 pin's level */
uint8_t imu_revision(void);
