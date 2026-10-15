/* THE WRIST WARDROBE: the mapped partition, the table, the .slmf reader,
 * the veil and the costume's blit. See wardrobe.h; the image's byte
 * layout is in design/watch-wardrobe.md ("The image on flash"). */
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include "wardrobe.h"
#include "artota.h"     /* ART OVER THE AIR: the flash behind its hooks (the end of this file) */
#include "gel.h"
#include "theme.h"
#include "hours.h"
#include "store.h"
#include "mark.h"
#include "app.h"
#include "bored.h"       /* THE BORED ROLL: the wrist's own morphs (the clocks, the die) */
#include "link.h"        /* ... told to the desk as `sense wrist:look form=<name>` */

/* --- the image ---------------------------------------------------------------- */
/* THE IMAGE, as tools/wristbake.py writes it (IMG_HEAD_FMT "<4sHHII8s8s",
 * IMG_ENTRY_FMT "<16sIIIBBBB"; its parse_image is the reference reader,
 * this is its mirror in C). Little-endian, offsets from the image's
 * first byte:
 * head, 32 bytes:
 *   0 char magic[4] "SLMA" | 4 u16 version 1 | 6 u16 count | 8 u32 total
 *   (the whole image's bytes) | 12 u32 crc32 (zlib's) of the TABLE's
 *   bytes | 16 char hash[8] (the rack's short git hash, NUL-padded) |
 *   24 char date[8] "YYYYMMDD"
 * table at 32: count x 32 bytes, SORTED BY NAME:
 *   0 char name[16] (NUL-padded, <= 15: the rack name) | 16 u32 offset
 *   (from the image's start, 16-byte aligned) | 20 u32 size | 24 u32
 *   crc32 (zlib's) of the blob | 28 u8 w | 29 u8 h (art px) | 30 u8 blit
 *   (the scale rule, 1..3) | 31 u8 flags: bit0 ACT (a second band, the
 *   act, inside the blob), bit1 SAGE (own colours; only under a mind
 *   line), bit2 HUMAN (the human home)
 * blobs: each a `.slmf` v1 exactly as watchart.py documents it (the
 *   palette inside; the bands "idle" looped + at most one act played
 *   once), zero-padded to 16 */
static const char     IMG_MAGIC[4] = { 'S', 'L', 'M', 'A' };
static const uint16_t IMG_VERSION  = 1;
static const int      HEAD_SIZE    = 32, ENTRY_SIZE = 32;
static const int      ENTRY_FLAG_ACT = 1, ENTRY_FLAG_SAGE = 2, ENTRY_FLAG_HUMAN = 4;
static const uint8_t  ART_SUBTYPE  = 0x40;

static const uint32_t VEIL_MS      = 340;      /* the desk's VEIL_MS: the light out, the swap at the midpoint, the light in */
static const int      MOTES        = 22;       /* the veil's motes */
static const float    MOTE_R0      = 6.0f, MOTE_R1 = 58.0f;   /* their radius from the base centre, in to out */
static const float    ACT_SELF_S   = 600.0f;   /* the act on its own, once a wear, after this much lit wear */
static const int      BLIT_MAX     = 3;
static const int      WRIST_TARGET_ROWS = 72;  /* the blit derived when the table says 0: ~0.75 of the gel's 96 rows (the plan's TARGET for a chibi / a Sonic) */

/* the mapped flash is byte-addressable through the cache but not
 * aligned: every field is read a byte at a time */
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

typedef struct {
    const uint8_t *blob; uint32_t size;
    uint16_t w, h; uint8_t bpp, npal, nbands; uint16_t nframes;
    uint8_t lift; int8_t faces; uint8_t axis_x2, flags; uint16_t ink_rows, stature_px; uint8_t nvariants;
    const uint8_t *pal;      /* npal x { u16 rgb565, u8 a } */
    const uint8_t *bands;    /* nbands x { char[16], u16 first, u16 count, u16 hold_ms, u8 flags, u8 variant } */
    const uint8_t *px;       /* nframes x frame_bytes */
    uint32_t stride, frame_bytes;
} slmf_t;

static struct {
    bool mounted;
    const esp_partition_t *part;
    spi_flash_mmap_handle_t handle;
    const uint8_t *base;     /* the image's first byte */
    uint32_t mapped;         /* bytes mapped */
    int count; uint32_t total;
    char hash[9], date[9];
    char why[64];            /* why it is not mounted */
} W;
static char look_missing[WARDROBE_ID_MAX + 1] = "";   /* the last `form=` not baked here (the `no art <form>` answer) */

/* the worn costume (idx < 0 = the gel) */
static struct {
    int idx;
    slmf_t f;
    uint16_t lut[16];        /* the palette premultiplied over black, in the panel's byte order */
    int blit;
    bool mirror;
    int band_idle, band_act; /* band indices, -1 = none */
    int band;                /* the band playing */
    int frame;               /* within the band: 0..count-1 (ping-pong counts up then down) */
    int dir;
    uint32_t frame_ms;       /* the frame's start */
    float wear_lit_s;        /* lit seconds since the wear */
    float act_at_s;          /* the once-a-wear act on its own after this much lit wear (ACT_SELF_S; a roll's wear: its midpoint) */
    bool self_acted;         /* the once-a-wear act played */
    int box_x, box_y, box_w, box_h;   /* the face box drawn last frame */
} C = { -1 };

/* the veil */
static struct {
    int phase;               /* 0 none, 1 out (the light takes what stands), 2 in (the light leaves what now stands) */
    uint32_t t0;
    int pending;             /* the form to stand after the swap (-1 = the gel) */
    uint32_t seed;
    char how[24];
} V;

static int sage_before = -2;    /* THE SAGE under the word: the form worn before it (-2 = the Sage is not up) */
static bool word_was = false;
/* THE BORED ROLL: the standing (or pending) wear is the wrist's own --
 * never saved to NVS (a reboot wakes in the gel or the desk's look), ended
 * by the desk's `look`, a `wardrobe off`, the house emptying, or its own
 * clock; own_last = the last form rolled (never twice running) */
static bool own = false;
static int  own_last = -1;
static const char *const HOW_ROLL = "the bored roll";
static inline bool how_is_desk(const char *how) { return how && (!strcmp(how, "the look") || !strcmp(how, "the desk's look")); }
static mark_t mk;

/* --- the reader --------------------------------------------------------------- */

static bool slmf_open(const uint8_t *blob, uint32_t size, slmf_t *f)
{
    if (size < 24 || memcmp(blob, "SLMF", 4) != 0 || blob[4] != 1) return false;
    memset(f, 0, sizeof *f);
    f->blob = blob; f->size = size;
    f->bpp = blob[5]; f->w = rd16(blob + 6); f->h = rd16(blob + 8);
    f->npal = blob[10]; f->nbands = blob[11]; f->nframes = rd16(blob + 12);
    f->lift = blob[14]; f->faces = (int8_t)blob[15]; f->axis_x2 = blob[16]; f->flags = blob[17];
    f->ink_rows = rd16(blob + 18); f->stature_px = rd16(blob + 20); f->nvariants = blob[22];
    if ((f->bpp != 4 && f->bpp != 8) || !f->w || !f->h || !f->npal || !f->nbands || !f->nframes) return false;
    f->pal = blob + 24;
    const uint8_t *p = f->pal + (uint32_t)f->npal * 3 + (uint32_t)f->nvariants * 16;
    f->bands = p;
    f->px = p + (uint32_t)f->nbands * 24;
    f->stride = ((uint32_t)f->w * f->bpp + 7) / 8;
    f->frame_bytes = f->stride * f->h;
    uint32_t need = (uint32_t)(f->px - blob) + f->frame_bytes * f->nframes;
    return need <= size;
}

static inline const uint8_t *band_at(const slmf_t *f, int i) { return f->bands + i * 24; }
static inline uint16_t band_first(const slmf_t *f, int i) { return rd16(band_at(f, i) + 16); }
static inline uint16_t band_count(const slmf_t *f, int i) { return rd16(band_at(f, i) + 18); }
static inline uint16_t band_hold(const slmf_t *f, int i) { uint16_t h = rd16(band_at(f, i) + 20); return h ? h : 120; }
static inline uint8_t  band_flags(const slmf_t *f, int i) { return band_at(f, i)[22]; }
static inline const char *band_name(const slmf_t *f, int i) { return (const char *)band_at(f, i); }   /* NUL-padded in flash */

static inline const uint8_t *entry_at(int i) { return W.base + HEAD_SIZE + i * ENTRY_SIZE; }
static inline const char *entry_name(int i) { return (const char *)entry_at(i); }
static inline uint32_t entry_off(int i) { return rd32(entry_at(i) + 16); }
static inline uint32_t entry_size(int i) { return rd32(entry_at(i) + 20); }
static inline uint32_t entry_crc(int i) { return rd32(entry_at(i) + 24); }
static inline uint8_t  entry_w(int i) { return entry_at(i)[28]; }
static inline uint8_t  entry_h(int i) { return entry_at(i)[29]; }
static inline uint8_t  entry_blit(int i) { return entry_at(i)[30]; }
static inline uint8_t  entry_flags(int i) { return entry_at(i)[31]; }

/* --- the mount ----------------------------------------------------------------- */

static void unmounted(const char *why)
{
    W.mounted = false;
    strncpy(W.why, why, sizeof W.why - 1); W.why[sizeof W.why - 1] = 0;
    LOG("wardrobe: not mounted -- %s: the built-in gel, as ever", why);
}

static void mount(void)
{
    memset(&W, 0, sizeof W);
    W.part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)ART_SUBTYPE, NULL);
    if (!W.part) { unmounted("no art partition (type data / subtype 0x40) in the table"); return; }
    /* the head first: 4 KB, to learn the image's size; then the whole image */
    const void *p = NULL; spi_flash_mmap_handle_t h = 0;
    if (esp_partition_mmap(W.part, 0, 4096, SPI_FLASH_MMAP_DATA, &p, &h) != ESP_OK) { unmounted("the head would not map"); return; }
    const uint8_t *head = (const uint8_t *)p;
    bool magic_ok = memcmp(head, IMG_MAGIC, 4) == 0;
    bool blank = head[0] == 0xff;
    uint16_t ver = rd16(head + 4), count = rd16(head + 6);
    uint32_t total = rd32(head + 8), table_crc = rd32(head + 12);
    char hash[9], date[9];
    memcpy(hash, head + 16, 8); hash[8] = 0;
    memcpy(date, head + 24, 8); date[8] = 0;
    spi_flash_munmap(h);
    if (!magic_ok) { unmounted(blank ? "the partition is blank (nothing flashed at 0x610000)" : "a bad magic (not an SLMA image)"); return; }
    if (ver != IMG_VERSION) { snprintf(W.why, sizeof W.why, "image version %u (this build reads %u)", ver, IMG_VERSION); unmounted(W.why); return; }
    if (!count || count > 255) { unmounted("a form count of 0, or over 255"); return; }
    if (total < (uint32_t)(HEAD_SIZE + count * ENTRY_SIZE) || total > W.part->size) { unmounted("the image's total is outside the partition"); return; }
    /* the whole image, mapped once (the plan: up to the 2 MB, 32 pages;
     * the fallback if the MMU refuses is the image alone, and it is
     * what is asked for here anyway -- 64 KB pages, rounded up) */
    uint32_t want = (total + 0xffffu) & ~0xffffu;
    if (want > W.part->size) want = W.part->size;
    if (esp_partition_mmap(W.part, 0, want, SPI_FLASH_MMAP_DATA, &p, &W.handle) != ESP_OK) { unmounted("the image would not map (the MMU refused the pages)"); return; }
    W.base = (const uint8_t *)p; W.mapped = want; W.count = count; W.total = total;
    memcpy(W.hash, hash, 9); memcpy(W.date, date, 9);
    /* the table's own crc (the head's word for it), then every entry
     * inside the image, its name terminated, its blob a .slmf */
    uint32_t tcrc = esp_rom_crc32_le(0, W.base + HEAD_SIZE, (uint32_t)count * ENTRY_SIZE);
    if (tcrc != table_crc) {
        snprintf(W.why, sizeof W.why, "the table's crc %08lx is not the head's %08lx (a torn flash?)", (unsigned long)tcrc, (unsigned long)table_crc);
        spi_flash_munmap(W.handle); W.base = NULL; unmounted(W.why); return;
    }
    for (int i = 0; i < count; i++) {
        uint32_t off = entry_off(i), sz = entry_size(i);
        if (off < (uint32_t)(HEAD_SIZE + count * ENTRY_SIZE) || sz < 24 || off + sz > total || entry_name(i)[15] != 0) {
            snprintf(W.why, sizeof W.why, "entry %d (%.15s) points outside the image", i, entry_name(i));
            spi_flash_munmap(W.handle); W.base = NULL; unmounted(W.why); return;
        }
        slmf_t f;
        if (!slmf_open(W.base + off, sz, &f)) {
            snprintf(W.why, sizeof W.why, "entry %d (%.15s) is not a .slmf v1 blob", i, entry_name(i));
            spi_flash_munmap(W.handle); W.base = NULL; unmounted(W.why); return;
        }
    }
    W.mounted = true;
    LOG("wardrobe: mounted -- %d forms, %lu KB, %s %s (mapped %lu KB at %p from 0x%lx)", count, (unsigned long)(total / 1024),
        W.hash, W.date, (unsigned long)(want / 1024), (const void *)W.base, (unsigned long)W.part->address);
}

/* --- the wear ------------------------------------------------------------------ */

static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

/* the palette to a LUT: rgb565 x alpha over black (a costume stands on
 * the black glass, never over the gel: alpha is a darkening) */
static void build_lut(const slmf_t *f)
{
    memset(C.lut, 0, sizeof C.lut);
    int n = f->npal < 16 ? f->npal : 16;
    for (int i = 1; i < n; i++) {
        uint16_t c = rd16(f->pal + i * 3); uint8_t a = f->pal[i * 3 + 2];
        int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
        r = (r * a + 127) / 255; g = (g * a + 127) / 255; b = (b * a + 127) / 255;
        C.lut[i] = swap16((uint16_t)((r << 11) | (g << 5) | b));
    }
}

static int derive_blit(const slmf_t *f, uint8_t said)
{
    if (said >= 1 && said <= BLIT_MAX) return said;
    int rows = f->ink_rows ? f->ink_rows : f->h;
    int b = (int)lrintf((float)WRIST_TARGET_ROWS / (float)rows);
    return b < 1 ? 1 : b > BLIT_MAX ? BLIT_MAX : b;
}

static void start_band(int band)
{
    C.band = band; C.frame = 0; C.dir = 1; C.frame_ms = millis();
}

/* the swap itself: what stands from now (no theatre here) */
static bool put_on(int idx)
{
    if (idx < 0) {
        C.idx = -1; C.band = -1; C.box_w = C.box_h = 0;
        return true;
    }
    slmf_t f;
    if (!W.mounted || idx >= W.count || !slmf_open(W.base + entry_off(idx), entry_size(idx), &f)) return false;
    C.idx = idx; C.f = f;
    build_lut(&f);
    C.blit = derive_blit(&f, entry_blit(idx));
    C.mirror = f.faces < 0;                 /* the art faces left: turned to face the body's right, as the gel's gaze rests */
    C.band_idle = C.band_act = -1;
    for (int i = 0; i < f.nbands; i++) {
        const char *n = band_name(&f, i);
        if (!strncmp(n, "idle", 4) && C.band_idle < 0) C.band_idle = i;
        else if (C.band_act < 0 && band_at(&f, i)[23] == 0) C.band_act = i;   /* the first base band that is not the idle: the one act */
    }
    if (C.band_idle < 0) C.band_idle = 0;
    start_band(C.band_idle);
    C.wear_lit_s = 0; C.self_acted = false; C.act_at_s = ACT_SELF_S;
    C.box_w = C.box_h = 0;
    return true;
}

static void veil_start(int pending, const char *how)
{
    V.pending = pending;
    strncpy(V.how, how ? how : "?", sizeof V.how - 1); V.how[sizeof V.how - 1] = 0;
    if (V.phase == 0) { V.phase = 1; V.t0 = millis(); V.seed = (uint32_t)esp_random(); }
    LOG("wardrobe: the veil (%s) -- %s stands, %s will", V.how, wardrobe_worn_id(), pending < 0 ? "the gel" : entry_name(pending));
}

static void save_worn(void)
{
    if (own) return;                                      /* THE BORED ROLL persists nothing: NVS keeps the desk's look */
    const char *id = wardrobe_worn_id();
    if (strcmp(store.worn, id) != 0) { strncpy(store.worn, id, sizeof store.worn - 1); store.worn[sizeof store.worn - 1] = 0; store_save_worn(); }
}

bool wardrobe_wear(const char *id, const char *how)
{
    bool gel = !id || !id[0] || !strcmp(id, "-") || !strcmp(id, "home") || !strcmp(id, "gel");
    int idx = gel ? -1 : wardrobe_find(id);
    if (!gel && idx < 0) { LOG("wardrobe: no art '%s' (%s): as it was", id, how ? how : "?"); return false; }
    int standing = V.phase ? V.pending : C.idx;
    if (idx == standing) return true;
    if (idx >= 0 && (entry_flags(idx) & ENTRY_FLAG_SAGE) && sage_before == -2 && strcmp(how, "serial") != 0) {
        LOG("wardrobe: the sage is not a costume (rule 5): it shows under a mind line only -- refused (%s)", how ? how : "?");
        return false;
    }
    if (own && strcmp(how, HOW_ROLL) != 0) {
        /* THE BORED ROLL ends under any other hand: the desk's look wins
         * (it knows what it pushed: no word back); a `wardrobe off` is
         * the wrist's own word, so the desk hears the gel */
        own = false; bored_end();
        if (idx < 0 && !how_is_desk(how)) link_sense("wrist:look", "form=gel");
        LOG("wardrobe: the roll's wear ends (%s)", how ? how : "?");
    }
    veil_start(idx, how);
    return true;
}

bool wardrobe_act(void)
{
    if (C.idx < 0 || C.band_act < 0 || V.phase) return false;
    if (C.band == C.band_act) return false;
    start_band(C.band_act);
    LOG("wardrobe: %s's act (%.15s)", entry_name(C.idx), band_name(&C.f, C.band_act));
    return true;
}

/* --- the clocks ------------------------------------------------------------------ */

void wardrobe_tick(float dt, bool lit, bool holding, bool word_on, bool bare)
{
    uint32_t now = millis();
    /* THE VEIL: out, the swap, in */
    if (V.phase) {
        uint32_t t = now - V.t0;
        if (V.phase == 1 && t >= VEIL_MS / 2) {
            if (!put_on(V.pending)) { LOG("wardrobe: the swap failed (%s): the gel", V.how); put_on(-1); own = false; }
            if (own && C.idx >= 0) C.act_at_s = bored_wear_s() * 0.5f;   /* THE BORED ROLL: the one act at the wear's midpoint (ACT_SELF_S 600 s outlasts a roll's wear) */
            save_worn();
            V.phase = 2;
        }
        if (t >= VEIL_MS) { V.phase = 0; LOG("wardrobe: the veil lifts -- %s (%s)", C.idx < 0 ? "the gel" : entry_name(C.idx), V.how); }
    }
    /* the band's clock (nothing moves in the dark) */
    if (C.idx >= 0 && lit && C.band >= 0) {
        uint16_t n = band_count(&C.f, C.band), hold = band_hold(&C.f, C.band);
        uint8_t fl = band_flags(&C.f, C.band);
        if (n > 1 && now - C.frame_ms >= hold) {
            C.frame_ms = now;
            int next = C.frame + C.dir;
            if (fl & 2) {                                        /* ping-pong */
                if (next >= n || next < 0) { C.dir = -C.dir; next = C.frame + C.dir; }
                C.frame = next;
            } else if (next >= n) {
                if (fl & 1) C.frame = 0;                         /* loop */
                else { start_band(C.band_idle); }                /* the act ends: the idle again */
            } else C.frame = next;
        }
        C.wear_lit_s += dt;
        if (!C.self_acted && C.wear_lit_s >= C.act_at_s && C.band != C.band_act && holding && sage_before == -2) {
            C.self_acted = true;
            if (wardrobe_act()) LOG("wardrobe: the once-a-wear act, on its own (%.0f s worn)", C.wear_lit_s);
        }
    }
    /* THE SAGE UNDER THE WORD (rule 5): the bubble's first frame brings
     * it, its last takes it back to what stood -- only while holding,
     * only when a sage is baked */
    if (W.mounted) {
        bool on = word_on && holding;
        if (on && !word_was) {
            int s = wardrobe_find("sage");
            if (s >= 0 && s != C.idx) { sage_before = V.phase ? V.pending : C.idx; veil_start(s, "the mind's line"); }
        } else if (!on && word_was && sage_before != -2) {
            int back = sage_before; sage_before = -2;
            veil_start(back, "the line read");
        }
        word_was = on;
    }
    /* THE BORED ROLL (rule 4's second half): on the bare glass, holding,
     * after a bored spell of lit seconds, a costume out of the table
     * through the veil; worn a spell; the veil home. The desk is told
     * each way (`sense wrist:look form=<name>` / `form=gel`) */
    if (own && !holding && C.idx >= 0 && !V.phase) {
        /* the house emptied under the roll's costume: it sinks with the
         * house; the gel is what the return finds (the desk's `look` after
         * `ok held` would end it anyway; no word: the house is empty) */
        own = false; bored_end();
        veil_start(-1, "the house emptied");
    }
    bored_in_t bin;
    bin.dt = dt; bin.lit = lit; bin.holding = holding;
    bin.mounted = W.mounted && W.count > 0;
    bin.bare = bare && !word_on && sage_before == -2 && V.phase == 0 && C.idx < 0;
    bin.own_worn = own && C.idx >= 0 && V.phase == 0;
    int ev = bored_tick(&bin);
    if (ev == BORED_WEAR) {
        int idx = bored_pick(W.count, entry_flags, own_last);
        if (idx >= 0) {
            own = true; own_last = idx;
            veil_start(idx, HOW_ROLL);
            char w[32]; snprintf(w, sizeof w, "form=%.15s", entry_name(idx));
            link_sense("wrist:look", w);
            LOG("wardrobe: bored -- %s for %.0f lit s (the wrist's own; the desk told)", entry_name(idx), bored_wear_s());
        } else { bored_end(); LOG("wardrobe: bored, but nothing on the rack to roll (sage / human only)"); }
    } else if (ev == BORED_OFF) {
        own = false; bored_end();
        if (sage_before != -2) sage_before = -1;          /* under a mind line: the Sage's exit brings the gel, not the roll's costume */
        else veil_start(-1, "the wear's spell over");
        link_sense("wrist:look", "form=gel");
    }
}

/* --- the draw --------------------------------------------------------------------- */

static inline bool in_disc(int x, int y)
{
    int dx = 2 * x + 1 - GEL_FACE, dy = 2 * y + 1 - GEL_FACE;
    return dx * dx + dy * dy <= GEL_FACE * GEL_FACE;
}

/* the wall's exact basis: gravity g = (sn, cs) down the wall, the
 * body's right a = (cs, -sn) -- gel_compose's own numbers at the four
 * quarters, snapped */
static void wall_basis(int wall, int *sn, int *cs)
{
    switch (wall & 3) {
    case GEL_WALL_BOTTOM: *sn = 0; *cs = 1; break;
    case GEL_WALL_RIGHT:  *sn = 1; *cs = 0; break;
    case GEL_WALL_TOP:    *sn = 0; *cs = -1; break;
    default:              *sn = -1; *cs = 0; break;
    }
}

static void draw_costume(uint16_t *fb, int wall, float lift, float slide)
{
    const slmf_t *f = &C.f;
    int sn, cs; wall_basis(wall, &sn, &cs);
    float d = GEL_FLOOR_DROP - lift;
    int Bx = GEL_FACE / 2 + (int)lrintf(d * sn + slide * cs);
    int By = GEL_FACE / 2 + (int)lrintf(d * cs - slide * sn);
    int B = C.blit;
    uint16_t first = band_first(f, C.band);
    const uint8_t *px = f->px + (uint32_t)(first + C.frame) * f->frame_bytes;
    int ax = (int)f->axis_x2 * B / 2;                     /* the body's axis column, in face px from the frame's left */
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (int sy = 0; sy < f->h; sy++) {
        const uint8_t *row = px + sy * f->stride;
        int v0 = -(f->h - sy + f->lift) * B;              /* body space: v down, the feet lift rows above the base */
        for (int sx = 0; sx < f->w; sx++) {
            int s = C.mirror ? f->w - 1 - sx : sx;
            uint8_t ci = f->bpp == 8 ? row[s] : (uint8_t)((s & 1) ? (row[s >> 1] & 15) : (row[s >> 1] >> 4));
            if (!ci) continue;
            uint16_t c = C.lut[ci & 15];
            int u0 = sx * B - ax;
            /* the block's two corners, body -> face, the rect between them */
            int fx0 = Bx + u0 * cs + v0 * sn,           fy0 = By - u0 * sn + v0 * cs;
            int fx1 = Bx + (u0 + B) * cs + (v0 + B) * sn, fy1 = By - (u0 + B) * sn + (v0 + B) * cs;
            int xa = fx0 < fx1 ? fx0 : fx1, xb = fx0 < fx1 ? fx1 : fx0;
            int ya = fy0 < fy1 ? fy0 : fy1, yb = fy0 < fy1 ? fy1 : fy0;
            if (xa < 0) xa = 0;
            if (ya < 0) ya = 0;
            if (xb > GEL_FACE) xb = GEL_FACE;
            if (yb > GEL_FACE) yb = GEL_FACE;
            for (int y = ya; y < yb; y++) {
                uint16_t *r = fb + y * GEL_FACE;
                for (int x = xa; x < xb; x++) if (in_disc(x, y)) r[x] = c;
            }
            if (xa < xb && ya < yb) {
                if (xa < x0) x0 = xa;
                if (ya < y0) y0 = ya;
                if (xb > x1) x1 = xb;
                if (yb > y1) y1 = yb;
            }
        }
    }
    if (x1 > x0) { C.box_x = x0; C.box_y = y0; C.box_w = x1 - x0; C.box_h = y1 - y0; }
    else C.box_w = C.box_h = 0;
    mark_val(&mk, (uint32_t)C.idx + 1); mark_val(&mk, (uint32_t)(first + C.frame)); mark_val(&mk, (uint32_t)wall);
    mark_box(&mk, C.box_x, C.box_y, C.box_w, C.box_h);
}

static inline uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }

static void draw_veil(uint16_t *fb, int wall, float lift, float slide)
{
    uint32_t t = millis() - V.t0; if (t > VEIL_MS) t = VEIL_MS;
    float k = (float)t / (float)VEIL_MS;                  /* 0..1 over the whole veil */
    float env = sinf(k * (float)M_PI);                    /* the light's envelope: in, out */
    int sn, cs; wall_basis(wall, &sn, &cs);
    float d = GEL_FLOOR_DROP - lift;
    int Bx = GEL_FACE / 2 + (int)lrintf(d * sn + slide * cs);
    int By = GEL_FACE / 2 + (int)lrintf(d * cs - slide * sn);
    const gel_rgba_t *s = &theme_pal()->shine;         /* the veil's light is the theme's shine: the Sage's own gold never (it is not a theme) */
    float lum = 0.35f + 0.65f * env;
    int R = (int)(s->r * lum * 31 + 0.5f), G = (int)(s->g * lum * 63 + 0.5f), Bc = (int)(s->b * lum * 31 + 0.5f);
    uint16_t c = swap16((uint16_t)((R << 11) | (G << 5) | Bc));
    uint32_t seed = V.seed;
    for (int i = 0; i < MOTES; i++) {
        uint32_t r1 = lcg(&seed), r2 = lcg(&seed), r3 = lcg(&seed);
        float ang = (float)(r1 & 1023) / 1024.0f * 2.0f * (float)M_PI;
        float spread = 0.6f + 0.4f * (float)(r2 & 255) / 255.0f;
        float phase = (float)(r3 & 255) / 255.0f * 0.3f;
        float kk = k - phase; if (kk < 0) continue;
        if (kk > 1) kk = 1;
        /* out of the body toward the light, then back: the swap at the middle */
        float rad = (MOTE_R0 + (MOTE_R1 - MOTE_R0) * sinf(kk * (float)M_PI)) * spread;
        /* the motes hang above the base centre, in the body's own space (up = -g) */
        float u = cosf(ang) * rad, v = -fabsf(sinf(ang)) * rad - 20.0f;
        int x = Bx + (int)lrintf(u * cs + v * sn), y = By + (int)lrintf(-u * sn + v * cs);
        int sz = (i % 3 == 0) ? 3 : 2;
        for (int yy = y; yy < y + sz; yy++) for (int xx = x; xx < x + sz; xx++)
            if (xx >= 0 && yy >= 0 && xx < GEL_FACE && yy < GEL_FACE && in_disc(xx, yy)) fb[yy * GEL_FACE + xx] = c;
        mark_rect(&mk, x, y, sz, sz, c);
    }
    mark_f(&mk, k);
}

void wardrobe_draw(uint16_t *fb, int wall, float lift, float slide, bool house_gel)
{
    if (!house_gel) { C.box_w = C.box_h = 0; return; }   /* the house is empty (or the page is up): nothing stands */
    if (C.idx >= 0) draw_costume(fb, wall, lift, slide);
    if (V.phase) draw_veil(fb, wall, lift, slide);
}

bool wardrobe_mark(dirty_rect_t *box) { return mark_take(&mk, box); }
bool wardrobe_covers_gel(void) { return C.idx >= 0; }
bool wardrobe_worn(void) { return C.idx >= 0 || (V.phase && V.pending >= 0); }
bool wardrobe_own(void) { return own; }
void wardrobe_roll_now(void) { bored_now(true); }

/* --- the wardrobe ------------------------------------------------------------------ */

bool wardrobe_mounted(void) { return W.mounted; }
int wardrobe_count(void) { return W.mounted ? W.count : 0; }
const char *wardrobe_id_at(int i) { return (W.mounted && i >= 0 && i < W.count) ? entry_name(i) : ""; }
const char *wardrobe_worn_id(void) { return C.idx < 0 ? "-" : entry_name(C.idx); }

int wardrobe_find(const char *id)
{
    if (!W.mounted || !id || !id[0]) return -1;
    for (int i = 0; i < W.count; i++) if (!strncmp(entry_name(i), id, 16)) return i;
    return -1;
}

bool wardrobe_theme(const char *name, const char *how)
{
    if (!theme_set(name)) { LOG("wardrobe: no palette '%s' (%s): the theme stays %s", name ? name : "", how ? how : "?", theme_name()); return false; }
    hours_retint();                                       /* THE HOURS' LIGHT tints the new base from the next tick */
    if (strcmp(store.theme, theme_name()) != 0) { strncpy(store.theme, theme_name(), sizeof store.theme - 1); store.theme[sizeof store.theme - 1] = 0; store_save_theme(); }
    LOG("wardrobe: the theme is %s%s (%s)", theme_name(), theme_reserved() ? " -- the dark band: the evil slime's glass" : "", how ? how : "?");
    return true;
}

const char *wardrobe_look_missing(void) { return look_missing; }

int wardrobe_look(const char *words, const char *how, bool quiet)
{
    if (!words) return 2;
    char buf[96]; strncpy(buf, words, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *save = NULL; int seen = 0, bad = 0;
    look_missing[0] = 0;
    for (char *tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0; const char *v = eq + 1;
        if (!strcmp(tok, "colour") || !strcmp(tok, "color") || !strcmp(tok, "pal")) { seen++; if (!wardrobe_theme(v, how)) bad++; }
        else if (!strcmp(tok, "form")) {
            seen++;
            /* `form=<form>[/<variant>]` (LOOK_FORM_MAX 25): the variant is
             * the desk's (`man/villager`), the wrist wears the plain body;
             * the table's names are 15 at most */
            char id[32]; strncpy(id, v, sizeof id - 1); id[sizeof id - 1] = 0;
            char *sl = strchr(id, '/'); if (sl) *sl = 0;
            id[WARDROBE_ID_MAX] = 0;
            bool gel = !id[0] || !strcmp(id, "-") || !strcmp(id, "home");
            bool ok;
            if (quiet) {                                  /* a boot's cached line: no veil, what stands stands */
                int idx = gel ? -1 : wardrobe_find(id);
                ok = gel || idx >= 0;
                if (ok) { put_on(idx); save_worn(); }
            } else ok = wardrobe_wear(gel ? "-" : id, how);
            if (!ok) { bad++; strncpy(look_missing, id, sizeof look_missing - 1); look_missing[sizeof look_missing - 1] = 0; }
        }
    }
    return seen ? (bad ? 1 : 0) : 2;
}

void wardrobe_begin(void)
{
    memset(&C, 0, sizeof C); C.idx = -1; C.band = -1; C.band_idle = C.band_act = -1;
    memset(&V, 0, sizeof V); V.pending = -1;
    memset(&mk, 0, sizeof mk);
    own = false; own_last = -1;
    bored_begin((uint32_t)esp_random());       /* THE BORED ROLL: the die seeded per boot */
    mount();
    wardrobe_art_hooks();                     /* ART OVER THE AIR: the partition behind artota's function pointers */
    /* the remembered theme, then the remembered wear -- no veil at a boot */
    if (store.theme[0] && !theme_set(store.theme)) LOG("wardrobe: the stored theme '%s' is not a palette this build knows: blue", store.theme);
    if (store.theme[0]) hours_retint();
    if (W.mounted && store.worn[0] && strcmp(store.worn, "-") != 0) {
        int idx = wardrobe_find(store.worn);
        if (idx >= 0 && !(entry_flags(idx) & ENTRY_FLAG_SAGE)) { put_on(idx); LOG("wardrobe: wearing %s again (nvs)", store.worn); }
        else LOG("wardrobe: the stored wear '%s' is not baked here: the gel", store.worn);
    }
    LOG("wardrobe: theme %s, worn %s", theme_name(), wardrobe_worn_id());
}

void wardrobe_status(char *out, int cap)
{
    if (!W.mounted) { snprintf(out, cap, "wardrobe: NOT MOUNTED (%s) | theme %s%s | worn - (the built-in gel)", W.why, theme_name(), theme_reserved() ? " (dark band)" : ""); return; }
    char roll[64]; bored_status(roll, sizeof roll);
    snprintf(out, cap, "wardrobe: mounted, %d forms, %lu KB, rack %s %s at 0x%lx | worn %s%s%s%s | theme %s%s | veil %s | act %s | %s",
             W.count, (unsigned long)(W.total / 1024), W.hash, W.date, (unsigned long)W.part->address,
             wardrobe_worn_id(), C.idx >= 0 ? " blit " : "", C.idx >= 0 ? (C.blit == 1 ? "1x" : C.blit == 2 ? "2x" : "3x") : "",
             own ? " (the wrist's own)" : "",
             theme_name(), theme_reserved() ? " (dark band)" : "",
             V.phase == 0 ? "-" : V.phase == 1 ? "out" : "in",
             C.idx < 0 ? "-" : C.band_act < 0 ? "none baked" : C.band == C.band_act ? "playing" : C.self_acted ? "played once" : "waits",
             roll);
}

void wardrobe_list(void)
{
    if (!W.mounted) { char l[200]; wardrobe_status(l, sizeof l); Serial.println(l); return; }
    Serial.printf("wardrobe: %d forms (%s %s), %lu KB of %lu\n", W.count, W.hash, W.date, (unsigned long)(W.total / 1024), (unsigned long)(W.part->size / 1024));
    for (int i = 0; i < W.count; i++) {
        slmf_t f; bool ok = slmf_open(W.base + entry_off(i), entry_size(i), &f);
        if (!ok) { Serial.printf("  %-15s  BAD BLOB\n", entry_name(i)); continue; }
        char bands[64] = ""; int n = 0;
        for (int b = 0; b < f.nbands && n < (int)sizeof bands - 20; b++)
            n += snprintf(bands + n, sizeof bands - n, "%s%.15s(%u)", b ? " " : "", band_name(&f, b), band_count(&f, b));
        Serial.printf("  %-15s %3ux%-3u %u-bit %2u cols %3u frames  blit %ux%s  %6lu B  %s%s%s%s\n", entry_name(i), f.w, f.h, f.bpp, f.npal, f.nframes,
                      derive_blit(&f, entry_blit(i)), entry_blit(i) ? "" : " (derived)", (unsigned long)entry_size(i), bands,
                      (entry_flags(i) & ENTRY_FLAG_ACT) ? "" : "  [idle only]",
                      (entry_flags(i) & ENTRY_FLAG_SAGE) ? "  [sage: under a mind line only]" : "",
                      (entry_flags(i) & ENTRY_FLAG_HUMAN) ? "  [the human home]" : "");
        if (entry_w(i) != f.w || entry_h(i) != f.h) Serial.printf("      (the table says %ux%u: the blob disagrees)\n", entry_w(i), entry_h(i));
    }
}

bool wardrobe_check(char *out, int cap)
{
    if (!W.mounted) { snprintf(out, cap, "wardrobe: not mounted (%s)", W.why); return false; }
    int bad = 0; uint32_t bytes = 0;
    for (int i = 0; i < W.count; i++) {
        uint32_t crc = esp_rom_crc32_le(0, W.base + entry_off(i), entry_size(i));
        bytes += entry_size(i);
        if (crc != entry_crc(i)) { bad++; LOG("wardrobe: %s crc %08lx, the table says %08lx", entry_name(i), (unsigned long)crc, (unsigned long)entry_crc(i)); }
    }
    snprintf(out, cap, "wardrobe: %d of %d blobs whole (%lu KB checked)%s", W.count - bad, W.count, (unsigned long)(bytes / 1024), bad ? " -- REFLASH the image" : "");
    return bad == 0;
}

/* --- ART OVER THE AIR: the flash behind artota's hooks ----------------------------- */

static char art_worn[WARDROBE_ID_MAX + 1] = "";   /* the costume that stood when the push began (the veil's swap saves "-" to NVS): back after the remount */

static bool art_erase(uint32_t off, uint32_t len, void *ctx) { (void)ctx; return W.part && esp_partition_erase_range(W.part, off, len) == ESP_OK; }
static bool art_write(uint32_t off, const uint8_t *p, uint32_t n, void *ctx) { (void)ctx; return W.part && esp_partition_write(W.part, off, p, n) == ESP_OK; }
static uint32_t art_crc(uint32_t seed, const uint8_t *p, uint32_t n) { return esp_rom_crc32_le(seed, p, n); }
static int art_release(void *ctx) { (void)ctx; return wardrobe_release(); }
static void art_log(const char *l, void *ctx) { (void)ctx; LOG("%s", l); }
static int art_done(bool whole, char *why, int cap, void *ctx)
{
    (void)ctx;
    int n = wardrobe_remount(whole, why, cap);
    if (n < 0 || !whole) return n;
    char l[200];
    if (!wardrobe_check(l, sizeof l)) { snprintf(why, cap, "a blob's crc (%.40s)", l); LOG("%s", l); return -1; }   /* `wardrobe check` at the end, as the plan says */
    LOG("%s", l);
    return n;
}

void wardrobe_art_hooks(void)
{
    artota_hooks_t h;
    memset(&h, 0, sizeof h);
    h.size = W.part ? W.part->size : 0;
    h.erase = art_erase; h.write = art_write; h.crc32 = art_crc;
    h.release = art_release; h.done = art_done; h.log = art_log;
    artota_begin_hooks(&h);
}

/* the release: a worn costume (or one coming through the veil) goes off
 * through the veil first -- its frames are read out of the mapped bytes,
 * so the bytes cannot go while it stands -- then the mapping is dropped
 * and the wardrobe is "no image" until a whole one lands */
int wardrobe_release(void)
{
    if (!W.part) return -1;
    if (!W.mounted) return 0;                 /* nothing mapped (never mounted, or released already) */
    if (C.idx >= 0 || V.phase) {
        int standing = V.phase ? V.pending : C.idx;
        if (standing >= 0 && !(entry_flags(standing) & ENTRY_FLAG_SAGE)) { strncpy(art_worn, entry_name(standing), WARDROBE_ID_MAX); art_worn[WARDROBE_ID_MAX] = 0; }
        if (!(V.phase && V.pending < 0)) { sage_before = -2; veil_start(-1, "the art over the air"); }
        return 1;
    }
    spi_flash_munmap(W.handle); W.handle = 0; W.base = NULL;
    W.mounted = false; sage_before = -2; word_was = false;
    strncpy(W.why, "no image -- the art over the air is writing", sizeof W.why - 1); W.why[sizeof W.why - 1] = 0;
    LOG("wardrobe: released for the art over the air -- the mapping dropped (%lu KB); today's gel serves%s%s",
        (unsigned long)(W.mapped / 1024), art_worn[0] ? "; back after: " : "", art_worn);
    return 0;
}

int wardrobe_remount(bool whole, char *why, int cap)
{
    if (W.mounted) { spi_flash_munmap(W.handle); W.handle = 0; W.base = NULL; W.mounted = false; }
    mount();
    if (!W.mounted) {
        if (!whole) {
            char inner[64]; strncpy(inner, W.why, sizeof inner - 1); inner[sizeof inner - 1] = 0;
            snprintf(W.why, sizeof W.why, "no image -- the push was abandoned (%.40s)", inner);
        }
        snprintf(why, cap, "%s", W.why);
        art_worn[0] = 0;
        return -1;
    }
    if (art_worn[0]) {
        int idx = wardrobe_find(art_worn);
        if (idx >= 0) veil_start(idx, "the art landed");
        else LOG("wardrobe: %s is not baked in the new image: the gel stays", art_worn);
        art_worn[0] = 0;
    }
    return W.count;
}
