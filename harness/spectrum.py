# Averaged power spectrum of spectrum.cpp's output; prints where it last exceeds -40/-60/-80 dB re its peak, in cycles per unit.
import sys, numpy as np
x = np.fromfile(sys.argv[1], np.float32).reshape(64, -1).astype(np.float64)
x -= x.mean(axis=1, keepdims=True)
w = np.blackman(x.shape[1])
p = (np.abs(np.fft.rfft(x * w, axis=1)) ** 2).mean(axis=0)
f = np.fft.rfftfreq(x.shape[1], 1 / 64)
db = 10 * np.log10(p / p.max() + 1e-30)
out = []
for t in (-40, -60, -80):
    above = np.nonzero(db > t)[0]
    out.append(f"{t} dB: {f[above[-1]]:.1f}")
print(sys.argv[2] if len(sys.argv) > 2 else "", "  ".join(out))
