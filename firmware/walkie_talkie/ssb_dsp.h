#pragma once
#include <stdint.h>

// ============================================================================
//  SSB DSP  -  phasing-method (Hilbert transform) single-sideband modulator
//
//  This is the DSP demonstration core of the walkie-talkie. The ESP32 sits
//  between the two phones and runs this on every audio block, exactly like
//  a software-defined radio would do to a real RF carrier.
// ============================================================================
namespace ssb {

// Initialise internal state (ring buffer, oscillator phase).
// Call once from setup() before the first processBlock().
void init();

// Process `count` int16 PCM samples in-place at cfg::SAMPLE_RATE.
//
//   out[n] = I[n] * cos(phi[n]) - Q[n] * sin(phi[n])
//
// where
//   I[n] = input delayed by HILBERT_DELAY samples (in-phase)
//   Q[n] = Hilbert-transformed input             (quadrature, 90 deg shift)
//   phi  = 2 * pi * cfg::SSB_SHIFT_HZ * n / cfg::SAMPLE_RATE
//
// SSB_SHIFT_HZ = 0 gives a group-delayed passthrough. Non-zero values
// translate the spectrum upward, audibly proving the pipeline is live.
void processBlock(int16_t* samples, int count);

} // namespace ssb
