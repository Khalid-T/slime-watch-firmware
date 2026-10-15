/* Waveshare ESP32-S3-Touch-LCD-1.28 (SKU 25098): the pins, from the
 * wiki's "Pin connection" table (docs.waveshare.com/ESP32-S3-Touch-LCD-1.28).
 */
#pragma once

/* LCD: GC9A01, 4-wire SPI (up to 80 MHz) */
#define PIN_LCD_SCLK  10
#define PIN_LCD_MOSI  11
#define PIN_LCD_MISO  12
#define PIN_LCD_DC     8
#define PIN_LCD_CS     9
#define PIN_LCD_RST   14
#define PIN_LCD_BL     2

/* the one I2C bus: touch and IMU share it */
#define PIN_I2C_SDA    6
#define PIN_I2C_SCL    7

/* CST816S capacitive touch */
#define PIN_TP_INT     5
#define PIN_TP_RST    13
#define ADDR_CST816S  0x15

/* QMI8658 6-axis IMU (SA0 high on this board; 0x6A probed as a fallback) */
#define PIN_IMU_INT1   4
#define PIN_IMU_INT2   3
#define ADDR_QMI8658  0x6B
#define ADDR_QMI8658_ALT 0x6A

/* battery sense: Vbat through 200k/100k onto GPIO1 (unused: on USB) */
#define PIN_BAT_ADC    1

/* BOOT reads as a user button once flashing is done */
#define PIN_BOOT       0

/* THE SH1.0 HEADER: 15, 16, 17, 18, 21, 33 -- the six pins the future BOM
 * gets, RESERVED here so nothing else takes them (bus.h checks at compile
 * time; README "HARDWARE ROOM" has the table and the causes). SDA/SCL 6/7
 * are NOT on the header (the wiki's table: they reach only the touch and
 * the IMU), so the add-ons live on a SECOND bus. All six can raise an
 * interrupt; only 0..21 are RTC GPIOs (a deep-sleep wake), so 33 takes
 * the one interrupt that never has to wake the chip. */
#define PIN_I2C2_SDA   18   /* the second bus (SDA/SCL crossed at the module's solder, 2026-10-03: 18 is SDA, 21 is SCL; the wires are the 09-25 lanes): MAX30101 0x57, PCF85063 0x51, DRV2605 0x5A, MAX17048 0x36 */
#define PIN_I2C2_SCL   21
#define PIN_INT_RTC    17   /* PCF85063 INT (open drain, low): an alarm can wake a deep sleep */
#define PIN_INT_GAUGE  16   /* MAX17048 ALRT (open drain, low): the cell low -- a wake to note it */
#define PIN_INT_PPG    33   /* MAX30101 INT (open drain, low): FIFO almost full -- only during a spot read, awake */
#define PIN_MOTOR      33   /* THE BARE MOTOR (2026-09-12): a coin motor to GND, straight off the pin -- SHARES the PPG's INT until the PPG is soldered (`bom` refuses both) */
#define PIN_BUTTON     15   /* the case's button (to GND); BOOT/GPIO0 stands in until the case exists */

#define SCREEN_W     240
#define SCREEN_H     240
