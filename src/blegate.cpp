/* THE BLUETOOTH GATE -- blegate.h says why; this is the rule, Arduino-free. */
#include "blegate.h"

bool blegate_should_be_up(const blegate_in_t *in)
{
    if (!in->enabled) return false;          /* his switch: OFF = never up */
    if (in->dark && !in->ear) return false;  /* the dark house has no radio of any kind -- unless THE DARK EAR (a peer bonded): the stack stays, the nap idles awake */
    if (in->wifi_on) return false;           /* ruling 58: BLE only when WiFi is down */
    return true;                             /* the dock, a lit glass, the radio resting: up (docked is not a veto) */
}

const char *blegate_why(const blegate_in_t *in)
{
    if (!in->enabled) return "off (SETTINGS)";
    if (in->dark && !in->ear) return "the dark house";
    if (in->wifi_on) return "the WiFi radio is up";
    return "up";
}

bool blegate_both(bool ble_up, bool wifi_on)
{
    return ble_up && wifi_on;
}
