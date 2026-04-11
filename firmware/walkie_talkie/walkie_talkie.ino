/* ============================================================================
 *  Walkie-Talkie ELP311  -  main sketch entry point
 *
 *  Module layout (all files live in this sketch folder):
 *    walkie_talkie.ino   this file: setup() / loop() glue
 *    config.h            compile-time configuration
 *    ssb_dsp.{h,cpp}     phasing-method SSB Hilbert DSP core
 *    ws_audio.{h,cpp}    WebSocket /audio relay
 *    web_client.h        embedded HTML client (generated from ../../web/
 *                        by ../../tools/build_web.py)
 *
 *  See ../../README.md for full setup instructions.
 * ========================================================================= */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "ssb_dsp.h"
#include "ws_audio.h"
#include "web_client.h"

static AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] Walkie-Talkie ELP311");

  pinMode(cfg::LED_PIN, OUTPUT);
  digitalWrite(cfg::LED_PIN, LOW);

  // --- DSP ---
  ssb::init();

  // --- WiFi AP ---
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(cfg::AP_SSID, cfg::AP_PASSWORD);
  Serial.printf("[wifi] AP %s  ssid=\"%s\"  ip=%s\n",
                ok ? "up" : "FAILED",
                cfg::AP_SSID,
                WiFi.softAPIP().toString().c_str());

  // --- WebSocket audio relay ---
  wsaudio::begin(server);

  // --- HTTP: serve embedded browser client ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "not found");
  });

  server.begin();
  Serial.println("[http] server started on port 80");
  Serial.println("[info] connect to WiFi and open http://192.168.4.1");
}

void loop() {
  wsaudio::tick();
  delay(5);
}
