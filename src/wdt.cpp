#include "wdt.h"
#include "app.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

/* arduino-esp32 core 2.x (IDF 4.4): esp_task_wdt_init(timeout_s, panic)
 * reconfigures the SDK's already-running TWDT (IDLE0 at 5 s) to our
 * timeout; esp_task_wdt_add(NULL) subscribes the calling task. A missed
 * feed panics ("Task watchdog got triggered"), and the panic handler
 * reboots: THE LAST DEATH reads it as ESP_RST_TASK_WDT at the next boot. */

static wdt_track_t s_track;
static bool s_armed = false;

void wdt_begin(void)
{
    esp_err_t e1 = esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_err_t e2 = esp_task_wdt_add(NULL);
    s_armed = (e1 == ESP_OK) && (e2 == ESP_OK || e2 == ESP_ERR_INVALID_ARG);   /* INVALID_ARG = already subscribed */
    if (s_armed) { esp_task_wdt_reset(); wdt_feed_take(&s_track, millis(), WDT_SPELL_SLEEP); }
    LOG("wdt: %s on loopTask, %d s (init %d, add %d)", s_armed ? "armed" : "NOT ARMED", WDT_TIMEOUT_S, (int)e1, (int)e2);
}

void wdt_feed(int spell)
{
    if (!s_armed) return;
    if (wdt_feed_take(&s_track, millis(), spell)) esp_task_wdt_reset();
}

int wdt_status(char *out, size_t cap)
{
    if (!s_armed) return snprintf(out, cap, "wdt NOT ARMED");
    return wdt_word(out, cap, &s_track, millis());
}
