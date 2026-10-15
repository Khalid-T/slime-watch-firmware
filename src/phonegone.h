/* THE PHONE LEFT BEHIND (design/watch-night-2026-10-12.md C17, THE
 * OPERATOR NIGHT): when the phone's BLE drops (READY's falling edge:
 * ble_phone_ready, the stack still up -- a drop WITH the stack is the
 * radio's own, a WiFi dial or the dark house, not the phone's) while
 * the wrist was walking inside the last PHONEGONE_WALK_MS, he walked
 * away from it. Nothing then (the glass is dark, or he is mid-stride).
 * On his NEXT raise, once: the gel looks toward the air door (the
 * wisps' door, the rim opposite the floor, wisp.cpp's DOOR_R) for
 * PHONEGONE_S, then at him. The debt is cleared when the phone is back
 * before the raise (READY's rising edge), and spent by the raise
 * whether or not the gel was there to look (an empty glass gets
 * nothing: the moment was the raise, not a debt kept for days).
 * Never a bump, never a word, never twice, no sense up the wire; iOS
 * drops BLE on its own now and then, so a false look is one glance.
 *
 * The module is the edges, the window and the debt, fed each lit
 * frame AND each nap poll (the phone leaves in the dark: ble_phone_ready
 * reads the peer table the host task keeps, wrist_walking is live on
 * the nap's once-a-poll wrist_tick). One clock, now_ms, so the nap's
 * 250 ms .. 1 s polls and the lit frames feed it alike. main turns the
 * door point into the gaze with the far tap's own `look_at`.
 *
 * Pure C, no Arduino. tools/phonegone_test.cpp runs it on the laptop:
 *   c++ -std=gnu++17 -I src -o /tmp/phonegone_test tools/phonegone_test.cpp -x c src/phonegone.c -lm && /tmp/phonegone_test */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHONEGONE_WALK_MS 60000u   /* the walking window: a drop this long after the last stride is a walk away from the phone */
#define PHONEGONE_S       1.5f     /* the look toward the air door, then at him (the far tap's own second and a half) */
#define PHONEGONE_R       116.0f   /* the air door's radius: the wisps' door (wisp.cpp DOOR_R), the rim opposite the floor */

typedef struct {
    uint32_t now_ms;
    bool up;             /* ble_is_up(): the stack is on -- a READY fall with the stack going down is the radio's, not a walk away */
    bool ready;          /* ble_phone_ready(): the phone on, bonded and subscribed */
    bool walking;        /* wrist_walking(): steps coming in now */
    bool raise;          /* this frame's wake was a raise (TELL_CUE_RAISE's moment); false in the nap */
    bool holding;        /* house_holding(): the gel is drawn */
    bool lit;            /* the glass is on */
    bool page;           /* THE WATCH PAGE up */
} phonegone_in_t;

enum { PHONEGONE_EV_NONE = 0, PHONEGONE_EV_GONE, PHONEGONE_EV_BACK, PHONEGONE_EV_LOOK };   /* walked away (the debt taken); the phone back (cleared); the raise's look began */

int   phonegone_tick(const phonegone_in_t *in);   /* one call a frame or a poll; the event */
bool  phonegone_owed(void);                       /* the debt is held: he walked away, the raise has not come */
bool  phonegone_on(void);                         /* the look runs */
float phonegone_left_s(void);                     /* seconds of it left, 0 = none */
int   phonegone_count(void);                      /* looks since boot */
int   phonegone_drops(void);                      /* walk-aways since boot (false ones too: the phone's word is all it has) */

/* the air door's glass point: the rim opposite the floor, phi the
 * pull's angle (bowl_phi: 0 = the floor at the bottom), r from (cx, cy) */
void  phonegone_point(float phi, float cx, float cy, float r, float *x, float *y);

#ifdef __cplusplus
}
#endif
