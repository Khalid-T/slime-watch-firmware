/* THE RAISE'S RULE, pure (2026-09-10 evening; Khalid, wearing the
 * watch: "the wake needs work, I turned it up and it did not light").
 * The nap's judgement of a motion with no IMU and no clock in it:
 * wake.cpp feeds it the poll's samples and the burst's summary,
 * tools/wake_test.cpp feeds it a synthetic wrist.
 *
 * THE CAUSE (read off wake.cpp as it was): a raise that is not brisk
 * was judged in pieces. The first poll that saw RAISE_DEG fired a burst
 * mid-raise; the burst ended before the face was up (the burst's "held"
 * test reads the jerk between 224 Hz samples, and a smooth turn of the
 * wrist jerks ~0.005 g -- it never looked like motion, so the burst
 * ended at BURST_MS whatever the wrist was doing); face < RAISE_FACE ->
 * "a turn, not a raise" -> the SLEEP POSE WAS RE-BASED to the mid-raise
 * pose. The rest of the raise never reached RAISE_DEG from the re-based
 * pose, and with no jerk over WAKE_JERK_G between two polls (a turn
 * under ~10 deg per 250 ms) the poll never fired again: the glass stayed
 * dark with the face toward the eyes. A raise in steps (the forearm up,
 * then the wrist turned) re-based at every step the same way.
 *
 * THE RULE NOW: every turn is measured from THE EPISODE POSE -- the pose
 * before the episode's first motion -- and the episode pose moves only
 * once the wrist has been STILL (the jerk between two polls under
 * still_jerk_g) for rebase_still_s at a pose rebase_min_deg off the old
 * one; a verdict that sleeps on (a turn, a bump) notes the burst's end
 * but re-bases nothing. A raise = turned >= raise_deg from the episode
 * pose OR the face come up >= face_rise from the episode's (the lesser
 * raise: a short twist toward the eyes), ending with the face >=
 * raise_face and held; the pick-up (peak >= pickup_g, face up) and the
 * shake (jerks over wake_jerk_g spanning shake_span_ms) as before. The
 * poll judges on a jerk over wake_jerk_g, on the pose turned >= raise_deg
 * from the episode pose, or on the face come up (>= raise_face now, >=
 * face_rise over the episode's) -- the turn and the face triggers only
 * once the pose is step_deg on from the last burst's end, so a turn is
 * judged once and not at every poll until the stillness re-bases it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "imu.h"          /* vec3_t; nothing else of it */

typedef struct {
    float    raise_deg;       /* a raise: the pose turned this far from THE EPISODE pose... */
    float    raise_face;      /* ...ending with the face toward the eyes: gravity out of the glass >= this */
    float    face_rise;       /* THE LESSER RAISE: the face come up this much from the episode's, to >= raise_face, with the turn under raise_deg */
    float    pickup_g;        /* picked up: the burst's peak jerk this or more, ending face toward the eyes */
    float    wake_jerk_g;     /* the poll's jerk trigger; and a jerk over this... */
    uint32_t shake_span_ms;   /* ...spanning this long in the burst = a shake */
    float    step_deg;        /* the turn / the face triggers need the pose this far on from the last burst's end (a turn is judged once) */
    float    still_jerk_g;    /* the poll's stillness: the jerk between two polls under this */
    float    rebase_still_s;  /* still this long... */
    float    rebase_min_deg;  /* ...at a pose this far from the episode's: the episode pose re-based there (a smaller drift is left) */
    float    quiet_face;      /* QUIET TIME: in the quiet hours a raise lights only with the face >= this */
} raise_tune_t;

typedef struct {
    vec3_t   episode;         /* THE EPISODE POSE: every turn is measured from here */
    float    episode_face;
    vec3_t   last_end;        /* the last burst's end pose (the episode's until one is judged) */
    vec3_t   last_poll;       /* the previous poll's sample */
    bool     have_poll;
    float    still_s;         /* seconds of stillness at the poll (0 the moment it moves, or a burst is judged) */
    float    jerk;            /* the last poll's jerk, g */
    float    rebase_deg;      /* the last re-base: how far the episode pose moved... */
    float    rebase_face_from;/* ...and the face it had before (the log's numbers) */
    uint32_t rebases;
    /* THE HANG SLUMP (design/watch-ideas.md A3): seconds the polled pose
     * has read as HANGING (the day log's posture 1: in-plane g >= 0.75 of
     * |g|, the glass more than ~48 deg from flat) -- hang_s the run under
     * way, hung_s the last run's length, kept once the pose leaves it so
     * the raise that follows can read how long the arm hung */
    float    hang_s, hung_s, unhang_s;
} raise_state_t;

enum { RAISE_BUMP, RAISE_TURN, RAISE_RAISE, RAISE_SHAKE };
extern const char *const RAISE_VERDICT_NAMES[4];

/* the numbers as the file ships them (wake.cpp keeps the live copy: `wake-set` writes it) */
const raise_tune_t *raise_defaults(void);
/* the nap begins: the episode pose is the one it fell asleep in */
void raise_begin(raise_state_t *s, const vec3_t *pose);
/* the poll: one raw sample, dt seconds after the last. Returns the
 * trigger to judge on -- "a jerk", "the pose turned", "the face came
 * up" -- or NULL to sleep on; *rebased is true when this poll moved the
 * episode pose (s->rebase_deg / rebase_face_from say from where) */
const char *raise_poll(raise_state_t *s, const raise_tune_t *t, const vec3_t *a, float dt, bool *rebased);
/* the verdict from the burst's summary: its held end pose, its peak
 * jerk, the span of jerks over wake_jerk_g; *turn_deg from the episode
 * pose, *face of the end pose */
int raise_judge(const raise_state_t *s, const raise_tune_t *t, const vec3_t *end, float peak_g, uint32_t over_span_ms,
                float *turn_deg, float *face);
/* after a verdict that sleeps on: the end pose noted as the last
 * burst's end and the next poll's reference; the stillness restarts */
void raise_judged(raise_state_t *s, const vec3_t *end);
/* QUIET TIME's rule: a raise in the quiet hours not ending face-up sleeps on */
bool raise_quiet(const raise_tune_t *t, int verdict, float face, bool quiet_hours);
/* THE HANG SLUMP (A3): how long the arm hung before the pose left the
 * hanging posture -- the run under way, or the one just left (0 = it
 * never hung this nap); wake.cpp reads it at the raise verdict against
 * HANG_MIN_S */
float raise_hung_s(const raise_state_t *s);
/* SHOWING SOMEONE (design/watch-ideas.md B18): awake, is ``pose`` the
 * glass turned away from him -- turned >= SHOW_DEG from ``look`` (the
 * pose he looked from), the face pointing off (under SHOW_FACE) with
 * gravity more ACROSS the strap than along it (the forearm still up:
 * a hanging arm has gravity along the strap, and that is the lowering,
 * not company). wake.cpp holds the clock (SHOW_HOLD_S steady) */
bool raise_show(const vec3_t *look, const vec3_t *pose);
/* gravity out of the glass, +1 = face up toward the eyes (the chip sits
 * on the back of the board, +z toward the back: face-up reads z < 0) */
float raise_face_of(const vec3_t *a);
float raise_angle_deg(const vec3_t *a, const vec3_t *b);
