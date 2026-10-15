#include "haptic.h"
#include "motor.h"
#include "pulse.h"       /* THE HEARTBEAT HANDED BACK: the beats' plan */
#include "bus.h"
#include "store.h"
#include "app.h"
#include <Arduino.h>
#include <stdio.h>

#define ADDR          0x5A
#define REG_STATUS    0x00   /* [7:5] device id: 3 DRV2605, 7 DRV2605L; bit3 DIAG_RESULT (1 = the last cal/diag failed) */
#define REG_MODE      0x01   /* bit7 reset, bit6 STANDBY, [2:0] 0 internal trigger, 7 auto-calibration */
#define REG_LIBRARY   0x03   /* 6 = the LRA library */
#define REG_WAVSEQ    0x04   /* ..0x0B: eight slots; 0 ends, bit7 = a wait of ([6:0] x 10 ms) */
#define REG_GO        0x0C
#define REG_RATED_V   0x16
#define REG_OD_CLAMP  0x17
#define REG_CAL_COMP  0x18
#define REG_CAL_BEMF  0x19
#define REG_FEEDBACK  0x1A   /* bit7 N_ERM_LRA, [6:4] brake factor, [3:2] loop gain, [1:0] BEMF gain */
#define REG_CONTROL3  0x1D   /* bit0 LRA_OPEN_LOOP (0 = closed loop, auto-resonance) */

#define MODE_TRIGGER  0x00
#define MODE_AUTOCAL  0x07
#define MODE_STANDBY  0x40

/* THE LRA ASSUMED: a 10 mm coin, 2.0 Vrms rated, 2.5 V peak, ~235 Hz
 * (the common breakout's). The datasheet's LRA arithmetic (sample time
 * 300 us): rated = Vrms / (20.58 mV x sqrt(1 - (4 x 300 us + 300 us) x f))
 * = 2.0 / (0.02058 x 0.805) = 121 -> 0x79; clamp = Vpk / 21.22 mV = 118 ->
 * 0x76. FIRST-DAY: check the LRA's own numbers, then `haptic cal`. */
#define RATED_V_LRA   0x79
#define OD_CLAMP_LRA  0x76

/* the gestures, from the chip's LRA library: 7 = Soft Bump 100 %, 54 =
 * Pulsing Medium 1 100 %; a wait = 0x80 | (ms / 10). Nothing from the
 * buzz/alert rows (14, 15, 47..51): a buzz is a phone, not a creature. */
#define WAIT(ms)      (uint8_t)(0x80 | ((ms) / 10))
static const uint8_t SEQ[HAPTIC_KINDS][8] = {
    { 7, 0, 0, 0, 0, 0, 0, 0 },                              /* bump: one soft bump */
    { 54, 54, 0, 0, 0, 0, 0, 0 },                            /* purr: two soft pulsings, run together */
    { 7, WAIT(120), 7, WAIT(120), 7, 0, 0, 0 },              /* triple hop: three bumps, a breath apart */
};
static const char *const KIND_NAMES[HAPTIC_KINDS] = { "bump", "purr", "hop" };

#define STANDBY_AFTER_MS 2000

static bool s_present = false;
static uint8_t s_id = 0;
static uint32_t s_played_ms = 0;
static bool s_awake = false;
static uint32_t s_plays = 0;

static bool wr(uint8_t r, uint8_t v) { return bus2_wr1(ADDR, r, v); }
static uint8_t rd(uint8_t r) { return bus2_rd1(ADDR, r); }

static void apply_cal(void)
{
    if (!store.haptic_cal[0] && !store.haptic_cal[1]) return;
    wr(REG_CAL_COMP, store.haptic_cal[0]);
    wr(REG_CAL_BEMF, store.haptic_cal[1]);
    wr(REG_FEEDBACK, (uint8_t)((rd(REG_FEEDBACK) & 0xFC) | (store.haptic_cal[2] & 0x03)));
}

void haptic_begin(void)
{
    s_present = false;
    uint8_t st = 0;
    if (!bus2_rd(ADDR, REG_STATUS, &st, 1)) { LOG("haptic: DRV2605 absent at 0x%02X (on the BOM)", ADDR); return; }
    s_id = st >> 5;
    if (s_id != 3 && s_id != 7) { LOG("haptic: 0x%02X answers but the device id is %u, not a DRV2605", ADDR, s_id); return; }
    wr(REG_MODE, MODE_TRIGGER);                               /* out of standby */
    delay(1);
    wr(REG_FEEDBACK, (uint8_t)(rd(REG_FEEDBACK) | 0x80));    /* LRA */
    wr(REG_RATED_V, RATED_V_LRA);
    wr(REG_OD_CLAMP, OD_CLAMP_LRA);
    wr(REG_LIBRARY, 6);
    wr(REG_CONTROL3, (uint8_t)(rd(REG_CONTROL3) & ~0x01));   /* closed loop: auto-resonance */
    apply_cal();
    wr(REG_MODE, MODE_STANDBY);
    s_awake = false;
    s_present = true;
    LOG("haptic: DRV2605%s found at 0x%02X, LRA library, calibration %s", s_id == 7 ? "L" : "", ADDR,
        (store.haptic_cal[0] || store.haptic_cal[1]) ? "applied" : "NEVER RUN (`haptic cal` once)");
}

void haptic_end(void)
{
    if (s_present) wr(REG_MODE, MODE_STANDBY);
    s_present = false;
}

bool haptic_present(void) { return s_present; }

bool haptic_play(int kind)
{
    if (kind < 0 || kind >= HAPTIC_KINDS) return false;
    if (!s_present) return motor_play(kind);      /* THE BARE MOTOR (motor.cpp) stands in for the DRV2605 */
    if (!s_awake) { wr(REG_MODE, MODE_TRIGGER); s_awake = true; delayMicroseconds(300); }
    bus2_wr(ADDR, REG_WAVSEQ, SEQ[kind], 8);
    wr(REG_GO, 1);
    s_played_ms = millis();
    s_plays++;
    return true;
}

/* THE HEARTBEAT HANDED BACK (pulse.h): the beats on the DRV2605 -- the
 * LRA library's 8 = Soft Bump 60 % (the bump's 7 at 100 % is the
 * creature's thump; a heartbeat is under it) once per beat, a wait of
 * (the period less the effect's own ~DRV_BEAT_MS) between; the
 * sequencer's eight slots hold HEARTBEAT_DRV_MAX four beats (120 bpm
 * and under is the whole two seconds; over it the first four), a wait
 * caps at the chip's 1270 ms (35 bpm's 1714 would ask more) */
#define DRV_BEAT_EFFECT  8
#define DRV_BEAT_MS      60
bool haptic_beats(float bpm, float secs)
{
    pulse_beats_t plan;
    if (!pulse_beats_plan(bpm, secs, &plan)) return false;
    if (!s_present) return motor_beats(bpm, secs);  /* THE BARE MOTOR's envelope */
    uint8_t seq[8] = { 0 };
    int n = plan.n > HEARTBEAT_DRV_MAX ? HEARTBEAT_DRV_MAX : plan.n, k = 0;
    uint32_t wait = plan.period_ms > DRV_BEAT_MS ? plan.period_ms - DRV_BEAT_MS : 10;
    if (wait > 1270) wait = 1270;
    for (int i = 0; i < n; i++) {
        seq[k++] = DRV_BEAT_EFFECT;
        if (i + 1 < n) seq[k++] = WAIT(wait);
    }
    if (!s_awake) { wr(REG_MODE, MODE_TRIGGER); s_awake = true; delayMicroseconds(300); }
    bus2_wr(ADDR, REG_WAVSEQ, seq, 8);
    wr(REG_GO, 1);
    s_played_ms = millis();
    s_plays++;
    return true;
}

void haptic_tick(void)
{
    if (!s_present || !s_awake) return;
    if (millis() - s_played_ms > STANDBY_AFTER_MS && !(rd(REG_GO) & 1)) {
        wr(REG_MODE, MODE_STANDBY);
        s_awake = false;
    }
}

/* the auto-calibration: the chip drives the LRA, finds its resonance and
 * back-EMF, and leaves the compensation in 0x18/0x19 and the BEMF gain
 * in 0x1A -- kept in NVS and re-applied at every boot. About a second
 * of the LRA humming. */
void haptic_calibrate(void)
{
    if (!s_present) { Serial.println("haptic: absent"); return; }
    wr(REG_MODE, MODE_AUTOCAL);
    wr(REG_GO, 1);
    uint32_t t0 = millis();
    while ((rd(REG_GO) & 1) && millis() - t0 < 1500) delay(10);
    bool failed = (rd(REG_STATUS) & 0x08) != 0;
    uint8_t comp = rd(REG_CAL_COMP), bemf = rd(REG_CAL_BEMF), fb = rd(REG_FEEDBACK);
    wr(REG_MODE, MODE_STANDBY);
    s_awake = false;
    if (failed) { Serial.printf("haptic: calibration FAILED (comp 0x%02X bemf 0x%02X fb 0x%02X) -- the LRA's rated/clamp numbers, or no LRA\n", comp, bemf, fb); return; }
    store.haptic_cal[0] = comp; store.haptic_cal[1] = bemf; store.haptic_cal[2] = fb & 0x03;
    store_save_haptic_cal();
    Serial.printf("haptic: calibrated in %lu ms -- comp 0x%02X bemf 0x%02X bemf-gain %u (stored)\n",
                  (unsigned long)(millis() - t0), comp, bemf, fb & 0x03);
}

void haptic_status(char *out, int cap)
{
    if (!s_present) { snprintf(out, cap, "haptic: absent (bom %s)", bus_on_bom(BOM_HAPTIC) ? "on" : "off"); return; }
    snprintf(out, cap, "haptic: DRV2605%s at 0x%02X, %s, %lu played, cal %s (comp 0x%02X bemf 0x%02X); kinds %s|%s|%s",
             s_id == 7 ? "L" : "", ADDR, s_awake ? "awake" : "standby", (unsigned long)s_plays,
             (store.haptic_cal[0] || store.haptic_cal[1]) ? "stored" : "NEVER RUN", store.haptic_cal[0], store.haptic_cal[1],
             KIND_NAMES[0], KIND_NAMES[1], KIND_NAMES[2]);
}
