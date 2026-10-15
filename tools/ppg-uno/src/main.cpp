/* MAX30102 bench with AGC (Maxim AN6409's rule: the DC must sit in 0.25..0.75
 * of the 18-bit range, the LED current picked to put it there). Prints the
 * current it settled on, then streams red ir at 100 Hz. */
#include <Arduino.h>
#include <Wire.h>
#define ADDR 0x57
#define FULL 262143L
#define DC_LO (FULL/4)          /* 65535 */
#define DC_HI (FULL*3/4)        /* 196607 */
static uint8_t pa = 0x1F;       /* start ~6 mA, AN6409's default; 0xFF = 50 mA */
static bool wr(uint8_t r, uint8_t v){ Wire.beginTransmission(ADDR); Wire.write(r); Wire.write(v); return Wire.endTransmission()==0; }
static int rd(uint8_t r){ Wire.beginTransmission(ADDR); Wire.write(r); if(Wire.endTransmission(false)) return -1; if(Wire.requestFrom(ADDR,(uint8_t)1)!=1) return -1; return Wire.read(); }
static bool sample(uint32_t *red, uint32_t *ir){
  int wp=rd(0x04), rp=rd(0x06); if(wp<0||rp<0) return false;
  if(((wp-rp)&0x1F)==0) return false;
  Wire.beginTransmission(ADDR); Wire.write(0x07); Wire.endTransmission(false);
  if(Wire.requestFrom(ADDR,(uint8_t)6)!=6) return false;
  uint32_t r=((uint32_t)Wire.read()<<16)|((uint32_t)Wire.read()<<8)|Wire.read();
  uint32_t i=((uint32_t)Wire.read()<<16)|((uint32_t)Wire.read()<<8)|Wire.read();
  *red=r&0x3FFFF; *ir=i&0x3FFFF; return true;
}
static void leds(uint8_t v){ wr(0x0C,v); wr(0x0D,v); wr(0x04,0); wr(0x05,0); wr(0x06,0); }
static long ir_mean(int n){ long acc=0; int got=0; uint32_t r,i; unsigned long t0=millis();
  while(got<n && millis()-t0<1500){ if(sample(&r,&i)){ acc+=i; got++; } else delay(5); }
  return got? acc/got : -1; }
void setup(){
  Serial.begin(115200); Wire.begin(); Wire.setClock(100000); delay(200);
  Serial.println(F("== MAX30102 AGC bench (Uno, TWI A4/A5) =="));
  Serial.print(F("PART_ID ")); Serial.print(rd(0xFF),HEX); Serial.print(F(" REV ")); Serial.println(rd(0xFE),HEX);
  wr(0x09,0x40); delay(50); wr(0x02,0); wr(0x03,0);
  wr(0x08,0x1F);            /* no averaging, rollover */
  wr(0x09,0x03);            /* red + IR */
  wr(0x0A,0x67);            /* range 16384 nA, 100 Hz, 411 us */
  leds(pa);
  Serial.println(F("AGC: finger on the window, please (2 s)")); delay(2000);
  for(int step=0; step<12; step++){
    long dc=ir_mean(30);
    Serial.print(F("agc: pa 0x")); Serial.print(pa,HEX); Serial.print(F(" ir dc ")); Serial.print(dc);
    Serial.print(F(" = ")); Serial.print(100.0*dc/FULL,0); Serial.println(F(" % of full scale"));
    if(dc<0){ Serial.println(F("agc: no samples")); break; }
    if(dc>=DC_LO && dc<=DC_HI){ Serial.println(F("agc: in the band (25..75 %) -- settled")); break; }
    if(dc>DC_HI){ if(pa<=0x08){ Serial.println(F("agc: too bright at the floor (pressed? ambient?)")); break; } pa=pa/2; }
    else { if(pa>=0xFF){ Serial.println(F("agc: dark at 50 mA (nothing on the window?)")); break; }
           /* proportional step up, capped at x2 */
           long want=(long)pa*(DC_LO+DC_HI)/2/(dc>0?dc:1); if(want>(long)pa*2) want=pa*2; if(want<=pa) want=pa+4; if(want>0xFF) want=0xFF; pa=(uint8_t)want; }
    leds(pa); delay(300);
  }
  Serial.print(F("settled: LED pa 0x")); Serial.print(pa,HEX); Serial.print(F(" (~")); Serial.print(pa*0.2,1); Serial.println(F(" mA)"));
  Serial.println(F("red ir"));
}
void loop(){ uint32_t r,i; if(sample(&r,&i)){ Serial.print(r); Serial.print(' '); Serial.println(i); } else delay(5); }
