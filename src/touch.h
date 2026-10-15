/* CST816S capacitive touch, raw over I2C. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool down;         /* a finger is on the glass */
    int x, y;          /* screen px */
    uint8_t gesture;   /* the chip's own reading (0x05 click, 0x0C long press) */
} touch_t;

bool touch_begin(void);          /* resets the chip, disables its auto-sleep */
void touch_read(touch_t *t);     /* polls the chip */
void touch_let_sleep(bool yes);  /* re-enable the chip's auto-sleep for the dark */
/* THE BUS UNSTUCK (2026-10-12): the light reset -- a RST pulse, then the
 * auto-sleep forbidden again (as touch_begin leaves it); 55 ms, no log
 * line; what touch_let_sleep(false) does and what bus.cpp's recovery does */
void touch_reset(void);
/* THE TOUCH LINE (doctor, 2026-10-12): the chip's health as the last poll
 * saw it -- a glass that answers nothing fails silently otherwise (every
 * poll `no finger`, no log line; the WATER LOCK looked the same that night) */
bool     touch_alive(void);      /* the last poll was answered */
uint32_t touch_nacks(void);      /* polls the chip did not answer since boot */
uint8_t  touch_chip_id(void);    /* 0xB4 = CST816S; 0 = never answered */

