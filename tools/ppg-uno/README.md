# The PPG bench on the Arduino Uno (2026-09-23: the module PROVEN ALIVE)

The MH-ET LIVE MAX30102 on an Uno, streaming red + IR at 100 Hz over
serial. Wiring: VIN -> 5V, GND -> GND, SDA -> A4, SCL -> A5 (INT, IRD,
RD unwired). The module's back-side 1V8/3V3 solder jumper MUST be
bridged middle -> 3V3 (it ships OPEN: the chip never ACKs until then --
the whole fault of the 09-17 night). Never bridge middle -> 1V8.

    cd firmware/watch/tools/ppg-uno
    PATH=~/.local/share/pio-venv/bin:$PATH pio run -e uno -t upload    # /dev/ttyACM1
    ~/.local/share/pio-venv/bin/python -c "import serial;s=serial.Serial('/dev/ttyACM1',115200);[print(s.readline().decode().strip()) for _ in range(400)]"

Prints `PART_ID 15 REV 3`, then `red ir` per sample. The settings that
showed a clean pulse (2026-09-23 23:40, ~58 bpm, autocorrelation 0.91,
the AC 1.6 % of a 53 k IR level): LEDs 0x3F (~12.6 mA) each, SPO2
config 0x67 (ADC range 16384 nA, 100 Hz, 411 us), no sample averaging.
0x7F (25 mA) SATURATES under a finger (262143 every sample); 0x24
(7 mA) is too dim to see the beat over the drift. A finger PRESSED
flattens the pulse: rest it lightly. These are the numbers ppg.cpp's
spot read should start from.

## Do and don't (2026-09-23, from the datasheet, Maxim's AN6409 as quoted
## by builders who used it, SparkFun's hookup guide, and the WristO2 paper)

THE ONE RULE (AN6409): the steady level (the DC) must sit between 0.25
and 0.75 of the 18-bit full scale -- 65,536 to 196,607 counts -- and
the LED current is what you turn to put it there. Under that the beat
is lost in the noise; over it the ADC clips (262,143 flat, as 25 mA
did under a finger here). The bench sketch now does this by itself
(`agc:` lines): it starts at 0x1F (~6 mA, the datasheet's default),
reads the IR DC over 30 samples, halves the current when over the band
and steps it up in proportion when under, and prints where it settled.
The same loop belongs in ppg.cpp's spot read (a wrist and a finger
need very different currents; skin tone moves it too).

DO
- Rest the finger LIGHTLY with even pressure over the WHOLE window;
  pressing hard squeezes the capillaries flat and the pulse ripple
  drops to nothing (seen here: 0.7 % pressed vs 1.6 % rested).
- Keep still for the read; the beat is ~1 % of the level, a twitch is
  10 %. On the watch: only read when the IMU says still (the spot read
  already waits for rest), and drop the read if it moved.
- Warm skin. A cold hand reads badly (SparkFun's note; perfusion).
- Shield the window from room light: a rubber band over a finger on
  the bench, the strap on the wrist. The chip cancels ambient light
  but a bright lamp still eats the range.
- Use the smallest ADC range that keeps the DC in the band (finer
  counts per nA); with the AGC picking the current, 16384 nA is the
  safe start and 4096 nA the finer choice once the level is known.
- Use IR for heart rate on this chip (red is dimmer through skin and
  noisier); the day a MAX30101 lands, GREEN is what every watch uses
  for heart rate (WristO2: "major consumer devices have switched to a
  single green LED ... better accuracy for heart rate").
- Sample at 100 Hz averaged 4 (25 samples/s) for the beat; 400 samples
  (4 s) is a window every algorithm in the literature is happy with.
- Power the LEDs from a rail that can give 50 mA pulses at 3.1 V+
  (the module's VIN 3.3-5 V); the 1.8 V core comes from the module's
  own regulator -- and the back-side jumper MUST bridge the middle pad
  to 3V3, or the chip never answers (our whole 09-17 fault).

DON'T
- Don't expect fingertip numbers on the wrist: lower perfusion, a
  flatter wave, more motion. The WristO2 study found fingertip
  algorithms "a poor match" there, over 90 % of SpO2 readings unusable
  without pruning; heart rate survives (a wrist-vs-finger study got
  85 % agreement after detrending). So: heart rate only, never SpO2,
  and only from reads the motion gate passed -- which is exactly what
  the watch-body brief says (the pulse as the breath, never a number).
- Don't run a fixed LED current: what is right for a finger clips on
  a pale wrist and reads dark on a darker one.
- Don't wear it loose: a moving gap = ambient light + motion; the
  strap should be snug enough that the window does not slide.
- Don't put the window over the wrist bone; the soft top of the wrist
  a finger-width up from the bone (where every watch sits) is the
  spot.
- Don't read through a thick or tinted cover: the PPG needs its own
  clear window with a light barrier between the LEDs and the
  photodiode (the potting rule already in the hardware notes).
- Don't leave the LEDs on between reads (SHDN / amplitude 0 -- the
  spot read shuts them after; tens of mA otherwise).

Sources: Maxim AN6409 "Recommended Configurations and Operating
Profiles for MAX30101/MAX30102 EV Kits" (the 0.25-0.75 rule, as quoted
in har-in-air's ESP8266 pulse meter), the MAX30102 datasheet (ranges,
rates, amplitudes), SparkFun's Pulse Oximeter hookup guide (pressure,
warmth, rubber band), WristO2 (arXiv 1906.07545: wrist placement, skin
tone, green LEDs, motion pruning), "Performance Comparison for Heart
Rate Signal Detection ... Fingertip and Wrist Using Sensor MAX30102"
(JBBBE 59:131, the 85 %).
