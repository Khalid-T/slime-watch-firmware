#include "imu.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>

/* QMI8658 registers */
#define REG_WHO_AM_I   0x00     /* 0x05 */
#define REG_REVISION   0x01
#define REG_CTRL1      0x02     /* serial interface / INT pads */
#define REG_CTRL2      0x03     /* accel: full scale + ODR */
#define REG_CTRL3      0x04     /* gyro: full scale + ODR */
#define REG_CTRL5      0x06     /* low-pass filters */
#define REG_CTRL7      0x08     /* sensor enables */
#define REG_CTRL9      0x0A     /* the command register (CTRL9 protocol) */
#define REG_CAL1_L     0x0B     /* WoM: the threshold, mg */
#define REG_CAL1_H     0x0C     /* WoM: INT select + initial level [7:6], blanking samples [5:0] */
#define REG_STATUSINT  0x2D     /* bit7 CmdDone (the CTRL9 handshake) */
#define REG_STATUS0    0x2E     /* bit0 accel data ready */
#define REG_STATUS1    0x2F     /* bit2 WoM event; read to clear */
#define REG_AX_L       0x35
#define REG_GX_L       0x3B     /* the gyro's six bytes follow the accel's: one 12-byte burst from REG_AX_L reads both */
#define REG_RESET      0x60     /* 0xB0 = soft reset */

#define CTRL1_BASE     0x40     /* address auto-increment, little-endian, INT pads off */
#define CTRL1_INT1_EN  0x08     /* INT1 pad driven (bit3; bit4 is INT2) */
#define ACC_RANGE_G    4.0f
#define CTRL2_4G_224HZ 0x15     /* aFS 4g (001), aODR 224 Hz (0101): the awake stream */
/* WoM wants the accel-only low-power ODRs (1100 128 Hz, 1101 21 Hz,
 * 1110 11 Hz, 1111 3 Hz). 21 Hz: a motion is seen within 50 ms and
 * the engine costs tens of microamps; the burst after a wake is read
 * at the full rate anyway. */
#define CTRL2_4G_LP21HZ 0x1D
#define CTRL7_ACC_ON   0x01
/* PLAY: THE SPIN -- the gyro with the light (imu_gyro_set): gFS 1024 dps
 * (110), gODR 224 Hz (0101) to match the accel's (in 6DOF mode the two
 * run at the gyro's rate); CTRL7 bit1 gEN */
#define GYR_RANGE_DPS  1024.0f
#define CTRL3_1024_224HZ 0x65
#define CTRL7_GYR_ON   0x02
#define CTRL9_CMD_ACK  0x00
#define CTRL9_CMD_WOM  0x08     /* CTRL_CMD_WRITE_WOM_SETTING */

static uint8_t s_addr = 0;
static uint8_t s_rev = 0;
static bool s_wom = false;
static bool s_gyro = false;
static vec3_t s_gyro_last = { 0, 0, 0 };
static bool s_ack = false;                   /* THE BUS UNSTUCK: the last transaction was ACKed */
static uint8_t s_wom_mg = 0, s_wom_blank = 0, s_wom_sel = 2;   /* the engine's last setting, for a re-init under it */

static bool wr(uint8_t reg, uint8_t v)
{
    Wire.beginTransmission(s_addr);
    Wire.write(reg);
    Wire.write(v);
    s_ack = Wire.endTransmission() == 0;
    return s_ack;
}

static bool rd(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) { s_ack = false; return false; }
    if (Wire.requestFrom((int)addr, (int)n) != (int)n) { s_ack = false; return false; }
    for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
    s_ack = true;
    return true;
}

bool imu_answered(void) { return s_addr && s_ack; }

static uint8_t rd1(uint8_t reg)
{
    uint8_t v = 0;
    rd(s_addr, reg, &v, 1);
    return v;
}

uint8_t imu_addr(void) { return s_addr; }
uint8_t imu_revision(void) { return s_rev; }
bool imu_wom_on(void) { return s_wom; }
uint8_t imu_status1(void) { return s_addr ? rd1(REG_STATUS1) : 0; }
bool imu_int1(void) { return digitalRead(PIN_IMU_INT1) == HIGH; }

/* the CTRL9 protocol: write the command, wait for CmdDone (STATUSINT
 * bit7 on the A, STATUS1 bit0 on the older die), acknowledge, wait for
 * it to clear. A handshake that never comes is reported, not waited on
 * for ever. */
static bool ctrl9(uint8_t cmd)
{
    wr(REG_CTRL9, cmd);
    uint32_t t0 = millis();
    bool done = false;
    while (millis() - t0 < 100) {
        if ((rd1(REG_STATUSINT) & 0x80) || (rd1(REG_STATUS1) & 0x01)) { done = true; break; }
        delay(1);
    }
    wr(REG_CTRL9, CTRL9_CMD_ACK);
    t0 = millis();
    while (millis() - t0 < 100) {
        if (!(rd1(REG_STATUSINT) & 0x80)) break;
        delay(1);
    }
    if (!done) Serial.printf("[imu] ctrl9 cmd 0x%02X: no CmdDone\n", cmd);
    return done;
}

uint8_t imu_begin(void)
{
    /* THE BUS UNSTUCK (2026-10-12): re-entrant -- the address is dropped
     * and probed afresh (a chip that went quiet may answer or not), and
     * what the chip was doing (the gyro with the lit glass, the motion
     * engine in the dark) is put back after the reset below */
    bool was_gyro = s_gyro, was_wom = s_wom;
    s_addr = 0; s_gyro = false; s_wom = false;
    const uint8_t cands[2] = { ADDR_QMI8658, ADDR_QMI8658_ALT };
    for (int i = 0; i < 2; i++) {
        uint8_t who = 0;
        if (rd(cands[i], REG_WHO_AM_I, &who, 1) && who == 0x05) {
            s_addr = cands[i];
            break;
        }
    }
    if (!s_addr) return 0;
    pinMode(PIN_IMU_INT1, INPUT_PULLDOWN);   /* WoM drives it HIGH from an initial LOW */
    wr(REG_RESET, 0xB0);
    delay(20);
    s_rev = rd1(REG_REVISION);
    wr(REG_CTRL1, CTRL1_BASE);
    wr(REG_CTRL2, CTRL2_4G_224HZ);
    wr(REG_CTRL5, 0x01);            /* accel LPF on, the mildest mode */
    wr(REG_CTRL7, CTRL7_ACC_ON);    /* accel enabled, gyro off */
    delay(10);
    s_wom = false;
    if (was_wom) imu_wom_enable_sel(s_wom_mg, s_wom_blank, s_wom_sel);   /* the re-init: the chip back as it was */
    else if (was_gyro) imu_gyro_set(true);
    return s_addr;
}

bool imu_wom_enable_sel(uint8_t mg, uint8_t blank, uint8_t sel)
{
    if (!s_addr) return false;
    if (!mg) mg = 1;
    s_wom_mg = mg; s_wom_blank = blank; s_wom_sel = sel;   /* kept for a re-init under the engine (imu_begin) */
    /* the datasheet's order: sensors off, the low-power ODR, the
     * threshold and the pin, the command, sensors on */
    wr(REG_CTRL7, 0x00);
    wr(REG_CTRL1, CTRL1_BASE | CTRL1_INT1_EN | 0x10);
    wr(REG_CTRL2, CTRL2_4G_LP21HZ);
    wr(REG_CAL1_L, mg);
    wr(REG_CAL1_H, (uint8_t)(((sel & 3) << 6) | (blank & 0x3F)));
    bool ok = ctrl9(CTRL9_CMD_WOM);
    wr(REG_CTRL7, CTRL7_ACC_ON);    /* accel only: the gyro is off under the engine (the wake turns it back on) */
    s_gyro = false;
    rd1(REG_STATUS1);               /* a stale event cleared */
    s_wom = ok;
    return ok;
}

bool imu_wom_enable(uint8_t mg, uint8_t blank)
{
    return imu_wom_enable_sel(mg, blank, 2);   /* 10: INT1, initial value 0 */
}

uint8_t imu_reg(uint8_t reg) { return s_addr ? rd1(reg) : 0; }

bool imu_read_raw(vec3_t *a)
{
    if (!s_addr) return false;
    uint8_t b[6];
    if (!rd(s_addr, REG_AX_L, b, 6)) return false;
    int16_t x = (int16_t)(b[0] | (b[1] << 8));
    int16_t y = (int16_t)(b[2] | (b[3] << 8));
    int16_t z = (int16_t)(b[4] | (b[5] << 8));
    const float k = ACC_RANGE_G / 32768.0f;
    a->x = x * k; a->y = y * k; a->z = z * k;
    return true;
}

bool imu_wom_disable(void)
{
    if (!s_addr) return false;
    wr(REG_CTRL7, 0x00);
    wr(REG_CAL1_L, 0x00);           /* threshold 0 = WoM off */
    wr(REG_CAL1_H, 0x00);
    bool ok = ctrl9(CTRL9_CMD_WOM);
    wr(REG_CTRL1, CTRL1_BASE);
    wr(REG_CTRL2, CTRL2_4G_224HZ);
    wr(REG_CTRL7, CTRL7_ACC_ON);
    s_gyro = false;
    rd1(REG_STATUS1);
    s_wom = false;
    return ok;
}

bool imu_gyro_set(bool on)
{
    if (!s_addr) return false;
    if (on == s_gyro) return true;
    if (on) {
        if (s_wom) return false;    /* under the motion engine the accel runs alone at its low-power rate */
        wr(REG_CTRL3, CTRL3_1024_224HZ);
        wr(REG_CTRL7, CTRL7_ACC_ON | CTRL7_GYR_ON);
    } else {
        wr(REG_CTRL7, CTRL7_ACC_ON);
    }
    s_gyro = on;
    s_gyro_last.x = s_gyro_last.y = s_gyro_last.z = 0;
    return true;
}

bool imu_gyro_on(void) { return s_gyro; }

bool imu_gyro(vec3_t *g)
{
    if (!s_gyro) return false;
    *g = s_gyro_last;
    return true;
}

bool imu_read(vec3_t *a)
{
    if (!s_addr) return false;
    uint8_t st = 0;
    if (!rd(s_addr, REG_STATUS0, &st, 1) || !(st & 0x01)) return false;
    uint8_t b[12];
    size_t n = s_gyro ? 12 : 6;     /* PLAY: THE SPIN -- the gyro's six bytes ride the same burst (REG_GX_L follows) */
    if (!rd(s_addr, REG_AX_L, b, n)) return false;
    int16_t x = (int16_t)(b[0] | (b[1] << 8));
    int16_t y = (int16_t)(b[2] | (b[3] << 8));
    int16_t z = (int16_t)(b[4] | (b[5] << 8));
    const float k = ACC_RANGE_G / 32768.0f;
    a->x = x * k;
    a->y = y * k;
    a->z = z * k;
    if (s_gyro) {
        int16_t gx = (int16_t)(b[6] | (b[7] << 8));
        int16_t gy = (int16_t)(b[8] | (b[9] << 8));
        int16_t gz = (int16_t)(b[10] | (b[11] << 8));
        const float kg = GYR_RANGE_DPS / 32768.0f;
        s_gyro_last.x = gx * kg; s_gyro_last.y = gy * kg; s_gyro_last.z = gz * kg;
    }
    return true;
}
