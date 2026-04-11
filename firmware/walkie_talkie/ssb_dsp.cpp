#include "ssb_dsp.h"
#include "config.h"
#include <math.h>

namespace ssb {

// ----------------------------------------------------------------------------
// 31-tap Hamming-windowed Hilbert FIR
// ----------------------------------------------------------------------------
// Ideal Hilbert impulse response: h[k] = 2 / (pi * k) for odd k, 0 for even k.
// Shifted by 15 samples to be causal; the resulting odd-indexed taps of the
// shifted form are zero, so the inner loop steps by 2 for a ~2x speedup.
// ----------------------------------------------------------------------------
static constexpr int HILBERT_TAPS  = 31;
static constexpr int HILBERT_DELAY = 15;           // (N-1)/2
static constexpr int RING_SIZE     = 64;           // power of two, > taps
static constexpr int RING_MASK     = RING_SIZE - 1;

static const float kHilbert[HILBERT_TAPS] = {
  -0.003395f,  0.0f, -0.005867f,  0.0f, -0.013438f,  0.0f, -0.028147f,  0.0f,
  -0.053497f,  0.0f, -0.098036f,  0.0f, -0.193577f,  0.0f, -0.630190f,  0.0f,
   0.630190f,  0.0f,  0.193577f,  0.0f,  0.098036f,  0.0f,  0.053497f,  0.0f,
   0.028147f,  0.0f,  0.013438f,  0.0f,  0.005867f,  0.0f,  0.003395f
};

// ----------------------------------------------------------------------------
//  Internal state
// ----------------------------------------------------------------------------
static float    ringBuf[RING_SIZE];
static uint32_t ringPos;
static float    phase;
static float    phaseInc;

// ----------------------------------------------------------------------------
void init() {
  for (int i = 0; i < RING_SIZE; i++) ringBuf[i] = 0.0f;
  ringPos  = 0;
  phase    = 0.0f;
  phaseInc = 2.0f * 3.14159265f * cfg::SSB_SHIFT_HZ / (float)cfg::SAMPLE_RATE;
}

// Convolve the current ring buffer with the Hilbert FIR.
// Skips zero-valued taps (every second one).
static inline float hilbertFilter() {
  float acc = 0.0f;
  for (int i = 0; i < HILBERT_TAPS; i += 2) {
    acc += kHilbert[i] * ringBuf[(ringPos - i) & RING_MASK];
  }
  return acc;
}

// ----------------------------------------------------------------------------
void processBlock(int16_t* samples, int count) {
  for (int i = 0; i < count; i++) {
    float s = samples[i] * (1.0f / 32768.0f);
    ringBuf[ringPos & RING_MASK] = s;

    float I = ringBuf[(ringPos - HILBERT_DELAY) & RING_MASK];   // delayed in-phase
    float Q = hilbertFilter();                                  // 90 deg quadrature

    float c  = cosf(phase);
    float sn = sinf(phase);
    float out = I * c - Q * sn;                                 // USB SSB

    phase += phaseInc;
    if (phase > 6.28318530718f) phase -= 6.28318530718f;

    if (out >  1.0f) out =  1.0f;
    if (out < -1.0f) out = -1.0f;
    samples[i] = (int16_t)(out * 32767.0f);

    ringPos++;
  }
}

} // namespace ssb
