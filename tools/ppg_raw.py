#!/usr/bin/env python3
"""Everything a still fingertip tells the MAX30102 -- from `ppg raw <secs>`.

The watch streams `raw <i> <red> <ir>` lines at 100 Hz (tools/monitor.py
logs them with the host's clock); this reads them off the log and prints:

  heart rate + every beat's interval          (the IR channel, band-passed)
  HRV: SDNN, RMSSD, pNN50, the min/max rate   (short-window values: label them so)
  breathing rate                              (the beat-to-beat rhythm and the
                                               baseline both sway with the breath)
  perfusion index                             (AC / DC of the IR, in %)
  a rough SpO2                                (the red/IR ratio-of-ratios; UNCALIBRATED
                                               -- the textbook 110 - 25 R, no clinical claim)
  the pulse shape                             (upstroke time, the dicrotic notch if seen)
  signal quality                              (autocorrelation, the intervals' spread)

    tools/ppg_raw.py [/tmp/watch.log] [--csv out.csv]
"""
import sys
import numpy as np
from scipy import signal

FS = 100.0


def load(path):
    red, ir = [], []
    for line in open(path, errors="replace"):
        parts = line.split()
        # "HH:MM:SS raw i red ir"
        if len(parts) >= 5 and parts[1] == "raw":
            try:
                red.append(int(parts[3])); ir.append(int(parts[4]))
            except ValueError:
                pass
    return np.array(red, float), np.array(ir, float)


def bandpass(x, lo, hi, fs=FS, order=3):
    b, a = signal.butter(order, [lo / (fs / 2), hi / (fs / 2)], btype="band")
    return signal.filtfilt(b, a, x)


def lowpass(x, hi, fs=FS, order=3):
    b, a = signal.butter(order, hi / (fs / 2), btype="low")
    return signal.filtfilt(b, a, x)


def beats(ir):
    """peaks of the band-passed IR = systolic peaks; the MAX30102 reads
    reflected light so more blood = LESS light: invert. The first second
    (the LEDs settling) is blanked."""
    ir = ir.copy(); ir[:int(FS)] = ir[int(FS)]
    y = -bandpass(ir, 0.5, 5.0)
    y = y / (np.std(y) or 1)
    # the beat is the steep systolic upstroke; the dicrotic wave that follows
    # (strong with a light touch) has a peak but no steep slope, so the
    # slope's peaks count beats without counting it twice
    dy = np.gradient(lowpass(y, 8.0))
    dy = dy / (np.std(dy) or 1)
    up, _ = signal.find_peaks(dy, distance=int(0.4 * FS), prominence=1.0)
    # the systolic peak = the first maximum of y after each upstroke
    peaks = []
    for u in up:
        seg = y[u:u + int(0.3 * FS)]
        if len(seg): peaks.append(u + int(np.argmax(seg)))
    peaks = np.array(peaks, int)
    # a beat closer than 0.6 x the typical interval is the dicrotic wave or a
    # jolt counted twice: keep the stronger of the pair
    if len(peaks) > 3:
        med = np.median(np.diff(peaks))
        keep = [peaks[0]]
        for p in peaks[1:]:
            if p - keep[-1] < 0.6 * med:
                if y[p] > y[keep[-1]]: keep[-1] = p
            else:
                keep.append(p)
        peaks = np.array(keep, int)
    return peaks, y


def main():
    path = "/tmp/watch.log"
    csv = None
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--csv": csv = args.pop(0)
        else: path = a
    red, ir = load(path)
    n = len(ir)
    if n < 5 * FS:
        print(f"only {n} raw samples in {path}; run `ppg raw 30` first"); return 1
    t = np.arange(n) / FS
    print(f"{n} samples, {n / FS:.1f} s at {FS:.0f} Hz")
    if csv:
        np.savetxt(csv, np.c_[t, red, ir], delimiter=",", header="t,red,ir", comments="")
        print(f"wrote {csv}")

    # the DC: how much light comes back (skin, pressure, the LED current)
    dc_ir, dc_red = ir.mean(), red.mean()
    print(f"\nlight: ir dc {dc_ir:.0f}  red dc {dc_red:.0f}  (of 262143 full scale: {dc_ir / 262143 * 100:.0f} % / {dc_red / 262143 * 100:.0f} %)")

    # beats
    pk, y = beats(ir)
    if len(pk) < 4:
        print("fewer than 4 beats found -- was the finger still?"); return 1
    rr = np.diff(pk) / FS                       # seconds between beats
    keep = (rr > 0.33) & (rr < 1.5)
    rr = rr[keep]
    hr_inst = 60 / rr
    hr = 60 / rr.mean()
    print(f"\nHEART RATE: {hr:.1f} bpm over {len(pk)} beats ({len(rr)} intervals kept)")
    print(f"  beat to beat: {' '.join(f'{v:.0f}' for v in hr_inst)} bpm")
    print(f"  slowest {hr_inst.min():.0f}  fastest {hr_inst.max():.0f}")

    # HRV (a 30 s window is short: these are indicative, not the 5 min clinical values)
    rr_ms = rr * 1000
    sdnn = rr_ms.std(ddof=1) if len(rr_ms) > 1 else 0
    d = np.diff(rr_ms)
    rmssd = np.sqrt(np.mean(d ** 2)) if len(d) else 0
    pnn50 = 100 * np.mean(np.abs(d) > 50) if len(d) else 0
    print(f"\nHRV (short window): SDNN {sdnn:.0f} ms  RMSSD {rmssd:.0f} ms  pNN50 {pnn50:.0f} %  mean RR {rr_ms.mean():.0f} ms")
    print("  reading it: RMSSD is the vagal (rest-and-digest) tone; 20-50 ms typical seated adult, higher relaxed, lower stressed/tired -- from 30 s take it as a hint")

    # breathing: (1) respiratory sinus arrhythmia -- the RR intervals sway with the breath;
    # (2) the baseline of the raw IR sways with the breath (venous return, chest movement)
    br = None
    if len(pk) >= 8:
        tb = pk[1:][keep] / FS
        fi = np.arange(tb[0], tb[-1], 0.25)
        rri = np.interp(fi, tb, rr)
        rri = rri - rri.mean()
        if len(rri) > 16:
            f, p = signal.periodogram(rri, fs=4.0)
            m = (f >= 0.1) & (f <= 0.5)
            if m.any():
                br = f[m][np.argmax(p[m])] * 60
    base = lowpass(ir - ir.mean(), 0.5)
    f2, p2 = signal.periodogram(base, fs=FS)
    m2 = (f2 >= 0.1) & (f2 <= 0.5)
    br2 = f2[m2][np.argmax(p2[m2])] * 60 if m2.any() else None
    print(f"\nBREATHING: from the heart's rhythm {br:.0f} /min" if br else "\nBREATHING: rhythm too short to tell", end="")
    print(f"; from the signal's baseline {br2:.0f} /min" if br2 else "")
    print("  (6-20 a minute at rest; the two agreeing is the honest answer, one alone is a guess)")

    # perfusion index: pulsatile / steady
    ac_ir = np.percentile(bandpass(ir, 0.5, 5.0), 97.5) - np.percentile(bandpass(ir, 0.5, 5.0), 2.5)
    ac_red = np.percentile(bandpass(red, 0.5, 5.0), 97.5) - np.percentile(bandpass(red, 0.5, 5.0), 2.5)
    pi = ac_ir / dc_ir * 100
    print(f"\nPERFUSION INDEX: {pi:.2f} %  (the pulse's share of the light: 0.02 % weak/cold to 20 % strong; fingertips at rest 1-5 %)")

    # SpO2, the ratio of ratios -- uncalibrated
    R = (ac_red / dc_red) / (ac_ir / dc_ir)
    spo2 = 110 - 25 * R
    print(f"SpO2 (UNCALIBRATED): R {R:.2f} -> about {spo2:.0f} %  (the textbook curve; a real meter is calibrated per device -- take the trend, not the number)")

    # pulse shape: average beat, upstroke time, dicrotic notch
    win = int(0.9 * FS)
    segs = []
    yy = -lowpass(ir - ir.mean(), 8.0)
    for p in pk:
        a = p - int(0.25 * FS)
        if a >= 0 and a + win <= n:
            s = yy[a:a + win]
            segs.append(s - s[0])
    if len(segs) >= 4:
        avg = np.mean(segs, axis=0)
        i_peak = int(np.argmax(avg))
        foot = int(np.argmin(avg[:i_peak])) if i_peak > 0 else 0
        upstroke = (i_peak - foot) / FS
        # the notch: a local minimum after the peak within 0.4 s
        after = avg[i_peak:i_peak + int(0.4 * FS)]
        mins, _ = signal.find_peaks(-after, prominence=(avg.max() - avg.min()) * 0.02)
        notch = (mins[0] / FS) if len(mins) else None
        print(f"\nPULSE SHAPE (average of {len(segs)} beats): upstroke {upstroke * 1000:.0f} ms foot-to-peak", end="")
        print(f", dicrotic notch {notch * 1000:.0f} ms after the peak" if notch else ", no dicrotic notch resolved")
        print("  (a short upstroke ~100-200 ms and a visible notch = elastic arteries and a clear signal)")
        bar = "".join("▁▂▃▄▅▆▇█"[int(7 * (v - avg.min()) / ((avg.max() - avg.min()) or 1))] for v in avg[::3])
        print(f"  {bar}")

    # quality
    x = ir[int(FS):]                            # the first second is the LEDs settling
    x = lowpass(x - x.mean(), 8.0); x = x - lowpass(x, 0.4); x = x / (x.std() or 1)
    ac = np.correlate(x, x, mode="full")[len(x) - 1:] / len(x)
    lag = np.argmax(ac[int(0.4 * FS):int(1.6 * FS)]) + int(0.4 * FS)
    cv = rr.std() / rr.mean()
    print(f"\nQUALITY: autocorrelation {ac[lag]:.2f} at {lag / FS:.2f} s (over 0.8 is a clean pulse), intervals' spread cv {cv:.2f} (under 0.25 is still)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
