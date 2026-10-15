/* THE PULSE AS THE BREATH and THE HEARTBEAT HANDED BACK (design/
 * watch-ideas.md D31 + D32; 2026-10-08, THE OVERHAUL WAVE): the one
 * true biometric the wrist has -- the MAX30102's spot read (ppg.cpp:
 * `ppg_spot(&bpm, &ok)`, one clean result at a time) -- given back
 * to him twice, never as a number on the glass:
 *
 *   the breath   (D31) the gel's RESTING breath follows his resting
 *                pulse, eased over a minute: a raise after a hard
 *                hour shows a quick breath, a raise at rest a slow
 *                one -- he sees himself calm down. rest.cpp owns the
 *                breath and reads the arithmetic from here.
 *   the beats    (D32) a pet while calm, with a pulse known, answers
 *                with HIS heartbeat on the motor -- one soft tap per
 *                pulse for two seconds -- instead of the purr. main
 *                chooses (THE BUMPS' gate still says whether anything
 *                plays at all), motor.cpp / haptic.cpp play the plan
 *                from here.
 *
 * Pure: no Arduino, every number a named constant with its cause;
 * tools/pulse_test.cpp pins the arithmetic, tools/rest_test.cpp the
 * breath that rides it. The pulse comes in as (bpm, age in seconds):
 * main keeps the stamp of the last clean result today (one static
 * pair beside `pulse_seen`); the sampler's ring in ppg.h, when it
 * lands, feeds the same two numbers from its newest clean entry. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* a pulse older than this is nobody's now: the breath drifts back to
 * the grain's rate, a pet gets the plain purr. The sampler (another
 * hand's, tonight) spots every 5 min at rest, so a worn wrist at rest
 * is never this old; THE PULSE ROW's 30 s read counts the same way */
#define PULSE_STALE_MIN      30
/* the honest band: under 35 the sensor read a table or a half-count,
 * over 190 a double-count or a shaking wrist -- either is ignored, so
 * a bad read never makes a dead-still or a panting gel, nor a rattle
 * under a pet */
#define PULSE_BPM_MIN        35.0f
#define PULSE_BPM_MAX        190.0f
/* THE DIVISOR (D31: "his resting pulse / 4"): a man's resting breath
 * is a quarter of his pulse -- 15 a minute at 60 bpm. The gel's
 * resting breath (main's BREATH_HZ 0.55 x the grain x vigor, 33 a
 * minute) IS the gel's own 15-a-minute, tuned by eye a year ago, so
 * 60 bpm = the gel's resting rate and the breath moves in proportion
 * (90 bpm = 1.5 x it = the walk's 0.80; 45 bpm = the hour-still's
 * 0.41). The literal 60 / 4 / 60 = 0.25 Hz would sit UNDER the
 * sleeper's 0.30 at his every calm and a hard hour's 110 bpm (0.46)
 * under today's rest -- the whole band read as sleep. Ruling 84. */
#define PULSE_REST_BPM       60.0f
/* the clamp: never slower than the sleeper's rate (a 35 bpm read on a
 * tired slime), never quicker than the walk's (a 90+ bpm read is the
 * walk's breath, no quicker -- a gel does not pant) */
#define PULSE_BREATH_MIN     0.30f
#define PULSE_BREATH_MAX     0.80f
/* D31: "eased 60 s" -- twice the bands' own 30 (BREATH_EASE_S): he
 * sees himself calm down, slowly; the same ease on the way back to
 * the grain's rate when the pulse goes stale */
#define PULSE_BREATH_EASE_S  60.0f
/* D32: "one beat per pulse for two seconds" */
#define HEARTBEAT_S          2.0f
/* the bare motor's beat: a kick to start the ERM (the bump's is 25 ms
 * at 100 %: a beat is half the kick) then a short soft body -- 52 ms
 * a beat all told at 45 % (the bump settles at 55 %); the DRV2605
 * plays its Soft Bump 60 % instead (haptic.cpp). Ruling 85. */
#define HEARTBEAT_KICK_MS    12
#define HEARTBEAT_TAP_MS     40
#define HEARTBEAT_TAP_PCT    45
/* the most beats one plan holds: 190 bpm x 2 s = 6.3; the DRV2605's
 * eight-slot sequencer takes four (effect + wait pairs) */
#define HEARTBEAT_MAX        8
#define HEARTBEAT_DRV_MAX    4

static inline bool pulse_valid(float bpm) { return bpm >= PULSE_BPM_MIN && bpm <= PULSE_BPM_MAX; }

/* a pulse the creature may act on now: in the band and younger than PULSE_STALE_MIN */
static inline bool pulse_fresh(float bpm, float age_s)
{
    return pulse_valid(bpm) && age_s >= 0 && age_s < PULSE_STALE_MIN * 60.0f;
}

/* THE PULSE AS THE BREATH: the resting target from a pulse, given the
 * gel's own resting rate (rest.cpp's grain x vigor number); an invalid
 * pulse hands the resting rate back unchanged */
static inline float pulse_breath_hz(float bpm, float rest_hz)
{
    if (!pulse_valid(bpm) || rest_hz <= 0) return rest_hz;
    float hz = rest_hz * bpm / PULSE_REST_BPM;
    return hz < PULSE_BREATH_MIN ? PULSE_BREATH_MIN : hz > PULSE_BREATH_MAX ? PULSE_BREATH_MAX : hz;
}

/* THE HEARTBEAT HANDED BACK: the plan -- how many beats fall inside
 * `secs` (the first at 0; a beat whose onset lands at or past the end
 * is not played) and the period between onsets. 0 = no plan (an
 * invalid pulse: the purr plays instead). */
typedef struct { int n; uint16_t period_ms; } pulse_beats_t;
static inline int pulse_beats_plan(float bpm, float secs, pulse_beats_t *out)
{
    if (!pulse_valid(bpm) || secs <= 0) { if (out) { out->n = 0; out->period_ms = 0; } return 0; }
    uint16_t period = (uint16_t)(60000.0f / bpm + 0.5f);
    int n = (int)(((uint32_t)(secs * 1000.0f) - 1) / period) + 1;
    if (n > HEARTBEAT_MAX) n = HEARTBEAT_MAX;
    if (out) { out->n = n; out->period_ms = period; }
    return n;
}
