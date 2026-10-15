/* THE DISK ASKS BEFORE IT FORMATS (2026-10-12, the operator night, T6).
 * `LittleFS.begin(true)` formatted the partition whenever a mount
 * failed -- the first flash's blank partition, yes, but also a mount
 * that failed for a reason the day files survive (a bad block, a
 * half-written directory, a core update that changed the format's
 * mind): ten days of his wrist gone without a word. Now the mount is
 * `begin(false)` and this table says what follows:
 *
 *   mounted, NVS `fsfmt` set         OK        nothing;
 *   mounted, no `fsfmt`              SET_KEY   the key is set, nothing else
 *                                              (the board on his wrist tonight:
 *                                              a formatted partition, ten day
 *                                              files, no key -- it mounts, so
 *                                              it is never formatted);
 *   failed, no `fsfmt`               FORMAT    the first flash, or his
 *                                              `uploadfs` (a blank partition):
 *                                              format once, set the key;
 *   failed, `fsfmt` set              REFUSE    NEVER format: the files may be
 *                                              there; doctor's day line and
 *                                              its verdict lead with DISK
 *                                              FAILED TO MOUNT; the minutes
 *                                              count as unlogged;
 *   failed, by hand (`day format`)   HAND_FORMAT  the one hand that may:
 *                                              format, set the key, remount;
 *   mounted, by hand                 HAND_REFUSE  the disk mounts: nothing to
 *                                              mend (`day rm` takes a file).
 *
 * Pure: the mount's answer, the key and the hand are parameters;
 * tools/fsguard_test.cpp runs the table on the laptop. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FSG_OK = 0,         /* mounted, the key set: nothing to do */
    FSG_SET_KEY,        /* mounted, no key yet: set it (never format a disk that mounts) */
    FSG_FORMAT,         /* failed, never formatted by this firmware: format once, set the key */
    FSG_REFUSE,         /* failed, formatted before: NEVER format; say so */
    FSG_HAND_FORMAT,    /* `day format` on a failed disk: format, set the key, remount */
    FSG_HAND_REFUSE     /* `day format` on a disk that mounts: refused with its reason */
};

static inline int fsg_decide(bool mounted, bool formatted_before, bool by_hand)
{
    if (by_hand) return mounted ? FSG_HAND_REFUSE : FSG_HAND_FORMAT;
    if (mounted) return formatted_before ? FSG_OK : FSG_SET_KEY;
    return formatted_before ? FSG_REFUSE : FSG_FORMAT;
}

/* does the verdict format the partition */
static inline bool fsg_formats(int v) { return v == FSG_FORMAT || v == FSG_HAND_FORMAT; }

static inline const char *fsg_word(int v)
{
    switch (v) {
    case FSG_OK:          return "mounted";
    case FSG_SET_KEY:     return "mounted; the fsfmt key set (never formatted by this firmware before)";
    case FSG_FORMAT:      return "failed to mount and never formatted: formatting once";
    case FSG_REFUSE:      return "DISK FAILED TO MOUNT (the files may be there; `day format` wipes it)";
    case FSG_HAND_FORMAT: return "formatting by hand";
    case FSG_HAND_REFUSE: return "the disk mounts: nothing to format (`day rm` takes a file, `day prune` the keep's)";
    }
    return "?";
}

#ifdef __cplusplus
}
#endif
