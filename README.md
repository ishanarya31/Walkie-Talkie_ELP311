# Walkie-Talkie ELP311

An ESP32-based two-phone walkie-talkie that uses **SSB (single-sideband) DSP** as its audio processing core. Both phones connect to the ESP32 over its own WiFi network, stream 8 kHz PCM audio over WebSockets, and the ESP32 acts as a DSP relay running a phasing-method Hilbert transform.

```
 Phone A mic ──┐                                   ┌── Phone B speaker
               │                                   │
               ▼     ws://192.168.4.1/audio        ▲
             ┌──────────────────────────────────────┐
             │             ESP32 (AP)               │
             │   WebSocket  →  SSB DSP  →  WebSocket│
             └──────────────────────────────────────┘
               ▲                                   │
 Phone B mic ──┘                                   └── Phone A speaker
```

No RF hardware, no licence, no router — the ESP32 hosts its own WiFi and the phones are the audio endpoints. The SSB modulation happens in software on the ESP32 so you still get the DSP learning experience end-to-end.

---

## Project layout

```
Walkie-Talkie_ELP311/
├── README.md                       ← you are here
│
├── firmware/
│   └── walkie_talkie/               ← Arduino sketch folder
│       ├── walkie_talkie.ino        ← entry point, setup() / loop()
│       ├── config.h                 ← SSID, password, sample rate, DSP params
│       ├── ssb_dsp.h                ← SSB DSP interface
│       ├── ssb_dsp.cpp              ← Hilbert FIR + USB modulator
│       ├── ws_audio.h               ← WebSocket relay interface
│       ├── ws_audio.cpp             ← /audio WebSocket handler + broadcast
│       └── web_client.h             ← embedded HTML (generated, see below)
│
├── web/                             ← editable browser UI sources
│   ├── index.html                   ← page structure
│   ├── style.css                    ← styling
│   └── app.js                       ← WebSocket, mic capture, playback, PTT
│
└── tools/
    └── build_web.py                 ← inlines web/ into web_client.h
```

### Module responsibilities

| Module               | Responsibility                                                       |
|----------------------|----------------------------------------------------------------------|
| `config.h`           | All tunable constants (SSID, password, sample rate, SSB shift, LED). |
| `ssb_dsp.{h,cpp}`    | 31-tap Hamming-windowed Hilbert FIR and phasing-method USB modulator.|
| `ws_audio.{h,cpp}`   | `/audio` WebSocket endpoint; receives PCM, runs DSP, broadcasts.     |
| `walkie_talkie.ino`  | Glue: WiFi AP, HTTP server, `setup()` / `loop()`.                    |
| `web_client.h`       | PROGMEM blob of the UI, served at `http://192.168.4.1/`. Generated.  |
| `web/*`              | Editable source of the browser UI.                                   |
| `tools/build_web.py` | Inlines `web/style.css` and `web/app.js` into `web_client.h`.        |

---

## Hardware

- Any ESP32 dev board (ESP32, ESP32-S2, ESP32-S3, or ESP32-C3)
- Two phones (Android Chrome works best) — or a laptop as a second "phone"
- USB cable for flashing

---

## Setup

### 1. Install the Arduino IDE and the ESP32 core

1. Download the Arduino IDE: <https://www.arduino.cc/en/software>
2. **File → Preferences → Additional Board Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager**, search **esp32** (by Espressif), install.
4. **Tools → Board → ESP32 Arduino**, pick your board (e.g. *ESP32 Dev Module*).

### 2. Install the required libraries

**Sketch → Include Library → Manage Libraries**, then install both:

- **ESP Async WebServer** (by *lacamera* or *me-no-dev* — either works)
- **AsyncTCP** (the ESP32 variant, by *dvarrel* or *me-no-dev*)

> Only install **one** version of AsyncTCP. Multiple copies cause link errors.

### 3. Flash the firmware

1. Open [firmware/walkie_talkie/walkie_talkie.ino](firmware/walkie_talkie/walkie_talkie.ino) in the Arduino IDE. The IDE will automatically include all the other `.h` / `.cpp` files in the same folder.
2. Plug in the ESP32 over USB.
3. **Tools → Port**, select the ESP32's COM port.
4. Click **Upload**.
5. Open **Tools → Serial Monitor** at **115200** baud. You should see:
   ```
   [boot] Walkie-Talkie ELP311
   [wifi] AP up  ssid="Walkie-ELP311"  ip=192.168.4.1
   [http] server started on port 80
   [info] connect to WiFi and open http://192.168.4.1
   ```

### 4. Talk

On **each phone** (or a laptop as a second endpoint):

1. Join WiFi network **`Walkie-ELP311`** (password **`walkie123`**).
2. Open <http://192.168.4.1> in **Chrome** (Chrome on Android, or Chrome/Edge on desktop).
3. Tap **Start** to grant microphone permission.
4. Hold **PUSH TO TALK** (or press **Space** on desktop) and speak.
5. The other device plays back the SSB-processed audio in real time.

The on-board LED (GPIO 2) blinks whenever audio is being relayed.

---

## Editing the UI

Edit the three files in [web/](web/) freely, then regenerate the PROGMEM blob and reflash:

```bash
python tools/build_web.py
```

This reads [web/index.html](web/index.html), inlines [web/style.css](web/style.css) and [web/app.js](web/app.js), and writes [firmware/walkie_talkie/web_client.h](firmware/walkie_talkie/web_client.h). Then re-upload the sketch from the Arduino IDE.

You can also test the UI standalone without reflashing at all — the JS auto-detects whether it is being served from the ESP32 or opened locally and picks the WebSocket URL accordingly:

```bash
cd web
python -m http.server 8080
# Open http://localhost:8080 in a browser on the phone/laptop that is
# joined to the Walkie-ELP311 AP. app.js will connect to ws://192.168.4.1/audio.
```

---

## Tuning the SSB demo

Open [firmware/walkie_talkie/config.h](firmware/walkie_talkie/config.h) and change:

```cpp
constexpr float SSB_SHIFT_HZ = 0.0f;   // try 200.0f or 500.0f
```

- **0 Hz** — clean passthrough. The phasing pipeline still runs, so CPU load and group delay are the same, but the audible output matches the input.
- **200–500 Hz** — voices sound distinctly frequency-shifted. This audibly proves the Hilbert-transform path is live and the DSP is actually doing work.

---

## How the SSB core works

[firmware/walkie_talkie/ssb_dsp.cpp](firmware/walkie_talkie/ssb_dsp.cpp) implements the **phasing method** of SSB generation:

1. Maintain a 64-sample ring buffer of recent input.
2. For each incoming sample `x[n]`:
   - `I = x[n - 15]` — input delayed by the filter's group delay (15 taps of 31).
   - `Q = Σ h[k] · x[n - k]` — Hilbert-transformed (90° phase-shifted) input.
3. Synthesize USB:

   ```
   y[n] = I · cos(2π f n / fs)  −  Q · sin(2π f n / fs)
   ```

Setting `f = 0` gives a group-delayed passthrough. Setting `f > 0` translates the spectrum upward by `f` Hz, which is what a real SSB transmitter does against its carrier frequency.

The Hilbert FIR is a 31-tap **Hamming-windowed** approximation to the ideal Hilbert impulse response `h[k] = 2 / (π k)` for odd `k`. Every second tap is zero, so the inner loop skips them for a roughly 2× speedup.

---

## Protocol on the wire

| Direction              | Payload                                               |
|------------------------|-------------------------------------------------------|
| Client → ESP32 (bin)   | 16-bit signed little-endian mono PCM, 8 kHz          |
| ESP32 → Client (bin)   | Same — SSB-processed, relayed from other clients     |
| ESP32 → Client (text)  | `{"type":"hello","sampleRate":8000}` on connect      |

Typical chunk size: 512 samples = 1024 bytes (fits one WebSocket frame over WiFi).

---

## Troubleshooting

| Symptom                                    | Fix                                                                  |
|--------------------------------------------|----------------------------------------------------------------------|
| `WiFi.h: No such file or directory`        | Wrong board selected. Pick an ESP32 board in *Tools → Board*.        |
| Link errors about `AsyncTCP`               | Multiple `AsyncTCP` versions installed. Keep exactly one.            |
| Page won't load at 192.168.4.1             | Disable mobile data — Android sometimes routes HTTP over cellular.   |
| Microphone permission denied on iPhone     | Safari requires HTTPS on LAN. Use Chrome/Edge on Android or desktop. |
| No audio on the receiving phone            | Make sure only **one** phone is holding PTT at a time.               |
| Audio crackles or cuts out                 | Stay within a few meters of the ESP32; keep other WiFi traffic off.  |
| `ScriptProcessorNode deprecated` warning   | Expected. Still works everywhere; we use it for simplicity.          |

---

## Suggested build order (for incremental debugging)

1. **Smoke test** — Flash, confirm AP comes up, open the page, see "connected" status. Don't worry about audio.
2. **Loopback** — Temporarily set `SSB_SHIFT_HZ = 0` (already default). Hold PTT on one phone, verify VU meter on sender, listen on the other.
3. **DSP validation** — Set `SSB_SHIFT_HZ = 300.0f`, reflash. You should hear a clear pitch shift, confirming the Hilbert path runs.
4. **Polish** — Tweak buffer sizes in [web/app.js](web/app.js) for your latency/quality trade-off.

---

## License

MIT — do whatever you want with it.
