/* HARDWARE ROOM: the second I2C bus, the reserved pins, the BOM bits and
 * the dark drivers (haptic.cpp, gauge.cpp, rtc.cpp, ppg.cpp). Every
 * add-on is compiled in and INERT: a driver does nothing until `bom
 * <part> on` says the part is soldered (an NVS bitmask, store.bom), then
 * its *_begin probes the address and logs found/absent. `bus` scans both
 * buses so a part can be confirmed the day it is soldered.
 *
 * The pins (board.h): the six on the SH1.0 header and nothing else --
 * the static_asserts below refuse a build that spends one of them.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "board.h"

/* the BOM bits (store.bom) */
#define BOM_HAPTIC  0x01    /* DRV2605 + a coin LRA, 0x5A */
#define BOM_GAUGE   0x02    /* MAX17048 fuel gauge, 0x36 */
#define BOM_RTC     0x04    /* PCF85063 RTC, 0x51 */
#define BOM_PPG     0x08    /* MAX30101 PPG, 0x57 (not the 30102: green) */
#define BOM_BUTTON  0x10    /* the case's button on PIN_BUTTON (until then BOOT/GPIO0) */
#define BOM_MOTOR   0x20    /* THE BARE MOTOR on PIN_MOTOR (33, to GND): no driver chip; shares the PPG's pin */
#define BOM_I2C_ANY (BOM_HAPTIC | BOM_GAUGE | BOM_RTC | BOM_PPG)

/* the reservation, checked at compile time */
#define PIN_BIT(p) (1ULL << (p))
#define PIN_RESERVED_MASK (PIN_BIT(PIN_I2C2_SDA) | PIN_BIT(PIN_I2C2_SCL) | PIN_BIT(PIN_INT_RTC) | \
                           PIN_BIT(PIN_INT_GAUGE) | PIN_BIT(PIN_INT_PPG) | PIN_BIT(PIN_BUTTON))
#define PIN_BOARD_MASK    (PIN_BIT(PIN_LCD_SCLK) | PIN_BIT(PIN_LCD_MOSI) | PIN_BIT(PIN_LCD_MISO) | \
                           PIN_BIT(PIN_LCD_DC) | PIN_BIT(PIN_LCD_CS) | PIN_BIT(PIN_LCD_RST) | PIN_BIT(PIN_LCD_BL) | \
                           PIN_BIT(PIN_I2C_SDA) | PIN_BIT(PIN_I2C_SCL) | PIN_BIT(PIN_TP_INT) | PIN_BIT(PIN_TP_RST) | \
                           PIN_BIT(PIN_IMU_INT1) | PIN_BIT(PIN_IMU_INT2) | PIN_BIT(PIN_BAT_ADC) | PIN_BIT(PIN_BOOT))
#define PIN_HEADER_MASK   (PIN_BIT(15) | PIN_BIT(16) | PIN_BIT(17) | PIN_BIT(18) | PIN_BIT(21) | PIN_BIT(33))
static_assert((PIN_RESERVED_MASK & PIN_BOARD_MASK) == 0, "a reserved header pin is also a board pin");
static_assert(PIN_RESERVED_MASK == PIN_HEADER_MASK, "the reserved pins must be exactly the SH1.0 header's six, each used once");
/* the deep-sleep rule: a wake source must be an RTC GPIO (0..21) */
static_assert(PIN_INT_RTC <= 21 && PIN_INT_GAUGE <= 21 && PIN_BUTTON <= 21, "a wake source is not an RTC GPIO");
/* for any future pin use: BUS_PIN_IS_FREE(p) fails the build on a reserved one */
#define BUS_PIN_IS_FREE(p) static_assert(!(PIN_BIT(p) & (PIN_RESERVED_MASK | PIN_BOARD_MASK)), "GPIO " #p " is taken")

#define BUS2_HZ 400000

/* THE BUS UNSTUCK (T4, 2026-10-12): bus 0 (Wire, GPIO 6/7: the touch and
 * the IMU) -- its clock and timeout (main.cpp's Wire.begin and the
 * recovery's use the same two), the rule's state (unstick.h, pure) and
 * the recovery. bus0_note is sense()'s one call a read: the IMU's answer
 * and the touch's last poll; when the rule asks, bus0_unstick runs
 * right there (the loop thread: no touch read is in flight) and one
 * line says so. bus0_state is doctor's. */
#define BUS0_HZ         400000
#define BUS0_TIMEOUT_MS 20              /* a stuck read costs a frame this much, not the core's 50 */
#include "unstick.h"
void bus0_note(bool imu_answered, bool touch_alive);
bool bus0_unstick(void);                /* nine clocks, a STOP, the bus up again, imu_begin, touch_reset; true = the IMU answered */
const unstick_t *bus0_state(void);

void bus_begin(void);                   /* setup(): Wire1 if any I2C part is on the BOM, then the four *_begin */
void bus_tick(void);                    /* loop(): the drivers' ticks (no-ops when absent); the motor's envelope */
bool bus_present(uint8_t bom_bit);      /* on the BOM and answered its probe */
bool bus_on_bom(uint8_t bom_bit);       /* on the BOM (soldered, says NVS) */
void bus_scan(void);                    /* `bus`: what answers on both buses, by name */
bool bus_cmd(int argc, char **argv);    /* `bom`, `bus`, `haptic`, `gauge`, `rtc`, `ppg`; false = not ours */
bool bus_button_down(void);             /* the case's button when on the BOM, else BOOT */

/* the second bus, for the drivers: Wire1 (register, then the bytes) */
bool bus2_wr(uint8_t addr, uint8_t reg, const uint8_t *v, uint8_t n);
bool bus2_wr1(uint8_t addr, uint8_t reg, uint8_t v);
bool bus2_rd(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);
uint8_t bus2_rd1(uint8_t addr, uint8_t reg);
bool bus2_ack(uint8_t addr);            /* an address answers */
