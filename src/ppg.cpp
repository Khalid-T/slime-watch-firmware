#include "ppg.h"
#include "agc.h"
#include "still.h"
#include "bus.h"
#include "store.h"
#include "app.h"
#include "daylog.h"               /* THE PULSE IN THE DAY: the minute's hr / hrv (wrist_note_pulse), worn */
#include "rest.h"                 /* THE SHELF RULE: a still run judged not worn is nobody's pulse */
#include "link.h"                 /* the dock (link_docked): on the cable, on a desk */
#include "spo2.h"                 /* THE RED AND THE INFRARED (2026-10-13, T19): the two-slot stream's ratio, uncalibrated; the night's lowest */
#include "perfusion.h"            /* THE PERFUSION INDEX (2026-10-14): the read's own quality, said by name; the day's tally and median */
#include "resp.h"                 /* THE BREATH READ (2026-10-14): the respiration rate off the ir's slow modulations; the night's median */
#include "beat.h"                 /* THE HONEST BEAT (2026-10-14): the beat counter -- the refractory after a beat, the alternation merged, the rest gate */
#include <Arduino.h>
#include <driver/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ADDR            0x57
#define REG_INT_STATUS1 0x00
#define REG_INT_ENABLE1 0x02
#define REG_FIFO_WR     0x04
#define REG_FIFO_OVF    0x05
#define REG_FIFO_RD     0x06
#define REG_FIFO_DATA   0x07
#define REG_FIFO_CONFIG 0x08   /* [7:5] sample average, bit4 rollover, [3:0] almost-full */
#define REG_MODE        0x09   /* bit7 shutdown, bit6 reset, [2:0] 2 HR (red), 3 SpO2, 7 multi-LED */
#define REG_SPO2_CONFIG 0x0A   /* [6:5] ADC range, [4:2] rate, [1:0] pulse width */
#define REG_LED1_PA     0x0C   /* red, 0.2 mA / LSB */
#define REG_LED2_PA     0x0D   /* IR */
#define REG_LED3_PA     0x0E   /* green (the 30101 only; the 30102 has no third LED) */
#define REG_MULTI_LED1  0x11   /* slot 1 [2:0], slot 2 [6:4]: 1 red, 2 IR, 3 green */
#define REG_MULTI_LED2  0x12
#define REG_DIE_TINT    0x1F   /* THE WARM SKIN (warm.c): the die temperature, a signed integer C ... */
#define REG_DIE_TFRAC   0x20   /* ... and its sixteenths (bits [3:0]) */
#define REG_DIE_CONFIG  0x21   /* bit0 TEMP_EN: starts one conversion, self-clearing when it is done (~29 ms) */
#define DIE_TEMP_WAIT_MS 50    /* the conversion polled this long at most (the ~29 ms plus a margin); past it, no reading */
#define REG_REV_ID      0xFE
#define REG_PART_ID     0xFF   /* 0x15 */

/* the spot's setup: 100 Hz, 4 averaged -> 25 samples/s; 18-bit (411 us);
 * ADC range 16384 nA; the LED at ~12.6 mA (0x3F). THE UNO BENCH
 * (2026-09-23, tools/ppg-uno/): 0x7F (25 mA) with the 4096 nA range
 * SATURATES under a finger (262143 every sample) and 0x24 (7 mA) is too
 * dim to see the beat over the drift; 0x3F + the widest range showed a
 * clean ~58 bpm pulse, the AC 1.6 % of a 53 k level. The FIFO holds 32
 * samples = 1.3 s at that rate -- any tick cadence drains it in time,
 * THE NAP's slowest poll (1 s after 30 min still, wake.cpp) included. */
#define FIFO_CONFIG_V   0x50   /* avg 4 (010), rollover */
#define SPO2_CONFIG_V   0x67   /* 11 range 16384 nA, 001 100 Hz, 11 411 us 18-bit */
#define LED_PA          AGC_PA_START   /* the AGC's first current; agc.h picks the rest */
#define AGC_WINDOW      30             /* samples per AGC read (1.2 s at 25/s) */
#define RATE_NOMINAL    25.0f

#define BUF_MAX         (PPG_SPOT_MAX_S * 30) /* 900 samples: room past the nominal 750 of the longest read (THE PULSE ROW's 30 s) */
#define DC_MIN          8000                  /* under this the green sees no skin (open air) */
/* REFRACT_S 0.33 (180 bpm) and INTERVAL_MAX_S 1.5 (40 bpm) live in beat.h since THE HONEST BEAT (BEAT_REFRACT_S, BEAT_INTERVAL_MAX_S) */
#define BEATS_MIN       6
#define CV_MAX          0.25f                 /* the intervals' spread over their mean: more = motion or noise */
/* THE PULSE IN THE DAY: a refused try (the cable, the shelf, no clock) is said once, then asked again after this
 * (not every frame: the gate's line would flood the log) */
#define AUTO_RETRY_S    60u

static bool s_present = false;
static uint8_t s_rev = 0;
static bool s_busy = false;
static int32_t *s_buf = NULL;
static int s_n = 0;
static uint32_t s_t0 = 0;
static bool s_have = false, s_ok = false;
static float s_bpm = 0, s_rmssd = 0;
static uint32_t s_spots = 0;
static uint32_t s_done = 0;             /* HIS spots finished (the raw stream and the creature's own not counted): main reads the change */
static int s_spot_s = PPG_SPOT_S;       /* this spot's length */
static char s_why[88] = "";             /* 56 -> 72 (2026-10-14): room for THE PERFUSION INDEX's word before the signal's own reason; -> 88: THE HONEST BEAT's `doubled beats? (138 bpm, rmssd 154 ms at rest)` behind it */
static float s_die_c = NAN;             /* THE WARM SKIN (warm.c): the die's temperature read at the END of each spot (his or the creature's own), NAN until one; main takes each by s_die_seq */
static uint32_t s_die_seq = 0;
/* THE RAW STREAM (2026-09-25, "how much info can you get from it"): after the AGC
 * settles, red AND ir together (two slots) at 100 Hz, no averaging, every
 * sample a `raw <i> <red> <ir>` line on the serial port for s_raw_s seconds --
 * the laptop does the arithmetic (tools/ppg_raw.py: HRV, breathing, perfusion,
 * a rough SpO2, the pulse shape). Nothing is kept on the watch. */
static bool s_raw = false;
static int s_raw_s = 0;
static uint32_t s_raw_n = 0;
static int s_slots = 1;                 /* FIFO slots in use: the spot's one LED, the raw stream's two (THE AGC PER LED judges both) */

/* THE STILL SPOT (still.h): one arbiter for whichever read is running */
static still_t s_still;
static bool s_asked = false;            /* the running read is HIS (the row's, the console's): the row counts it, ppg_spot() gets it */
static bool s_lit = false;              /* the LEDs are on (the read proper, or the AGC) */
static bool s_lit_once = false;         /* the registers were set up once this read (a relight after a restart keeps them) */
static float s_quiet_s = 0;             /* the wrist still (under STILL_JERK_G) this long, as main feeds it -- THE PULSE IN THE DAY starts on it */
static uint32_t s_feed_ms = 0;          /* ppg_dark_poll's own dt clock */

/* THE PULSE IN THE DAY: the ring, the tallies, the gap */
typedef struct { uint8_t bpm, rmssd; int16_t minute; uint32_t at_ms; } recent_t;
static recent_t s_recent[PPG_RECENT_N];
static int s_recent_head = 0, s_recent_n = 0;
static uint32_t s_recent_seq = 0;        /* +1 per push: the breath and the gentle wake take a new result once (THE MERGE, 2026-10-09) */
static uint32_t s_auto_last_ms = 0;     /* the last START of the creature's own read (the gap counts from it) */
static uint32_t s_auto_tries = 0, s_auto_clean = 0, s_auto_failed = 0;
static uint32_t s_auto_gate_ms = 0;     /* the last refused try (AUTO_RETRY_S before another) */
static uint32_t s_rest_doubled = 0;     /* THE HONEST BEAT: the creature's own reads refused today as doubled beats (rule 3); cleared at the day's close with the perfusion tally */
static char s_auto_gate[40] = "";       /* why the last try was refused ("" = it ran) */

/* THE RED AND THE INFRARED (spo2.h): the ratio's window over whichever two-slot stream runs (the raw stream at
 * 100 Hz, the creature's own read at 25 Hz -- red lit beside its ir since 2026-10-13); the night keeper; the
 * sleep span last seen end, so the span's end is caught once. A few dozen bytes, no buffer. */
static spo2_t s_spo2;
static spo2_night_t s_spo2_night;
static uint32_t s_spo2_span_row = 0;    /* rest_last_span's end_row when last looked at */
static bool s_spo2_span_seen = false;   /* the first look only learns the row (a replayed span is no night of tonight's) */
static uint32_t s_spo2_look_ms = 0;     /* the span check's own clock: once a second */

/* THE PERFUSION INDEX (perfusion.h, 2026-10-14): the last read's index in percent (-1 = not measured: too few samples,
 * or the LEDs never lit), the day's tally per band + the median's histogram (taken at the day's close by daylog.cpp's
 * close_file), and whether HIS last spot failed for a THIN perfusion (the PULSE row says THIN where it said NONE).
 * ~80 bytes, no buffer. */
static float s_perf_pct = -1;
static perf_day_t s_perf_day;
static bool s_perf_thin = false;

/* THE BREATH READ (resp.h, 2026-10-14): the respiration rate over whichever stream runs, read on its ir (or his
 * asked read's one LED: the breath rides any of them), one sample at a time, closed at the read's end; the night
 * keeper (a 37-byte histogram: the median once a night); the last clean rate for `status` and the gel's road
 * (rest_in_t.breath_bpm -- carried, not followed: ruling 154). A few dozen floats, no buffer. */
static resp_t s_resp;
static resp_night_t s_resp_night;
static float s_resp_bpm = 0, s_resp_conf = 0;   /* the last clean rate (never the raw stream's: a finger on the bench is not his rest) */
static uint32_t s_resp_at_ms = 0;

static bool wr(uint8_t r, uint8_t v) { return bus2_wr1(ADDR, r, v); }
static uint8_t rd(uint8_t r) { return bus2_rd1(ADDR, r); }

static void shutdown_leds(void)
{
    wr(REG_MODE, 0x80);
    s_lit = false;
}

/* THE RAIL (bench 2026-09-13): the watch's FPC adapter passes GND and the
   GPIOs but neither 3V3 nor VSYS, so the module's VIN is fed from a
   header GPIO driven HIGH (store.ppg_rail; `ppg rail 21`). The module
   takes 1.8-5 V into its own 3.3 V regulator; the pin at its strongest
   drive gives ~3.2 V under the module's few mA, the LED pulses ride the
   module's 47 uF. A bench answer -- the real fix is the adapter's rail.
   Raised before the probe (the regulators and the cap need a moment),
   dropped with the part. */
static void rail_up(void)
{
    if (!store.ppg_rail) return;
    pinMode(store.ppg_rail, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)store.ppg_rail, GPIO_DRIVE_CAP_3);
    digitalWrite(store.ppg_rail, HIGH);
    delay(80);
    LOG("ppg: the rail -- GPIO %d HIGH is the module's VIN", store.ppg_rail);
}

static void rail_down(void)
{
    if (!store.ppg_rail) return;
    digitalWrite(store.ppg_rail, LOW);
    pinMode(store.ppg_rail, INPUT);
}

void ppg_begin(void)
{
    s_present = false;
    rail_up();
    uint8_t id = 0;
    if (!bus2_rd(ADDR, REG_PART_ID, &id, 1)) { LOG("ppg: MAX30101 absent at 0x%02X (on the BOM)", ADDR); return; }
    if (id != 0x15) { LOG("ppg: 0x%02X answers but PART_ID is 0x%02X, not a MAX3010x", ADDR, id); return; }
    s_rev = rd(REG_REV_ID);
    wr(REG_MODE, 0x40);                                      /* reset */
    uint32_t t0 = millis();
    while ((rd(REG_MODE) & 0x40) && millis() - t0 < 100) delay(1);
    rd(REG_INT_STATUS1);
    shutdown_leds();
    s_present = true;
    LOG("ppg: MAX3010x rev 0x%02X found at 0x%02X, shut down until a spot read by the %s LED (INT unwired: the FIFO is polled); the pulse in the day %s",
        s_rev, ADDR, ppg_led_name(store.ppg_led), store.ppg_auto_off ? "OFF (`ppg auto on`)" : "on: a 10 s read every 5 min at rest");
}

static void drop_read(void)
{
    if (s_present && s_lit) shutdown_leds();
    if (s_buf) { free(s_buf); s_buf = NULL; }
    s_busy = false; s_raw = false; s_asked = false; s_lit = false;
    still_end(&s_still);
}

void ppg_end(void)
{
    drop_read();
    if (s_present) shutdown_leds();
    s_present = false;
    rail_down();
}

static uint8_t s_pa = AGC_PA_START;   /* THE AGC: the LED current the spot settled on (the one LED) */
static agc2_t s_agc2;                  /* THE AGC PER LED: the raw stream's two (red, ir) */
static int s_agc_steps = 0;
static bool s_agc = false;             /* true while the AGC is still choosing (the samples so far are discarded) */
static char s_agc_why[64];

static void set_pa(uint8_t pa)
{
    int led = store.ppg_led;
    wr(REG_LED1_PA, led == PPG_LED_RED ? pa : 0);
    wr(REG_LED2_PA, led == PPG_LED_IR ? pa : 0);
    wr(REG_LED3_PA, led == PPG_LED_GREEN ? pa : 0);
}

static void set_pa2(void)              /* the two slots' own currents */
{
    wr(REG_LED1_PA, s_agc2.pa[0]); wr(REG_LED2_PA, s_agc2.pa[1]); wr(REG_LED3_PA, 0);
}

static void clear_fifo(void) { wr(REG_FIFO_WR, 0); wr(REG_FIFO_OVF, 0); wr(REG_FIFO_RD, 0); }

bool ppg_present(void) { return s_present; }
bool ppg_spot_busy(void) { return s_busy; }
bool ppg_auto_on(void) { return !store.ppg_auto_off; }

/* the buffer for a read: the samples, then the high-passed copy (or the raw AGC's second slot) */
static bool take_buffer(void)
{
    s_buf = (int32_t *)malloc(2 * BUF_MAX * sizeof *s_buf);
    return s_buf != NULL;
}

/* THE LEDS LIGHT: the registers for a read (the spot's one slot, the raw
 * stream's two), the AGC from its start. Called by the arbiter's word
 * (still_tick -> STILL_LIGHT), at once for the raw stream. */
static void light(void)
{
    s_n = 0;
    wr(REG_FIFO_CONFIG, FIFO_CONFIG_V);
    wr(REG_SPO2_CONFIG, SPO2_CONFIG_V);
    int led = store.ppg_led;
    s_agc = true; s_agc_steps = 0; s_agc_why[0] = 0;
    if (s_raw || (!s_asked && led == PPG_LED_IR)) {         /* THE RED AND THE INFRARED: the creature's own read carries red beside its ir (spo2.h); his asked read keeps his one LED */
        s_slots = 2;
        agc2_begin(&s_agc2, LED_PA, LED_PA);
        set_pa2();
        wr(REG_MULTI_LED1, 0x21);                            /* slot 1 red, slot 2 ir: each judged on its own (THE AGC PER LED) */
    } else {
        s_slots = 1;
        s_pa = LED_PA;
        set_pa(s_pa);
        wr(REG_MULTI_LED1, led == PPG_LED_RED ? 0x01 : led == PPG_LED_IR ? 0x02 : 0x03);   /* slot 1: the one LED; nothing else */
    }
    wr(REG_MULTI_LED2, 0x00);
    clear_fifo();
    wr(REG_MODE, 0x07);                                      /* multi-LED, out of shutdown */
    s_lit = true; s_lit_once = true;
    s_t0 = millis();
}

/* the LEDs again after a restart: the current kept (the AGC's choice, or
 * its progress), the samples from nothing */
static void relight(void)
{
    clear_fifo();
    wr(REG_MODE, 0x07);
    s_lit = true;
    s_n = 0;
    s_t0 = millis();
}

bool ppg_raw_start(int secs)
{
    if (!s_present || s_busy) return false;
    if (!take_buffer()) return false;
    s_raw = true; s_raw_s = secs < 5 ? 5 : secs > 120 ? 120 : secs; s_raw_n = 0;
    s_asked = true; s_busy = true; s_spot_s = PPG_SPOT_S;
    still_end(&s_still);                                     /* the raw stream is the bench's: his finger, the laptop's judgement, no arbiter */
    light();
    s_spots++;
    return true;
}

bool ppg_spot_start(void) { return ppg_spot_start_s(PPG_SPOT_S); }

/* a read begins: the buffer, the arbiter waiting for the wrist; the LEDs stay dark until it says */
static bool begin_read(int secs, bool asked)
{
    if (!take_buffer()) return false;
    s_spot_s = secs < 5 ? 5 : secs > PPG_SPOT_MAX_S ? PPG_SPOT_MAX_S : secs;
    s_n = 0; s_raw = false; s_asked = asked; s_lit = false; s_lit_once = false;
    s_agc = false; s_agc_why[0] = 0;
    s_perf_pct = -1;                                         /* THE PERFUSION INDEX: measured in finish(), or not at all */
    still_begin(&s_still, (float)s_spot_s, PPG_WAIT_STILL_S);
    s_busy = true;
    s_t0 = millis();
    return true;
}

bool ppg_spot_start_s(int secs)
{
    if (!s_present) return false;
    if (s_busy) {
        if (s_raw || s_asked) return false;
        LOG("ppg: the creature's own read gives way to his ask (%s)", s_lit ? "the LEDs were on" : "it was waiting for the wrist");
        s_auto_failed++;
        drop_read();
    }
    if (!begin_read(secs, true)) return false;
    s_spots++;
    LOG("ppg: a %d s spot asked -- the LEDs wait for the wrist to be still (%.0f s at most)", s_spot_s, (double)PPG_WAIT_STILL_S);
    return true;
}

/* THE RED AND THE INFRARED: one sample pair of a two-slot stream into the
 * ratio's window -- the raw stream after its AGC (100 Hz), the creature's
 * own read after its (25 Hz); `first` (re)starts the window at the stream's
 * first sample (the AGC's last step, a restart's relight). A closed window
 * is one log line and nothing else -- never the glass, never a word from
 * the creature; a number taken while he sleeps goes to the night keeper. */
static void spo2_sample(int32_t red, int32_t ir, bool first, float rate)
{
    if (first) spo2_begin(&s_spo2, rate, s_agc2.v[0] == AGC_DARK);   /* the AGC's word on red (ruling 83: on the wrist, often DARK) */
    if (!spo2_feed(&s_spo2, red, ir, s_quiet_s)) return;
    char w[96]; spo2_word(&s_spo2, w, sizeof w);
    LOG("ppg: %s", w);
    if (s_spo2.v == SPO2_OK && !s_raw && rest_dozing_now()) spo2_night_note(&s_spo2_night, s_spo2.spo2);
}

/* THE BREATH READ: one sample of whichever one slot carries the beat (the ir of a two-slot stream, his asked
 * read's one LED) into the breath's trackers; `first` (re)starts them at the stream's first sample proper. */
static void resp_sample(int32_t x, bool first, float rate)
{
    if (first) resp_begin(&s_resp, rate);
    resp_feed(&s_resp, x);
}

/* the read is over: the breath's verdict, one log line and nothing else -- never the glass, never a word from
 * the creature. A clean number off a read on the wrist (never the raw stream's) is kept for `status` and the
 * gel's road, and goes to the night keeper while he sleeps. */
static void resp_done(void)
{
    if (!s_resp.seen) return;
    bool ok = resp_close(&s_resp, s_quiet_s);
    char w[160]; resp_word(&s_resp, w, sizeof w);
    LOG("ppg: %s", w);
    s_resp.seen = 0;                                         /* closed once; the next read's first sample begins anew */
    if (!ok || s_raw) return;
    s_resp_bpm = s_resp.bpm; s_resp_conf = s_resp.conf; s_resp_at_ms = millis();
    if (rest_dozing_now()) resp_night_note(&s_resp_night, s_resp.bpm);
}

/* a sleep span's end (spo2_night_tick found it): the night's median breath to the day header once; a nap's
 * numbers, or a night with none, dropped with a line */
static void resp_night_end(bool night)
{
    int n = 0, med = resp_night_take(&s_resp_night, &n);
    if (!night) { if (n) LOG("ppg: breath -- %d read%s in a nap, not a night: dropped", n, n == 1 ? "" : "s"); return; }
    if (!med) { LOG("ppg: breath -- the night ended with no number (no still read with a breath in it)"); return; }
    wrist_note_resp_night(med);
    LOG("ppg: the night's median breath ~%d /min over %d read%s -> the day header", med, n, n == 1 ? "" : "s");
}

static void drain(void)
{
    if (!s_lit) return;
    uint8_t w = rd(REG_FIFO_WR) & 0x1F, r = rd(REG_FIFO_RD) & 0x1F;
    int n = (w - r) & 0x1F;
    if (!n) return;
    if (s_raw && !s_agc) {                                   /* the stream: two slots, 6 bytes a sample, straight out */
        if (n > 21) n = 21;                                  /* Wire's rx buffer is 128 B: 21 x 6 = 126 (2026-09-25: 186 asked -> CORRUPT HEAP, a reboot); the rest next tick */
        uint8_t b[21 * 6];
        if (!bus2_rd(ADDR, REG_FIFO_DATA, b, (uint8_t)(n * 6))) return;
        for (int i = 0; i < n; i++) {
            uint32_t red = ((uint32_t)b[i * 6] << 16 | (uint32_t)b[i * 6 + 1] << 8 | b[i * 6 + 2]) & 0x3FFFF;
            uint32_t ir = ((uint32_t)b[i * 6 + 3] << 16 | (uint32_t)b[i * 6 + 4] << 8 | b[i * 6 + 5]) & 0x3FFFF;
            spo2_sample((int32_t)red, (int32_t)ir, s_raw_n == 0, 100.0f);   /* THE RED AND THE INFRARED: raw_config's 100 Hz */
            resp_sample((int32_t)ir, s_raw_n == 0, 100.0f);                  /* THE BREATH READ: the stream's ir, the bench's truth */
            Serial.printf("raw %lu %lu %lu\n", (unsigned long)s_raw_n++, (unsigned long)red, (unsigned long)ir);
        }
        return;
    }
    if (s_slots == 2) {                                      /* THE AGC PER LED: the raw stream's AGC reads both slots, red into the first half, ir into the second; since 2026-10-13 the creature's own read too (THE RED AND THE INFRARED) */
        while (n > 0) {                                      /* Wire's 128 B takes 21 pairs a read; the deep nap's 1 s poll finds up to 25 waiting (the FIFO holds 32): the rest in a second read, not next tick */
            int k = n > 21 ? 21 : n;
            uint8_t b[21 * 6];
            if (!bus2_rd(ADDR, REG_FIFO_DATA, b, (uint8_t)(k * 6))) return;
            for (int i = 0; i < k && s_n < BUF_MAX; i++) {
                s_buf[s_n] = (int32_t)(((uint32_t)b[i * 6] << 16 | (uint32_t)b[i * 6 + 1] << 8 | b[i * 6 + 2]) & 0x3FFFF);
                s_buf[BUF_MAX + s_n] = (int32_t)(((uint32_t)b[i * 6 + 3] << 16 | (uint32_t)b[i * 6 + 4] << 8 | b[i * 6 + 5]) & 0x3FFFF);
                if (!s_agc) spo2_sample(s_buf[s_n], s_buf[BUF_MAX + s_n], s_n == 0, RATE_NOMINAL);   /* THE RED AND THE INFRARED: the read proper's pairs, the AGC's not */
                if (!s_agc) resp_sample(s_buf[BUF_MAX + s_n], s_n == 0, RATE_NOMINAL);               /* THE BREATH READ: the ir half */
                s_n++;
            }
            n -= k;
        }
        return;
    }
    uint8_t b[32 * 3];
    if (!bus2_rd(ADDR, REG_FIFO_DATA, b, (uint8_t)(n * 3))) return;
    for (int i = 0; i < n && s_n < BUF_MAX; i++) {
        s_buf[s_n] = (int32_t)(((uint32_t)b[i * 3] << 16 | (uint32_t)b[i * 3 + 1] << 8 | b[i * 3 + 2]) & 0x3FFFF);
        if (!s_agc) resp_sample(s_buf[s_n], s_n == 0, RATE_NOMINAL);   /* THE BREATH READ: his asked read's one LED carries the breath too */
        s_n++;
    }
}

/* THE PULSE IN THE DAY: a clean result into the ring and the minute */
static void recent_push(int bpm, int rmssd_ms)
{
    recent_t *r = &s_recent[s_recent_head];
    r->bpm = (uint8_t)(bpm > 255 ? 255 : bpm);
    r->rmssd = (uint8_t)(rmssd_ms > 255 ? 255 : rmssd_ms < 0 ? 0 : rmssd_ms);
    r->minute = (int16_t)clock_local_minute();
    r->at_ms = millis();
    s_recent_head = (s_recent_head + 1) % PPG_RECENT_N;
    if (s_recent_n < PPG_RECENT_N) s_recent_n++;
    s_recent_seq++;
    wrist_note_pulse(r->bpm, r->rmssd);
}

/* the read is over, one way or the other: the LEDs off, the buffer freed,
 * the result to whoever asked (his: ppg_spot and the row; the creature's:
 * the ring and the minute) */
/* THE WARM SKIN (warm.c, watch-night C20): one die-temperature conversion
 * at the read's end, while the part is still out of shutdown (the LEDs go
 * off right after): TEMP_EN set, its self-clearing bit polled (~29 ms,
 * DIE_TEMP_WAIT_MS at most -- one hitch of a frame or two, once per
 * spot, at rest), then TINT + TFRAC in one two-byte read. No buffer.
 * A read that never lit the LEDs (the wrist never still) gets none;
 * a bus fault leaves the last reading as it was. */
static void die_temp_read(void)
{
    if (!s_present || !s_lit) return;
    uint8_t b[2];
    if (!wr(REG_DIE_CONFIG, 0x01)) return;
    uint32_t t0 = millis();
    while ((rd(REG_DIE_CONFIG) & 0x01) && millis() - t0 < DIE_TEMP_WAIT_MS) delay(1);
    if (millis() - t0 >= DIE_TEMP_WAIT_MS) { LOG("ppg: the die's temperature never came (%d ms)", DIE_TEMP_WAIT_MS); return; }
    if (!bus2_rd(ADDR, REG_DIE_TINT, b, 2)) return;
    s_die_c = (float)(int8_t)b[0] + (float)(b[1] & 0x0F) * 0.0625f;
    s_die_seq++;
}
float ppg_die_temp_c(void) { return s_die_c; }
uint32_t ppg_die_temp_seq(void) { return s_die_seq; }

static void settle(bool ok, float bpm, float rmssd_ms)
{
    bool asked = s_asked;
    int secs = s_spot_s;
    die_temp_read();                                     /* THE WARM SKIN: before the LEDs' shutdown */
    drop_read();
    char pw[48] = "";                                    /* THE PERFUSION INDEX: every read's line carries it (perfusion.h), when it was measured */
    if (s_perf_pct >= 0) { int k = snprintf(pw, sizeof pw, "; "); perf_word(s_perf_pct, pw + k, (int)sizeof pw - k); }
    if (asked) {
        s_ok = ok; s_bpm = bpm; s_rmssd = ok ? rmssd_ms : 0; s_have = true;
        s_perf_thin = !ok && s_perf_pct >= 0 && perf_band(s_perf_pct) == PERF_THIN;   /* the PULSE row's one word: THIN where it said NONE */
        s_done++;
        LOG("ppg: spot %s (%d s) -- %s%s", ok ? "ok" : "NOT ok", secs, s_why, pw);
        if (ok) LOG("ppg: %.0f bpm at rest, rmssd %.0f ms", bpm, rmssd_ms);
        return;
    }
    if (ok) {
        s_auto_clean++;
        recent_push((int)(bpm + 0.5f), (int)(rmssd_ms + 0.5f));
        int m = clock_local_minute();
        LOG("ppg: the pulse in the day -- %.0f bpm, rmssd %.0f ms at %02d:%02d (%s%s); the ring holds %d", bpm, rmssd_ms,
            m < 0 ? 0 : m / 60, m < 0 ? 0 : m % 60, s_why, pw, s_recent_n);
    } else {
        s_auto_failed++;
        LOG("ppg: the pulse in the day -- no clean read (%s%s); the next in %d min", s_why, pw, PPG_AUTO_EVERY_MIN);
    }
}

/* the arithmetic: the DC (the skin's own reflection) must be there; a
 * 0.5 s moving mean taken out; peaks above 40 % of the largest, at
 * least a refractory apart; the intervals in the human range averaged,
 * their spread the honesty check -- and, since 2026-10-08, the RMSSD
 * of the successive intervals (the day log's hrv byte). The arm's
 * motion is the arbiter's business now (still.h); the cv is the
 * signal's own second opinion. */
static void finish(void)
{
    resp_done();                                             /* THE BREATH READ: the read's samples are all in -- its one line */
    float elapsed = (millis() - s_t0) / 1000.0f;
    float rate = s_n > 1 && elapsed > 1 ? s_n / elapsed : RATE_NOMINAL;
    bool ok = false; float bpm = 0, rmssd = 0;
    int n = s_n;
    if (n < (int)(rate * 5)) { snprintf(s_why, sizeof s_why, "only %d samples", n); goto done; }
    {
        const int32_t *x = s_slots == 2 ? s_buf + BUF_MAX : s_buf;   /* THE RED AND THE INFRARED: a two-slot read's beat is read on its ir half (the red half is spo2.h's, taken on the way in) */
        int64_t sum = 0;
        for (int i = 0; i < n; i++) sum += x[i];
        int32_t dc = (int32_t)(sum / n);
        if (dc < DC_MIN) {
            s_perf_pct = 0; perf_day_note(&s_perf_day, 0);   /* THE PERFUSION INDEX: no skin, no AC -- the day's tally counts a NONE */
            snprintf(s_why, sizeof s_why, "nothing on the sensor (dc %ld)", (long)dc); goto done;
        }
        /* high-pass into the buffer's other half: x - the mean of the 13 around it */
        int32_t *hp = s_slots == 2 ? s_buf : s_buf + BUF_MAX;
        int w = 6;
        int32_t hpmax = 0;
        double hpsq = 0;                                     /* THE PERFUSION INDEX: the AC's power about the local mean, summed over the read */
        for (int i = 0; i < n; i++) {
            int a = i - w < 0 ? 0 : i - w, b = i + w >= n ? n - 1 : i + w;
            int64_t s = 0;
            for (int j = a; j <= b; j++) s += x[j];
            hp[i] = x[i] - (int32_t)(s / (b - a + 1));
            if (hp[i] > hpmax) hpmax = hp[i];
            hpsq += (double)hp[i] * (double)hp[i];
        }
        /* THE PERFUSION INDEX (perfusion.h): the AC's RMS told peak-to-peak over the DC -- spo2.h's ruler -- over the whole
         * read; the day's tally takes every measured read, clean or not (a thin one is the strap's word, not the heart's) */
        s_perf_pct = perf_index(sqrtf((float)(hpsq / n)), (float)dc);
        perf_day_note(&s_perf_day, s_perf_pct);
        /* THE HONEST BEAT (beat.h, 2026-10-14): the peaks over 40 % of the tallest, counted with a refractory after
         * each beat (0.55 of the period) and the short / long alternation of a counted notch merged -- the week's
         * 139 / 141 / 138 bpm at rest were the dicrotic notch counted twice over */
        beat_t bt;
        beat_run(&bt, hp, n, rate, (int32_t)(hpmax * BEAT_THR_FRAC));
        int beats = bt.beats;
        float cv = bt.cv;
        bpm = bt.bpm; rmssd = bt.rmssd;
        if (beats < BEATS_MIN) { snprintf(s_why, sizeof s_why, "%d beats found, need %d", beats, BEATS_MIN); goto thin; }
        if (cv > CV_MAX) { snprintf(s_why, sizeof s_why, "irregular (cv %.2f): the signal, not the wrist", cv); goto thin; }
        if (bpm < 40 || bpm > 180) { snprintf(s_why, sizeof s_why, "%.0f bpm is not a pulse", bpm); goto thin; }
        if (!s_asked && beat_rest_doubtful(bpm, rmssd)) {   /* rule 3: the creature's own rest read over 110 bpm or 120 ms is doubled beats until proven, never filed; his asked read keeps the band */
            s_rest_doubled++;
            snprintf(s_why, sizeof s_why, "doubled beats? (%.0f bpm, rmssd %.0f ms at rest)", bpm, rmssd);
            goto thin;
        }
        snprintf(s_why, sizeof s_why, "%d beats, cv %.2f, %.1f Hz%s%s", beats, cv, rate, bt.merged ? ", a notch merged out" : bt.refused ? ", a notch refused" : "",
                 s_still.restarts ? ", after a restart" : "");
        ok = true;
        goto done;
    }
thin:   /* THE PERFUSION INDEX: a read that failed on its signal with a THIN or NONE perfusion names the strap as the cause
         * (`THIN perfusion (the strap): irregular (cv 0.32)`), not the heart; a FAIR or GOOD one keeps the signal's own words */
    {
        char reason[sizeof s_why]; snprintf(reason, sizeof reason, "%s", s_why);
        perf_fail_word(s_perf_pct, reason, s_why, sizeof s_why);
    }
done:
    settle(ok, bpm, rmssd);
}

/* THE AGC (agc.h, AN6409's rule): the first reads only choose the LED
 * current -- every AGC_WINDOW samples the DC is judged, the current moved,
 * the FIFO and the buffer thrown away; the spot starts when it settles
 * (or gives up: dark = nothing on the window, bright = pressed / the room).
 * THE AGC PER LED: the raw stream's two slots each judged on their own
 * (agc2_step), each at its own current after. */
/* the stream's configuration: both LEDs at their own currents, 100 Hz, no
 * averaging, two slots -- applied whether the AGC settled, gave up, or saw
 * no samples at all (2026-09-25, the wrist read: the bus dropped out during
 * the AGC, the stream ran in the spot's single-slot 25 Hz config and every
 * sample was read as two -- garbage; now the bad start is said out loud). */
static void raw_config(void)
{
    wr(REG_MODE, 0x80 | 0x07);
    set_pa2();
    wr(REG_MULTI_LED1, 0x21);                                /* slot 1 red, slot 2 ir */
    wr(REG_FIFO_CONFIG, 0x10);                               /* avg 1, rollover */
    clear_fifo();
    wr(REG_MODE, 0x07);
    s_t0 = millis();
    LOG("ppg raw: %d s of red+ir at 100 Hz, red 0x%02X (~%.1f mA) ir 0x%02X (~%.1f mA) -- hold still", s_raw_s,
        s_agc2.pa[0], agc_ma(s_agc2.pa[0]), s_agc2.pa[1], agc_ma(s_agc2.pa[1]));
}

static const char *verdict_word(agc_verdict_t v)
{
    return v == AGC_SETTLED ? "settled" : v == AGC_DARK ? "DARK" : v == AGC_BRIGHT ? "BRIGHT" : "unsettled";
}

static void agc_tick(void)
{
    if (s_n < AGC_WINDOW) {
        if (millis() - s_t0 > 4000u) {
            s_agc = false; s_t0 = millis();
            snprintf(s_agc_why, sizeof s_agc_why, "no samples for the agc");
            LOG("ppg: agc got %d samples in 4 s (needs %d) -- the bus is dropping out (the harness moving?); going on at pa 0x%02X anyway", s_n, AGC_WINDOW, s_raw ? s_agc2.pa[1] : s_pa);
            s_n = 0;
            if (s_raw) raw_config();
        }
        return;
    }
    int64_t sum = 0, sum2 = 0;
    for (int i = 0; i < s_n; i++) { sum += s_buf[i]; if (s_slots == 2) sum2 += s_buf[BUF_MAX + i]; }
    long dc = (long)(sum / s_n), dc2 = (long)(sum2 / s_n);
    agc_verdict_t v;
    if (s_slots == 2) {
        v = agc2_step(&s_agc2, dc, dc2);
        s_agc_steps = s_agc2.steps;
        if (v == AGC_AGAIN) {
            set_pa2(); clear_fifo(); s_n = 0; s_t0 = millis();
            return;
        }
        snprintf(s_agc_why, sizeof s_agc_why, "%s: red %s 0x%02X dc %ld %%, ir %s 0x%02X dc %ld %%", verdict_word(v),
                 verdict_word(s_agc2.v[0]), s_agc2.pa[0], dc * 100 / AGC_FULL, verdict_word(s_agc2.v[1]), s_agc2.pa[1], dc2 * 100 / AGC_FULL);
    } else {
        uint8_t pa = s_pa;
        v = agc_step(dc, &pa);
        s_agc_steps++;
        if (v == AGC_AGAIN && s_agc_steps < AGC_STEPS_MAX) {
            s_pa = pa; set_pa(s_pa);
            clear_fifo();
            s_n = 0;
            s_t0 = millis();                                 /* the 4 s "no samples" clock is PER STEP (2026-09-25: the wrist needs more steps up than a fingertip, and the clock ran from the first) */
            return;
        }
        snprintf(s_agc_why, sizeof s_agc_why, "%s at 0x%02X (~%.1f mA), dc %ld = %ld %%", verdict_word(v), s_pa, agc_ma(s_pa), dc, dc * 100 / AGC_FULL);
    }
    LOG("ppg: agc %s after %d steps", s_agc_why, s_agc_steps);
    s_agc = false;
    s_n = 0; s_t0 = millis();                                /* the spot proper starts now, at this current */
    if (s_raw) raw_config();
}

static void finish_raw(void)
{
    uint32_t ms = millis() - s_t0;
    uint32_t n = s_raw_n;
    resp_done();                                             /* THE BREATH READ: the stream's verdict, the bench's truth */
    drop_read();
    float hz = ms ? n * 1000.0f / ms : 0.0f;
    LOG("ppg raw: done -- %lu samples in %lu ms (%.1f Hz)%s", (unsigned long)n, (unsigned long)ms, hz, hz < 90 ? " -- UNDER 90 Hz: the bus dropped samples, do not trust this read" : "");
}

void ppg_tick(void)
{
    if (!s_present || !s_busy || !s_lit) return;
    drain();
    if (s_agc) { agc_tick(); return; }
    if (s_raw) { if (millis() - s_t0 >= (uint32_t)s_raw_s * 1000u) finish_raw(); return; }
    if (millis() - s_t0 >= (uint32_t)s_spot_s * 1000u || s_n >= BUF_MAX) finish();
}

/* --- THE STILL SPOT: the arbiter's word, done on the bus ----------------------- */
static void still_word(int w, float jerk_g)
{
    switch (w) {
    case STILL_LIGHT:
        if (!s_lit_once) { light(); LOG("ppg: the wrist still %.0f s -- the LEDs light, the agc chooses, then %d s", (double)STILL_ARM_S, s_spot_s); }
        else { relight(); LOG("ppg: still again -- the read starts over (%d s%s)", s_spot_s, s_agc ? ", the agc still choosing" : ""); }
        break;
    case STILL_RESTART:
        shutdown_leds();
        s_n = 0;
        LOG("ppg: moved %.0f s in (%.2f g) -- the samples thrown away, the LEDs dark, waiting for the wrist again (%d of %d)",
            (double)s_still.spoil_at_s, (double)s_still.spoil_g, s_still.restarts, STILL_RESTART_MAX);
        break;
    case STILL_GAVE_UP:
        snprintf(s_why, sizeof s_why, "%s", still_why(&s_still));
        settle(false, 0, 0);
        break;
    default: (void)jerk_g; break;
    }
}

/* --- THE PULSE IN THE DAY: the gates, the start ----------------------------------- */
static const char *auto_gate(bool lit)
{
    (void)lit;
    if (store.ppg_auto_off) return "off (`ppg auto on`)";
    if (!s_present) return "no sensor";
    if (s_busy) return s_raw ? "the raw stream" : s_asked ? "his read" : "";
    if (charging() || link_docked()) return "on the cable";
    if (rest_on_shelf()) return "on the shelf";
    if (!wrist_worn() && !strcmp(rest_state_name(), "awake")) return "not worn";   /* no motion in 10 min and no night / nap judged: nobody's wrist */
    if (s_auto_last_ms && millis() - s_auto_last_ms < (uint32_t)PPG_AUTO_EVERY_MIN * 60000u) return "";   /* inside the gap: nothing to say */
    if (s_quiet_s < STILL_ARM_S) return "";                  /* not still yet: the next frame */
    return NULL;                                             /* go */
}

static void auto_try(bool lit)
{
    if (s_busy || store.ppg_auto_off || !s_present) return;
    if (s_auto_last_ms && millis() - s_auto_last_ms < (uint32_t)PPG_AUTO_EVERY_MIN * 60000u) return;
    if (s_quiet_s < STILL_ARM_S) return;
    const char *why = auto_gate(lit);
    if (why && why[0]) {
        if (strcmp(why, s_auto_gate) || millis() - s_auto_gate_ms >= AUTO_RETRY_S * 1000u) {
            if (strcmp(why, s_auto_gate)) LOG("ppg: the pulse in the day kept back -- %s", why);
            snprintf(s_auto_gate, sizeof s_auto_gate, "%s", why);
            s_auto_gate_ms = millis();
        }
        return;
    }
    if (why) return;
    s_auto_gate[0] = 0;
    if (!begin_read(PPG_AUTO_S, false)) { LOG("ppg: the pulse in the day -- no heap for the buffer"); s_auto_last_ms = millis(); return; }
    s_auto_last_ms = millis();
    s_auto_tries++;
    LOG("ppg: the pulse in the day -- a %d s read of its own (%s, the wrist still %.0f s)", PPG_AUTO_S, lit ? "lit" : "the dark house", (double)s_quiet_s);
}

/* THE RED AND THE INFRARED: a sleep span's end (rest's word, a new end_row), looked for once a second in both
 * houses -- the night's lowest number goes to the day header once (wrist_note_spo2_min, reserved[4]); a nap's
 * numbers, or a night with none, are dropped with a line. Never the glass. */
static void spo2_night_tick(void)
{
    if (millis() - s_spo2_look_ms < 1000u) return;
    s_spo2_look_ms = millis();
    rest_span_t sp;
    bool have = rest_last_span(&sp);
    if (!s_spo2_span_seen) { s_spo2_span_seen = true; s_spo2_span_row = have ? sp.end_row : 0; return; }   /* the first look learns the row (none yet, or a replayed one): no night of tonight's ended */
    if (!have || sp.end_row == s_spo2_span_row) return;
    s_spo2_span_row = sp.end_row;
    resp_night_end(sp.night);                                /* THE BREATH READ: the same span's end, the night's median */
    int n = 0, lo = spo2_night_take(&s_spo2_night, &n);
    if (!sp.night) { if (n) LOG("ppg: spo2 -- %d window%s in a nap, not a night: dropped", n, n == 1 ? "" : "s"); return; }
    if (!lo) { LOG("ppg: spo2 -- the night ended with no number (the red dark, or no still read)"); return; }
    wrist_note_spo2_min(lo);
    LOG("ppg: the night's lowest spo2 ~%d over %d window%s -> the day header (uncalibrated)", lo, n, n == 1 ? "" : "s");
}

void ppg_feed(float jerk_g, float dt, bool lit)
{
    if (dt < 0) dt = 0;
    if (jerk_g < STILL_JERK_G) { s_quiet_s += dt; if (s_quiet_s > 1e6f) s_quiet_s = 1e6f; } else s_quiet_s = 0;
    if (!s_present) return;
    if (s_busy && !s_raw) {
        int w = still_tick(&s_still, dt, jerk_g);
        if (w != STILL_NONE) still_word(w, jerk_g);
    }
    if (!lit) ppg_tick();                                    /* lit, bus_tick drains the FIFO each frame; dark, the nap's poll is the tick */
    auto_try(lit);
    spo2_night_tick();                                       /* THE RED AND THE INFRARED: the night's end, once */
}

void ppg_dark_poll(const vec3_t *prev_pose, const vec3_t *pose)
{
    uint32_t now = millis();
    float dt = s_feed_ms ? (now - s_feed_ms) / 1000.0f : 0.25f;
    if (dt > 2.0f) dt = 2.0f;                                /* the first poll after a long lit spell: not a leap */
    s_feed_ms = now;
    float jx = pose->x - prev_pose->x, jy = pose->y - prev_pose->y, jz = pose->z - prev_pose->z;
    ppg_feed(sqrtf(jx * jx + jy * jy + jz * jz), dt, false);
}

/* --- his spot's readback ------------------------------------------------------------ */
bool ppg_spot_left(int *left_s, bool *settling)
{
    if (!s_busy || s_raw || !s_asked) return false;
    bool waiting = still_waiting(&s_still);
    int gone = (s_agc || waiting || !s_lit) ? 0 : (int)((millis() - s_t0) / 1000u);
    if (left_s) *left_s = gone >= s_spot_s ? 0 : s_spot_s - gone;
    if (settling) *settling = s_agc || waiting || !s_lit;
    return true;
}

bool ppg_spot_waiting(void) { return s_busy && !s_raw && s_asked && still_waiting(&s_still); }
bool ppg_own_left(int *left_s, bool *glow)   /* THE EAR TO THE WRIST: the creature's own read, as the row's readback above */
{
    if (!s_busy || s_raw || s_asked) return false;
    int gone = (s_agc || !s_lit || still_waiting(&s_still)) ? 0 : (int)((millis() - s_t0) / 1000u);
    if (left_s) *left_s = gone >= s_spot_s ? 0 : s_spot_s - gone;
    if (glow) *glow = s_lit;
    return true;
}
uint32_t ppg_spot_done(void) { return s_done; }
const char *ppg_spot_why(void) { return s_why; }
/* THE PERFUSION INDEX: his last spot failed for a THIN perfusion (the row's one word); the last read's index; the day's
 * byte at the close (daylog.cpp's close_file), which clears the tally for tomorrow */
bool ppg_spot_thin(void) { return s_have && s_perf_thin; }
float ppg_perfusion_last(void) { return s_perf_pct; }
uint8_t ppg_perfusion_day_take(int *n_out)
{
    int n = 0;
    uint8_t b = perf_day_take(&s_perf_day, &n);
    if (n_out) *n_out = n;
    s_rest_doubled = 0;                                      /* THE HONEST BEAT: the day's refused tally closes with the day */
    if (n) LOG("ppg: the day's perfusion -- %d read%s, the median %s -> the day header", n, n == 1 ? "" : "s",
               b == PERF_HDR_NONE ? "under a tenth of a percent (NONE: nothing on the sensor)" : perf_band_name(perf_hdr_band(b)));
    return b;
}
float ppg_spot_rmssd(void) { return s_rmssd; }

bool ppg_spot(float *bpm, bool *ok)
{
    if (!s_have) return false;
    if (bpm) *bpm = s_bpm;
    if (ok) *ok = s_ok;
    return true;
}

/* --- THE PULSE IN THE DAY: the ring --------------------------------------------------- */
int ppg_recent_count(void) { return s_recent_n; }
uint32_t ppg_recent_seq(void) { return s_recent_seq; }

static const recent_t *recent_at(int i)
{
    if (i < 0 || i >= s_recent_n) return NULL;
    return &s_recent[(s_recent_head - 1 - i + PPG_RECENT_N) % PPG_RECENT_N];
}

bool ppg_recent(int i, int *bpm, int *rmssd_ms, int *minute, uint32_t *age_s)
{
    const recent_t *r = recent_at(i);
    if (!r) return false;
    if (bpm) *bpm = r->bpm;
    if (rmssd_ms) *rmssd_ms = r->rmssd;
    if (minute) *minute = r->minute;
    if (age_s) *age_s = (millis() - r->at_ms) / 1000u;
    return true;
}

bool ppg_rest_now(int *bpm, uint32_t *age_s)
{
    const recent_t *r = recent_at(0);
    if (!r) return false;
    uint32_t age = (millis() - r->at_ms) / 1000u;
    if (age > PPG_REST_FRESH_S) return false;
    if (bpm) *bpm = r->bpm;
    if (age_s) *age_s = age;
    return true;
}

/* THE BREATH READ: the last clean rate off the wrist, if younger than PPG_REST_FRESH_S; 0 = none fresh */
float ppg_breath_now(float *conf, uint32_t *age_s)
{
    if (s_resp_bpm <= 0) return 0;
    uint32_t age = (millis() - s_resp_at_ms) / 1000u;
    if (age > PPG_REST_FRESH_S) return 0;
    if (conf) *conf = s_resp_conf;
    if (age_s) *age_s = age;
    return s_resp_bpm;
}

bool ppg_recent_span(uint32_t within_s, int *n, int *lo, int *hi, int *mean)
{
    int cnt = 0, l = 999, h = 0; long sum = 0;
    for (int i = 0; i < s_recent_n; i++) {
        const recent_t *r = recent_at(i);
        if (within_s && (millis() - r->at_ms) / 1000u > within_s) break;   /* newest first: the first too old ends it */
        cnt++; sum += r->bpm;
        if (r->bpm < l) l = r->bpm;
        if (r->bpm > h) h = r->bpm;
    }
    if (!cnt) return false;
    if (n) *n = cnt;
    if (lo) *lo = l;
    if (hi) *hi = h;
    if (mean) *mean = (int)((sum + cnt / 2) / cnt);
    return true;
}

void ppg_status(char *out, int cap)
{
    if (!s_present) { snprintf(out, cap, "ppg: absent (bom %s)", bus_on_bom(BOM_PPG) ? "on" : "off"); return; }
    int n = 0;
    if (s_busy) {
        n = snprintf(out, cap, "ppg: MAX3010x at 0x%02X, a %d s %s running by the %s LED: %s, %d samples in %.1f s, pa 0x%02X", ADDR, s_raw ? s_raw_s : s_spot_s,
                     s_raw ? "raw stream" : s_asked ? "spot (his)" : "read of its own", ppg_led_name(store.ppg_led),
                     still_waiting(&s_still) ? "waiting for the wrist to be still" : s_agc ? "the agc choosing" : "reading",
                     s_n, s_lit ? (millis() - s_t0) / 1000.0f : 0.0f, s_pa);
    } else if (!s_have) {
        n = snprintf(out, cap, "ppg: MAX3010x rev 0x%02X at 0x%02X, shut down, LED %s; no spot of his yet (`ppg spot`, %d s, hold still)", s_rev, ADDR, ppg_led_name(store.ppg_led), PPG_SPOT_S);
    } else {
        n = snprintf(out, cap, "ppg: MAX3010x at 0x%02X, LED %s, last spot %s: %.0f bpm, rmssd %.0f ms (%s); agc %s; %lu spots",
                     ADDR, ppg_led_name(store.ppg_led), s_ok ? "ok" : "NOT ok", s_bpm, s_rmssd, s_why, s_agc_why, (unsigned long)s_spots);
    }
    if (n < 0 || n >= cap) return;
    if (s_die_seq) n += snprintf(out + n, cap - n, " | die %.1f C (%lu reads)", s_die_c, (unsigned long)s_die_seq);   /* THE WARM SKIN: the last spot's die temperature (serial only) */
    if (n < 0 || n >= cap) return;
    {   /* THE PERFUSION INDEX: the last measured read's word, then the day's tally per band (serial only, never the glass) */
        n += snprintf(out + n, cap - n, " | ");
        if (n < 0 || n >= cap) return;
        if (s_perf_pct >= 0) { n += perf_word(s_perf_pct, out + n, cap - n); if (n < 0 || n >= cap) return; n += snprintf(out + n, cap - n, "; "); if (n < 0 || n >= cap) return; }
        n += perf_tally_word(&s_perf_day, out + n, cap - n);
        if (n < 0 || n >= cap) return;
    }
    /* THE BREATH READ: the last clean rate off the wrist and its age, the reads' tally (serial only; never the glass) */
    if (s_resp_bpm > 0) n += snprintf(out + n, cap - n, " | breath ~%.0f/min (confidence %.1f, %lu min ago; %d reads, %d with a number)",
                                      (double)s_resp_bpm, (double)s_resp_conf, (unsigned long)((millis() - s_resp_at_ms) / 60000u), s_resp.reads, s_resp.oks);
    else n += snprintf(out + n, cap - n, " | breath none yet (%d reads)", s_resp.reads);
    if (n < 0 || n >= cap) return;
    /* THE PULSE IN THE DAY: the switch, the tallies, the ring's tail */
    n += snprintf(out + n, cap - n, " | the pulse in the day %s: %lu tried, %lu clean, %lu not%s%s; rest reads refused today: doubled %lu; recent",
                  store.ppg_auto_off ? "OFF" : "on", (unsigned long)s_auto_tries, (unsigned long)s_auto_clean, (unsigned long)s_auto_failed,
                  s_auto_gate[0] ? ", kept back: " : "", s_auto_gate, (unsigned long)s_rest_doubled);   /* THE HONEST BEAT: rule 3's day tally */
    if (!s_recent_n) { snprintf(out + n, cap - n, " none"); return; }
    for (int i = 0; i < s_recent_n && i < 6 && n < cap; i++) {
        const recent_t *r = recent_at(i);
        n += snprintf(out + n, cap - n, "%s %u/%u@%02d:%02d", i ? "," : "", r->bpm, r->rmssd, r->minute < 0 ? 0 : r->minute / 60, r->minute < 0 ? 0 : r->minute % 60);
    }
    if (s_recent_n > 6 && n < cap) snprintf(out + n, cap - n, " (+%d)", s_recent_n - 6);
}

const char *ppg_led_name(int led)
{
    return led == PPG_LED_GREEN ? "green" : led == PPG_LED_RED ? "red" : "ir";
}
