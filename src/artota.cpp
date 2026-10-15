/* ART OVER THE AIR: the chunk state machine (artota.h has the protocol).
 * Pure: the flash, the crc, the wardrobe's release and remount are the
 * hooks'; tools/artota_test.cpp drives it against a RAM buffer. */
#include "artota.h"
#include <stdio.h>
#include <string.h>

static artota_hooks_t H;
static struct {
    int state;
    uint32_t total;          /* the image's bytes, from `begin` */
    uint32_t written;        /* the next byte wanted */
    uint32_t erased_to;      /* the partition is 0xff up to here (from 0) */
    uint32_t crc;            /* the running crc32 of the bytes written, in order */
    bool erased_any;
    char last[96];           /* the last verdict (status) */
} S;

static void say(const char *line)
{
    if (H.log) H.log(line, H.ctx);
}

static int reply(char *out, int cap, int rc, const char *line)
{
    if (out && cap > 0) { strncpy(out, line, (size_t)cap - 1); out[cap - 1] = 0; }
    strncpy(S.last, line, sizeof S.last - 1); S.last[sizeof S.last - 1] = 0;
    return rc;
}

static void reset(void)
{
    memset(&S, 0, sizeof S);
    S.state = ARTOTA_IDLE;
}

void artota_begin_hooks(const artota_hooks_t *h)
{
    if (h) H = *h; else memset(&H, 0, sizeof H);
    reset();
}

/* the release, asked at `begin` and on every tick while RELEASING: the
 * wardrobe's veil takes a worn costume off first (VEIL_MS on the
 * board), then the mapping is dropped and the bytes may go */
static void try_release(void)
{
    if (S.state != ARTOTA_RELEASING) return;
    int r = H.release ? H.release(H.ctx) : 0;
    if (r == 0) { S.state = ARTOTA_WRITING; say("art: the wardrobe is off and unmapped -- writing"); }
    else if (r < 0) { say("art: the wardrobe would not release: abandoned"); reset(); }
}

static void abandon(const char *why);

int artota_begin(uint32_t total, char *out, int cap)
{
    char l[96];
    bool again = S.state != ARTOTA_IDLE;
    if (again) abandon("a second begin: the write in progress dropped");   /* the host started over (a re-run of art-push.sh) */
    if (total < ARTOTA_HEAD || total > H.size) {
        snprintf(l, sizeof l, "no big total=%lu max=%lu", (unsigned long)total, (unsigned long)H.size);
        return reply(out, cap, 1, l);
    }
    bool gone = S.erased_any;
    reset();
    S.erased_any = gone;                     /* a restart does not bring the old image back */
    S.total = total;
    S.state = ARTOTA_RELEASING;
    snprintf(l, sizeof l, "art: begin -- %lu B to write (%lu chunks of %lu)%s", (unsigned long)total,
             (unsigned long)((total + ARTOTA_CHUNK - 1) / ARTOTA_CHUNK), (unsigned long)ARTOTA_CHUNK, again ? " (again)" : "");
    say(l);
    try_release();
    if (S.state == ARTOTA_IDLE) return reply(out, cap, 1, "no release");
    snprintf(l, sizeof l, "ok begin total=%lu%s", (unsigned long)total, again ? " again" : "");
    return reply(out, cap, 0, l);
}

static void abandon(const char *why)
{
    char l[160];
    snprintf(l, sizeof l, "art: ABANDONED at %lu of %lu B (%s)%s", (unsigned long)S.written, (unsigned long)S.total, why,
             S.erased_any ? " -- the old image is gone: no image until a whole one lands; today's gel serves" : " -- nothing was erased: the old image stands");
    say(l);
    bool gone = S.erased_any;
    reset();
    S.erased_any = gone;
    if (H.done) { char w[64]; int n = H.done(false, w, sizeof w, H.ctx); (void)n; }
}

void artota_abandon(const char *why)
{
    if (S.state == ARTOTA_IDLE) return;
    abandon(why ? why : "dropped");
}

int artota_chunk(uint32_t off, uint32_t len, uint32_t crc, const uint8_t *body, char *out, int cap)
{
    char l[96];
    if (S.state == ARTOTA_IDLE) return reply(out, cap, 1, "no begin");
    if (S.state == ARTOTA_RELEASING) { try_release(); if (S.state != ARTOTA_WRITING) return reply(out, cap, 1, S.state == ARTOTA_IDLE ? "no release" : "no wait"); }
    if (len == 0 || len > ARTOTA_CHUNK || off > S.total || off + len > S.total || !body) {
        snprintf(l, sizeof l, "no len off=%lu len=%lu total=%lu", (unsigned long)off, (unsigned long)len, (unsigned long)S.total);
        return reply(out, cap, 1, l);
    }
    uint32_t have = H.crc32 ? H.crc32(0, body, len) : 0;
    if (have != crc) {
        snprintf(l, sizeof l, "no crc off=%lu got=%08lx said=%08lx", (unsigned long)off, (unsigned long)have, (unsigned long)crc);
        reply(out, cap, 1, l);
        abandon("a chunk's crc is not its body's");
        return 1;
    }
    if (off < S.written) {
        /* a chunk already taken, again (curl's retry after a lost reply): not written twice */
        if (off + len <= S.written) { snprintf(l, sizeof l, "ok again off=%lu next=%lu", (unsigned long)off, (unsigned long)S.written); return reply(out, cap, 0, l); }
        snprintf(l, sizeof l, "no gap want=%lu", (unsigned long)S.written);   /* straddles the next byte: not a retry, not in order */
        return reply(out, cap, 1, l);
    }
    if (off > S.written) { snprintf(l, sizeof l, "no gap want=%lu", (unsigned long)S.written); return reply(out, cap, 1, l); }
    /* the erase, one 64 KB sector at a time, only as far as this chunk reaches */
    while (S.erased_to < off + len) {
        uint32_t n = ARTOTA_SECTOR;
        if (S.erased_to + n > H.size) n = H.size - S.erased_to;
        if (!H.erase || !H.erase(S.erased_to, n, H.ctx)) { reply(out, cap, 1, "no erase"); abandon("the flash would not erase"); return 1; }
        S.erased_any = true;
        S.erased_to += n;
    }
    if (!H.write || !H.write(off, body, len, H.ctx)) { reply(out, cap, 1, "no write"); abandon("the flash would not write"); return 1; }
    S.crc = H.crc32 ? H.crc32(S.crc, body, len) : 0;
    S.written = off + len;
    snprintf(l, sizeof l, "ok off=%lu next=%lu", (unsigned long)off, (unsigned long)S.written);
    return reply(out, cap, 0, l);
}

int artota_end(uint32_t crc, char *out, int cap)
{
    char l[96];
    if (S.state == ARTOTA_IDLE) return reply(out, cap, 1, "no begin");
    if (S.state == ARTOTA_RELEASING) { try_release(); if (S.state != ARTOTA_WRITING) return reply(out, cap, 1, S.state == ARTOTA_IDLE ? "no release" : "no wait"); }
    if (S.written < S.total) { snprintf(l, sizeof l, "no short have=%lu of=%lu", (unsigned long)S.written, (unsigned long)S.total); return reply(out, cap, 1, l); }
    if (crc != S.crc) {
        snprintf(l, sizeof l, "no image crc got=%08lx said=%08lx", (unsigned long)S.crc, (unsigned long)crc);
        reply(out, cap, 1, l);
        abandon("the image's crc is not the host's");
        return 1;
    }
    char why[64] = "";
    int forms = H.done ? H.done(true, why, sizeof why, H.ctx) : 0;
    uint32_t total = S.total;
    reset();
    if (forms < 0) {
        char m[160];
        S.erased_any = true;                 /* the bytes are new and do not mount: the old image is gone all the same */
        snprintf(m, sizeof m, "art: %lu B landed and whole by crc, but the image does not mount (%s)", (unsigned long)total, why); say(m);
        snprintf(l, sizeof l, "no mount %s", why);
        return reply(out, cap, 1, l);
    }
    snprintf(l, sizeof l, "art: %lu B landed, crc whole, mounted: %d forms", (unsigned long)total, forms); say(l);
    snprintf(l, sizeof l, "ok forms=%d", forms);
    return reply(out, cap, 0, l);
}

void artota_tick(void) { try_release(); }
int  artota_state(void) { return S.state; }
bool artota_active(void) { return S.state != ARTOTA_IDLE; }
uint32_t artota_written(void) { return S.written; }
uint32_t artota_total(void) { return S.total; }
bool artota_erased_any(void) { return S.erased_any; }

void artota_status(char *out, int cap)
{
    if (S.state == ARTOTA_IDLE) snprintf(out, cap, "art: idle%s | last %s", S.erased_any ? " (the last push left no image)" : "", S.last[0] ? S.last : "-");
    else snprintf(out, cap, "art: %s %lu of %lu B (erased to %lu KB) | last %s", S.state == ARTOTA_RELEASING ? "begun, the veil;" : "writing",
                  (unsigned long)S.written, (unsigned long)S.total, (unsigned long)(S.erased_to / 1024), S.last[0] ? S.last : "-");
}
