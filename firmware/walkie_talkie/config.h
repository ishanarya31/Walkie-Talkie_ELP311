#pragma once

// ============================================================================
//  Walkie-Talkie ELP311  -  compile-time configuration
//
//  Every tunable constant lives here. Change values, reflash, done.
// ============================================================================
namespace cfg {

  // --- WiFi Access Point ------------------------------------------------
  constexpr const char* AP_SSID     = "Walkie-ELP311";
  constexpr const char* AP_PASSWORD = "walkie123";   // min 8 chars

  // --- Audio pipeline ---------------------------------------------------
  constexpr int SAMPLE_RATE = 8000;     // 16-bit mono PCM on the wire

  // --- Hardware ---------------------------------------------------------
  constexpr int LED_PIN = 2;            // on-board LED, blinks on audio RX

  // --- SSB demonstration ------------------------------------------------
  // Non-zero values (Hz) audibly shift the spectrum via the phasing method,
  // proving the Hilbert DSP pipeline is active. 0.0f = clean passthrough.
  // Try 200.0f or 500.0f to hear the SSB translator in action.
  constexpr float SSB_SHIFT_HZ = 0.0f;

} // namespace cfg
