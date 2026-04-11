#pragma once
#include <ESPAsyncWebServer.h>

// ============================================================================
//  WebSocket audio relay
//
//  Attaches an AsyncWebSocket at "/audio", receives binary PCM frames from
//  any connected client, runs the SSB DSP on them, and broadcasts the result
//  to every other connected client. Half-duplex walkie-talkie behaviour.
// ============================================================================
namespace wsaudio {

// Attach the /audio WebSocket handler to `server`.
// Call from setup() before server.begin().
void begin(AsyncWebServer& server);

// Periodically prune dropped clients. Call from loop().
void tick();

} // namespace wsaudio
