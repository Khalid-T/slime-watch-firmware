/* THE NOTICE CLEARED FROM THE WRIST (2026-10-12, the operator night,
 * T17): the one thing the wrist ever SAYS BACK to the phone's
 * Notification Center.
 *
 * ANCS's Control Point takes a third command beside the two asks
 * (ancs.cpp's GetNotificationAttributes 0 and GetAppAttributes 1):
 * PerformNotificationAction, CommandID 2 -- six bytes, the CommandID,
 * the notification's UID (u32, little-endian), and the ActionID: 0 the
 * POSITIVE action (answer a call, open the thing), 1 the NEGATIVE
 * (dismiss; a ringing call DECLINED). The watch sends the negative
 * ONLY, ever -- a hold on an open notice card says "I have read it,
 * take it off the phone's shade" (ruling 139); nothing on the wrist
 * answers a call or opens an app on his behalf, nothing goes unasked.
 * The frame builder refuses any other action by building nothing.
 *
 * The UID is the phone's (the Notification Source's, per notice;
 * ancs.cpp keeps it per app in its note map) -- a notice the wrist
 * made up itself (`ancs fake`, the desk's wisps have none at all)
 * carries a uid from ANCSACT_FAKE_BASE up and is never sent: the
 * phone would only answer "invalid parameter". Pure: no Arduino;
 * tools/ancsact_test.cpp walks it. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ANCSACT_CMD        2            /* CommandID: PerformNotificationAction */
#define ANCSACT_POSITIVE   0            /* ActionID: answer / open -- NEVER sent (ruling 139) */
#define ANCSACT_NEGATIVE   1            /* ActionID: dismiss / decline -- the one the wrist sends */
#define ANCSACT_LEN        6            /* the frame: cmd, uid LE32, action */
#define ANCSACT_FAKE_BASE  0xFFFF0000u  /* `ancs fake`'s uids live here: the wrist's own, never the phone's */

/* a uid the phone would know: not one of the wrist's own fakes, not the empty 0 */
static inline bool ancsact_uid_real(uint32_t uid) { return uid != 0 && uid < ANCSACT_FAKE_BASE; }

/* the frame into out[ANCSACT_LEN]; returns its length, or 0 (nothing
 * written) for any action but the negative, or a uid the phone would
 * not know */
static inline int ancsact_frame(uint8_t *out, uint32_t uid, uint8_t action)
{
    if (action != ANCSACT_NEGATIVE || !ancsact_uid_real(uid)) return 0;
    out[0] = ANCSACT_CMD;
    out[1] = (uint8_t)uid; out[2] = (uint8_t)(uid >> 8); out[3] = (uint8_t)(uid >> 16); out[4] = (uint8_t)(uid >> 24);
    out[5] = ANCSACT_NEGATIVE;
    return ANCSACT_LEN;
}
