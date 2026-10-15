#include "touch.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>

#define REG_GESTURE     0x01
#define REG_FINGERS     0x02
#define REG_XH          0x03
#define REG_CHIP_ID     0xA7    /* 0xB4 = CST816S */
#define REG_FW_VER      0xA9
#define REG_DIS_AUTOSLP 0xFE    /* 1 = never sleep on its own */

static bool     s_alive = false;   /* THE TOUCH LINE: the last poll answered */
static uint32_t s_nacks = 0;
static uint8_t  s_id = 0;

static bool wr(uint8_t reg, uint8_t v)
{
    Wire.beginTransmission(ADDR_CST816S);
    Wire.write(reg);
    Wire.write(v);
    return Wire.endTransmission() == 0;
}

static bool rd(uint8_t reg, uint8_t *buf, size_t n)
{
    Wire.beginTransmission(ADDR_CST816S);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ADDR_CST816S, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

bool touch_begin(void)
{
    pinMode(PIN_TP_INT, INPUT_PULLUP);
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW);
    delay(10);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(100);
    uint8_t id = 0, fw = 0;
    bool ok = rd(REG_CHIP_ID, &id, 1);
    s_alive = ok; s_id = ok ? id : 0;
    rd(REG_FW_VER, &fw, 1);
    Serial.printf("[touch] chip id 0x%02X fw %u (%s)\n", id, fw,
                  ok ? "ok" : "no answer");
    wr(REG_DIS_AUTOSLP, 0x01);
    return ok;
}

void touch_reset(void)
{
    /* asleep on its own the chip answers nothing on the bus (a wake by a
     * raise or a word found it that way: every poll a NACK); only RST or
     * a finger brings it back, so pulse RST, then forbid its sleep again */
    digitalWrite(PIN_TP_RST, LOW);
    delay(5);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(50);
    wr(REG_DIS_AUTOSLP, 0x01);
}

void touch_let_sleep(bool yes)
{
    if (yes) { wr(REG_DIS_AUTOSLP, 0x00); return; }
    touch_reset();
}

void touch_read(touch_t *t)
{
    uint8_t b[6];
    if (!rd(REG_GESTURE, b, 6)) {
        t->down = false;
        s_alive = false; s_nacks++;
        return;
    }
    s_alive = true;
    t->gesture = b[0];
    t->down = (b[1] & 0x0F) != 0;
    t->x = ((b[2] & 0x0F) << 8) | b[3];
    t->y = ((b[4] & 0x0F) << 8) | b[5];
}

bool     touch_alive(void)   { return s_alive; }
uint32_t touch_nacks(void)   { return s_nacks; }
uint8_t  touch_chip_id(void) { return s_id; }
