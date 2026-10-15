/* THE SKY -- sky.h says what; this is how. */
#include "sky.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* --- the vocabulary and its light ------------------------------------------------ */
enum { SKY_NONE = -1, SKY_RAIN = 0, SKY_SNOW, SKY_COLD, SKY_HOT, SKY_CLEAR, SKY_STORM, SKY_WIND, SKY_DARK, SKY_N };
static const char *const NAMES[SKY_N] = { "rain", "snow", "cold", "hot", "clear", "storm", "wind", "dark" };

/* the tint: factors on the body, the shade and the shine (r, g, b),
 * each calm and small -- light on the gel, never a new palette */
typedef struct { float body[3], shade[3], shine[3]; } tint_t;
static const tint_t TINTS[SKY_N] = {
    /* rain  */ { { 0.96f, 0.96f, 0.98f }, { 0.96f, 0.96f, 1.00f }, { 0.96f, 0.97f, 1.00f } },
    /* snow  */ { { 1.06f, 1.06f, 1.06f }, { 1.08f, 1.08f, 1.08f }, { 1.10f, 1.10f, 1.10f } },
    /* cold  */ { { 0.96f, 1.00f, 1.00f }, { 0.96f, 1.00f, 1.00f }, { 0.85f, 0.95f, 1.00f } },
    /* hot   */ { { 1.00f, 0.97f, 0.90f }, { 1.00f, 0.98f, 0.92f }, { 1.00f, 0.96f, 0.88f } },
    /* clear */ { { 1.02f, 1.02f, 1.02f }, { 1.00f, 1.00f, 1.00f }, { 1.05f, 1.05f, 1.05f } },
    /* storm */ { { 0.88f, 0.88f, 0.90f }, { 0.85f, 0.85f, 0.88f }, { 0.95f, 0.95f, 0.97f } },
    /* wind  */ { { 1.00f, 1.00f, 1.00f }, { 0.96f, 0.96f, 0.96f }, { 1.04f, 1.04f, 1.04f } },
    /* dark  */ { { 0.90f, 0.90f, 0.90f }, { 0.85f, 0.85f, 0.85f }, { 0.92f, 0.92f, 0.92f } },
};

/* --- the state ------------------------------------------------------------------- */
static struct {
    int   word[SKY_WORDS];      /* SKY_NONE past the held ones */
    int   n;
    float age;                  /* since told */
    char  who[16];
    float shiver_t;             /* > 0 while the cold's shiver plays */
    float lid_t;                /* > 0 while the heat's droop holds */
    float laps;                 /* the storm's extra laps, in [0, 1) */
    float heavy;                /* THE SKY'S WEIGHT: 0..1, ramping toward 1 while rain / storm is held */
    float phase;                /* the followed breath */
    float last_rest;            /* rest's phase last seen */
    bool  phase_init;
    char  logline[64];
} S;

static int find(const char *w)
{
    for (int i = 0; i < SKY_N; i++) if (!strcmp(w, NAMES[i])) return i;
    return SKY_NONE;
}

static bool has(int k)
{
    for (int i = 0; i < S.n; i++) if (S.word[i] == k) return true;
    return false;
}

void sky_begin(void)
{
    memset(&S, 0, sizeof S);
    for (int i = 0; i < SKY_WORDS; i++) S.word[i] = SKY_NONE;
}

void sky_forget(void)
{
    for (int i = 0; i < SKY_WORDS; i++) S.word[i] = SKY_NONE;
    S.n = 0; S.age = 0; S.who[0] = 0;
    S.shiver_t = S.lid_t = 0;
    /* the storm's laps and the followed breath are left where they are: no rewind, no jump */
}

bool sky_valid_word(const char *w) { return w && find(w) != SKY_NONE; }

bool sky_set(const char *words, const char *who)
{
    if (!words || !words[0] || !strcmp(words, "none")) { sky_forget(); return true; }
    char buf[48]; strncpy(buf, words, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    int got[SKY_WORDS]; int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        int k = find(tok);
        if (k == SKY_NONE) return false;                  /* one unknown word: nothing changes */
        bool dup = false;
        for (int i = 0; i < n; i++) if (got[i] == k) dup = true;
        if (dup) continue;
        if (n < SKY_WORDS) got[n++] = k;                  /* a third word is not carried */
    }
    if (!n) return false;
    sky_forget();
    for (int i = 0; i < n; i++) S.word[i] = got[i];
    S.n = n;
    S.age = 0;
    strncpy(S.who, who ? who : "?", sizeof S.who - 1);
    return true;
}

const char *sky_tick(float dt)
{
    if (dt < 0) dt = 0; else if (dt > 0.25f) dt = 0.25f;
    if (S.shiver_t > 0) { S.shiver_t -= dt; if (S.shiver_t < 0) S.shiver_t = 0; }
    if (S.lid_t > 0) { S.lid_t -= dt; if (S.lid_t < 0) S.lid_t = 0; }
    {   /* THE SKY'S WEIGHT: a linear ramp over SKY_HEAVY_EASE_S toward 1 while wet, toward 0 when not (a forget eases out too) */
        float want = (has(SKY_RAIN) || has(SKY_STORM)) ? 1.0f : 0.0f;
        float step = dt / SKY_HEAVY_EASE_S;
        if (S.heavy < want) { S.heavy += step; if (S.heavy > want) S.heavy = want; }
        else if (S.heavy > want) { S.heavy -= step; if (S.heavy < want) S.heavy = want; }
    }
    if (!S.n) return NULL;
    S.age += dt;
    if (has(SKY_STORM)) S.laps = fmodf(S.laps + SKY_STORM_LAPS_HZ * dt, 1.0f);
    if (S.age >= SKY_HOLD_S) {
        snprintf(S.logline, sizeof S.logline, "sky: %s%s%s forgotten (held %.0f s)", NAMES[S.word[0]],
                 S.n > 1 ? " " : "", S.n > 1 ? NAMES[S.word[1]] : "", SKY_HOLD_S);
        sky_forget();
        return S.logline;
    }
    return NULL;
}

void sky_raise(void)
{
    if (has(SKY_COLD)) S.shiver_t = SKY_SHIVER_S;
    if (has(SKY_HOT)) S.lid_t = SKY_HOT_LID_S;
}

void sky_fx(sky_fx_t *out)
{
    out->squash = 0; out->a2 = 0; out->eye_kind = -1; out->gel_extra = S.laps; out->breath_k = 1.0f;
    out->heavy = S.heavy;                                  /* eases out after a forget: read before the early return */
    if (!S.n) return;
    if (has(SKY_RAIN)) out->squash = SKY_RAIN_SQUASH;
    if (S.shiver_t > 0) {
        float t = SKY_SHIVER_S - S.shiver_t;               /* time into the shiver */
        out->a2 = SKY_SHIVER_A2 * sinf(2.0f * (float)M_PI * SKY_SHIVER_HZ * t) * (S.shiver_t / SKY_SHIVER_S);
    }
    if (S.lid_t > 0) out->eye_kind = GEL_EYE_DROOPY;
    if (has(SKY_HOT)) out->breath_k = SKY_HOT_BREATH_K;
}

float sky_heavy(void) { return S.heavy; }

float sky_breath_phase(float rest_phase)
{
    if (!S.phase_init) { S.phase = rest_phase; S.last_rest = rest_phase; S.phase_init = true; return S.phase; }
    float d = rest_phase - S.last_rest;
    S.last_rest = rest_phase;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;     /* rest's phase may wrap */
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    S.phase += d * (has(SKY_HOT) ? SKY_HOT_BREATH_K : 1.0f);
    if (S.phase > 1000.0f) S.phase -= 2.0f * (float)M_PI * 159.0f;   /* kept small; a whole number of turns */
    return S.phase;
}

static void mul3(gel_rgba_t *c, const float k[3])
{
    c->r *= k[0]; c->g *= k[1]; c->b *= k[2];
    if (c->r > 1) c->r = 1;
    if (c->g > 1) c->g = 1;
    if (c->b > 1) c->b = 1;
}

void sky_tint(gel_pal_t *pal)
{
    for (int i = 0; i < S.n; i++) {
        const tint_t *t = &TINTS[S.word[i]];
        mul3(&pal->body, t->body);
        mul3(&pal->shade, t->shade);
        mul3(&pal->shine, t->shine);
    }
}

bool sky_known(void) { return S.n > 0; }
bool sky_hot(void) { return has(SKY_HOT); }
const char *sky_word(int i) { return (i >= 0 && i < S.n) ? NAMES[S.word[i]] : ""; }
float sky_age_s(void) { return S.n ? S.age : 0; }

void sky_status(char *out, int cap)
{
    if (!S.n) { snprintf(out, cap, "sky: none known (nothing shows) | the words: rain snow cold hot clear storm wind dark"); return; }
    char heavy[48] = "";
    if (S.heavy > 0) snprintf(heavy, sizeof heavy, " | heavier: damping x%.2f (%.0f %% in)", sky_heavy_k(S.heavy), S.heavy * 100.0f);
    snprintf(out, cap, "sky: %s%s%s (%s), %.0f s old, forgotten in %.0f s%s%s%s%s%s%s",
             NAMES[S.word[0]], S.n > 1 ? " " : "", S.n > 1 ? NAMES[S.word[1]] : "", S.who,
             S.age, SKY_HOLD_S - S.age,
             has(SKY_RAIN) ? " | fuller" : "", has(SKY_COLD) ? " | the shine cooled, a shiver on the raise" : "",
             has(SKY_HOT) ? " | a slow breath, droopy on the raise, a drink welcome (the settle deeper, the eyes longer, the hair the rain's)" : "", has(SKY_STORM) ? " | the bubble clock quick" : "",
             (has(SKY_SNOW) || has(SKY_CLEAR) || has(SKY_WIND) || has(SKY_DARK)) ? " | the light" : "", heavy);
}
