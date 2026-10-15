/* THE WRIST WARDROBE (design/watch-wardrobe.md; Khalid 2026-09-11: "the
 * watch should have all forms baked into it but the watch can only
 * MORPH, no summoning"; "it doesn't even need an exit. The exit can be
 * the veil on the watch, like how we do the morph").
 *
 * Every costume the wrist can wear is baked into a flash partition of
 * its own (partitions.csv: `art`, type data / subtype 0x40, 2 MB at
 * 0x610000) as one image -- a 32-byte head, a table of forms, then the
 * exporter's own `.slmf` blobs (IDLE + ONE ACT, 4 bpp, cropped to the
 * ink; watchart.py has the byte layout of a blob; the image's is in the
 * design doc, "The image on flash (firmware reading)", and in the
 * README). The partition is MAPPED once at boot (esp_partition_mmap:
 * the flash MMU, the same mechanism the app's own rodata rides), and
 * every frame is blitted out of the mapped bytes straight into the
 * framebuffer: no frame, no form, no palette is ever copied into RAM
 * (the one RAM cost is a 16-entry RGB565 LUT per wear, 32 bytes).
 *
 * MORPH ONLY. The wrist has one body on the glass: the gel, or one
 * costume standing where the gel's base centre was, on the wall, riding
 * the wall's lean and slide as the gel does, sinking with the house's
 * departure as the gel does. It arrives and leaves through THE VEIL --
 * VEIL_MS of motes in the theme's shine over the base centre, the swap
 * at the midpoint (gel -> costume, costume -> gel, costume -> costume:
 * never a walk, never a summon, never a finale). The idle band loops;
 * the act plays once on a pet while worn, and once on its own per wear
 * after ACT_SELF_S of lit wear; nothing else moves.
 *
 * THE HANDOVER carries it as a verb of its own (watch-link.md, `look`):
 * the wrist asks `look` after every `ok held` -> `ok look form=<form>
 * [/<variant>] colour=<palette>`, and the desk PUSHES the same line
 * (`look form=... colour=...`) after every `ok held` and whenever the
 * look changes while the wrist holds; the wrist answers `ok` or `no art
 * <form>`. link.cpp hands the words here (wardrobe_look). `form=-` is
 * the gel, `human` the human home; a form not baked = `no art`, the
 * body stays as it was; a reserved colour (umbra ...) = the evil
 * crossing -> the dark theme. The worn form and the theme are kept in
 * NVS (store.worn / store.theme) so a boot wears what it wore, no veil.
 *
 * THE WRIST'S OWN MORPHS (rule 4's second half, 2026-09-23; bored.h has
 * the clocks): on the bare glass, holding, after a bored spell of lit
 * seconds at the desk's calm cadence, the die picks a costume out of the
 * table (never a sage, never the human home, never the last twice), the
 * veil brings it, it is worn a few lit minutes (the one act at the
 * midpoint), the veil takes it home. The desk is told on the open door as
 * the other senses go: `sense wrist:look form=<name>`, then `form=gel`.
 * The desk's pushed `look` always wins and ends the roll's wear (no word
 * back: the desk knows what it pushed); `wardrobe off` ends it too (the
 * desk hears `form=gel`); the house emptying ends it silently. Never
 * saved: NVS keeps the desk's look only.
 *
 * THE SAGE (rule 5): a baked "sage" is never worn by the wire's word or
 * a bored roll -- it surfaces only while a mind line is on the glass
 * (word_showing(), the house holding): the veil brings it on the
 * bubble's first frame and takes it back on the last, and it keeps its
 * own white and gold whatever the theme (no costume is ever tinted).
 *
 * DEGRADES TO TODAY'S GEL: no partition, no image, a bad magic or
 * version, a table that points outside the image -- the wardrobe is
 * simply not mounted, every wear answers false, the gel is drawn as
 * before; the watch half never waits on the art. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "dirty.h"

#define WARDROBE_ID_MAX 15

void wardrobe_begin(void);                 /* find + mmap + index; then the remembered wear, no veil (after store_begin and hours_begin) */
/* one call a frame from loop(): the band clocks, the veil's clock, the
 * act's own once-a-wear, the Sage under the word (word_on = the bubble
 * is on the glass), THE BORED ROLL (bare = no page, awake, the body free:
 * no touch, pet, fall, wake ease, theatre, stretch, peek) */
void wardrobe_tick(float dt, bool lit, bool holding, bool word_on, bool bare);
/* the draw, after the gel's compose (or the black glass), before the
 * face: the costume at the wall (0..3, GEL_WALL_*) with the gel's own
 * lift / slide, then the veil's motes; nothing when the house is empty */
void wardrobe_draw(uint16_t *fb, int wall, float lift, float slide, bool house_gel);
bool wardrobe_mark(dirty_rect_t *box);     /* THE MARK: what changed on the glass this frame */
bool wardrobe_covers_gel(void);            /* a costume stands where the gel would: main.cpp does not raster the gel */
bool wardrobe_worn(void);                  /* a costume is on (or coming through the veil) */
bool wardrobe_own(void);                   /* THE BORED ROLL: the wear is the wrist's own (never the desk's, never NVS) */
void wardrobe_roll_now(void);              /* `wardrobe roll`: the urge now, the costume for certain (a test aid) */

/* the wardrobe */
bool wardrobe_mounted(void);
int  wardrobe_count(void);
int  wardrobe_find(const char *id);        /* the table's index, -1 = not baked */
const char *wardrobe_id_at(int i);         /* "" past the end */
/* WEAR: the veil, then the costume (id "-" / "home" / "" = the gel);
 * false = not baked (nothing changes). how = who asked (the log) */
bool wardrobe_wear(const char *id, const char *how);
bool wardrobe_act(void);                   /* the worn costume's one act, now (a pet while worn); false = none / not worn / already playing */
/* THE LOOK: `form=<id> pal=<name>` words (any order, unknown keys
 * skipped; `colour=` / `color=` are `pal=` too) from the handover or a
 * pushed ask. Returns 0 = every word taken, 1 = a form not baked (the
 * rest taken), 2 = nothing readable. quiet = no veil (a boot's cached
 * line) */
int  wardrobe_look(const char *words, const char *how, bool quiet);
const char *wardrobe_look_missing(void);   /* after a look that said 1: the form that is not baked (the `no art <form>` answer) */
bool wardrobe_theme(const char *name, const char *how);   /* the theme by name / index, NVS; false = no such palette */
const char *wardrobe_worn_id(void);        /* "-" for the gel */
void wardrobe_status(char *out, int cap);  /* one line: mounted? count worn pal veil act */
void wardrobe_list(void);                  /* every baked form, one line each, over serial */
bool wardrobe_check(char *out, int cap);   /* every blob's crc32 against the table: true = whole */

/* ART OVER THE AIR (artota.h; net.cpp's `POST /art`): the flash behind
 * the state machine's hooks, installed at wardrobe_begin. The release
 * takes a worn costume off through the veil first (1 = not yet), then
 * drops the MMU mapping (0; -1 = no partition); the remount maps the new
 * image, runs the crc check and brings the worn costume back through the
 * veil if the new image still bakes it (the count, or -1 with why) */
void wardrobe_art_hooks(void);
int  wardrobe_release(void);
int  wardrobe_remount(bool whole, char *why, int cap);
