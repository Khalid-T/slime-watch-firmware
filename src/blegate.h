/* THE BLUETOOTH GATE (2026-09-10 21:50, Khalid's ruling 58: "do BLE
 * only when WiFi is down").
 *
 * The ESP32-S3 has BLE; the default build left it out for the heap
 * (README "ANCS": measured with WiFi + TLS + BLE all up the heap sat at
 * 35-37 KB free, under the 60 KB floor). The two never share the heap
 * now: BLE is UP only while the WiFi radio is OFF. The moment THE RADIO
 * BUDGET brings WiFi up for a dial, BLE is torn down FIRST (before
 * WiFi.begin); BLE comes back after net_radio_off (the linger's end).
 * The dark house has neither (the nap is light sleep: the BLE
 * controller's clock would not survive it, and the phone's notices
 * wait for the light as the desk's wisps do). On the serial dock the
 * radio never dials (RADIO_DOCKED), so BLE stays up: the dock is not a
 * veto. NEVER both stacks at once: blegate_both() is the predicate
 * ble.cpp refuses on (a log line, not a crash) and doctor's verdict
 * reads.
 *
 * Pure (no Arduino): tools/blegate_test.cpp walks every combination on
 * the laptop; ble.cpp feeds it the live inputs at each radio edge. */
#pragma once
#include <stdbool.h>

typedef struct {
    bool enabled;       /* the BLUETOOTH row (NVS `bleoff` absent / false) */
    bool dark;          /* the dark house (app.state == 2) */
    bool wifi_on;       /* net_radio_is_on(): the WiFi radio up in any state (scanning, joining, joined) */
    bool docked;        /* the desk on the serial port (never a veto: the radio does not dial on the dock) */
    bool ear;           /* THE DARK EAR (2026-10-06, ruling 78 his way): a peer is bonded -- the dark glass is no veto, the stack stays up through the nap */
} blegate_in_t;

/* should the BLE stack be up now: enabled, the glass not dark, the WiFi radio off */
bool blegate_should_be_up(const blegate_in_t *in);
/* why not (or "up"): "off (SETTINGS)", "the dark house", "the WiFi radio is up" -- the log's word */
const char *blegate_why(const blegate_in_t *in);
/* the rule broken: both stacks up at once (true = wrong) */
bool blegate_both(bool ble_up, bool wifi_on);
