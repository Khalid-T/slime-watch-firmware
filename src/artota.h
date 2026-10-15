/* ART OVER THE AIR (design/watch-wardrobe.md 5.7: "later, when the cable
 * is a nuisance"): a new wardrobe.bin into the `art` partition through
 * the HTTP door, no cable -- `POST /art` in chunks (net.cpp parses the
 * door, this is the state machine; wardrobe.cpp owns the flash).
 *
 * THE PROTOCOL, one request a step, every reply one line of text:
 *   POST /art?begin=<total>               "ok begin total=N"      the write starts: the wardrobe off (the veil), the mapping released
 *   POST /art?off=<n>&len=<n>&crc=<hex>   "ok off=N next=N"       the body (at most ARTOTA_CHUNK 4096 B) written at <off>, in order
 *   POST /art?end=<hex>                   "ok forms=N"            the whole image's crc32 (zlib's) against the bytes written; the remount; `wardrobe check`
 * and the refusals, each "no <word>": `no begin` (a chunk or an end
 * with no write open), `no big` (a total the partition cannot hold, or
 * under a head), `no wait` (the veil is still taking the costume off:
 * try again in half a second), `no len` (an empty chunk, over 4096, or
 * past the total), `no gap want=N` (an offset ahead of the next byte:
 * the write stands, send from N), `no crc` (the chunk's own crc32 is
 * not the body's: THE WRITE IS ABANDONED -- the old image is gone
 * once a sector was erased), `no short have=N of=M` (an end before
 * every byte: the write stands), `no image crc` (the whole's crc is not
 * the host's: abandoned), `no mount <why>` (the bytes landed but do not
 * mount: abandoned). A chunk already taken, sent again (a lost reply,
 * curl's retry) is `ok again off=N`. A second `begin` mid-write drops
 * the write in progress and starts over (logged).
 *
 * THE FLASH BEHIND FUNCTION POINTERS (artota_hooks_t): the erase (64 KB
 * sectors, as the write advances: never the whole partition at once,
 * never ahead of the bytes), the write, the crc, the release (the
 * wardrobe off + the MMU mapping dropped; 1 = not yet, the veil) and
 * the done (the remount, the count) -- so tools/artota_test.cpp runs
 * the machine against a RAM buffer. No Arduino in this file. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ARTOTA_CHUNK   4096u        /* a chunk's most: one 4 KB buffer, the heap's whole cost of a push */
#define ARTOTA_SECTOR  65536u       /* the erase unit: the MMU's page, the flash's block */
#define ARTOTA_HEAD    32u          /* an image is at least its SLMA head */

typedef struct {
    uint32_t size;                                                    /* the partition's bytes */
    bool (*erase)(uint32_t off, uint32_t len, void *ctx);             /* off and len multiples of 4096 */
    bool (*write)(uint32_t off, const uint8_t *p, uint32_t n, void *ctx);
    uint32_t (*crc32)(uint32_t seed, const uint8_t *p, uint32_t n);   /* zlib's crc32 (the ROM's esp_rom_crc32_le on the board) */
    int  (*release)(void *ctx);                                       /* 0 = the wardrobe is off and unmapped; 1 = not yet (the veil); -1 = cannot */
    int  (*done)(bool whole, char *why, int cap, void *ctx);          /* the end: remount (whole) or note the loss; the form count, or -1 with why */
    void (*log)(const char *line, void *ctx);                         /* the board's LOG (NULL = quiet) */
    void *ctx;
} artota_hooks_t;

enum { ARTOTA_IDLE = 0, ARTOTA_RELEASING = 1, ARTOTA_WRITING = 2 };

void artota_begin_hooks(const artota_hooks_t *h);
/* each returns 0 = ok, 1 = refused; `out` is the reply line (no newline) */
int  artota_begin(uint32_t total, char *out, int cap);
int  artota_chunk(uint32_t off, uint32_t len, uint32_t crc, const uint8_t *body, char *out, int cap);
int  artota_end(uint32_t crc, char *out, int cap);
void artota_abandon(const char *why);       /* the door closed on it, a hand: the write dropped (the image is gone if a sector was erased) */
void artota_tick(void);                     /* the release asked again while RELEASING (the veil ends on a later frame) */
int  artota_state(void);
bool artota_active(void);                   /* a write is open (RELEASING or WRITING) */
uint32_t artota_written(void);
uint32_t artota_total(void);
bool artota_erased_any(void);               /* a sector went: the old image is gone */
void artota_status(char *out, int cap);     /* one line: the state, the bytes, the last verdict */
