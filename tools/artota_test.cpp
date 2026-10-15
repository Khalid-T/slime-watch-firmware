/* ART OVER THE AIR on the laptop (src/artota.cpp: the chunk state machine
 * behind `POST /art`; the flash behind function pointers, here a RAM
 * "partition" of 320 KB whose erases and writes are counted):
 *   c++ -std=gnu++17 -I src -o /tmp/artota_test tools/artota_test.cpp src/artota.cpp && /tmp/artota_test
 * A chunk or an end with no begin is `no begin`; a begin too big for the
 * partition (or under a head) is `no big`; a begin while the wardrobe
 * still veils is taken and the first chunk waits (`no wait`) until the
 * release says 0; an image of 3 sectors + a tail lands in 4 KB chunks IN
 * ORDER, the erase one 64 KB sector at a time and only as far as the
 * bytes reach (4 erases for 200,000 B: never the whole partition), the
 * bytes in the RAM partition equal to the image, the end's crc32 (zlib's,
 * the same the ROM computes) whole -> `ok forms=<n>` from the done hook;
 * a chunk sent again is `ok again` and not written twice; a chunk AHEAD
 * of the next byte is `no gap want=<n>` and the write stands (the right
 * chunk after it is taken); a chunk whose crc is not its body's is `no
 * crc` and THE WRITE IS ABANDONED (the done hook told "not whole", the
 * next chunk `no begin`; the first chunk's bad crc costs no erase -- the
 * old image stands); an end before every byte is `no short` and the
 * write stands; an end with the wrong image crc is `no image crc` and
 * abandoned; a second begin mid-write drops the write in progress
 * (abandoned, told) and starts over from zero; a done hook that cannot
 * mount is `no mount <why>`; an odd-sized last chunk; an end whose
 * chunk was the exact partition size. */
#include "artota.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* zlib's crc32 (the ROM's esp_rom_crc32_le on the board), bitwise -- no
 * -lz: crc32(0, "123456789", 9) = 0xcbf43926 is checked first */
static uint32_t crc32(uint32_t seed, const uint8_t *p, uint32_t n)
{
    uint32_t c = ~seed;
    for (uint32_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u))); }
    return ~c;
}

#define PART (320 * 1024)                       /* five 64 KB sectors: the image below fills four, the fifth must stay untouched */
static uint8_t part[PART];
static int erases = 0, writes = 0, releases = 0, release_answer = 0, dones = 0, done_whole = -1, done_forms = 7;
static char last_log[200] = "";
static int checks = 0, failed = 0;

static void check(bool ok, const char *what)
{
    checks++;
    if (!ok) { failed++; printf("  FAIL: %s\n", what); }
}
#define CHECK(x) check((x), #x)

static bool erase(uint32_t off, uint32_t len, void *ctx)
{
    (void)ctx;
    CHECK(off % 4096 == 0 && len % 4096 == 0 && off + len <= PART);
    memset(part + off, 0xff, len);
    erases++;
    return true;
}
static bool write(uint32_t off, const uint8_t *p, uint32_t n, void *ctx)
{
    (void)ctx;
    CHECK(off + n <= PART);
    bool blank = true; for (uint32_t i = 0; i < n; i++) if (part[off + i] != 0xff) blank = false;
    CHECK(blank);                             /* never a write into an unerased byte */
    memcpy(part + off, p, n);
    writes++;
    return true;
}
static uint32_t crc(uint32_t seed, const uint8_t *p, uint32_t n) { return (uint32_t)crc32(seed, p, n); }
static int release(void *ctx) { (void)ctx; releases++; return release_answer; }
static int done(bool whole, char *why, int cap, void *ctx)
{
    (void)ctx; dones++; done_whole = whole;
    if (!whole) { snprintf(why, cap, "no image"); return -1; }
    if (done_forms < 0) { snprintf(why, cap, "a bad magic"); return -1; }
    return done_forms;
}
static void logf(const char *l, void *ctx) { (void)ctx; strncpy(last_log, l, sizeof last_log - 1); printf("    [log] %s\n", l); }

static void fresh(void)
{
    artota_hooks_t h; memset(&h, 0, sizeof h);
    h.size = PART; h.erase = erase; h.write = write; h.crc32 = crc; h.release = release; h.done = done; h.log = logf;
    artota_begin_hooks(&h);
    memset(part, 0x5a, sizeof part);          /* an "old image" everywhere */
    erases = writes = releases = dones = 0; done_whole = -1; done_forms = 7; release_answer = 0;
}

static uint8_t image[200000];
static void make_image(void) { uint32_t s = 12345; for (size_t i = 0; i < sizeof image; i++) { s = s * 1664525u + 1013904223u; image[i] = (uint8_t)(s >> 24); } memcpy(image, "SLMA", 4); }

/* one chunk of the image at off, its true crc (or a wrong one) */
static int send(uint32_t off, uint32_t len, bool bad_crc, char *out, int cap)
{
    uint32_t c = crc(0, image + off, len);
    if (bad_crc) c ^= 0xdeadbeef;
    return artota_chunk(off, len, c, image + off, out, cap);
}

int main(void)
{
    char out[128];
    CHECK(crc32(0, (const uint8_t *)"123456789", 9) == 0xcbf43926u);   /* the crc is zlib's */
    make_image();
    const uint32_t TOTAL = sizeof image;      /* 200,000 B: 3 sectors and a tail, 49 chunks (the last 3,392 B) */
    const uint32_t IMG_CRC = crc(0, image, TOTAL);

    puts("-- no begin: a chunk and an end are refused, nothing touched");
    fresh();
    CHECK(artota_chunk(0, 4096, 0, image, out, sizeof out) == 1 && !strcmp(out, "no begin"));
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 1 && !strcmp(out, "no begin"));
    CHECK(!artota_active() && erases == 0 && writes == 0 && dones == 0);

    puts("-- no big: over the partition, or under a head");
    CHECK(artota_begin(PART + 1, out, sizeof out) == 1 && !strncmp(out, "no big", 6));
    CHECK(artota_begin(31, out, sizeof out) == 1 && !strncmp(out, "no big", 6));
    CHECK(!artota_active() && releases == 0);

    puts("-- the veil: the release says 'not yet' -> begun, the chunk waits (`no wait`), then goes");
    release_answer = 1;
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0 && !strcmp(out, "ok begin total=200000"));
    CHECK(artota_state() == ARTOTA_RELEASING && releases == 1);
    CHECK(send(0, 4096, false, out, sizeof out) == 1 && !strcmp(out, "no wait"));
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 1 && !strcmp(out, "no wait"));
    artota_tick(); CHECK(artota_state() == ARTOTA_RELEASING && releases == 4);   /* the begin, the chunk, the end, the tick: each asked */
    release_answer = 0;
    artota_tick(); CHECK(artota_state() == ARTOTA_WRITING && erases == 0);
    CHECK(part[0] == 0x5a);                   /* the old image stands until the first byte */

    puts("-- the whole image, in order: the erase a sector at a time as the bytes reach it");
    for (uint32_t off = 0; off < TOTAL; off += ARTOTA_CHUNK) {
        uint32_t len = TOTAL - off < ARTOTA_CHUNK ? TOTAL - off : ARTOTA_CHUNK;
        int rc = send(off, len, false, out, sizeof out);
        char want[64]; snprintf(want, sizeof want, "ok off=%lu next=%lu", (unsigned long)off, (unsigned long)(off + len));
        CHECK(rc == 0 && !strcmp(out, want));
        if (off == 0) CHECK(erases == 1);                     /* the first sector only */
        if (off == 65536) CHECK(erases == 2);                 /* the second, when the bytes reach it */
        if (off + len == TOTAL) CHECK(len == 3392);
    }
    CHECK(erases == 4 && writes == 49 && artota_written() == TOTAL);
    CHECK(memcmp(part, image, TOTAL) == 0);
    for (uint32_t i = TOTAL; i < 4 * ARTOTA_SECTOR; i++) if (part[i] != 0xff) { CHECK(false); break; }   /* the 4th sector erased to its end */
    CHECK(part[4 * ARTOTA_SECTOR] == 0x5a);                    /* never the whole partition */

    puts("-- a chunk again (a lost reply): ok again, not written twice");
    CHECK(send(TOTAL - 3392, 3392, false, out, sizeof out) == 0 && !strncmp(out, "ok again off=196608", 19));
    CHECK(send(4096, 4096, false, out, sizeof out) == 0 && !strncmp(out, "ok again", 8));
    CHECK(writes == 49);

    puts("-- the end: the image's crc whole -> the done hook (remount + check) -> ok forms=7");
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 0 && !strcmp(out, "ok forms=7"));
    CHECK(dones == 1 && done_whole == 1 && !artota_active() && !artota_erased_any());
    CHECK(artota_chunk(0, 4096, 0, image, out, sizeof out) == 1 && !strcmp(out, "no begin"));

    puts("-- a gap: a chunk ahead of the next byte is refused, the write stands, the right one is taken");
    fresh();
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0 && artota_state() == ARTOTA_WRITING);
    CHECK(send(0, 4096, false, out, sizeof out) == 0);
    CHECK(send(8192, 4096, false, out, sizeof out) == 1 && !strcmp(out, "no gap want=4096"));
    CHECK(send(2048, 4096, false, out, sizeof out) == 1 && !strcmp(out, "no gap want=4096"));   /* straddles: not a retry */
    CHECK(artota_active() && writes == 1 && dones == 0);
    CHECK(send(4096, 4096, false, out, sizeof out) == 0 && !strcmp(out, "ok off=4096 next=8192"));

    puts("-- no len: empty, over 4096, past the total");
    CHECK(send(8192, 0, false, out, sizeof out) == 1 && !strncmp(out, "no len", 6));
    CHECK(artota_chunk(8192, 4097, 0, image + 8192, out, sizeof out) == 1 && !strncmp(out, "no len", 6));
    CHECK(artota_chunk(TOTAL - 100, 4096, 0, image, out, sizeof out) == 1 && !strncmp(out, "no len", 6));
    CHECK(artota_active() && writes == 2);

    puts("-- a bad crc mid-write: no crc, THE WRITE IS ABANDONED (the done hook told: not whole; the image gone)");
    CHECK(send(8192, 4096, true, out, sizeof out) == 1 && !strncmp(out, "no crc off=8192", 15));
    CHECK(!artota_active() && dones == 1 && done_whole == 0 && artota_erased_any());
    CHECK(strstr(last_log, "ABANDONED") && strstr(last_log, "the old image is gone"));
    CHECK(send(12288, 4096, false, out, sizeof out) == 1 && !strcmp(out, "no begin"));
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 1 && !strcmp(out, "no begin"));
    { char st[160]; artota_status(st, sizeof st); CHECK(strstr(st, "idle") && strstr(st, "left no image")); }

    puts("-- a bad crc on the FIRST chunk: refused before any erase -- the old image stands");
    fresh();
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0);
    CHECK(send(0, 4096, true, out, sizeof out) == 1 && !strncmp(out, "no crc", 6));
    CHECK(!artota_active() && erases == 0 && writes == 0 && dones == 1 && done_whole == 0 && !artota_erased_any());
    CHECK(part[0] == 0x5a && strstr(last_log, "nothing was erased"));

    puts("-- an end too early: no short, the write stands");
    fresh();
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0);
    for (uint32_t off = 0; off < 65536; off += ARTOTA_CHUNK) CHECK(send(off, ARTOTA_CHUNK, false, out, sizeof out) == 0);
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 1 && !strcmp(out, "no short have=65536 of=200000"));
    CHECK(artota_active() && dones == 0);

    puts("-- an end with the wrong image crc: no image crc, abandoned");
    for (uint32_t off = 65536; off < TOTAL; off += ARTOTA_CHUNK) { uint32_t len = TOTAL - off < ARTOTA_CHUNK ? TOTAL - off : ARTOTA_CHUNK; CHECK(send(off, len, false, out, sizeof out) == 0); }
    CHECK(artota_end(IMG_CRC ^ 1, out, sizeof out) == 1 && !strncmp(out, "no image crc", 12));
    CHECK(!artota_active() && dones == 1 && done_whole == 0 && artota_erased_any());

    puts("-- a second begin mid-write: the write in progress dropped (told), over from zero");
    fresh();
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0);
    for (uint32_t off = 0; off < 3 * ARTOTA_CHUNK; off += ARTOTA_CHUNK) CHECK(send(off, ARTOTA_CHUNK, false, out, sizeof out) == 0);
    CHECK(artota_written() == 12288 && erases == 1);
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0 && !strcmp(out, "ok begin total=200000 again"));
    CHECK(dones == 1 && done_whole == 0 && artota_state() == ARTOTA_WRITING && artota_written() == 0 && releases == 2);
    CHECK(send(4096, 4096, false, out, sizeof out) == 1 && !strcmp(out, "no gap want=0"));   /* from zero again */
    CHECK(send(0, 4096, false, out, sizeof out) == 0 && erases == 2);                           /* the sector erased again: the restart wrote fresh */
    for (uint32_t off = 4096; off < TOTAL; off += ARTOTA_CHUNK) { uint32_t len = TOTAL - off < ARTOTA_CHUNK ? TOTAL - off : ARTOTA_CHUNK; CHECK(send(off, len, false, out, sizeof out) == 0); }
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 0 && !strcmp(out, "ok forms=7") && memcmp(part, image, TOTAL) == 0);

    puts("-- a second begin that is too big: the write dropped all the same, nothing open after");
    fresh();
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0 && send(0, 4096, false, out, sizeof out) == 0);
    CHECK(artota_begin(PART + 1, out, sizeof out) == 1 && !strncmp(out, "no big", 6));
    CHECK(!artota_active() && dones == 1 && done_whole == 0);

    puts("-- the bytes land but do not mount: no mount <why>");
    fresh(); done_forms = -1;
    CHECK(artota_begin(TOTAL, out, sizeof out) == 0);
    for (uint32_t off = 0; off < TOTAL; off += ARTOTA_CHUNK) { uint32_t len = TOTAL - off < ARTOTA_CHUNK ? TOTAL - off : ARTOTA_CHUNK; CHECK(send(off, len, false, out, sizeof out) == 0); }
    CHECK(artota_end(IMG_CRC, out, sizeof out) == 1 && !strcmp(out, "no mount a bad magic"));
    CHECK(!artota_active() && dones == 1 && done_whole == 1 && artota_erased_any());

    puts("-- the release refused (-1): no release, nothing open");
    fresh(); release_answer = -1;
    CHECK(artota_begin(TOTAL, out, sizeof out) == 1 && !strcmp(out, "no release") && !artota_active());

    puts("-- an image the exact size of the partition: every sector erased, no more");
    fresh();
    static uint8_t big[PART]; for (size_t i = 0; i < PART; i++) big[i] = (uint8_t)(i * 7);
    CHECK(artota_begin(PART, out, sizeof out) == 0);
    for (uint32_t off = 0; off < PART; off += ARTOTA_CHUNK) CHECK(artota_chunk(off, ARTOTA_CHUNK, crc(0, big + off, ARTOTA_CHUNK), big + off, out, sizeof out) == 0);
    CHECK(erases == PART / ARTOTA_SECTOR && writes == PART / ARTOTA_CHUNK);
    CHECK(artota_end(crc(0, big, PART), out, sizeof out) == 0 && memcmp(part, big, PART) == 0);

    printf("%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
