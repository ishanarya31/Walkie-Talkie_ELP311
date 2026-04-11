#include "ws_audio.h"
#include "ssb_dsp.h"
#include "config.h"
#include <Arduino.h>

namespace wsaudio {

// ----------------------------------------------------------------------------
//  Module-private WebSocket endpoint
// ----------------------------------------------------------------------------
static AsyncWebSocket ws("/audio");

// Broadcast to all connected clients except the sender.
static void broadcastExcept(uint32_t senderId, uint8_t* data, size_t len) {
  for (AsyncWebSocketClient* c : ws.getClients()) {
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

      digitalWrite(cfg::LED_PIN, HIGH);
      ssb::processBlock((int16_t*)data, len / 2);
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
