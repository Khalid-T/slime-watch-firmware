#include "bus.h"
#include "store.h"
#include "app.h"
#include "haptic.h"
#include "motor.h"
#include "gauge.h"
#include "rtc.h"
#include "ppg.h"
#include "imu.h"        /* THE BUS UNSTUCK: the recovery re-inits both of bus 0's chips */
#include "touch.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <stdlib.h>

static bool s_wire1 = false;

/* --- the second bus ---------------------------------------------------------- */

static void wire1_up(void)
{
    if (s_wire1) return;
    Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL, BUS2_HZ);
    Wire1.setTimeOut(20);
    s_wire1 = true;
}

static void wire1_down(void)
{
    if (!s_wire1) return;
    Wire1.end();
    s_wire1 = false;
}

/* --- THE BUS UNSTUCK (T4, 2026-10-12): bus 0, the touch and the IMU ------- */

static unstick_t s_bus0 = { false, false, 0, 0, 0, 0, 0, 0, 0, UNSTICK_WAIT_MS, 0 };   /* unstick_init's zeros; the ladder at its first rung */

const unstick_t *bus0_state(void) { return &s_bus0; }

bool bus0_unstick(void)
{
    /* GPIO 6/7 are driven by hand for ~0.1 ms here. Never while a touch
     * read is in flight: touch_read, imu_read and this all run from the
     * loop thread (sense(), then touch_tick(); no task touches Wire), so
     * none is in flight when sense() calls down to here. */
    Wire.end();
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);               /* SDA released: the stuck slave's to drive */
    pinMode(PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);
    digitalWrite(PIN_I2C_SCL, HIGH);
    delayMicroseconds(5);
    for (int i = 0; i < 9; i++) {                      /* nine clocks: the byte it was stuck in, finished */
        digitalWrite(PIN_I2C_SCL, LOW);  delayMicroseconds(5);
        digitalWrite(PIN_I2C_SCL, HIGH); delayMicroseconds(5);
    }
    pinMode(PIN_I2C_SDA, OUTPUT_OPEN_DRAIN);           /* a STOP: SDA low, then high, with SCL high */
    digitalWrite(PIN_I2C_SDA, LOW);  delayMicroseconds(5);
    digitalWrite(PIN_I2C_SDA, HIGH); delayMicroseconds(5);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, BUS0_HZ);
    Wire.setTimeOut(BUS0_TIMEOUT_MS);
    uint8_t ia = imu_begin();                          /* re-entrant: probes afresh, the gyro / the engine back */
    touch_reset();                                     /* a RST pulse, the auto-sleep forbidden again (55 ms) */
    return ia != 0;
}

void bus0_note(bool imu_answered, bool touch_alive)
{
    uint32_t now = millis();
    bool was_pending = s_bus0.pending;
    unstick_touch(&s_bus0, touch_alive, now);
    if (!unstick_note(&s_bus0, imu_answered, now)) {
        if (was_pending && imu_answered)
            Serial.printf("[%8lu] bus0: the IMU answers again (recovery %u good)\n", (unsigned long)now, (unsigned)s_bus0.tries);
        return;
    }
    unsigned dead = s_bus0.dead, failed = s_bus0.failed;
    uint32_t silent_s = unstick_silent_s(&s_bus0, now);
    bool found = bus0_unstick();
    unstick_tried(&s_bus0, now);
    Serial.printf("[%8lu] bus0: the IMU unanswered %u reads in a row (silent %lu s) -- unstuck: nine clocks and a STOP, the bus up again, the IMU %s, the touch reset (recovery %u, %u failed before; the next in %lu s if it stays silent)\n",
                  (unsigned long)now, dead, (unsigned long)silent_s, found ? "found" : "NOT FOUND",
                  (unsigned)s_bus0.tries, failed, (unsigned long)(s_bus0.wait_ms / 1000));
}

bool bus2_wr(uint8_t addr, uint8_t reg, const uint8_t *v, uint8_t n)
{
    if (!s_wire1) return false;
    Wire1.beginTransmission(addr);
    Wire1.write(reg);
    if (n) Wire1.write(v, n);
    return Wire1.endTransmission() == 0;
}

bool bus2_wr1(uint8_t addr, uint8_t reg, uint8_t v) { return bus2_wr(addr, reg, &v, 1); }

bool bus2_rd(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n)
{
    if (!s_wire1) return false;
    Wire1.beginTransmission(addr);
    Wire1.write(reg);
    if (Wire1.endTransmission(false) != 0) return false;
    if (Wire1.requestFrom((int)addr, (int)n) != (int)n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire1.read();
    return true;
}

uint8_t bus2_rd1(uint8_t addr, uint8_t reg)
{
    uint8_t v = 0;
    bus2_rd(addr, reg, &v, 1);
    return v;
}

bool bus2_ack(uint8_t addr)
{
    if (!s_wire1) return false;
    Wire1.beginTransmission(addr);
    return Wire1.endTransmission() == 0;
}

/* --- the BOM --------------------------------------------------------------- */

typedef struct { uint8_t bit; const char *name; const char *part; } bom_row_t;
static const bom_row_t BOM[] = {
    { BOM_HAPTIC, "haptic", "DRV2605 + LRA 0x5A" },
    { BOM_GAUGE,  "gauge",  "MAX17048 0x36" },
    { BOM_RTC,    "rtc",    "PCF85063 0x51" },
    { BOM_PPG,    "ppg",    "MAX3010x 0x57 (the 30102 on the bench)" },
    { BOM_BUTTON, "button", "the case's button, GPIO 15" },
    { BOM_MOTOR,  "motor",  "a bare coin motor, GPIO 33 to GND" },
};
#define N_BOM ((int)(sizeof BOM / sizeof BOM[0]))

bool bus_on_bom(uint8_t bit) { return (store.bom & bit) != 0; }

bool bus_present(uint8_t bit)
{
    if (!bus_on_bom(bit)) return false;
    switch (bit) {
    case BOM_HAPTIC: return haptic_present();
    case BOM_GAUGE:  return gauge_present();
    case BOM_RTC:    return rtc_present();
    case BOM_PPG:    return ppg_present();
    case BOM_BUTTON: return true;
    case BOM_MOTOR:  return motor_present();
    }
    return false;
}

/* one part: the pins it owns, then its probe */
static void part_begin(uint8_t bit)
{
    if (bit & BOM_I2C_ANY) wire1_up();
    switch (bit) {
    case BOM_HAPTIC: haptic_begin(); break;                              /* no interrupt pin */
    case BOM_GAUGE:  pinMode(PIN_INT_GAUGE, INPUT_PULLUP); gauge_begin(); break;
    case BOM_RTC:    pinMode(PIN_INT_RTC, INPUT_PULLUP); rtc_begin(); break;
    case BOM_PPG:    if (!bus_on_bom(BOM_MOTOR)) pinMode(PIN_INT_PPG, INPUT_PULLUP); ppg_begin(); break;   /* the INT is unwired (polled); the motor may hold 33 */
    case BOM_BUTTON: pinMode(PIN_BUTTON, INPUT_PULLUP); LOG("bus: the button on GPIO %d", PIN_BUTTON); break;
    case BOM_MOTOR:  motor_begin(); break;                               /* GPIO 33 as PWM out */
    }
}

static void part_end(uint8_t bit)
{
    switch (bit) {
    case BOM_HAPTIC: haptic_end(); break;
    case BOM_GAUGE:  gauge_end(); pinMode(PIN_INT_GAUGE, INPUT); break;
    case BOM_RTC:    rtc_end(); pinMode(PIN_INT_RTC, INPUT); break;
    case BOM_PPG:    ppg_end(); if (!bus_on_bom(BOM_MOTOR)) pinMode(PIN_INT_PPG, INPUT); break;
    case BOM_BUTTON: pinMode(PIN_BUTTON, INPUT); break;
    case BOM_MOTOR:  motor_end(); break;
    }
    if (!(store.bom & BOM_I2C_ANY)) wire1_down();
}

void bus_begin(void)
{
    if (!store.bom) {
        LOG("bus: no add-ons on the BOM (`bom <part> on` the day one is soldered; `bus` scans); GPIO %d %d %d %d %d %d reserved",
            PIN_I2C2_SDA, PIN_I2C2_SCL, PIN_INT_RTC, PIN_INT_GAUGE, PIN_INT_PPG, PIN_BUTTON);
        return;
    }
    if (store.bom & BOM_I2C_ANY) {
        wire1_up();
        LOG("bus: Wire1 on SDA %d SCL %d at %d kHz", PIN_I2C2_SDA, PIN_I2C2_SCL, BUS2_HZ / 1000);
    }
    for (int i = 0; i < N_BOM; i++)
        if (store.bom & BOM[i].bit) part_begin(BOM[i].bit);
}

void bus_tick(void)
{
    if (!store.bom) return;
    haptic_tick();
    motor_tick();
    rtc_tick();
    ppg_tick();
}

bool bus_button_down(void)
{
    if (bus_on_bom(BOM_BUTTON)) return digitalRead(PIN_BUTTON) == LOW;
    return digitalRead(PIN_BOOT) == LOW;
}

/* --- the scan ---------------------------------------------------------------- */

static const char *addr_name(uint8_t a)
{
    switch (a) {
    case ADDR_CST816S:     return "CST816S touch";
    case ADDR_QMI8658:     return "QMI8658 imu";
    case ADDR_QMI8658_ALT: return "QMI8658 imu (SA0 low)";
    case 0x36: return "MAX17048 gauge";
    case 0x51: return "PCF85063 rtc";
    case 0x57: return "MAX3010x ppg";
    case 0x5A: return "DRV2605 haptic";
    default:   return "?";
    }
}

static int scan_one(TwoWire &w, const char *label)
{
    int n = 0;
    Serial.printf("bus %s:", label);
    for (uint8_t a = 0x08; a < 0x78; a++) {
        w.beginTransmission(a);
        if (w.endTransmission() != 0) continue;
        Serial.printf(" 0x%02X=%s", a, addr_name(a));
        n++;
    }
    Serial.println(n ? "" : " (nothing answers)");
    return n;
}

void bus_scan(void)
{
    scan_one(Wire, "0 (SDA 6 SCL 7)");
    bool was = s_wire1;
    wire1_up();
    char lbl[40]; snprintf(lbl, sizeof lbl, "1 (SDA %d SCL %d)", PIN_I2C2_SDA, PIN_I2C2_SCL);
    int n = scan_one(Wire1, lbl);
    if (!was) wire1_down();
    if (n && !(store.bom & BOM_I2C_ANY))
        Serial.println("bus: a part answers on bus 1 but the BOM says none is soldered (`bom <part> on`)");
}

/* the bench's second question (2026-09-13, both lines pulled high and no
   ACK): is it the wires crossed, or a joint that passes DC but not the
   clock? Bus 1 scanned again at 100 kHz on the pins as wired, then with
   SDA and SCL swapped -- a part that answers on the swap has its wires
   crossed at the module; one that answers only slow has a poor joint. */
static void bus_try(void)
{
    bool was = s_wire1;
    wire1_down();
    Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL, 100000);
    Wire1.setTimeOut(20);
    char la[48]; snprintf(la, sizeof la, "1 slow (SDA %d SCL %d at 100 kHz)", PIN_I2C2_SDA, PIN_I2C2_SCL);
    int a = scan_one(Wire1, la);
    Wire1.end();
    Wire1.begin(PIN_I2C2_SCL, PIN_I2C2_SDA, 100000);
    Wire1.setTimeOut(20);
    char lb[52]; snprintf(lb, sizeof lb, "1 SWAPPED (SDA %d SCL %d at 100 kHz)", PIN_I2C2_SCL, PIN_I2C2_SDA);
    int b = scan_one(Wire1, lb);
    Wire1.end();
    pinMode(PIN_I2C2_SDA, INPUT);
    pinMode(PIN_I2C2_SCL, INPUT);
    if (b && !a) Serial.println("bus: it answers on the SWAP -- SDA and SCL are crossed at the module: move the two wires");
    else if (a) Serial.println("bus: it answers only slow -- a joint or a wire is marginal at 400 kHz");
    else Serial.println("bus: silent both ways at 100 kHz -- the lines are high but no part acknowledges: a joint on SDA or SCL, the module's jumper, or a dead chip");
    if (was) wire1_up();
}

/* the bench's line test (2026-09-13, the PPG soldered and still silent): drop
   the bus, read SDA and SCL as plain inputs and then with the chip's weak
   pull-ups. Floating (0 bare, 1 pulled): nothing pulls the line up -- the
   module is unpowered, or this wire never reaches it. Held LOW (0, 0): a
   short to GND, or an unpowered part clamping the line. HIGH both (1, 1):
   a pull-up is on the line -- the module is powered and the wire reaches it
   (then a silent scan is SDA/SCL swapped, or the jumper on the 1V8 side). */
/* THE FIND (2026-09-24, loose wires on the header: "check other pins"):
   every header GPIO read against the S3's pull-down -- a pin an external
   pull-up holds HIGH is one the module's SDA or SCL actually lands on --
   then every ordered pair of the held-high pins scanned as SDA/SCL at
   100 kHz. The motor's pin and a rail are released for the test and put
   back after. Answers the question "which pins are the wires really on". */
static void bus_find(void)
{
    bool was = s_wire1;
    wire1_down();
    const int pins[6] = {15, 16, 17, 18, 21, 33};
    int held[6]; int nh = 0;
    for (int i = 0; i < 6; i++) {
        pinMode(pins[i], INPUT_PULLDOWN); delay(3);
        int d = digitalRead(pins[i]);
        pinMode(pins[i], INPUT_PULLUP); delay(3);
        int u = digitalRead(pins[i]);
        pinMode(pins[i], INPUT);
        Serial.printf("bus: GPIO %2d pulled-down %d pulled-up %d -- %s\n", pins[i], d, u,
                      d && u ? "HELD HIGH by something outside (a pull-up: a module line lands here)" : !d && !u ? "held LOW outside" : "floating (nothing on it)");
        if (d && u) held[nh++] = pins[i];
    }
    int found = 0;
    for (int i = 0; i < nh; i++) for (int j = 0; j < nh; j++) {
        if (i == j) continue;
        Wire1.begin(held[i], held[j], 100000);
        Wire1.setTimeOut(20);
        char name[48]; snprintf(name, sizeof name, "find (SDA %d SCL %d)", held[i], held[j]);
        if (scan_one(Wire1, name)) { found++; Serial.printf("bus: >>> it ANSWERS with SDA on GPIO %d and SCL on GPIO %d\n", held[i], held[j]); }
        Wire1.end();
        pinMode(held[i], INPUT); pinMode(held[j], INPUT);
    }
    if (nh < 2) Serial.println("bus: fewer than two pins are held high -- the module's SDA/SCL wires do not reach the header (or the module is unpowered)");
    else if (!found) Serial.println("bus: pins are held high but no pair answers -- the lines reach the header but the chip does not ACK (a joint, the jumper)");
    if (was) wire1_up();
    if (bus_on_bom(BOM_MOTOR)) pinMode(PIN_MOTOR, OUTPUT);   /* the motor's pin back as an output (its LEDC is re-attached by its next play) */
}

static void bus_lines(void)
{
    bool was = s_wire1;
    wire1_down();
    const int pins[2] = {PIN_I2C2_SDA, PIN_I2C2_SCL};
    const char *names[2] = {"SDA", "SCL"};
    for (int i = 0; i < 2; i++) {
        pinMode(pins[i], INPUT);
        delay(2);
        int bare = digitalRead(pins[i]);
        pinMode(pins[i], INPUT_PULLUP);
        delay(2);
        int pulled = digitalRead(pins[i]);
        pinMode(pins[i], INPUT_PULLDOWN);   /* THE PULL-DOWN READ (2026-09-24): a floating pin reads 1 bare, so the bare read proves nothing; the module's 4.7 k pull-up beats the S3's ~45 k pull-down -- a wired, powered line stays 1 here, a floating one falls to 0 */
        delay(2);
        int down = digitalRead(pins[i]);
        pinMode(pins[i], INPUT);
        const char *verdict = (down && pulled) ? "HIGH against the pull-down: a real pull-up is on the line -- the module is powered and this wire reaches it"
                            : (!bare && !pulled) ? "held LOW: a short to GND, or an unpowered part clamping it"
                            : (!down && pulled) ? "FLOATING: nothing pulls it up (it fell to the pull-down) -- the module unpowered (VIN/GND/the 1V8-3V3 jumper), or this wire not reaching it"
                            : "odd: high bare, low with the pull-up";
        Serial.printf("bus: %s (GPIO %d) bare %d pulled %d pulled-down %d -- %s\n", names[i], pins[i], bare, pulled, down, verdict);
    }
    if (was) wire1_up();
}

/* --- the console ------------------------------------------------------------- */

static void bom_list(void)
{
    Serial.printf("bom: 0x%02X --", store.bom);
    for (int i = 0; i < N_BOM; i++)
        Serial.printf(" %s=%s", BOM[i].name,
                      !(store.bom & BOM[i].bit) ? "off" : bus_present(BOM[i].bit) ? "FOUND" : "absent");
    Serial.println();
    Serial.println("     `bom <haptic|gauge|rtc|ppg|button|motor> on|off` (NVS; applied now)");
}

bool bus_cmd(int argc, char **argv)
{
    const char *v = argv[0];
    if (!strcmp(v, "bom")) {
        if (argc < 3) { bom_list(); return true; }
        const bom_row_t *row = NULL;
        for (int i = 0; i < N_BOM; i++) if (!strcmp(argv[1], BOM[i].name)) row = &BOM[i];
        if (!row) { Serial.println("bom: haptic | gauge | rtc | ppg | button | motor"); return true; }
        bool on = !strcmp(argv[2], "on");
        if (!on && strcmp(argv[2], "off")) { Serial.println("bom <part> on|off"); return true; }
        if (on && !(store.bom & row->bit)) { store.bom |= row->bit; store_save_bom(); part_begin(row->bit); }
        else if (!on && (store.bom & row->bit)) { store.bom &= ~row->bit; store_save_bom(); part_end(row->bit); }
        Serial.printf("bom: %s %s (%s) -- stored\n", row->name, on ? "on" : "off", row->part);
        return true;
    }
    if (!strcmp(v, "bus")) {
        if (argc > 1 && !strcmp(argv[1], "lines")) bus_lines();
        else if (argc > 1 && !strcmp(argv[1], "try")) bus_try();
        else if (argc > 1 && !strcmp(argv[1], "find")) bus_find();
        else bus_scan();
        return true;
    }
    if (!strcmp(v, "haptic")) {
        if (!bus_on_bom(BOM_HAPTIC) && !bus_on_bom(BOM_MOTOR)) { Serial.println("haptic: not on the BOM (`bom haptic on` for the DRV2605, `bom motor on` for the bare motor)"); return true; }
        if (argc < 2) {
            char l[200];
            if (bus_on_bom(BOM_HAPTIC)) { haptic_status(l, sizeof l); Serial.println(l); }
            if (bus_on_bom(BOM_MOTOR)) { motor_status(l, sizeof l); Serial.println(l); }
            return true;
        }
        if (!strcmp(argv[1], "cal")) { haptic_calibrate(); return true; }
        if (!strcmp(argv[1], "buzz")) {       /* the bench test, the bare motor only: never the creature's */
            uint32_t ms = argc > 2 ? (uint32_t)atoi(argv[2]) : 300;
            int pct = argc > 3 ? atoi(argv[3]) : 100;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            uint32_t hz = argc > 4 ? (uint32_t)atoi(argv[4]) : 8000;
            Serial.printf("haptic: buzz %lu ms at %d %% %lu Hz %s\n", (unsigned long)ms, pct, (unsigned long)hz, motor_buzz(ms, (uint8_t)pct, hz) ? "playing" : "NOT played (no motor)");
            return true;
        }
        if (!strcmp(argv[1], "off")) { motor_off(); Serial.println("haptic: off"); return true; }
        if (!strcmp(argv[1], "hold")) {       /* the multimeter's test: GPIO 33 plain HIGH */
            uint32_t s = argc > 2 ? (uint32_t)atoi(argv[2]) : 20;
            Serial.printf("haptic: hold GPIO %d HIGH for %lu s %s\n", PIN_MOTOR, (unsigned long)(s > 60 ? 60 : s), motor_hold(s) ? "-- measure it" : "NOT held (no motor on the BOM)");
            return true;
        }
        int kind = !strcmp(argv[1], "bump") ? HAPTIC_BUMP : !strcmp(argv[1], "purr") ? HAPTIC_PURR :
                   !strcmp(argv[1], "hop") ? HAPTIC_HOP3 : -1;
        if (kind < 0) { Serial.println("haptic bump|purr|hop | haptic cal | haptic buzz [<ms> [<pct> [<hz>|0=dc]]] | haptic hold [<s>] | haptic off"); return true; }
        Serial.printf("haptic: %s %s\n", argv[1], haptic_play(kind) ? "played" : "NOT played (absent)");
        return true;
    }
    if (!strcmp(v, "gauge")) {
        if (!bus_on_bom(BOM_GAUGE)) { Serial.println("gauge: not on the BOM (`bom gauge on`)"); return true; }
        char l[160]; gauge_status(l, sizeof l); Serial.println(l);
        return true;
    }
    if (!strcmp(v, "rtc")) {
        if (!bus_on_bom(BOM_RTC)) { Serial.println("rtc: not on the BOM (`bom rtc on`)"); return true; }
        if (argc > 1 && !strcmp(argv[1], "sync")) {
            if (!clock_is_set()) { Serial.println("rtc: the clock is unset -- nothing to write"); return true; }
            Serial.printf("rtc: %s\n", rtc_set((uint32_t)time(NULL)) ? "written from the clock" : "NOT written");
            return true;
        }
        char l[160]; rtc_status(l, sizeof l); Serial.println(l);
        return true;
    }
    if (!strcmp(v, "ppg")) {
        if (argc > 1 && !strcmp(argv[1], "rail")) {     /* a header GPIO as the module's VIN (bench 2026-09-13; ppg.cpp rail_up) */
            if (argc < 3) { Serial.printf("ppg rail <gpio>|off (now %s%d)\n", store.ppg_rail ? "GPIO " : "off", store.ppg_rail); return true; }
            int pin = !strcmp(argv[2], "off") ? 0 : atoi(argv[2]);
            if (pin && pin != 16) { Serial.println("ppg rail: 16 only (the header's one spare GPIO; 15 is the button's, 33 the motor's, 21/18 the bus, 17 a dead lane on this board -- and a GPIO could not feed the module anyway, 2026-09-25: 0.41 V)"); return true; }
            if (bus_on_bom(BOM_PPG)) part_end(BOM_PPG);
            store.ppg_rail = (uint8_t)pin; store_save_ppg_rail();
            if (bus_on_bom(BOM_PPG)) part_begin(BOM_PPG);
            Serial.printf("ppg: rail %s%d -- stored%s\n", pin ? "GPIO " : "off", pin, pin ? " (wire the module's VIN to it; GND to GND)" : "");
            return true;
        }
        if (!bus_on_bom(BOM_PPG)) { Serial.println("ppg: not on the BOM (`bom ppg on`)"); return true; }
        if (argc > 1 && !strcmp(argv[1], "raw")) {     /* the raw stream for the laptop's arithmetic (tools/ppg_raw.py) */
            int secs = argc > 2 ? atoi(argv[2]) : 30;
            Serial.printf("ppg: %s\n", ppg_raw_start(secs) ? "the agc, then red+ir raw at 100 Hz -- finger on, hold still" : "NOT started (absent or busy)");
            return true;
        }
        if (argc > 1 && !strcmp(argv[1], "spot")) {       /* `ppg spot [secs]`: 5..30 (2026-10-03: THE PULSE ROW's 30 from the console too) */
            int secs = argc > 2 ? atoi(argv[2]) : PPG_SPOT_S;
            secs = secs < 5 ? 5 : secs > PPG_SPOT_MAX_S ? PPG_SPOT_MAX_S : secs;
            bool went = ppg_spot_start_s(secs);
            Serial.printf("ppg: %s\n", went ? "a spot read -- hold still" : "NOT started (absent or busy)");
            if (went) Serial.printf("ppg: %d s (after the agc)\n", secs);
            return true;
        }
        if (argc > 1 && !strcmp(argv[1], "auto")) {    /* THE PULSE IN THE DAY (2026-10-08): the creature's own 10 s read every 5 min at rest, on|off (NVS) */
            if (argc < 3) { Serial.printf("ppg auto on|off (now %s): a %d s read every %d min at rest, by itself -- the day log's pulse and the ring\n", store.ppg_auto_off ? "OFF" : "on", PPG_AUTO_S, PPG_AUTO_EVERY_MIN); return true; }
            bool off = !strcmp(argv[2], "off");
            bool changed = off != store.ppg_auto_off;
            store.ppg_auto_off = off;
            if (changed) store_save_ppg_auto();
            Serial.printf("ppg: the pulse in the day %s%s\n", off ? "OFF -- it reads only when asked (the row, `ppg spot`)" : "on -- a read of its own every 5 min at rest, never on the cable or the shelf", changed ? " (stored)" : "");
            return true;
        }
        if (argc > 1 && !strcmp(argv[1], "led")) {     /* which LED the spot reads by: ir (the 30102), green (a 30101), red */
            int led = argc > 2 ? (!strcmp(argv[2], "ir") ? PPG_LED_IR : !strcmp(argv[2], "green") ? PPG_LED_GREEN : !strcmp(argv[2], "red") ? PPG_LED_RED : -1) : -1;
            if (led < 0) { Serial.printf("ppg led ir|green|red (now %s)\n", ppg_led_name(store.ppg_led)); return true; }
            store.ppg_led = (uint8_t)led; store_save_ppg_led();
            Serial.printf("ppg: led %s -- stored (the next spot uses it)\n", ppg_led_name(led));
            return true;
        }
        char l[400]; ppg_status(l, sizeof l); Serial.println(l);   /* the ring's tail rides the line (2026-10-08) */
        return true;
    }
    return false;
}
