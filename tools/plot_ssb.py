import numpy as np
import matplotlib.pyplot as plt

fs = 8000
t = np.arange(0, 1.0, 1/fs)

# Single clean tone at 1000 Hz for unambiguous SSB demo
f1 = 1000
x = np.sin(2 * np.pi * f1 * t)

HILBERT_TAPS = 31
HILBERT_DELAY = 15
RING_SIZE = 64
RING_MASK = RING_SIZE - 1

kHilbert = [
  -0.003395,  0.0, -0.005867,  0.0, -0.013438,  0.0, -0.028147,  0.0,
  -0.053497,  0.0, -0.098036,  0.0, -0.193577,  0.0, -0.630190,  0.0,
   0.630190,  0.0,  0.193577,  0.0,  0.098036,  0.0,  0.053497,  0.0,
   0.028147,  0.0,  0.013438,  0.0,  0.005867,  0.0,  0.003395
]

ringBuf = np.zeros(RING_SIZE)
ringPos = 0
phase = 0.0
shift_hz = 1500.0  # Large shift so input (1000 Hz) → output (2500 Hz), clearly separated
phaseInc = 2.0 * np.pi * shift_hz / fs

y = np.zeros_like(x)

for i in range(len(x)):
    s = x[i]
    ringBuf[ringPos & RING_MASK] = s
    I = ringBuf[(ringPos - HILBERT_DELAY) & RING_MASK]
    Q = 0.0
    for j in range(0, HILBERT_TAPS, 2):
        Q += kHilbert[j] * ringBuf[(ringPos - j) & RING_MASK]
    c = np.cos(phase)
    sn = np.sin(phase)
    out = I * c - Q * sn
    phase += phaseInc
    if phase > 2 * np.pi:
        phase -= 2 * np.pi
    y[i] = out
    ringPos += 1

N = 4096
window = np.hamming(N)

X = np.fft.fft(x[:N] * window)
X_mag = 20 * np.log10(np.abs(X[:N//2]) + 1e-10)
freqs = np.fft.fftfreq(N, 1/fs)[:N//2]

Y = np.fft.fft(y[:N] * window)
Y_mag = 20 * np.log10(np.abs(Y[:N//2]) + 1e-10)

f1_shifted = f1 + shift_hz  # 2500 Hz

# ── Plotting ──────────────────────────────────────────────────────────────────
plt.style.use('dark_background')
fig, axes = plt.subplots(2, 1, figsize=(12, 7))
fig.patch.set_facecolor('#0d1117')
for ax in axes:
    ax.set_facecolor('#161b22')

XLIM = (0, 4000)
YLIM = (-70, 60)

# ── Top: Input ────────────────────────────────────────────────────────────────
ax1 = axes[0]
ax1.plot(freqs, X_mag, color='#58a6ff', linewidth=1.8)
ax1.fill_between(freqs, YLIM[0], X_mag, where=(X_mag > YLIM[0]), color='#58a6ff', alpha=0.18)
ax1.axvline(f1, color='#58a6ff', linestyle='--', alpha=0.8, linewidth=1.2)
ax1.text(f1 + 40, 45, f'Input tone\n{f1} Hz', color='#58a6ff', fontsize=9, va='top')
ax1.set_title('INPUT — Single Baseband Tone @ 1000 Hz', fontsize=12, fontweight='bold', color='#e6edf3', pad=10)
ax1.set_ylabel('Magnitude (dB)', color='#8b949e')
ax1.set_xlim(*XLIM)
ax1.set_ylim(*YLIM)
ax1.tick_params(colors='#8b949e')
ax1.spines[:].set_color('#30363d')
ax1.grid(True, alpha=0.15, color='#8b949e')

# ── Bottom: Output ────────────────────────────────────────────────────────────
ax2 = axes[1]
ax2.plot(freqs, Y_mag, color='#f78166', linewidth=1.8)
ax2.fill_between(freqs, YLIM[0], Y_mag, where=(Y_mag > YLIM[0]), color='#f78166', alpha=0.18)

# Shifted peak marker
ax2.axvline(f1_shifted, color='#f78166', linestyle='--', alpha=0.8, linewidth=1.2)
ax2.text(f1_shifted + 40, 45, f'Shifted tone\n{int(f1_shifted)} Hz', color='#f78166', fontsize=9, va='top')

# Ghost reference of where input WAS
ax2.axvline(f1, color='#58a6ff', linestyle=':', alpha=0.5, linewidth=1.2)
ax2.text(f1 + 40, 10, f'Was here\n({f1} Hz)', color='#58a6ff', fontsize=8, va='top', alpha=0.7)

# Arrow showing the shift
ax2.annotate(
    '', xy=(f1_shifted, -10), xytext=(f1, -10),
    arrowprops=dict(arrowstyle='->', color='#f0e68c', lw=1.8)
)
ax2.text((f1 + f1_shifted) / 2, -6, f'+{int(shift_hz)} Hz USB shift', color='#f0e68c',
         fontsize=9, ha='center', va='bottom')

ax2.set_title(f'OUTPUT — SSB Upper Sideband, Shifted +{int(shift_hz)} Hz → Now at {int(f1_shifted)} Hz',
              fontsize=12, fontweight='bold', color='#e6edf3', pad=10)
ax2.set_xlabel('Frequency (Hz)', color='#8b949e')
ax2.set_ylabel('Magnitude (dB)', color='#8b949e')
ax2.set_xlim(*XLIM)
ax2.set_ylim(*YLIM)
ax2.tick_params(colors='#8b949e')
ax2.spines[:].set_color('#30363d')
ax2.grid(True, alpha=0.15, color='#8b949e')

plt.tight_layout(pad=2.0)
plt.savefig('ssb_spectrum.png', dpi=200, bbox_inches='tight', facecolor=fig.get_facecolor())
print("Plot saved to ssb_spectrum.png")
