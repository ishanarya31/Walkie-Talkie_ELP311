#include "ws_audio.h"
#include "ssb_dsp.h"
#include "config.h"
#include <Arduino.h>
#include <driver/i2s.h>

namespace wsaudio {

// ----------------------------------------------------------------------------
//  Module-private WebSocket endpoint
// ----------------------------------------------------------------------------
static AsyncWebSocket ws("/audio");

// Helper to handle ESPAsyncWebServer API differences (pointers vs references)
template <typename T>
static AsyncWebSocketClient* getWsClientPtr(T* ptr) { return ptr; }
template <typename T>
static AsyncWebSocketClient* getWsClientPtr(T& ref) { return &ref; }

// Broadcast to all connected clients except the sender.
static void broadcastExcept(uint32_t senderId, uint8_t* data, size_t len) {
  for (auto&& item : ws.getClients()) {
    AsyncWebSocketClient* c = getWsClientPtr(item);
    if (c && c->id() != senderId
          && c->status() == WS_CONNECTED
          && c->canSend()) {
      c->binary(data, len);
    }
  }
}

// ----------------------------------------------------------------------------
//  WebSocket event handler
// ----------------------------------------------------------------------------
static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[ws] client %u connected from %s\n",
                    client->id(),
                    client->remoteIP().toString().c_str());
      client->text("{\"type\":\"hello\",\"sampleRate\":8000}");
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[ws] client %u disconnected\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->opcode != WS_BINARY) break;
      // Only handle unfragmented frames; client chunks are ~1 KB.
      if (!(info->final && info->index == 0 && info->len == len)) break;
      if ((len & 1) != 0) break;   // must be 16-bit aligned

      int count = len / 2;
      int16_t* pcm = (int16_t*)data;

      // 1. Save original signal
      // We use static buffers to avoid heap fragmentation entirely
      static int16_t original[1024]; 
      if (count > 1024) count = 1024; // Defensive guard
      memcpy(original, pcm, count * sizeof(int16_t));

      digitalWrite(cfg::LED_PIN, HIGH);
      
      // 2. Process DSP (pcm is modified IN-PLACE and becomes the Modulated Signal)
      ssb::processBlock(pcm, count);

      // 3. Stereo interlacing for diagnostic DAC output
      // Internal DAC mode expects 16-bit standard stereo
      static int16_t stereo[2048];
      for (int i = 0; i < count; i++) {
        stereo[i * 2]     = original[i];  // Right channel (DAC1 - Pin 25)
        stereo[i * 2 + 1] = pcm[i];       // Left channel (DAC2 - Pin 26)
      }

      // Write DMA block to I2S peripheral
      size_t written = 0;
      i2s_write(I2S_NUM_0, stereo, count * 2 * sizeof(int16_t), &written, portMAX_DELAY);

      // 4. Relay to other phones
      broadcastExcept(client->id(), data, len);
      digitalWrite(cfg::LED_PIN, LOW);
      break;
    }

    default:
      break;
  }
}

// ----------------------------------------------------------------------------
void begin(AsyncWebServer& server) {
  // Initialize internal 8-bit DAC via I2S
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = cfg::SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
#else
      .communication_format = I2S_COMM_FORMAT_I2S_MSB, // Fallback for older ESP-IDF
#endif
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = true
  };
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN); // Enable DAC on GPIO 25 & 26
  i2s_set_pin(I2S_NUM_0, NULL);

  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void tick() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 1000) {
    ws.cleanupClients();
    last = now;
  }
}

} // namespace wsaudio
