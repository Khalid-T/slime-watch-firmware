#include "raise.h"
#include <math.h>

/* which way the glass faces in the IMU's frame: the chip sits on the
 * back of the board, its +z toward the back, so a face-up watch reads
 * z ~ -1 g (`wake-cal` prints "face": lay it flat, face up, and it
 * should read about +1.00; flip this sign if it reads -1). The day
 * log's flat / lying posture uses the same sign. */
static const float FACE_UP_Z = -1.0f;
/* THE HANG SLUMP (design/watch-ideas.md A3): the hanging posture as the
 * day log names it (daylog.cpp wrist_posture_of: in-plane g >= 0.75 of
 * |g| = hanging) -- the same line, so "posture hanging" means one thing */
static const float HANG_INPLANE = 0.75f;
static const float HANG_GONE_S  = 5.0f;    /* out of the hanging posture this long and the run is forgotten (a raise from the hang is judged within ~2 s of leaving it) */
/* SHOWING SOMEONE (design/watch-ideas.md B18): the glass turned away
 * from him -- SHOW_DEG from the pose he looked from, the face pointing
 * off (gravity out of the glass under SHOW_FACE, the lowering's own
 * line) -- with gravity more ACROSS the strap than along it: the forearm
 * still up, the wrist rotated to face the person across the table. The
 * strap runs 12 to 6 on the glass; main's axis map 6 puts the glass's
 * 12-6 axis on the IMU's x (screen down = +y, -x), so ALONG the strap is
 * |x| and ACROSS it is |y|. A hanging arm has gravity along the strap
 * (the lowering's road); company keeps the forearm level. */
static const float SHOW_DEG  = 30.0f;
static const float SHOW_FACE = 0.25f;

const char *const RAISE_VERDICT_NAMES[4] = { "a bump, not a raise", "a turn, not a raise", "a raise", "a shake" };

/* the numbers, each with its cause (wake.cpp's `wake-set` changes the live copy) */
static const raise_tune_t DEFAULTS = {
    20.0f,    /* raise_deg: the pose must turn this far from the episode pose (was 30; a tethered board in the hand turns less) */
    0.15f,    /* raise_face: ...and end with the face toward the eyes (was 0.35 ~70 deg; 0.15 ~50 deg, a glance not a full turn) */
    0.20f,    /* face_rise: THE LESSER RAISE -- the face up 0.20 from the episode's (a twist of >= ~12 deg toward the eyes) to >= raise_face; the arm hanging reads ~0, so a 17 deg twist to 0.21 is a raise where 20 deg would have missed it */
    0.35f,    /* pickup_g: PICKED UP -- a desk puck lies face-up, so a lift turns it 2-4 deg (Khalid's lifts, 09-09 22:45): a burst's peak jerk this or more, ending face toward the eyes, is a raise */
    0.18f,    /* wake_jerk_g: a shake still wakes it -- jerk over this between two polls fires the burst; in the burst... */
    150,      /* shake_span_ms: ...jerks over it spanning this long = a shake (a knock rings for less) */
    5.0f,     /* step_deg: a re-judge needs the pose this far on from the last burst's end (a raw sample's noise is ~1 deg; a turn in the sheets is judged once, then left to the stillness) */
    0.08f,    /* still_jerk_g: still = the jerk between two polls under this (~4.6 deg per poll: a raise slower than ~18 deg/s at 250 ms looks still, and its turn trigger fires first) */
    2.0f,     /* rebase_still_s: still this long at a new pose = he lies there now: the episode pose moves (a stepped raise pauses less; a longer pause is a new episode, and THE LESSER RAISE covers the twist that follows it) */
    5.0f,     /* rebase_min_deg: a drift under this is left where the episode is (else every 2 s still would re-base on noise) */
    0.5f,     /* quiet_face: QUIET TIME -- in the quiet hours a raise lights only with the face >= this (~60 deg from flat) */
};

const raise_tune_t *raise_defaults(void) { return &DEFAULTS; }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float vlen(const vec3_t *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }

float raise_face_of(const vec3_t *a)
{
    float l = vlen(a);
    return l < 1e-3f ? 0.0f : FACE_UP_Z * a->z / l;
}

float raise_angle_deg(const vec3_t *a, const vec3_t *b)
{
    float la = vlen(a), lb = vlen(b);
    if (la < 1e-3f || lb < 1e-3f) return 0.0f;
    float c = (a->x * b->x + a->y * b->y + a->z * b->z) / (la * lb);
    return acosf(clampf(c, -1.0f, 1.0f)) * 180.0f / (float)M_PI;
}

void raise_begin(raise_state_t *s, const vec3_t *pose)
{
    s->episode = *pose;
    s->episode_face = raise_face_of(pose);
    s->last_end = *pose;
    s->last_poll = *pose;
    s->have_poll = true;
    s->still_s = 0;
    s->jerk = 0;
    s->rebase_deg = 0;
    s->rebase_face_from = s->episode_face;
    s->rebases = 0;
    s->hang_s = s->hung_s = s->unhang_s = 0;
}

/* the hanging posture, the day log's line */
static bool hanging(const vec3_t *a)
{
    float l = vlen(a);
    if (l < 0.3f) return false;
    return sqrtf(a->x * a->x + a->y * a->y) / l >= HANG_INPLANE;
}

float raise_hung_s(const raise_state_t *s) { return s->hung_s; }

bool raise_show(const vec3_t *look, const vec3_t *pose)
{
    if (raise_angle_deg(pose, look) < SHOW_DEG) return false;   /* still where he looked from */
    if (raise_face_of(pose) >= SHOW_FACE) return false;         /* the face still toward him */
    return fabsf(pose->y) > fabsf(pose->x);                     /* across the strap: the forearm up, not hanging */
}

const char *raise_poll(raise_state_t *s, const raise_tune_t *t, const vec3_t *a, float dt, bool *rebased)
{
    vec3_t d = { a->x - s->last_poll.x, a->y - s->last_poll.y, a->z - s->last_poll.z };
    s->jerk = s->have_poll ? vlen(&d) : 0.0f;
    s->last_poll = *a;
    s->have_poll = true;
    /* THE HANG SLUMP: the run of polls in the hanging posture; the last
     * run's length is kept once the pose leaves it (the raise's own polls
     * leave it before the verdict) */
    if (hanging(a)) { s->hang_s += dt; if (s->hang_s > 1e6f) s->hang_s = 1e6f; s->hung_s = s->hang_s; s->unhang_s = 0; }
    else { s->hang_s = 0; s->unhang_s += dt; if (s->unhang_s > HANG_GONE_S) s->hung_s = 0; }
    if (s->jerk < t->still_jerk_g) { s->still_s += dt; if (s->still_s > 1e6f) s->still_s = 1e6f; }
    else s->still_s = 0;
    *rebased = false;
    /* THE EPISODE POSE moves only once he has lain still at a new pose */
    if (s->still_s >= t->rebase_still_s) {
        float off = raise_angle_deg(a, &s->episode);
        if (off >= t->rebase_min_deg) {
            s->rebase_deg = off;
            s->rebase_face_from = s->episode_face;
            s->episode = *a;
            s->episode_face = raise_face_of(a);
            s->last_end = *a;
            s->rebases++;
            *rebased = true;
        }
        return NULL;                          /* still: nothing to judge */
    }
    if (s->jerk >= t->wake_jerk_g) return "a jerk";
    /* the turn and the face are judged from the episode pose -- but only
     * once the pose has moved on from the last verdict's end, else a
     * turn in the sheets would be judged at every poll until the
     * stillness re-bases it */
    if (raise_angle_deg(a, &s->last_end) < t->step_deg) return NULL;
    if (raise_angle_deg(a, &s->episode) >= t->raise_deg) return "the pose turned";
    float face = raise_face_of(a);
    if (face >= t->raise_face && face - s->episode_face >= t->face_rise) return "the face came up";
    return NULL;
}

int raise_judge(const raise_state_t *s, const raise_tune_t *t, const vec3_t *end, float peak_g, uint32_t over_span_ms,
                float *turn_deg, float *face)
{
    *turn_deg = raise_angle_deg(end, &s->episode);
    *face = raise_face_of(end);
    float rise = *face - s->episode_face;
    /* a raise first: a brisk raise jerks like a shake, and it is the
     * held pose that says what it was */
    if (*face >= t->raise_face && (*turn_deg >= t->raise_deg || rise >= t->face_rise)) return RAISE_RAISE;
    if (over_span_ms >= t->shake_span_ms) return RAISE_SHAKE;
    if (peak_g >= t->pickup_g && *face >= t->raise_face) return RAISE_RAISE;   /* picked up, face toward the eyes */
    if (*turn_deg >= t->raise_deg) return RAISE_TURN;
    return RAISE_BUMP;
}

void raise_judged(raise_state_t *s, const vec3_t *end)
{
    s->last_end = *end;
    s->last_poll = *end;
    s->have_poll = true;
    s->still_s = 0;
}

bool raise_quiet(const raise_tune_t *t, int verdict, float face, bool quiet_hours)
{
    return quiet_hours && verdict == RAISE_RAISE && face < t->quiet_face;
}
