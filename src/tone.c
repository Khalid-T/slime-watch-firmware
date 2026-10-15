/* THE WORD'S TONE -- tone.h says what; this is how. */
#include "tone.h"
#include <string.h>
#include <math.h>

static struct {
    int      tone;                    /* the beat playing (TONE_NONE = none) */
    bool     was_showing;             /* the edge detector's memory */
    uint32_t start_ms;                /* when the beat began (the show) */
} T;

static const char *const NAMES[] = { "plain", "glad (!)", "trailing off (...)" };

int tone_of(const char *text)
{
    if (!text) return TONE_NONE;
    int n = (int)strlen(text);
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
    if (n == 0) return TONE_NONE;
    if (text[n - 1] == '!') return TONE_GLAD;
    if (n >= 3 && text[n - 1] == '.' && text[n - 2] == '.' && text[n - 3] == '.') return TONE_TRAIL;
    if (n >= 3 && (unsigned char)text[n - 3] == 0xE2 && (unsigned char)text[n - 2] == 0x80 && (unsigned char)text[n - 1] == 0xA6) return TONE_TRAIL;   /* the one-glyph ellipsis, UTF-8 */
    return TONE_NONE;
}

void tone_begin(void) { memset(&T, 0, sizeof T); }

/* the beat's length, its fade not counted */
static float beat_s(int tone)
{
    if (tone == TONE_GLAD)  return TONE_HAPPY_S > TONE_SETTLE_S ? TONE_HAPPY_S : TONE_SETTLE_S;
    if (tone == TONE_TRAIL) return TONE_BLINK_S + TONE_DROOP_S;
    return 0;
}

static float since_s(uint32_t now_ms) { return (float)(now_ms - T.start_ms) * 0.001f; }

int tone_tick(bool showing, const char *text, uint32_t now_ms)
{
    int started = TONE_NONE;
    if (showing && !T.was_showing) {
        /* the word came to show: its tone's beat, once; a plain word nothing */
        int tone = tone_of(text);
        if (tone != TONE_NONE) { T.tone = tone; T.start_ms = now_ms; started = tone; }
    }
    T.was_showing = showing;
    if (T.tone != TONE_NONE && since_s(now_ms) >= beat_s(T.tone) + TONE_FADE_S) T.tone = TONE_NONE;   /* the beat and its fade done */
    return started;
}

void tone_fx(tone_fx_t *out, uint32_t now_ms)
{
    out->eye_kind = -1; out->eye_alpha = 0; out->squash = 0;
    if (T.tone == TONE_NONE) return;
    float t = since_s(now_ms), beat = beat_s(T.tone);
    if (t >= beat + TONE_FADE_S) return;
    if (t >= beat) { out->eye_alpha = 1.0f - (t - beat) / TONE_FADE_S; return; }   /* the fade: the eyes' presence only, the look the draw's */
    out->eye_alpha = 1.0f;
    if (T.tone == TONE_GLAD) {
        if (t < TONE_HAPPY_S) out->eye_kind = GEL_EYE_HAPPY;
        if (t < TONE_SETTLE_S) out->squash = TONE_SETTLE_SQUASH * sinf((float)M_PI * t / TONE_SETTLE_S);   /* one bright settle: a half sine, idle's own shape */
    } else {
        out->eye_kind = t < TONE_BLINK_S ? GEL_EYE_CLOSED : GEL_EYE_DROOPY;   /* the slow blink, then the lids half-down */
    }
}

bool tone_on(uint32_t now_ms) { return T.tone != TONE_NONE && since_s(now_ms) < beat_s(T.tone) + TONE_FADE_S; }

const char *tone_name(int tone) { return (tone >= TONE_NONE && tone <= TONE_TRAIL) ? NAMES[tone] : NAMES[0]; }
