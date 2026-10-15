#include "wake.h"
#include "wdt.h"                /* THE LOOP'S PULSE: fed through the awake-ear loop, at the light sleep's edge, inside wom-test */
#include "motor.h"
#include "app.h"
#include "board.h"
#include "daylog.h"
#include "clockwork.h"
#include "gentle.h"          /* THE GENTLE WAKE: the pulse fed where a spot finishes in the dark */
#include "ppg.h"
#include "rest.h"            /* QUIET TIME: the quiet hours at the nap's own minute (the grain's, else 23-7; night=1 with no clock) */
#include "raise.h"           /* THE RAISE'S RULE, pure: the episode pose, the triggers, the verdict (tools/wake_test.cpp runs it on the laptop) */
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <math.h>
#include <string.h>

/* --- the tunables ------------------------------------------------------------ */

/* THE NAP: the chip light-sleeps until the QMI8658's motion engine
 * flips INT1, the touch pulls TP_INT low, a byte lands on the UART, or
 * the minute turns (the day log's record). No polling. */
static const uint8_t  WOM_MG        = 120;   /* the engine's threshold, milli-g: a wrist turning, not a table's hum (tune 100-200) */
static const uint8_t  WOM_BLANK     = 20;    /* samples the engine ignores after arming: at 21 Hz ~1 s -- with 4 it fired 8 times on a still board as its filter settled */
static const uint32_t WOM_SETTLE_MS = 30;    /* the pin finds its resting level this long after arming (seen: 15 ms) */
static const uint32_t NAP_TIMER_US  = 60000000; /* the fallback: once a minute for the record, with no clock */
static const uint32_t NAP_POLL_US   = 250000;   /* THE SAFETY NET (Khalid, 2026-09-09 22:30: three naps, three raises, the engine fired 0 times): the timer also polls the pose 4x a second and judges a raise itself; the engine's word is welcome when it comes */
/* THE NAP BREATHES (the optimization pass, 2026-09-10): the 250 ms poll
 * is what made the raise work, and it stays -- but a wrist that has lain
 * still for a while is polled slower: after NAP_SLOW1_S of stillness
 * every NAP_POLL_SLOW1_US, after NAP_SLOW2_S every NAP_POLL_SLOW2_US (a
 * raise still lands within the second: the poll judges the turn of the
 * pose, not the jerk between two samples). Any motion the nap sees --
 * the engine's word or the poll's own verdict, a bump included -- puts
 * the poll back at 250 ms. The minute's record is untouched: its timer
 * is min'd against the poll as before. */
static const uint32_t NAP_SLOW1_S       = 300;      /* 5 min still: 500 ms */
static const uint32_t NAP_POLL_SLOW1_US = 500000;
static const uint32_t NAP_SLOW2_S       = 1800;     /* 30 min still: 1 s */
static const uint32_t NAP_POLL_SLOW2_US = 1000000;
static bool           NAP_ENGINE    = false;    /* the QMI8658's wake-on-motion, as configured tonight, freezes the raw registers and never toggled INT1 on a raise (22:40): OFF until it is understood; the poll runs the nap. `wake-engine on` to try it */
static const int      FROZEN_POLLS  = 8;        /* the raw sample identical this many polls with the engine armed = its stream froze: the engine is dropped, polling alone */
static const uint32_t NAP_LOG_MIN   = 5;     /* the dark log line, every this many minutes */
/* RAISE-TO-WAKE: on the engine's word a short BURST is read at the full
 * rate; a raise = the pose turned toward the face and held (InfiniTime's
 * rule); a bump rings and returns; a shake keeps jerking. THE EPISODE
 * POSE (2026-09-10 evening, Khalid wearing it: "I turned it up and it
 * did not light"): the turn is measured from the pose before the
 * episode's first motion, and that pose moves only after 2 s of
 * stillness -- not at every "turn" verdict, which cut a slow or a
 * stepped raise in pieces and re-based the reference mid-raise (the
 * cause, in full, at the top of raise.h). The rule itself -- the
 * triggers, the verdict, the re-base -- is raise.cpp's, with no IMU in
 * it; the numbers below are the burst's alone, the rest are `tune`. */
static const uint32_t BURST_SKIP_MS = 40;    /* the stream's first samples after the mode switch are its filter settling (fake 0.5 g jerks): dropped */
static const uint32_t BURST_MS      = 300;   /* the burst: long enough for a brisk raise to finish */
static const uint32_t BURST_MAX_MS  = 700;   /* still moving at 300 ms: keep watching this long, then judge */
static const float    HOLD_S        = 0.15f; /* the tail of the burst that must be quiet: a raise is held */
static const float    HOLD_JERK_G   = 0.08f; /* quiet = every jerk in the tail under this... */
static const float    HOLD_TURN_DEG = 3.0f;  /* ...AND the pose turned less than this across the tail (a smooth turn of the wrist jerks ~0.005 g between 224 Hz samples: the jerk alone never saw it moving, so the burst ended at 300 ms mid-raise) */
/* the rule's numbers, live (`wake-set` writes them; raise.cpp's DEFAULTS
 * carry each one's cause): raise_deg 20, raise_face 0.15, face_rise
 * 0.20, pickup_g 0.35, wake_jerk_g 0.18, shake_span_ms 150, step_deg 5,
 * still_jerk_g 0.08, rebase_still_s 2.0, rebase_min_deg 5, quiet_face 0.5 */
static raise_tune_t   tune = *raise_defaults();
/* QUIET TIME (design/watch-inspiration.md A, Pebble's Quiet Time: the
 * wrist-flick backlight off at night "on the assumption that you don't
 * want to be lighting up your wrist every time you roll over"). In the
 * quiet hours (rest.cpp's: the grain's `quiet`, else 23-7; the desk's
 * night=1 with no clock) a burst judged A RAISE that does not END
 * FACE-UP -- gravity out of the glass under tune.quiet_face (~60 deg
 * from flat; by day raise_face 0.15 lights it) -- is a turn in the
 * sheets: it sleeps on, the glass never lit, the radio never woken (the
 * episode pose is left where it was; the stillness re-bases it 2 s
 * later, as after any turn). A raise that does end face-up lights as
 * today (THE NIGHT LIGHT). By day nothing changes. The rule is
 * raise_quiet() -- quiet_hours && verdict == raise && face < quiet_face
 * -> sleep on -- and wake_test runs it; rest_test checks rest_night_at. */
static bool           quiet_on      = true;    /* THE SETTINGS PAGE: the rule itself, off = every raise lights (wake_quiet_set) */
/* LOWER-TO-SLEEP: awake, the inverse of the raise -- the pose turns
 * LOWER_DEG away from the one he looked from and the face turns from
 * the eyes (gravity out of the glass under LOWER_FACE: the arm hanging,
 * the face down or away), held LOWER_HOLD_S with no touch for
 * TOUCH_QUIET_S: dark within the second. A board tilted on the desk
 * never lowers (nothing was looked from); a tilt that keeps the face up
 * (the bowl at play) never lowers either. */
static float          LOWER_DEG     = 30.0f;   /* live: wake-set */
static float          LOWER_FACE    = 0.25f;   /* live: wake-set */
static const float    LOWER_HOLD_S  = 0.8f;
static const float    LOWER_QUIET_S = 0.3f;  /* ...and come to rest: no motion over main's MOTION_G for this long (a bowl at play, a swinging arm: not a lowering -- the 12/20 s timers take those) */
static const float    TOUCH_QUIET_S = 1.0f;
static const float    LOOK_STEADY_S = 0.5f;  /* the face up and quiet this long = "he is looking from here" */
/* THE HANG SLUMP (design/watch-ideas.md A3, 2026-09-23): a raise from an
 * arm that hung -- the nap's polls in the hanging posture (raise.cpp's
 * run, the day log's own posture line) for HANG_MIN_S or more before
 * the raise -- is told to main, which draws the gel spread on the wall
 * and standing up over its first half second, the eyes after */
static const float    HANG_MIN_S    = 60.0f;
/* SHOWING SOMEONE (design/watch-ideas.md B18, 2026-09-23): awake, the
 * glass turned away from him (raise_show: >= 30 deg from the look pose,
 * the face pointing off, gravity across the strap -- the forearm still
 * up) and held steady SHOW_HOLD_S is company: once per look, main plays
 * the gel's most presentable thing and dims his rings. While the pose is
 * company's the lowering clock rests (the idle timers still end the
 * spell); a hanging arm keeps the lowering's road. */
static const float    SHOW_HOLD_S   = 2.0f;
/* which way the glass faces in the IMU's frame: raise.cpp's FACE_UP_Z
 * (the chip sits on the back of the board; `wake-cal` prints "face") */

/* --- the state --------------------------------------------------------------- */

static raise_state_t rs;                 /* THE EPISODE POSE and the poll's stillness (raise.cpp) */
static vec3_t look_pose = {0, 0, 1};     /* the pose he looked from */
static bool   look_valid = false;
static float  look_steady_s = 0, lower_s = 0;
static bool   wom_armed = false;
static int    wom_level = 0;              /* INT1's resting level after arming: the engine TOGGLES it on motion */
static struct {
    char     cause[40];
    uint32_t motion, raises, shakes, bumps, turns, lowers;
    uint32_t quiet;                             /* QUIET TIME: raises slept through as turns in the sheets */
    uint32_t nap_min, last_nap_motion;
    uint32_t nap_motion, t_nap, last_log_min;   /* the nap in progress */
    uint32_t still_since;                       /* the nap's last motion (or its start): the poll ladder's clock */
    bool     from_hang;                         /* THE HANG SLUMP: the raise that lit the glass came from a hanging arm */
    float    hung_s;                            /*   ...how long it hung (the log's number) */
    uint32_t slumps;
    float    show_s;                            /* SHOWING SOMEONE: seconds the pose has been company's */
    bool     shown, show_pending;               /*   read once per look; waiting for main to take it */
    uint32_t shows;
} st;

/* the poll interval for a wrist still this long (THE NAP BREATHES) */
static uint32_t nap_poll_us(uint32_t still_s)
{
    if (still_s >= NAP_SLOW2_S) return NAP_POLL_SLOW2_US;
    if (still_s >= NAP_SLOW1_S) return NAP_POLL_SLOW1_US;
    return NAP_POLL_US;
}

static float vlen(const vec3_t *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }
float wake_face_of(const vec3_t *a) { return raise_face_of(a); }
static float angle_deg(const vec3_t *a, const vec3_t *b) { return raise_angle_deg(a, b); }

/* --- the engine ---------------------------------------------------------------- */

static vec3_t poll_last = {0, 0, 0};
static bool   poll_have = false;
static int    poll_same = 0;

static bool arm(void)
{
    wom_armed = imu_wom_enable(WOM_MG, WOM_BLANK);
    if (!wom_armed) return false;
    delay(WOM_SETTLE_MS);
    wom_level = imu_int1() ? 1 : 0;
    return true;
}

/* the burst after the engine's word (or the poll's trigger): the full
 * rate read for BURST_MS, longer while it keeps moving, up to
 * BURST_MAX_MS; its summary -- the held end pose (the tail's mean), the
 * peak jerk, the span of jerks over the wake jerk -- goes to raise.cpp
 * for the verdict. "Moving" is a jerk in the tail over HOLD_JERK_G OR
 * the pose turned HOLD_TURN_DEG across the tail: the second is new --
 * a smooth raise never jerked, so the old burst ended at 300 ms with
 * the wrist still on its way up. */
static void burst(vec3_t *end_pose, float *peak, uint32_t *over_span_ms)
{
    uint32_t t0 = millis();
    vec3_t last = {0, 0, 0}, sum = {0, 0, 0};
    int n = 0;
    float peak_jerk = 0;
    uint32_t over_first = 0, over_last = 0;
    const int TAIL = 40;                      /* ~180 ms at 224 Hz: the held pose's window */
    vec3_t tail[TAIL]; float tailj[TAIL]; int th = 0, tn = 0;
    while (true) {
        uint32_t el = millis() - t0;
        if (el >= BURST_MAX_MS) break;
        if (el >= BURST_MS) {
            float mj = 0;
            for (int i = 0; i < tn; i++) if (tailj[i] > mj) mj = tailj[i];
            int oldest = tn < TAIL ? 0 : th, newest = (th + TAIL - 1) % TAIL;
            if (tn >= (int)(HOLD_S * 224.0f) && mj < HOLD_JERK_G
                && angle_deg(&tail[oldest], &tail[newest]) < HOLD_TURN_DEG) break;   /* held: judged */
        }
        vec3_t a;
        if (!imu_read(&a)) { delayMicroseconds(500); continue; }
        if (el < BURST_SKIP_MS) continue;
        wrist_sample(&a, false);
        vec3_t d = { a.x - last.x, a.y - last.y, a.z - last.z };
        float j = n ? vlen(&d) : 0.0f;
        last = a;
        n++;
        if (j > peak_jerk) peak_jerk = j;
        if (j > tune.wake_jerk_g) { if (!over_first) over_first = millis(); over_last = millis(); }
        tail[th] = a; tailj[th] = j; th = (th + 1) % TAIL; if (tn < TAIL) tn++;
    }
    for (int i = 0; i < tn; i++) { sum.x += tail[i].x; sum.y += tail[i].y; sum.z += tail[i].z; }
    if (tn) { sum.x /= tn; sum.y /= tn; sum.z /= tn; } else sum = rs.last_end;
    *end_pose = sum;
    *peak = peak_jerk;
    *over_span_ms = over_first ? over_last - over_first : 0;
}

/* --- the nap ------------------------------------------------------------------- */

void wake_nap_begin(const vec3_t *pose)
{
    raise_begin(&rs, pose);               /* THE EPISODE POSE: the one it falls asleep in */
    look_valid = false;
    lower_s = 0;
    st.nap_motion = 0;
    st.last_log_min = 0;
    st.t_nap = millis();
    st.still_since = st.t_nap;
    st.from_hang = false; st.hung_s = 0;
    st.show_s = 0; st.shown = false; st.show_pending = false;
    poll_last = *pose; poll_have = true; poll_same = 0;
    if (!NAP_ENGINE) { wom_armed = false; return; }
    if (!arm()) LOG("nap: the IMU's wake-on-motion did not arm; the poll and the touch alone");
}

/* motion in the dark, by the engine's word or the poll's: the burst,
 * the verdict, and the nap goes on unless it was a raise or a shake */
static const char *judge(vec3_t *pose, const char *by)
{
    st.nap_motion++;
    st.motion++;
    st.still_since = millis();            /* motion of any verdict: the poll back at its quickest */
    clockwork_stir(st.still_since);       /* THE GENTLE WAKE: a stir in the dark, whatever the verdict */
    imu_status1();
    if (wom_armed) { imu_wom_disable(); wom_armed = false; }
    vec3_t end; float peak; uint32_t over;
    burst(&end, &peak, &over);
    float turn, face;
    int v = raise_judge(&rs, &tune, &end, peak, over, &turn, &face);
    bool quiet_hours = rest_night_at(clock_local_minute());
    /* the line a serial capture needs to explain a miss: the turn FROM
     * THE EPISODE POSE, the face it ended with and the episode's, how far
     * on from the last verdict's end, the stillness before, the hours */
    LOG("dark: motion (%s) -> %s (turned %.0f deg from the episode pose, face %.2f from the episode's %.2f, peak jerk %.2f g over %lu ms, %.0f deg on from the last end, still %.1f s before, %s)",
        by, RAISE_VERDICT_NAMES[v], turn, face, rs.episode_face, peak, (unsigned long)over,
        angle_deg(&end, &rs.last_end), rs.still_s, quiet_hours ? "the quiet hours" : "by day");
    *pose = end;
    poll_last = end; poll_have = true; poll_same = 0;
    if (quiet_on && raise_quiet(&tune, v, face, quiet_hours)) {
        /* QUIET TIME: a raise in the quiet hours that did not end face-up
         * lights nothing -- the nap goes on (the same NULL path as a turn:
         * wake_up never runs, the radio stays down; the episode pose is
         * left for the stillness to re-base) */
        st.quiet++;
        LOG("quiet time: a turn in the sheets, not a raise (face %.2f under %.2f in the quiet hours)", face, tune.quiet_face);
        raise_judged(&rs, &end);
        wrist_tick(false);
        if (NAP_ENGINE && poll_same < FROZEN_POLLS) arm();
        return NULL;
    }
    if (v == RAISE_RAISE) {
        st.raises++;
        /* THE HANG SLUMP: the arm hung a minute or more before this raise */
        st.hung_s = raise_hung_s(&rs);
        st.from_hang = st.hung_s >= HANG_MIN_S;
        if (st.from_hang) { st.slumps++; LOG("dark: the raise came from a hanging arm (%.0f s): the hang slump", st.hung_s); }
        return "a raise";
    }
    if (v == RAISE_SHAKE) { st.shakes++; return "a shake"; }
    /* a turn or a bump: NULL, and main's nap() loops straight back into
     * the light sleep -- wake_up() (the screen, the radio's net_on, the
     * link's redial) never runs for these. Pinned here on purpose: a
     * bump in the night must not cost a WiFi join. The end pose is noted
     * (the next poll's step is from it); THE EPISODE POSE stays -- it
     * moves only after the stillness (raise_poll), never at a verdict. */
    if (v == RAISE_TURN) st.turns++; else st.bumps++;
    raise_judged(&rs, &end);
    wrist_tick(false);
    if (NAP_ENGINE && poll_same < FROZEN_POLLS) arm();   /* never re-arm an engine whose stream froze */
    return NULL;
}

/* THE DARK EAR (2026-10-06, Khalid: "how would I get a notification when I
 * turn the watch on?" -- the dark house took the BLE stack down with the
 * glass; ruling 78): with a Bluetooth peer bonded the nap does not
 * light-sleep (the controller's clock would not survive it) -- it idles
 * awake for the same spell, ticking the BLE tenants every EAR_TICK_MS and
 * watching the same three wake sources by hand (the touch line, the
 * engine's INT1, the serial port). main registers the two callbacks:
 * wanted() says whether the ear is on, tick() runs the stack's work. */
static bool (*ear_wanted)(void) = NULL;
static void (*ear_tick)(void) = NULL;
static const uint32_t EAR_TICK_MS = 25;
void wake_set_ear(bool (*wanted)(void), void (*tick)(void)) { ear_wanted = wanted; ear_tick = tick; }
/* THE EAR'S LEDGER: the last spell's kind and length, read by main after each wake_nap_once */
static bool last_spell_ear = false;
static uint32_t last_spell_ms = 0;
void wake_nap_last(bool *awake_ear, uint32_t *ms) { if (awake_ear) *awake_ear = last_spell_ear; if (ms) *ms = last_spell_ms; }

const char *wake_nap_once(vec3_t *pose)
{
    /* the timer: to just past the next minute's turn, so the record is
     * written from here; a minute flat with no clock */
    uint64_t us = NAP_TIMER_US;
    if (clock_is_set()) {
        struct tm lt; clock_local(&lt, NULL);
        us = (uint64_t)(60 - lt.tm_sec) * 1000000ULL + 500000ULL;
    }
    uint32_t poll = nap_poll_us((millis() - st.still_since) / 1000);   /* the safety net's poll, slower the longer it lies still */
    if (us > poll) us = poll;
    esp_sleep_enable_timer_wakeup(us);
    gpio_wakeup_enable((gpio_num_t)PIN_TP_INT, GPIO_INTR_LOW_LEVEL);
    if (wom_armed) {
        /* the engine toggles INT1 on motion: wake on the level it is not at */
        if ((imu_int1() ? 1 : 0) != wom_level) wom_level = imu_int1() ? 1 : 0;   /* moved during the work: taken as the new rest */
        gpio_wakeup_enable((gpio_num_t)PIN_IMU_INT1, wom_level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    }
    esp_sleep_enable_gpio_wakeup();
    /* a word on the serial port wakes it too (the dock, the provisioning
     * script: send a blank line first, the waking bytes are lost) */
    uart_set_wakeup_threshold(UART_NUM_0, 3);
    esp_sleep_enable_uart_wakeup(UART_NUM_0);
    esp_sleep_wakeup_cause_t cause;
    uint32_t t_spell = millis();
    if (ear_wanted && ear_tick && ear_wanted()) {
        /* THE DARK EAR: awake through the spell, the stack ticking; the motor may be mid-bump (a phone's notice) */
        uint32_t t0 = millis(), span = (uint32_t)(us / 1000ULL);
        cause = ESP_SLEEP_WAKEUP_TIMER;
        last_spell_ear = true;
        for (;;) {
            wdt_feed(WDT_SPELL_EAR);           /* THE LOOP'S PULSE: the ear's spell is the loop's thread, lingering */
            ear_tick();
            if (Serial.available()) { cause = ESP_SLEEP_WAKEUP_UART; break; }
            if (digitalRead(PIN_TP_INT) == LOW) { cause = ESP_SLEEP_WAKEUP_GPIO; break; }
            if (wom_armed && (imu_int1() ? 1 : 0) != wom_level) { cause = ESP_SLEEP_WAKEUP_GPIO; break; }
            if ((uint32_t)(millis() - t0) >= span) break;
            delay(EAR_TICK_MS);
        }
    } else {
        motor_off();                               /* THE BARE MOTOR never hums through a nap (LEDC stops, the pin would hold) */
        wdt_feed(WDT_SPELL_SLEEP);                 /* THE LOOP'S PULSE: fresh at the edge; the MWDT does not count through light sleep */
        esp_light_sleep_start();
        cause = esp_sleep_get_wakeup_cause();
        wdt_feed(WDT_SPELL_SLEEP);
        last_spell_ear = false;
    }
    uint32_t now = millis();
    last_spell_ms = now - t_spell;                     /* THE EAR'S LEDGER: the spell's length, whichever kind */
    if (cause == ESP_SLEEP_WAKEUP_UART) return "a word on the serial port";
    if (cause == ESP_SLEEP_WAKEUP_GPIO && digitalRead(PIN_TP_INT) == LOW) return "a touch";
    if (cause == ESP_SLEEP_WAKEUP_GPIO) return judge(pose, "the engine");
    /* the timer: 4x a second -- one raw sample (read straight from the
     * registers: the stream's data-ready flag does not set in WoM mode)
     * for the posture, the minute's record, and THE SAFETY NET: the pose
     * judged here too, since the engine has been known to say nothing */
    vec3_t a = *pose;
    if (imu_read_raw(&a) && vlen(&a) > 0.3f) {
        if (a.x == poll_last.x && a.y == poll_last.y && a.z == poll_last.z) poll_same++; else poll_same = 0;
        poll_last = a; poll_have = true;
        if (wom_armed && poll_same >= FROZEN_POLLS) {
            imu_wom_disable(); wom_armed = false;
            LOG("nap: the engine's stream froze (%d polls identical): polling alone", FROZEN_POLLS);
        }
        wrist_sample(&a, false); *pose = a;
        /* THE RAISE'S RULE: the jerk since the last poll, the turn from
         * THE EPISODE POSE, the face come up -- and the stillness that
         * re-bases the episode pose (raise.cpp) */
        bool rebased = false;
        const char *trigger = raise_poll(&rs, &tune, &a, (float)us / 1e6f, &rebased);
        if (rs.jerk >= tune.still_jerk_g) { st.still_since = now; clockwork_stir(now); }   /* any motion the poll sees: the ladder back at 250 ms (a raise after a long stillness lands at the quick poll); THE GENTLE WAKE's stir too */
        if (rebased)
            LOG("dark: the episode pose re-based (%.0f deg from the old, face %.2f -> %.2f, still %.1f s; %lu re-bases this nap)",
                rs.rebase_deg, rs.rebase_face_from, rs.episode_face, rs.still_s, (unsigned long)rs.rebases);
        if (trigger) {
            char by[40];
            snprintf(by, sizeof by, "the poll: %s", trigger);
            return judge(pose, by);
        }
    }
    wrist_tick(false);
    if (clockwork_due(now, clock_local_minute(), clock_local_date())) return "the clockwork";   /* CLOCKWORK: the alarm's minute (the one thing allowed to wake him), the timer's zero when he is up */
    {   /* THE GENTLE WAKE (D35): a finished spot fed to the rule (today's spots end lit; the coming background ring lands here), then the window's say */
        float bpm; bool ok;
        if (ppg_spot(&bpm, &ok)) gentle_pulse_seen(ppg_spot_done(), bpm, ok, now, rest_dozing_now());
        { static uint32_t ring_seen = 0; int rb = 0; uint32_t rage = 0;   /* THE PULSE IN THE DAY's reads in the dark (ppg_dark_poll) land in the ring: the gentle wake takes each once */
          if (ppg_recent_seq() != ring_seen) { ring_seen = ppg_recent_seq(); if (ppg_rest_now(&rb, &rage)) gentle_pulse((float)rb, true, now, rest_dozing_now()); } }
        if (clockwork_gentle_due(now, clock_local_minute(), clock_local_date(), rest_dozing_now())) return "the clockwork";
    }
    uint32_t nap_min = (now - st.t_nap) / 60000;
    if (nap_min - st.last_log_min >= NAP_LOG_MIN) {
        st.last_log_min = nap_min;
        LOG("dark: %lu min, %lu motion wakes (%.1f a minute), acc %.2f %.2f %.2f, INT1 %d, poll %lu ms (still %lu s)",
            (unsigned long)nap_min, (unsigned long)st.nap_motion,
            nap_min ? (float)st.nap_motion / nap_min : 0.0f, a.x, a.y, a.z, imu_int1(),
            (unsigned long)(poll / 1000), (unsigned long)((now - st.still_since) / 1000));
    }
    return NULL;
}

void wake_woke(const char *why, const vec3_t *pose)
{
    if (imu_wom_on()) imu_wom_disable();
    wom_armed = false;
    strncpy(st.cause, why, sizeof st.cause - 1);
    if (!strcmp(why, "a raise") || !strcmp(why, "a shake") || !strcmp(why, "a touch")) clockwork_stir(millis());   /* THE GENTLE WAKE: his hand or his arm is a stir */
    look_valid = false;
    look_steady_s = 0;
    lower_s = 0;
    st.show_s = 0; st.shown = false; st.show_pending = false;
    if (st.t_nap) {
        uint32_t s = (millis() - st.t_nap) / 1000;
        st.nap_min += s / 60;
        st.last_nap_motion = st.nap_motion;
        LOG("dark: napped %lu s, %lu motion wakes", (unsigned long)s, (unsigned long)st.nap_motion);
        st.t_nap = 0;
    }
    if (!strcmp(why, "a raise")) { look_pose = *pose; look_valid = true; }
}

/* --- awake: lower-to-sleep -------------------------------------------------- */

bool wake_lowered(float dt, const vec3_t *pose_lp, uint32_t last_motion_ms, uint32_t last_touch_ms,
                  bool touch_down, bool busy)
{
    float face = wake_face_of(pose_lp);
    uint32_t now = millis();
    bool quiet = now - last_motion_ms > 150;
    if (face >= tune.raise_face && quiet) {
        look_steady_s += dt;
        if (look_steady_s >= LOOK_STEADY_S) { look_pose = *pose_lp; look_valid = true; st.shown = false; }   /* he looks again: a new window for company */
    } else look_steady_s = 0;
    if (!look_valid) { lower_s = 0; st.show_s = 0; return false; }
    float turn = angle_deg(pose_lp, &look_pose);
    bool at_rest = now - last_motion_ms > (uint32_t)(LOWER_QUIET_S * 1000) && !busy;
    /* SHOWING SOMEONE: the glass turned away and held with the forearm up
     * is company, not a lowering -- the lowering clock rests while it is */
    if (at_rest && raise_show(&look_pose, pose_lp)) {
        st.show_s += dt;
        if (st.show_s >= SHOW_HOLD_S && !st.shown) {
            st.shown = true; st.show_pending = true; st.shows++;
            LOG("shown: the glass turned %.0f deg from the look and held %.1f s (face %.2f, across the strap): company", turn, st.show_s, face);
        }
        lower_s = 0;
        return false;
    }
    st.show_s = 0;
    bool lowered = face < LOWER_FACE && turn >= LOWER_DEG && at_rest;
    bool untouched = now - last_touch_ms > (uint32_t)(TOUCH_QUIET_S * 1000) && !touch_down;
    if (!(lowered && untouched)) { lower_s = 0; return false; }
    lower_s += dt;
    if (lower_s < LOWER_HOLD_S) return false;
    st.lowers++;
    LOG("lowered: %.0f deg from the look, face %.2f, held %.1f s", turn, face, lower_s);
    lower_s = 0;
    look_valid = false;
    return true;
}

/* --- serial ------------------------------------------------------------------- */

static void status(void)
{
    Serial.printf("[%8lu] wake: last %s | motion wakes %lu (raises %lu shakes %lu bumps %lu turns %lu quiet-time turns %lu), last nap %lu, "
                  "napped %lu min | lowered %lu | wom %u mg blank %u | poll %lu ms, %lu ms after %lu s still, %lu ms after %lu s | raise >= %.0f deg from the episode pose "
                  "(or the face up %.2f from the episode's) face >= %.2f held %.2f s (jerk < %.2f g, turn < %.0f deg) pick-up >= %.2f g shake > %.2f g over %lu ms "
                  "| the episode pose re-based after %.1f s still (jerk < %.2f g) at >= %.0f deg off, a re-judge %.0f deg on from the last end (episode now: face %.2f, %lu re-bases, still %.1f s) "
                  "| quiet time: a raise in the quiet hours lights only face >= %.2f (%s now) | lower >= %.0f deg face < %.2f held %.1f s at rest %.1f s touch quiet %.1f s "
                  "| the hang slump: hung %.0f s before the last raise (%s; needs %.0f), %lu slumps | shown: %lu (held %.1f s, %s)\n",
                  (unsigned long)millis(), st.cause[0] ? st.cause : "-",
                  (unsigned long)st.motion, (unsigned long)st.raises, (unsigned long)st.shakes,
                  (unsigned long)st.bumps, (unsigned long)st.turns, (unsigned long)st.quiet, (unsigned long)st.last_nap_motion,
                  (unsigned long)st.nap_min, (unsigned long)st.lowers, WOM_MG, WOM_BLANK,
                  (unsigned long)(NAP_POLL_US / 1000), (unsigned long)(NAP_POLL_SLOW1_US / 1000), (unsigned long)NAP_SLOW1_S,
                  (unsigned long)(NAP_POLL_SLOW2_US / 1000), (unsigned long)NAP_SLOW2_S,
                  tune.raise_deg, tune.face_rise, tune.raise_face, HOLD_S, HOLD_JERK_G, HOLD_TURN_DEG, tune.pickup_g, tune.wake_jerk_g, (unsigned long)tune.shake_span_ms,
                  tune.rebase_still_s, tune.still_jerk_g, tune.rebase_min_deg, tune.step_deg, rs.episode_face, (unsigned long)rs.rebases, rs.still_s,
                  tune.quiet_face, rest_night_at(clock_local_minute()) ? "the quiet hours" : "by day",
                  LOWER_DEG, LOWER_FACE, LOWER_HOLD_S, LOWER_QUIET_S, TOUCH_QUIET_S,
                  st.hung_s, st.from_hang ? "the slump" : "no slump", HANG_MIN_S, (unsigned long)st.slumps,
                  (unsigned long)st.shows, SHOW_HOLD_S, st.show_s > 0 ? "company now" : st.shown ? "read this look" : "-");
}

/* `wake-cal`: the resting pose, to tune the thresholds by hand */
static void cal(void)
{
    vec3_t a = {0, 0, 0}, s = {0, 0, 0};
    int n = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < 200) { if (imu_read(&a)) { s.x += a.x; s.y += a.y; s.z += a.z; n++; } delay(2); }
    if (n) { s.x /= n; s.y /= n; s.z /= n; }
    Serial.printf("[%8lu] wake-cal: pose %.2f %.2f %.2f (|a| %.2f, %d samples) face %.2f (flat face-up should read +1.00) "
                  "posture %s | from the episode pose %.0f deg (its face %.2f) | the look pose %s%.0f deg off | INT1 %d wom %s\n",
                  (unsigned long)millis(), s.x, s.y, s.z, vlen(&s), n, wake_face_of(&s),
                  wrist_posture_name(wrist_posture_of(&s)), angle_deg(&s, &rs.episode), rs.episode_face,
                  look_valid ? "" : "(none) ", look_valid ? angle_deg(&s, &look_pose) : 0.0f,
                  imu_int1(), imu_wom_on() ? "on" : "off");
}

/* `wom-test [mg] [seconds] [sel] [blank]`: arm the engine awake and
 * watch both INT pins, the status registers and the raw data -- the
 * tuning aid for WOM_MG / WOM_BLANK (a still board: one settle toggle
 * then nothing; a turn of the wrist: a toggle). ``sel`` is CAL1_H[7:6]
 * by hand (2 = INT1, the default). Then the ordinary stream again. */
static void wom_test(int mg, int secs, int sel, int blank)
{
    if (mg <= 0) mg = WOM_MG;
    if (secs <= 0) secs = 4;
    if (sel < 0) sel = 2;
    if (blank < 0) blank = WOM_BLANK;
    pinMode(PIN_IMU_INT2, INPUT_PULLDOWN);
    bool ok = imu_wom_enable_sel((uint8_t)mg, (uint8_t)blank, (uint8_t)sel);
    uint32_t t0 = millis(), last = 0;
    int events = 0, samples = 0;
    int p1 = digitalRead(PIN_IMU_INT1), p2 = digitalRead(PIN_IMU_INT2);
    vec3_t a = {0, 0, 0}, raw = {0, 0, 0};
    Serial.printf("[%8lu] wom-test: armed %s at %d mg sel %d blank %d; INT1(gpio%d) %d INT2(gpio%d) %d; ctrl1 %02X ctrl2 %02X ctrl7 %02X cal1 %02X %02X; watching %d s\n",
                  (unsigned long)t0, ok ? "ok" : "FAILED", mg, sel, blank, PIN_IMU_INT1, p1, PIN_IMU_INT2, p2,
                  imu_reg(0x02), imu_reg(0x03), imu_reg(0x08), imu_reg(0x0B), imu_reg(0x0C), secs);
    while (millis() - t0 < (uint32_t)secs * 1000) {
        wdt_feed(WDT_SPELL_AID);               /* THE LOOP'S PULSE: a long aid is not a hang */
        if (imu_read(&a)) samples++;
        imu_read_raw(&raw);
        int q1 = digitalRead(PIN_IMU_INT1), q2 = digitalRead(PIN_IMU_INT2);
        if (q1 != p1 || q2 != p2) {
            uint8_t s0 = imu_reg(0x2E), si = imu_reg(0x2D), s1 = imu_status1();
            events++;
            Serial.printf("[%8lu] wom-test: INT1 %d INT2 %d (status0 %02X statusint %02X status1 %02X) raw %.2f %.2f %.2f\n",
                          (unsigned long)millis(), q1, q2, s0, si, s1, raw.x, raw.y, raw.z);
            p1 = q1; p2 = q2;
        }
        if (millis() - last >= 1000) {
            last = millis();
            Serial.printf("[%8lu] wom-test: %d ready samples, %d pin changes, INT1 %d INT2 %d, raw %.2f %.2f %.2f\n",
                          (unsigned long)last, samples, events, p1, p2, raw.x, raw.y, raw.z);
        }
        delay(5);
    }
    imu_wom_disable();
    Serial.printf("[%8lu] wom-test: done, %d pin changes in %d s; the full stream again (INT1 %d)\n",
                  (unsigned long)millis(), events, secs, digitalRead(PIN_IMU_INT1));
}

bool wake_raise_from_hang(void) { return st.from_hang; }
bool wake_take_shown(void) { bool p = st.show_pending; st.show_pending = false; return p; }
bool wake_showing(void) { return st.show_s > 0; }

void wake_quiet_set(bool on) { quiet_on = on; }
bool wake_quiet_on(void) { return quiet_on; }

void wake_cmd(int argc, char **argv)
{
    const char *v = argv[0];
    if (!strcmp(v, "wake")) status();
    else if (!strcmp(v, "wake-cal")) cal();
    else if (!strcmp(v, "wom-test"))
        wom_test(argc > 1 ? atoi(argv[1]) : 0, argc > 2 ? atoi(argv[2]) : 0,
                 argc > 3 ? atoi(argv[3]) : -1, argc > 4 ? atoi(argv[4]) : -1);
    else if (!strcmp(v, "wake-engine")) {
        NAP_ENGINE = argc > 1 && !strcmp(argv[1], "on");
        Serial.printf("[%8lu] wake-engine: %s (takes effect at the next nap)\n", (unsigned long)millis(), NAP_ENGINE ? "on" : "off -- the poll runs the nap");
    }
    else if (!strcmp(v, "wake-set")) {
        /* wake-set raise-deg raise-face [lower-deg lower-face [quiet-face
         * [face-rise rebase-still-s step-deg]]]: tune the raise (and the
         * lowering, QUIET TIME's face, and THE EPISODE POSE's numbers)
         * live, on his real motion, no reflash */
        if (argc > 2) { tune.raise_deg = atof(argv[1]); tune.raise_face = atof(argv[2]); }
        if (argc > 4) { LOWER_DEG = atof(argv[3]); LOWER_FACE = atof(argv[4]); }
        if (argc > 5) tune.quiet_face = atof(argv[5]);
        if (argc > 8) { tune.face_rise = atof(argv[6]); tune.rebase_still_s = atof(argv[7]); tune.step_deg = atof(argv[8]); }
        Serial.printf("[%8lu] wake-set: raise >= %.0f deg face >= %.2f | lower >= %.0f deg face < %.2f | quiet time lights face >= %.2f | the face up %.2f, re-based after %.1f s still, a re-judge %.0f deg on\n",
                      (unsigned long)millis(), tune.raise_deg, tune.raise_face, LOWER_DEG, LOWER_FACE, tune.quiet_face,
                      tune.face_rise, tune.rebase_still_s, tune.step_deg);
    }
}
