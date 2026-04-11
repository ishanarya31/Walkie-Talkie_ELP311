/* ============================================================================
 *  Walkie-Talkie ELP311  -  browser client
 *
 *  Connects to the ESP32 WebSocket at /audio, captures microphone audio at
 *  the browser's native sample rate, linearly resamples to 8 kHz, sends it
 *  as 16-bit signed PCM, and schedules incoming audio for gap-free playback.
 * ========================================================================= */
(() => {
  const TARGET_RATE = 8000;
  const BUF_SIZE    = 1024;   // ScriptProcessor buffer

  // -- WS URL auto-detection ---------------------------------------------
  // Served by the ESP32 -> use current host.
  // Opened standalone (file:// or dev server) -> hardcode the AP address.
  const wsUrl = (() => {
    if (location.protocol === "file:" || !location.host) {
      return "ws://192.168.4.1/audio";
    }
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    return `${proto}//${location.host}/audio`;
  })();

  // -- DOM helpers -------------------------------------------------------
  const $ = (id) => document.getElementById(id);
  const logEl = $("log");
  const log = (...args) => {
    const line = args.map(a => typeof a === "object" ? JSON.stringify(a) : String(a)).join(" ");
    logEl.textContent += line + "\n";
    logEl.scrollTop = logEl.scrollHeight;
  };
  const setStatus = (cls, txt) => {
    $("stDot").className = "dot " + cls;
    $("stTxt").textContent = txt;
  };

  // -- State -------------------------------------------------------------
  let ws           = null;
  let audioCtx     = null;
  let micStream    = null;
  let micSource    = null;
  let processor    = null;
  let transmitting = false;
  let nextPlayTime = 0;
  let rxTimeout    = 0;

  // -- WebSocket ---------------------------------------------------------
  function connectWS() {
    log("connecting", wsUrl);
    ws = new WebSocket(wsUrl);
    ws.binaryType = "arraybuffer";

    ws.onopen = () => {
      $("wsDot").className = "dot ok";
      $("wsTxt").textContent = "connected";
      log("ws open");
    };
    ws.onclose = () => {
      $("wsDot").className = "dot";
      $("wsTxt").textContent = "disconnected";
      log("ws closed, retrying in 2s");
      setTimeout(connectWS, 2000);
    };
    ws.onerror = () => log("ws error");
    ws.onmessage = (e) => {
      if (typeof e.data === "string") {
        try {
          const msg = JSON.parse(e.data);
          if (msg.type === "hello") log("hello from esp32, sr=" + msg.sampleRate);
        } catch {}
        return;
      }
      if (transmitting) return;   // half-duplex: ignore while talking
      playChunk(e.data);
    };
  }

  // -- Playback (linear-interp upsample to native AudioContext rate) -----
  function playChunk(arrayBuf) {
    if (!audioCtx) return;
    const int16  = new Int16Array(arrayBuf);
    const outRate = audioCtx.sampleRate;
    const ratio   = outRate / TARGET_RATE;
    const outLen  = Math.floor(int16.length * ratio);
    const f32     = new Float32Array(outLen);
    for (let i = 0; i < outLen; i++) {
      const srcIdx = i / ratio;
      const i0 = Math.floor(srcIdx);
      const i1 = Math.min(i0 + 1, int16.length - 1);
      const frac = srcIdx - i0;
      f32[i] = (int16[i0] * (1 - frac) + int16[i1] * frac) / 32768;
    }

    const buf = audioCtx.createBuffer(1, outLen, outRate);
    buf.copyToChannel(f32, 0);
    const src = audioCtx.createBufferSource();
    src.buffer = buf;
    src.connect(audioCtx.destination);

    const now = audioCtx.currentTime;
    if (nextPlayTime < now + 0.02) nextPlayTime = now + 0.02;
    src.start(nextPlayTime);
    nextPlayTime += buf.duration;

    setStatus("rx", "receiving");
    clearTimeout(rxTimeout);
    rxTimeout = setTimeout(() => setStatus("", "idle"), 200);
  }

  // -- Capture (linear-interp downsample to 8 kHz) -----------------------
  async function startAudio() {
    try {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      if (audioCtx.state === "suspended") await audioCtx.resume();
      $("srTxt").textContent = audioCtx.sampleRate + " Hz";

      micStream = await navigator.mediaDevices.getUserMedia({
        audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true },
        video: false
      });
      micSource = audioCtx.createMediaStreamSource(micStream);
      processor = audioCtx.createScriptProcessor(BUF_SIZE, 1, 1);

      processor.onaudioprocess = (ev) => {
        if (!transmitting || !ws || ws.readyState !== WebSocket.OPEN) return;
        const input = ev.inputBuffer.getChannelData(0);
        const inRate = audioCtx.sampleRate;
        const ratio  = inRate / TARGET_RATE;
        const outLen = Math.floor(input.length / ratio);
        const int16  = new Int16Array(outLen);
        let peak = 0;
        for (let i = 0; i < outLen; i++) {
          const srcIdx = i * ratio;
          const i0 = Math.floor(srcIdx);
          const i1 = Math.min(i0 + 1, input.length - 1);
          const frac = srcIdx - i0;
          const s = input[i0] * (1 - frac) + input[i1] * frac;
          const v = Math.max(-1, Math.min(1, s));
          if (Math.abs(v) > peak) peak = Math.abs(v);
          int16[i] = (v * 32767) | 0;
        }
        $("meter").style.width = Math.round(peak * 100) + "%";
        try { ws.send(int16.buffer); } catch {}
      };

      micSource.connect(processor);
      // Muted gain sink so the graph actually runs without self-feedback.
      const sink = audioCtx.createGain();
      sink.gain.value = 0;
      processor.connect(sink);
      sink.connect(audioCtx.destination);

      $("ptt").disabled = false;
      $("startBtn").disabled = true;
      $("startBtn").textContent = "Audio ready";
      log("audio ready, sr=" + audioCtx.sampleRate);
    } catch (err) {
      log("mic error:", err.message || err);
      alert("Microphone access failed: " + (err.message || err));
    }
  }

  // -- PTT ---------------------------------------------------------------
  function ptt(on) {
    transmitting = on;
    $("ptt").classList.toggle("active", on);
    setStatus(on ? "tx" : "", on ? "transmitting" : "idle");
    if (!on) $("meter").style.width = "0%";
  }

  // -- UI wiring ---------------------------------------------------------
  $("startBtn").addEventListener("click", startAudio);

  const pttBtn = $("ptt");
  const down = (e) => { e.preventDefault(); ptt(true); };
  const up   = (e) => { e.preventDefault(); ptt(false); };
  pttBtn.addEventListener("touchstart",  down, { passive: false });
  pttBtn.addEventListener("touchend",    up,   { passive: false });
  pttBtn.addEventListener("touchcancel", up,   { passive: false });
  pttBtn.addEventListener("mousedown",   down);
  pttBtn.addEventListener("mouseup",     up);
  pttBtn.addEventListener("mouseleave",  up);
  // Spacebar PTT for desktop testing
  window.addEventListener("keydown", (e) => { if (e.code === "Space" && !e.repeat) ptt(true); });
  window.addEventListener("keyup",   (e) => { if (e.code === "Space") ptt(false); });

  connectWS();
})();
