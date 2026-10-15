/* DOCTOR: one health report for Khalid's morning -- `doctor` over serial,
 * `GET /doctor` (text/plain) on the watch's page. About eight lines,
 * under 2 KB: the build id (tools/build_id.py bakes the git hash),
 * uptime, the heap (free / min / largest) and the PSRAM, the static RAM,
 * the frame rate (the last window's composite and its MOVING rate: the
 * one the >= 35 rule is about), the house and its cause, the link (the
 * door held, the lease, the last ok), WiFi, the clock and who set it,
 * the day log (files, today's rows, unsynced days), the last night, the
 * BOM bits and what answered on the buses, the water lock, the alarm,
 * the sky, the grain line -- and a VERDICT: `all well`, or the first
 * thing wrong (no framebuffer / no IMU / no disk, the heap under the
 * 60 KB floor, a dip under 30 KB since boot, under 35 fps while moving,
 * no clock, no networks, no doors, a link that never came up, no day
 * file for today, a BOM part that did not answer, WiFi down). Reads
 * every module's public face; changes nothing. */
#pragma once
#include <stddef.h>

#define DOCTOR_CAP 4096                 /* the report's buffer (malloc'd for the seconds it takes; 2048 until BLUETOOTH's ~300 B line, 09-10 21:50) */

/* the whole report into out (NUL-terminated); returns its length */
int doctor_text(char *out, size_t cap);
/* `doctor` over serial: the report, line by line */
void doctor_print(void);
/* the build id (tools/build_id.py's hash, a `+` when the tree was dirty)
 * for THE WATCH PAGE's ABOUT: this file is the one that includes the
 * generated header, so main need not recompile on every commit */
const char *doctor_build_id(void);
