/* THE WORD'S TONE on the laptop (tone.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/tone_test tools/tone_test.cpp -x c src/tone.c -lm && /tmp/tone_test
 * What a tone is (the trimmed text's end: `!` glad, `...` or the
 * one-glyph ellipsis trailing off; a stop, nothing, two dots, a `?`
 * -- the question's -- plain; a `?!` is glad, a `!?` the question's);
 * the show: a held word plays nothing, the show starts the beat once,
 * the word staying plays nothing more, a new word is a new show; the
 * glad beat (HAPPY for 1.2 s, the settle's half sine peaking at 0.3 s
 * and gone by 0.6, the eyes' presence 1 through it and fading over
 * 0.4 s after); the trailing beat (CLOSED for 0.30 s, DROOPY to 1.30,
 * no squash ever, the fade after); the dismissal mid-beat lets it run;
 * a question plays nothing (question.c's); the boot forgets; the
 * 32-bit clock's wrap. */
#include "tone.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now = 5000;
static bool showing = false;
static const char *text = "";
static int last_ev = TONE_NONE, evs = 0;
/* a frame: 30 ms, the word's state fed as main feeds it */
static void frame(void) { int e = tone_tick(showing, text, now); if (e) { last_ev = e; evs++; } now += 30; }
static void frames_ms(uint32_t ms) { uint32_t end = now + ms; while (now < end) frame(); }
static tone_fx_t fx(void) { tone_fx_t f; tone_fx(&f, now); return f; }

int main(void)
{
    tone_begin();

    puts("-- what a tone is");
    CHECK(tone_of("well done!") == TONE_GLAD, "'well done!' is glad");
    CHECK(tone_of("well done!  ") == TONE_GLAD, "... trailing spaces trimmed");
    CHECK(tone_of("well done!!!") == TONE_GLAD, "three bangs are one glad word");
    CHECK(tone_of("and then...") == TONE_TRAIL, "'and then...' trails off");
    CHECK(tone_of("and then\xe2\x80\xa6") == TONE_TRAIL, "the one-glyph ellipsis trails off too");
    CHECK(tone_of("and then..") == TONE_NONE, "two dots are not an ellipsis");
    CHECK(tone_of("the desk waves.") == TONE_NONE, "a plain stop is plain");
    CHECK(tone_of("the desk waves") == TONE_NONE, "no punctuation is plain");
    CHECK(tone_of("coffee later?") == TONE_NONE, "a question is the question's (nothing here)");
    CHECK(tone_of("really?!") == TONE_GLAD && tone_of("really!?") == TONE_NONE, "the last mark decides: ?! glad, !? the question's");
    CHECK(tone_of("!") == TONE_GLAD && tone_of("...") == TONE_TRAIL, "the shortest glad word, the shortest trailing one");
    CHECK(tone_of("") == TONE_NONE && tone_of(NULL) == TONE_NONE && tone_of("   ") == TONE_NONE, "the empty word, NULL and spaces are plain");
    CHECK(tone_of("a! b.") == TONE_NONE, "a bang inside is not the end's");
    CHECK(!strcmp(tone_name(TONE_GLAD), "glad (!)") && !strcmp(tone_name(TONE_TRAIL), "trailing off (...)") && !strcmp(tone_name(TONE_NONE), "plain") && !strcmp(tone_name(99), "plain"), "the names");

    puts("-- a held word plays nothing; the show starts the beat once");
    text = "well done!"; showing = false; evs = 0;
    frames_ms(300);
    CHECK(evs == 0 && !tone_on(now) && fx().eye_kind == -1 && fx().squash == 0, "held (not showing): nothing");
    showing = true; frame();
    CHECK(evs == 1 && last_ev == TONE_GLAD && tone_on(now), "shown: the glad beat, one event");
    uint32_t shown = now - 30;
    frames_ms(2000);
    CHECK(evs == 1, "the word staying on the glass: no second beat");

    puts("-- the glad beat: happy eyes, one bright settle");
    showing = false; text = ""; frame(); frames_ms(2000);
    text = "the sun is out!"; showing = true; evs = 0; frame(); shown = now - 30;
    CHECK(evs == 1 && last_ev == TONE_GLAD, "a new word: a new show, a new beat");
    now = shown; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_HAPPY && f.eye_alpha == 1.0f && f.squash < 0.001f, "at the show: HAPPY, the eyes in, the settle starting from 0 (%.3f)", f.squash); }
    now = shown + 300; { tone_fx_t f = fx(); CHECK(f.squash > TONE_SETTLE_SQUASH * 0.99f && f.squash <= TONE_SETTLE_SQUASH, "0.3 s: the settle's peak (%.3f)", f.squash); }
    now = shown + 150; { tone_fx_t f = fx(); float a = f.squash; now = shown + 450; f = fx(); CHECK(a > 0.03f && a < 0.05f && f.squash > 0.03f && f.squash < 0.05f, "0.15 s and 0.45 s: the half sine's shoulders (%.3f, %.3f)", a, f.squash); }
    now = shown + 600; { tone_fx_t f = fx(); CHECK(f.squash < 0.001f && f.eye_kind == GEL_EYE_HAPPY, "0.6 s: the settle done, the eyes still HAPPY"); }
    now = shown + 1199; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_HAPPY && f.eye_alpha == 1.0f, "1.199 s: HAPPY to the end"); }
    now = shown + 1200; { tone_fx_t f = fx(); CHECK(f.eye_kind == -1 && f.eye_alpha > 0.99f && f.squash == 0, "1.2 s: the look is the draw's, the eyes' presence starts its fade (%.2f)", f.eye_alpha); }
    now = shown + 1400; { tone_fx_t f = fx(); CHECK(f.eye_kind == -1 && f.eye_alpha > 0.45f && f.eye_alpha < 0.55f && tone_on(now), "1.4 s: half faded (%.2f), still on", f.eye_alpha); }
    now = shown + 1600; { tone_fx_t f = fx(); CHECK(f.eye_kind == -1 && f.eye_alpha == 0 && f.squash == 0 && !tone_on(now), "1.6 s: gone"); }
    now = shown + 1600; frame();
    CHECK(!tone_on(now) && fx().eye_alpha == 0, "the tick after: nothing");

    puts("-- the trailing beat: one slow blink, the lids half-down a second");
    showing = false; text = ""; frame();
    text = "and then..."; showing = true; evs = 0; frame(); shown = now - 30;
    CHECK(evs == 1 && last_ev == TONE_TRAIL, "shown: the trailing beat, one event");
    now = shown; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_CLOSED && f.eye_alpha == 1.0f && f.squash == 0, "at the show: the lids CLOSED, nothing in the body"); }
    now = shown + 299; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_CLOSED, "0.299 s: still closed (idle's blink is 0.12: this one is slow)"); }
    now = shown + 300; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_DROOPY && f.squash == 0, "0.3 s: the lids half-down"); }
    now = shown + 1299; { tone_fx_t f = fx(); CHECK(f.eye_kind == GEL_EYE_DROOPY && f.eye_alpha == 1.0f, "1.299 s: still half-down"); }
    now = shown + 1300; { tone_fx_t f = fx(); CHECK(f.eye_kind == -1 && f.eye_alpha > 0.99f, "1.3 s: the look back to the draw's, the fade starts"); }
    now = shown + 1700; { tone_fx_t f = fx(); CHECK(f.eye_kind == -1 && f.eye_alpha == 0 && !tone_on(now), "1.7 s: gone"); }
    {   /* no squash on any frame of it */
        bool any = false;
        for (uint32_t t = 0; t <= 1700; t += 10) { now = shown + t; if (fx().squash != 0) any = true; }
        CHECK(!any, "no squash on any frame of the trailing beat (the box trailed off; nothing in the body moves)");
    }
    now = shown + 1700; frame();

    puts("-- a dismissal mid-beat lets it run");
    showing = false; text = ""; frame();
    text = "look!"; showing = true; evs = 0; frame(); shown = now - 30;
    frames_ms(100);
    showing = false; text = ""; frame();                /* the tap on the bubble at 0.1 s */
    CHECK(tone_on(now) && fx().eye_kind == GEL_EYE_HAPPY, "the bubble tapped away at 0.1 s: the beat runs on (the word was read)");
    frames_ms(2000);
    CHECK(!tone_on(now) && evs == 1, "... and ends by itself, once");

    puts("-- a question and a plain word play nothing");
    text = "coffee later?"; showing = true; evs = 0; frame(); frames_ms(500);
    CHECK(evs == 0 && !tone_on(now) && fx().eye_kind == -1, "a question shown: nothing (question.c's)");
    showing = false; text = ""; frame();
    text = "the desk waves from across the room"; showing = true; frame(); frames_ms(500);
    CHECK(evs == 0 && !tone_on(now), "a plain word shown: nothing");
    showing = false; text = ""; frame();
    text = "the desk waves."; showing = true; frame(); frames_ms(500);
    CHECK(evs == 0 && !tone_on(now), "a word with a plain stop: nothing");
    showing = false; text = ""; frame();

    puts("-- a replaced word is a new show");
    text = "one!"; showing = true; evs = 0; frame(); frames_ms(100);
    showing = false; text = "two..."; frame();          /* replaced by a held word: the bubble is gone a frame */
    showing = true; frame();
    CHECK(evs == 2 && last_ev == TONE_TRAIL && fx().eye_kind == GEL_EYE_CLOSED, "the glad word replaced by a trailing one: the new beat takes over");
    frames_ms(2000); showing = false; text = ""; frame();

    puts("-- the boot forgets; the clock's wrap");
    text = "again!"; showing = true; frame();
    CHECK(tone_on(now), "a beat playing");
    tone_begin();
    CHECK(!tone_on(now) && fx().eye_kind == -1, "after a boot: nothing playing");
    frame();
    CHECK(tone_on(now), "a word showing at the first tick after a boot plays (the show is what main sees)");
    showing = false; text = ""; frame(); frames_ms(2000);
    now = 0xFFFFFFF0u; text = "wrap!"; showing = true; frame();
    now = 0x00000100u;                                   /* 0.27 s later, across the wrap */
    { tone_fx_t f = fx(); CHECK(tone_on(now) && f.eye_kind == GEL_EYE_HAPPY && f.squash > 0.05f, "across the 32-bit wrap: the beat at 0.27 s (%.3f)", f.squash); }
    now = 0x00000100u + 2000; frame();
    CHECK(!tone_on(now), "... and over");

    printf("tone_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
