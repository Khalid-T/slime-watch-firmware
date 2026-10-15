/* BLUETOOTH: ble.h says what; blegate.cpp is the rule; blestack.cpp is the stack.
 *
 * THE HEAP RULE (README "BLUETOOTH"): >= 60 KB free with BLE up and the
 * TLS session down. The two stacks never share the heap: the BLE stack
 * (~35 KB internal, measured) is torn down before the WiFi radio comes
 * up for a dial, and comes back once the radio is off. MEASURED
 * 2026-10-06 on the board (build 58cc617+): the stack takes 34,956 B at
 * each rise and gives it all back (104,680 -> 69,724 -> 104,680); the
 * TLS session held reads 63,252 B free before BLE ever rose, 63,176 and
 * 63,164 after the first and second up/down cycles -- the "25 KB held"
 * of the 09-10 boot is not there. The line `ble: up ... heap A -> B free
 * (the stack took C; min M, largest L)` at each rise and the 30 s
 * budget line are the board's numbers to read; the host cannot measure
 * this. */
#include "ble.h"
#include "blegate.h"
#include "blestack.h"
#include "ancs.h"
#include "bleuart.h"
#include "app.h"
#include "net.h"
#include "link.h"
#include "store.h"
#include "haptic.h"
#include "motor.h"
#include "rest.h"      /* the quiet hours: a dark notice is silent through them */
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

static const uint32_t REPORT_MS = 30000;      /* the budget line while the stack is up */

static bool up = false;
static uint32_t up_ms = 0, report_ms = 0, heap_before = 0;
static uint32_t rises = 0, refused_wifi = 0;  /* how often the stack came up since boot; how often a radio edge found WiFi up (the both-stacks refusal) */
static char pair_word[8];
static int  last_bonds = 0;                   /* bonds as of the last rise (the ear's test needs it with the stack down) */
static bool ear_said = false;
static int  again_kind = -1; static uint32_t again_at_ms = 0;   /* a dark notice's second play */

static void gate_in(blegate_in_t *in)
{
    in->enabled = !store.ble_off && !(spent_line() && !charging());   /* THE BROWNOUT LOOP (2026-10-14): under the spent line off the cable the stack stays DOWN -- its burst at boot is what resets a 3.5 V cell; the ear is not cheap there (the operator's ruling, his to keep) */
    in->dark    = app.state == 2;                /* the dark house (app.h: 0 awake, 1 dim, 2 dark; go_dark sets it before net_off) */
    in->wifi_on = net_radio_is_on();
    in->docked  = link_docked();
    in->ear     = ble_keeps_dark();
}

void ble_begin(void)
{
    blestack_begin();
    LOG("ble: %s (NVS) -- the stack comes up at the wake, only while the WiFi radio is down (ruling 58); down for every dial, back after it", store.ble_off ? "OFF" : "on");
}

void ble_up(const char *why)
{
    if (up) return;
    blegate_in_t in; gate_in(&in);
    if (!blegate_should_be_up(&in)) {
        /* the WiFi case is the one worth a line (it is the rule at work); OFF and the dark house are quiet */
        if (in.wifi_on && in.enabled && !in.dark) { refused_wifi++; LOG("ble: stays down (%s) -- %s: never both stacks", why, blegate_why(&in)); }
        return;
    }
    /* THE ASSERT (a log line and a refusal, never a crash): the gate said up, the radio must be off */
    if (blegate_both(true, net_radio_is_on())) { LOG("ble: REFUSED (%s) -- the WiFi radio is up and the gate said up: a bug in the edges, not a crash", why); return; }
    heap_before = ESP.getFreeHeap();
    if (!blestack_on()) { LOG("ble: the stack would not start (%s); heap %u free", why, (unsigned)ESP.getFreeHeap()); return; }
    up = true; rises++;
    last_bonds = blestack_bonds() > 0 ? blestack_bonds() : 0;
    up_ms = report_ms = millis();
    uint32_t after = ESP.getFreeHeap();
    LOG("ble: up (%s) -- advertising as \"%s\" (%d bond%s); heap %u -> %u free (the stack took %u; min %u, largest %u), psram %u free",
        why, blestack_name(), blestack_bonds(), blestack_bonds() == 1 ? "" : "s",
        (unsigned)heap_before, (unsigned)after, (unsigned)(heap_before - after),
        (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getFreePsram());
}

void ble_down(const char *why)
{
    if (!up) return;
    uint32_t t0 = millis(), before = ESP.getFreeHeap();
    blestack_off();
    up = false;
    LOG("ble: down (%s) in %lu ms; heap %u -> %u free (was up %lu s)", why, (unsigned long)(millis() - t0),
        (unsigned)before, (unsigned)ESP.getFreeHeap(), (unsigned long)((t0 - up_ms) / 1000));
}

bool ble_is_up(void) { return up; }
bool ble_keeps_dark(void) { return !store.ble_off && last_bonds > 0; }
bool ble_bonded(void) { return (up ? blestack_bonds() : last_bonds) > 0; }
bool ble_phone_ready(void) { return up && ancs_connected(); }
bool ble_desk_open(void) { return up && bleuart_linked(); }
int  ble_desk_rssi(void) { return up ? bleuart_desk_rssi() : 0; }   /* THE DESK NEXT DOOR */

void ble_dark(void)
{
    if (!ble_keeps_dark()) { ear_said = false; ble_down("the dark house"); return; }
    if (!ear_said) { ear_said = true; LOG("ble: THE DARK EAR -- the stack stays up through the dark glass (%d bond%s): a phone's notice bumps the wrist, its wisp waits for the light; the nap idles awake", last_bonds, last_bonds == 1 ? "" : "s"); }
    ble_up("the dark ear");                           /* through the gate: back up if a dial had it down */
}

void ble_ear_tick(void)
{
    ble_tick(0.025f);
    motor_tick();                                     /* a dark bump's envelope runs here: loop() is not running */
}

void ble_dark_notice(int kind, int repeats)
{
    if (kind < 0 || store.bumps_off) return;
    if (rest_night_now()) { LOG("ble: a notice in the dark, kept silent (the quiet hours)"); return; }
    bool played = haptic_play(kind);
    if (played && repeats > 1) { again_kind = kind; again_at_ms = millis() + 250; }
    LOG("ble: a notice on the dark glass -- felt now%s (the wisp waits for the light)", played ? "" : " (no motor)");
}
bool ble_enabled(void) { return !store.ble_off; }

void ble_set_enabled(bool on, const char *how)
{
    bool changed = on == store.ble_off;
    store.ble_off = !on;
    if (changed) store_save_ble();
    LOG("ble: %s (%s%s)", on ? "ON -- up whenever the WiFi radio is down" : "OFF -- never up", how, changed ? ", stored" : "");
    if (on) ble_up("the switch"); else { blestack_pair_close("the switch off"); ble_down("the switch"); }
}

int ble_pair_open(uint32_t secs, const char *how)
{
    if (store.ble_off) { LOG("ble: no pairing window -- the BLUETOOTH row is OFF (%s); tap it to ON first", how); return -1; }
    blestack_pair_open(secs, how);
    ble_up("the pairing window");                       /* through the gate: up now if the radio is down */
    return (int)blestack_pair_left_s();
}

uint32_t ble_pair_left_s(void) { return blestack_pair_left_s(); }
void ble_pair_close(const char *why) { blestack_pair_close(why); }
void ble_forget(void) { blestack_forget(); }

const char *ble_word(void)
{
    if (store.ble_off) return "OFF";
    uint32_t left = blestack_pair_left_s();
    if (left) { snprintf(pair_word, sizeof pair_word, "%lu S", (unsigned long)left); return pair_word; }   /* THE PAIRING WINDOW: the count, in the shine (as the PULSE row counts) */
    bool phone = up && ancs_connected(), desk = up && bleuart_linked();
    if (phone && desk) return "BOTH";
    if (phone) return "PHONE";
    if (desk) return "DESK";
    return "ON --";                               /* advertising -- or down for a dial: the switch is ON, nobody on (five glyphs: what row 3's chord leaves beside BLUETOOTH) */
}

void ble_tick(float dt)
{
    if (again_kind >= 0 && (int32_t)(millis() - again_at_ms) >= 0) { haptic_play(again_kind); again_kind = -1; }
    ancs_tick(dt);
    bleuart_tick();
    blestack_tick();
    uint32_t bonds = blestack_take_bonded();
    if (bonds) {
        /* a bond landed: one bump (BUMPS permitting), whoever it was */
        bool played = !store.bumps_off && haptic_play(HAPTIC_BUMP);
        LOG("ble: a bond landed (%lu)%s", (unsigned long)bonds, played ? " -- one bump" : "");
    }
    if (up && millis() - report_ms >= REPORT_MS) {
        /* THE BUDGET LINE: what the rule is judged on (README "BLUETOOTH") */
        report_ms = millis();
        char a[120]; ancs_status(a, sizeof a);
        char b[160]; blestack_status(b, sizeof b);
        LOG("ble: %s; %s; heap %u free (min %u, largest %u), fps %.1f, wifi %s, link %s",
            b, a, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
            (unsigned)ESP.getMaxAllocHeap(), app.fps, net_state_name(), link_state_name());
    }
}

void ble_status(char *out, int cap)
{
    blegate_in_t in; gate_in(&in);
    char a[120]; ancs_status(a, sizeof a);
    char b[200]; blestack_status(b, sizeof b);
    char u[160]; bleuart_status(u, sizeof u);
    char st[48];
    if (store.ble_off) strcpy(st, "OFF");
    else if (up) snprintf(st, sizeof st, "up %lu s", (unsigned long)((millis() - up_ms) / 1000));
    else snprintf(st, sizeof st, "down (%s)", blegate_why(&in));
    snprintf(out, cap, "ble %s: %s | %s | %s | heap %u free (min %u, largest %u) | rises %lu, held down by wifi %lu | the rule: BLE only while the WiFi radio is down",
             st, b, a, u, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
             (unsigned long)rises, (unsigned long)refused_wifi);
}
