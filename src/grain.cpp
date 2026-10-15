/* THE GRAIN ON THE WRIST -- grain.h says what; this is how.
 *
 * The heart is a port of CPython's random.Random seeded with a str:
 * seed(str) = int.from_bytes(s + sha512(s).digest(), "big") fed to
 * MT19937's init_by_array as little-endian 32-bit words (the top word
 * zero-padded), then random() = the 53-bit float from two draws and
 * uniform(a, b) = a + (b - a) * random(). The SHA-512 and the twister
 * are the textbook ones (FIPS 180-4; Matsumoto-Nishimura 2002). The
 * twister's 2.5 KB state lives on the heap only while deriving -- the
 * knobs are recomputed only when the grain line changes.
 * No Arduino. */
#include "grain.h"
#include "gel.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* --- the tunables, with their cause ----------------------------------------- */
static const int   SLIME_ID        = 1;       /* the desk's Slime._next_id starts at 1: the one soul's first body */
static const int   DEFAULT_SEED    = 0;       /* never told: the desk's own "no grain" seed (watchlink sends 0 then too) */
/* regard -> the eyes' rise on engage(): 0.35 s at regard 1, 0.9 s at -1 (linear) */
static const float EYE_RISE_WARM_S = 0.35f;
static const float EYE_RISE_COLD_S = 0.90f;
static const float LINGER_GAIN     = 0.40f;   /* content eyes after a stroke: x(1 + gain x regard), warmth only */
static const float DWELL_GAIN      = 0.12f;   /* the desk's REGARD_DWELL_GAIN: idle a touch livelier, warmth only */
static const float REGARD_SHY_AT   = -0.25f;  /* the desk's: under it the glance's first face is a notch cooler */
/* THE BELLY TRAVELS: the desk's fullness as a resting lump -- a stomach
 * that came down the wire is calmer than a meal just fed (the feed's
 * lump is 1.0), so BELLY_LUMP_X of it; and it sinks over BELLY_SINK_S,
 * the grain line's own once-an-hour cadence (the next line refreshes it) */
static const float BELLY_LUMP_X    = 0.40f;
static const float BELLY_SINK_S    = 3600.0f;

/* --- the state ------------------------------------------------------------- */
static struct {
    bool  told;
    int   seed;
    float regard;
    int   night;
    char  mood[20];
    grain_knobs_t k;
    float belly;            /* THE BELLY TRAVELS: the desk's fullness at the line, 0 = none */
    float belly_age_s;      /* since the line */
} G = { false, DEFAULT_SEED, 0.0f, 0, "-",
        { 1, 1, 1, 1, 1, 1,  1, 0, 1, 1, 1,  0.625f, 1, 1, false,  -1 }, 0, 0 };

/* --- SHA-512 (FIPS 180-4), one shot ------------------------------------------- */
static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};
static inline uint64_t rotr64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

/* sha512 of a short message (under 111 bytes: one or two blocks) */
static void sha512(const uint8_t *msg, size_t len, uint8_t out[64])
{
    uint64_t h[8] = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
    };
    uint8_t block[256];                          /* the padded message: two blocks at most */
    memset(block, 0, sizeof block);
    if (len > 200) len = 200;
    memcpy(block, msg, len);
    block[len] = 0x80;
    size_t nblocks = (len + 1 + 16 <= 128) ? 1 : 2;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) block[nblocks * 128 - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (size_t b = 0; b < nblocks; b++) {
        uint64_t w[80];
        const uint8_t *p = block + b * 128;
        for (int i = 0; i < 16; i++) {
            w[i] = 0;
            for (int j = 0; j < 8; j++) w[i] = (w[i] << 8) | p[i * 8 + j];
        }
        for (int i = 16; i < 80; i++) {
            uint64_t s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
            uint64_t s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint64_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 80; i++) {
            uint64_t S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
            uint64_t ch = (e & f) ^ (~e & g);
            uint64_t t1 = hh + S1 + ch + K512[i] + w[i];
            uint64_t S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
            uint64_t maj = (a & bb) ^ (a & c) ^ (bb & c);
            uint64_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = bb; bb = a; a = t1 + t2;
        }
        h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) out[i * 8 + j] = (uint8_t)(h[i] >> (56 - 8 * j));
}

/* --- MT19937, as CPython drives it ------------------------------------------- */
#define MT_N 624
#define MT_M 397
typedef struct { uint32_t mt[MT_N]; int mti; } mt_t;

static void mt_init_genrand(mt_t *s, uint32_t seed)
{
    s->mt[0] = seed;
    for (s->mti = 1; s->mti < MT_N; s->mti++)
        s->mt[s->mti] = 1812433253u * (s->mt[s->mti - 1] ^ (s->mt[s->mti - 1] >> 30)) + (uint32_t)s->mti;
}
static void mt_init_by_array(mt_t *s, const uint32_t *key, int key_length)
{
    mt_init_genrand(s, 19650218u);
    int i = 1, j = 0;
    int k = MT_N > key_length ? MT_N : key_length;
    for (; k; k--) {
        s->mt[i] = (s->mt[i] ^ ((s->mt[i - 1] ^ (s->mt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
        i++; j++;
        if (i >= MT_N) { s->mt[0] = s->mt[MT_N - 1]; i = 1; }
        if (j >= key_length) j = 0;
    }
    for (k = MT_N - 1; k; k--) {
        s->mt[i] = (s->mt[i] ^ ((s->mt[i - 1] ^ (s->mt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
        i++;
        if (i >= MT_N) { s->mt[0] = s->mt[MT_N - 1]; i = 1; }
    }
    s->mt[0] = 0x80000000u;
}
static uint32_t mt_genrand(mt_t *s)
{
    static const uint32_t mag01[2] = { 0u, 0x9908b0dfu };
    uint32_t y;
    if (s->mti >= MT_N) {
        int kk;
        for (kk = 0; kk < MT_N - MT_M; kk++) {
            y = (s->mt[kk] & 0x80000000u) | (s->mt[kk + 1] & 0x7fffffffu);
            s->mt[kk] = s->mt[kk + MT_M] ^ (y >> 1) ^ mag01[y & 1u];
        }
        for (; kk < MT_N - 1; kk++) {
            y = (s->mt[kk] & 0x80000000u) | (s->mt[kk + 1] & 0x7fffffffu);
            s->mt[kk] = s->mt[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 1u];
        }
        y = (s->mt[MT_N - 1] & 0x80000000u) | (s->mt[0] & 0x7fffffffu);
        s->mt[MT_N - 1] = s->mt[MT_M - 1] ^ (y >> 1) ^ mag01[y & 1u];
        s->mti = 0;
    }
    y = s->mt[s->mti++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= (y >> 18);
    return y;
}
/* random.random(): 53 bits from two draws, CPython's exact expression */
static double mt_random(mt_t *s)
{
    uint32_t a = mt_genrand(s) >> 5, b = mt_genrand(s) >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}
static double mt_uniform(mt_t *s, double a, double b) { return a + (b - a) * mt_random(s); }

/* random.Random(f"{seed}:{slime_id}") */
static bool mt_seed_str(mt_t *s, const char *str)
{
    size_t n = strlen(str);
    if (n > 64) return false;
    uint8_t bytes[64 + 64];                      /* s + sha512(s), big-endian as one integer */
    memcpy(bytes, str, n);
    sha512((const uint8_t *)str, n, bytes + n);
    size_t total = n + 64;
    int keyused = (int)((total + 3) / 4);        /* the top byte is an ASCII digit, never 0: ceil(bits / 32) = ceil(bytes / 4) */
    uint32_t key[(64 + 64 + 3) / 4];
    for (int i = 0; i < keyused; i++) {          /* little-endian words of the big-endian integer */
        uint32_t w = 0;
        for (int j = 0; j < 4; j++) {
            size_t byte = (size_t)i * 4 + j;     /* from the least significant end */
            if (byte < total) w |= (uint32_t)bytes[total - 1 - byte] << (8 * j);
        }
        key[i] = w;
    }
    mt_init_by_array(s, key, keyused);
    return true;
}

void grain_derive(int seed, int slime_id, double out[11])
{
    for (int i = 0; i < 11; i++) out[i] = 1.0;
    out[7] = 0.0;
    mt_t *s = (mt_t *)malloc(sizeof *s);
    if (!s) return;
    char str[32];
    snprintf(str, sizeof str, "%d:%d", seed, slime_id);
    if (mt_seed_str(s, str)) {
        /* the desk's six, in grain.knobs' order */
        out[0] = mt_uniform(s, 0.85, 1.18);      /* bored */
        out[1] = mt_uniform(s, 0.85, 1.15);      /* blink */
        out[2] = mt_uniform(s, 0.85, 1.15);      /* saccade */
        out[3] = mt_uniform(s, 0.88, 1.15);      /* dwell */
        out[4] = mt_uniform(s, 0.92, 1.08);      /* breath */
        out[5] = mt_uniform(s, 0.96, 1.06);      /* hop */
        /* the wrist's own, after them */
        out[6] = mt_uniform(s, 0.90, 1.10);      /* breath_amp */
        out[7] = mt_uniform(s, 0.0, 5.0);        /* favourite, floored */
        out[8] = mt_uniform(s, 0.85, 1.15);      /* settle */
        out[9] = mt_uniform(s, 0.85, 1.15);      /* slide */
        out[10] = mt_uniform(s, 0.85, 1.15);     /* wake_hold */
    }
    free(s);
}

/* --- the knobs ------------------------------------------------------------- */
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static int mood_eye(const char *mood)
{
    if (!mood || !mood[0] || !strcmp(mood, "-")) return -1;
    if (!strcmp(mood, "happy")) return GEL_EYE_HAPPY;
    if (!strcmp(mood, "content")) return GEL_EYE_CONTENT;
    if (!strcmp(mood, "sad") || !strcmp(mood, "low") || !strcmp(mood, "sour")) return GEL_EYE_DROOPY;
    if (!strcmp(mood, "annoyed")) return GEL_EYE_SQUINT;
    return GEL_EYE_OPEN;
}

static void recompute(void)
{
    double d[11];
    grain_derive(G.seed, SLIME_ID, d);
    grain_knobs_t *k = &G.k;
    k->bored = (float)d[0]; k->blink = (float)d[1]; k->saccade = (float)d[2];
    k->dwell = (float)d[3]; k->breath = (float)d[4]; k->hop = (float)d[5];
    k->breath_amp = (float)d[6];
    int fav = (int)d[7]; if (fav < 0) fav = 0; if (fav > 4) fav = 4;
    k->favourite = 1 + fav;                      /* IDLE_LOOK .. IDLE_GLANCE_UP */
    k->settle = (float)d[8]; k->slide = (float)d[9]; k->wake_hold = (float)d[10];
    float r = clampf(G.regard, -1, 1);
    float warm = r > 0 ? r : 0;
    k->eye_rise_s = EYE_RISE_COLD_S + (EYE_RISE_WARM_S - EYE_RISE_COLD_S) * (r + 1.0f) * 0.5f;
    k->linger = 1.0f + LINGER_GAIN * warm;
    k->idle_gap = k->bored * clampf(1.0f - DWELL_GAIN * warm, 0.7f, 1.3f);
    k->cool = r < REGARD_SHY_AT;
    k->mood_eye = mood_eye(G.mood);
}

void grain_begin(void)
{
    G.told = false;
    G.seed = DEFAULT_SEED; G.regard = 0; G.night = 0;
    strcpy(G.mood, "-");
    G.belly = 0; G.belly_age_s = 0;
    recompute();
}

/* --- THE BELLY TRAVELS ------------------------------------------------------- */
void grain_belly(float belly)
{
    G.belly = clampf(belly, 0, 1);
    G.belly_age_s = 0;
}
void grain_tick(float dt)
{
    if (G.belly > 0 && dt > 0) { G.belly_age_s += dt; if (G.belly_age_s >= BELLY_SINK_S) G.belly = 0; }
}
float grain_belly_now(void)
{
    if (G.belly <= 0) return 0;
    return G.belly * (1.0f - G.belly_age_s / BELLY_SINK_S);
}
float grain_lump(void) { return BELLY_LUMP_X * grain_belly_now(); }

void grain_update(int seed, float regard, int night, const char *mood)
{
    bool same = G.told && seed == G.seed && regard == G.regard && night == G.night
                && mood && !strcmp(mood, G.mood);
    G.told = true;
    G.seed = seed; G.regard = regard; G.night = night;
    strncpy(G.mood, mood && mood[0] ? mood : "-", sizeof G.mood - 1);
    G.mood[sizeof G.mood - 1] = 0;
    if (!same) recompute();
}

const grain_knobs_t *grain_knobs(void) { return &G.k; }
bool  grain_told(void)   { return G.told; }
int   grain_seed(void)   { return G.seed; }
float grain_regard(void) { return G.regard; }
const char *grain_mood(void) { return G.mood; }
bool  grain_night(void)  { return G.night != 0; }

static const char *const FAV_NAMES[] = { "none", "the look", "the blink", "the settle", "the bubble", "the glance up" };
static const char *const EYE_NAMES[] = { "open", "closed", "sleep", "wide", "happy", "content", "droopy", "squint" };

void grain_status(char *out, int cap)
{
    const grain_knobs_t *k = &G.k;
    int fav = k->favourite >= 0 && k->favourite <= 5 ? k->favourite : 0;
    int n = snprintf(out, cap, "grain: seed %d (%s) regard %+.2f mood %s night %d | desk's (exact): bored %.3f blink %.3f saccade %.3f dwell %.3f breath %.3f hop %.3f",
                     G.seed, G.told ? "the desk's" : "default, never told", G.regard, G.mood, G.night,
                     k->bored, k->blink, k->saccade, k->dwell, k->breath, k->hop);
    if (n < cap)
        n += snprintf(out + n, cap - n, " | wrist's: breath-amp %.3f favourite %s settle %.3f slide %.3f wake-hold %.3f",
                      k->breath_amp, FAV_NAMES[fav], k->settle, k->slide, k->wake_hold);
    if (n < cap)
        n += snprintf(out + n, cap - n, " | regard's: eye-rise %.2f s linger x%.2f idle-gap x%.3f cool %s | mood's eye %s",
                      k->eye_rise_s, k->linger, k->idle_gap, k->cool ? "yes" : "no",
                      k->mood_eye >= 0 && k->mood_eye < 8 ? EYE_NAMES[k->mood_eye] : "-");
    if (n < cap) {
        if (G.belly > 0) snprintf(out + n, cap - n, " | belly %.2f (lump %.2f, %d min old)", grain_belly_now(), grain_lump(), (int)(G.belly_age_s / 60));
        else snprintf(out + n, cap - n, " | belly none");
    }
}
