/* MAX17048 fuel gauge (0x36 on the second bus): the body feels its own
 * cell running down -- a ModelGauge percent instead of the ADC's
 * divider guess. A DARK DRIVER: compiled in, inert until `bom gauge on`.
 * Raw registers (16-bit, MSB first), no library.
 *
 * Host-clean (no Arduino here): face.cpp includes this for the hook. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void gauge_begin(void);             /* probe VERSION (0x001x), the low-cell alert at 10 % on PIN_INT_GAUGE */
void gauge_end(void);
bool gauge_present(void);
/* the ONE call: the cell's millivolts and percent (cached 1 s); false = absent or the read failed */
bool gauge_read(uint16_t *mv, uint8_t *pct);
/* THE FACE'S HOOK: the gauge's mV when it is present and answers, else
 * the ADC's. face.cpp carries a weak fallback that returns adc_mv, so
 * the laptop build of face.cpp needs no gauge. */
uint16_t gauge_prefer(uint16_t adc_mv);
bool gauge_alert(void);             /* ALRT is low (the cell under the threshold) */
void gauge_status(char *out, int cap);
