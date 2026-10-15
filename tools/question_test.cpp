/* THE WORD'S QUESTION on the laptop (question.c is pure C, no Arduino):
 *   c++ -std=gnu++17 -I src -o /tmp/question_test tools/question_test.cpp -x c src/question.c && /tmp/question_test
 * What is a question (the trimmed text ends in `?`; a `?` inside is
 * not; a plain word is not); the echo (whole words under 48, a long
 * question cut at a space, a first word longer than the limit cut
 * outright); the window: a question opens it at its SHOW (not before:
 * a held word is not asked), a plain word opens nothing; a pet inside
 * 10 s = yes with the echo and the window closes (a second pet, a
 * dismissal after it: nothing); a dismissal inside = no; a pet at
 * 9.9 s counts, at 10.0 s not; the window running out = one SILENCE
 * log line and nothing to vote; the word leaving the glass (replaced,
 * or dismissed through a path that did not ask) = gone, logged; a new
 * question after an old one asks afresh; the status line through it
 * all; the boot forgets. */
#include "question.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, ...) do { checks++; if (cond) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                              else { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now = 5000;
static bool showing = false;
static const char *text = "";
static const char *last_log = NULL;
/* a frame: 30 ms, the word's state fed as main feeds it */
static void frame(void) { const char *l = question_tick(showing, text, now); if (l) last_log = l; now += 30; }
static void frames_ms(uint32_t ms) { uint32_t end = now + ms; while (now < end) frame(); }

int main(void)
{
    char st[200], echo[QUESTION_ECHO_MAX + 1];
    question_begin();

    puts("-- what is a question");
    CHECK(question_is("coffee later?"), "'coffee later?' is a question");
    CHECK(question_is("coffee later?  "), "... trailing spaces trimmed");
    CHECK(!question_is("coffee later"), "'coffee later' is not");
    CHECK(!question_is("what? no."), "a ? inside is not (it ends in a full stop)");
    CHECK(question_is("?  "), "a lone ? is (the box's shortest question)");
    CHECK(!question_is("") && !question_is(NULL), "the empty word and NULL are not");

    puts("-- the echo");
    question_echo("coffee later?", echo, sizeof echo);
    CHECK(!strcmp(echo, "coffee later?"), "a short question is carried whole: '%s'", echo);
    question_echo("  shall we walk down to the water at the end of the day today?", echo, sizeof echo);
    CHECK(strlen(echo) <= QUESTION_ECHO_MAX && echo[strlen(echo) - 1] != ' ' && !strcmp(echo, "shall we walk down to the water at the end of"),
          "a long one is cut at a space, whole words, under %d: '%s'", QUESTION_ECHO_MAX, echo);
    question_echo("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?", echo, sizeof echo);
    CHECK(strlen(echo) == QUESTION_ECHO_MAX, "a first word longer than the limit is cut outright (%zu)", strlen(echo));
    question_echo("x", echo, 1);
    CHECK(echo[0] == 0, "a cap of 1 holds the empty string");

    puts("-- a held word is not asked; the show opens the window");
    question_status(st, sizeof st, now); CHECK(strstr(st, "none asked") != NULL, "status at boot: %s", st);
    text = "coffee later?"; showing = false; last_log = NULL;
    frames_ms(300);
    CHECK(!question_open(now) && last_log == NULL, "held (not showing): no window, no log");
    showing = true; frame();
    CHECK(question_open(now), "shown: the window is open");
    CHECK(last_log && strstr(last_log, "coffee later?") && strstr(last_log, "yes"), "... one log line: %s", last_log);
    CHECK(question_left_ms(now) > 9900 && question_left_ms(now) <= 10000, "... %lu ms left", (unsigned long)question_left_ms(now));
    question_status(st, sizeof st, now); CHECK(strstr(st, "open") && strstr(st, "coffee later?"), "status: %s", st);

    puts("-- a pet inside the window is yes, once");
    frames_ms(2000);
    echo[0] = 0;
    CHECK(question_yes(now, echo, sizeof echo) && !strcmp(echo, "coffee later?"), "a pet at 2 s: wrist:yes '%s'", echo);
    CHECK(!question_open(now), "... the window closed with the vote");
    CHECK(!question_yes(now, echo, sizeof echo), "a second pet: nothing (one vote a question)");
    CHECK(!question_no(now, echo, sizeof echo), "a dismissal after the yes: nothing");
    question_status(st, sizeof st, now); CHECK(strstr(st, "-- yes") != NULL, "status: %s", st);
    last_log = NULL;
    frames_ms(10000);
    CHECK(last_log == NULL, "the window's end after a vote: no silence line");
    showing = false; text = ""; frame();
    CHECK(last_log == NULL, "the word dismissed after the vote: nothing more");

    puts("-- a dismissal inside the window is no");
    text = "walk now?"; showing = true; last_log = NULL; frame();
    CHECK(question_open(now) && last_log && strstr(last_log, "walk now?"), "a new question asks afresh");
    frames_ms(4000);
    CHECK(question_no(now, echo, sizeof echo) && !strcmp(echo, "walk now?"), "the tap on the bubble at 4 s: wrist:no '%s'", echo);
    showing = false; text = ""; last_log = NULL; frame();
    CHECK(!question_open(now) && last_log == NULL, "the bubble gone with it: no window, no log");
    question_status(st, sizeof st, now); CHECK(strstr(st, "-- no") != NULL, "status: %s", st);

    puts("-- the edge of the window");
    text = "tea?"; showing = true; frame();
    uint32_t shown = now - 30;
    now = shown + 9900;
    CHECK(question_yes(now, echo, sizeof echo), "a pet at 9.9 s counts");
    showing = false; text = ""; frame();
    text = "tea?"; showing = true; frame();
    shown = now - 30;
    now = shown + 10000;
    CHECK(!question_yes(now, echo, sizeof echo), "a pet at 10.0 s does not");
    CHECK(!question_no(now, echo, sizeof echo), "nor a dismissal");

    puts("-- silence");
    showing = false; text = ""; frame();
    text = "lunch?"; showing = true; last_log = NULL; frame();
    CHECK(question_open(now), "asked");
    last_log = NULL;
    frames_ms(QUESTION_S * 1000 + 100);
    CHECK(!question_open(now), "10 s on: the window shut");
    CHECK(last_log && strstr(last_log, "silence") && strstr(last_log, "nothing goes"), "... one log line: %s", last_log);
    CHECK(!question_yes(now, echo, sizeof echo) && !question_no(now, echo, sizeof echo), "no vote after it: a pet is a pet, a tap a tap");
    question_status(st, sizeof st, now); CHECK(strstr(st, "silence") != NULL, "status: %s", st);
    last_log = NULL; frames_ms(1000);
    CHECK(last_log == NULL, "the silence line is said once");
    showing = false; text = ""; frame();

    puts("-- the word leaving the glass closes the window");
    text = "nap?"; showing = true; frame(); frames_ms(1000);
    showing = false; text = "a new word"; last_log = NULL; frame();   /* replaced by a held word: the bubble is gone */
    CHECK(!question_open(now) && last_log && strstr(last_log, "gone"), "replaced while open: gone, logged: %s", last_log);
    CHECK(!question_yes(now, echo, sizeof echo), "... no vote on a question that left");
    showing = true; last_log = NULL; frame();
    CHECK(!question_open(now) && last_log == NULL, "the new word shown: plain, no window, no log");
    showing = false; text = ""; frame();

    puts("-- a plain word never asks");
    text = "the desk waves from across the room"; showing = true; last_log = NULL; frame(); frames_ms(500);
    CHECK(!question_open(now) && last_log == NULL, "shown: nothing");
    CHECK(!question_yes(now, echo, sizeof echo) && !question_no(now, echo, sizeof echo), "a pet, a tap: nothing to vote on");
    showing = false; text = ""; frame();

    puts("-- the boot forgets");
    text = "again?"; showing = true; frame();
    CHECK(question_open(now), "asked");
    question_begin();
    CHECK(!question_open(now) && !question_yes(now, echo, sizeof echo), "after a boot: nothing open");
    question_status(st, sizeof st, now); CHECK(strstr(st, "none asked") != NULL, "status: %s", st);
    /* the boot's edge detector starts clean: a word already showing at the first tick opens afresh */
    frame();
    CHECK(question_open(now), "a question showing at the first tick after a boot asks (the show is what main sees)");

    printf("question_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
