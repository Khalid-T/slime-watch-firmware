#include "motor.h"
#include "haptic.h"
#include "pulse.h"       /* THE HEARTBEAT HANDED BACK: the beats' plan and the tap's numbers */
#include "board.h"
#include "bus.h"
#include "app.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <stdio.h>

/* LEDC channel 4 sits on timer 2: the backlight is channel 7 on timer 3
 * (lcd.h), so a motor's frequency never touches the glass's. 8 kHz, 8
 * bits: above a coin motor's mechanical response, below the ear's
 * whine. */
#define MOTOR_CH      4
#define MOTOR_HZ      8000
#define MOTOR_BITS    8
#define BUZZ_CAP_MS   2000

/* the envelopes: (duty %, ms) steps, 0 ms ends. A bare ERM needs a kick
 * to start (its stall current is the pin's whole budget), so every
 * gesture opens at 100 % for a few ms and settles. */
typedef struct { uint8_t pct; uint16_t ms; } step_t;
#define MAX_STEPS 8
static const step_t ENV[HAPTIC_KINDS][MAX_STEPS] = {
    /* bump: one soft thump */
    { {100, 25}, {55, 45}, {0, 0} },
    /* purr: two rolling swells, run together */
    { {100, 20}, {40, 90}, {70, 80}, {35, 90}, {70, 80}, {30, 70}, {0, 0} },
    /* triple hop: three thumps, a breath apart */
    { {100, 25}, {60, 35}, {0, 120}, {100, 25}, {60, 35}, {0, 120}, {100, 25}, {60, 45} },
};
static const char *const KIND_NAMES[HAPTIC_KINDS] = { "bump", "purr", "hop" };
/* THE HEARTBEAT HANDED BACK (pulse.h): the beats' envelope is built per
 * play from his pulse -- (kick, tap, the gap to the next onset) per
 * beat, the last beat without its gap, a 0 ms step to end -- and plays
 * through the same stepper as the table's three */
static step_t s_beats[HEARTBEAT_MAX * 3 + 1];
static uint32_t s_beat_plays = 0;

static bool s_present = false;
static const step_t *s_env = NULL;      /* the envelope playing, or NULL */
static int s_env_n = MAX_STEPS;         /* its length (the table's rows are MAX_STEPS wide; the beats' is the plan's) */
static int s_step = 0;
static uint32_t s_step_at = 0;          /* millis() the current step began */
static uint32_t s_buzz_until = 0;       /* the bench pulse's end, or 0 */
static uint32_t s_plays = 0;
static uint8_t s_duty = 0;
static uint32_t s_hz = MOTOR_HZ;        /* the PWM's frequency now (`haptic buzz` can move it); 0 = raw DC, the pin driven plain */
static bool s_dc = false;

static void pwm_up(uint32_t hz)
{
    if (hz == 0) {                      /* raw DC: LEDC off the pin, a plain output */
        ledcDetachPin(PIN_MOTOR);
        pinMode(PIN_MOTOR, OUTPUT);
        digitalWrite(PIN_MOTOR, LOW);
        s_dc = true; s_hz = 0;
        return;
    }
    uint32_t got = ledcSetup(MOTOR_CH, hz, MOTOR_BITS);
    ledcAttachPin(PIN_MOTOR, MOTOR_CH);
    if (got != hz) LOG("motor: LEDC asked %lu Hz, got %lu", (unsigned long)hz, (unsigned long)got);
    s_dc = false; s_hz = got ? got : hz;
    gpio_set_drive_capability((gpio_num_t)PIN_MOTOR, GPIO_DRIVE_CAP_3);   /* the strongest the pin has: ~40 mA */
}

static void set_duty(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_duty = pct;
    if (s_dc) { digitalWrite(PIN_MOTOR, pct >= 50 ? HIGH : LOW); return; }
    ledcWrite(MOTOR_CH, pct >= 100 ? (1u << MOTOR_BITS) : (uint32_t)pct * (1u << MOTOR_BITS) / 100);   /* 2^bits = fully on */
}

void motor_begin(void)
{
    s_present = false;
    pwm_up(MOTOR_HZ);
    set_duty(0);
    s_env = NULL; s_buzz_until = 0;
    s_present = true;
    LOG("motor: a bare motor on GPIO %d (LEDC ch %d, %d Hz, drive 3); bump / purr / hop as envelopes", PIN_MOTOR, MOTOR_CH, MOTOR_HZ);
}

void motor_off(void)
{
    if (!s_present) return;
    set_duty(0);
    s_env = NULL; s_buzz_until = 0;
}

void motor_end(void)
{
    if (!s_present) return;
    motor_off();
    if (!s_dc) ledcDetachPin(PIN_MOTOR);
    pinMode(PIN_MOTOR, INPUT);
    s_present = false;
}

bool motor_present(void) { return s_present; }

bool motor_play(int kind)
{
    if (!s_present || kind < 0 || kind >= HAPTIC_KINDS) return false;
    s_buzz_until = 0;
    s_env = ENV[kind];
    s_env_n = MAX_STEPS;
    s_step = 0;
    s_step_at = millis();
    set_duty(s_env[0].pct);
    s_plays++;
    return true;
}

bool motor_beats(float bpm, float secs)
{
    pulse_beats_t plan;
    if (!s_present || !pulse_beats_plan(bpm, secs, &plan)) return false;
    int n = 0;
    uint16_t gap = plan.period_ms > HEARTBEAT_KICK_MS + HEARTBEAT_TAP_MS ? (uint16_t)(plan.period_ms - HEARTBEAT_KICK_MS - HEARTBEAT_TAP_MS) : 1;
    for (int i = 0; i < plan.n; i++) {
        s_beats[n].pct = 100; s_beats[n++].ms = HEARTBEAT_KICK_MS;
        s_beats[n].pct = HEARTBEAT_TAP_PCT; s_beats[n++].ms = HEARTBEAT_TAP_MS;
        if (i + 1 < plan.n) { s_beats[n].pct = 0; s_beats[n++].ms = gap; }
    }
    s_beats[n].pct = 0; s_beats[n].ms = 0;
    s_buzz_until = 0;
    s_env = s_beats;
    s_env_n = n + 1;
    s_step = 0;
    s_step_at = millis();
    set_duty(s_env[0].pct);
    s_plays++; s_beat_plays++;
    return true;
}

bool motor_buzz(uint32_t ms, uint8_t pct, uint32_t hz)
{
    if (!s_present) return false;
    if (ms > BUZZ_CAP_MS) ms = BUZZ_CAP_MS;
    if (hz != s_hz) { set_duty(0); pwm_up(hz); }
    s_env = NULL;
    s_buzz_until = millis() + ms;
    set_duty(pct);
    s_plays++;
    return true;
}

bool motor_hold(uint32_t s)
{
    if (!s_present) return false;
    if (s > 60) s = 60;
    s_env = NULL;
    set_duty(0);
    pwm_up(0);                          /* raw DC: the pin as a plain output */
    s_buzz_until = millis() + s * 1000;
    set_duty(100);
    s_plays++;
    return true;
}

void motor_tick(void)
{
    if (!s_present) return;
    uint32_t now = millis();
    if (s_buzz_until) {
        if ((int32_t)(now - s_buzz_until) >= 0) { s_buzz_until = 0; set_duty(0); }
        return;
    }
    if (!s_env) return;
    if (now - s_step_at < s_env[s_step].ms) return;
    s_step++;
    if (s_step >= s_env_n || s_env[s_step].ms == 0) { s_env = NULL; set_duty(0); return; }
    s_step_at = now;
    set_duty(s_env[s_step].pct);
}

void motor_status(char *out, int cap)
{
    if (!s_present) { snprintf(out, cap, "motor: absent (bom %s)", bus_on_bom(BOM_MOTOR) ? "on" : "off"); return; }
    snprintf(out, cap, "motor: bare, GPIO %d, %s %lu Hz, %s (duty %u %%, ledc %lu, pin %d), %lu played (%lu as his heartbeat); kinds %s|%s|%s|beats; `haptic buzz <ms> [<pct> [<hz>|0=dc]]`",
             PIN_MOTOR, s_dc ? "raw DC" : "LEDC ch 4", (unsigned long)s_hz, (s_env || s_buzz_until) ? "PLAYING" : "idle", s_duty,
             (unsigned long)(s_dc ? 0 : ledcRead(MOTOR_CH)), digitalRead(PIN_MOTOR), (unsigned long)s_plays, (unsigned long)s_beat_plays,
             KIND_NAMES[0], KIND_NAMES[1], KIND_NAMES[2]);
}
