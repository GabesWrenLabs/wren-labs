# Wren Tally firmware: switcher protocols and radio link (notes for the build)

v0.1, 2026-10-10 (Wren). From memory, so each line is marked VERIFY until a fetch of the primary
source confirms it. Target: ESP32-C3-MINI-1, Arduino framework under PlatformIO (fallback ESP-IDF).

## Base: switcher side
### Blackmagic ATEM (ATEM Mini and up)
- Transport: UDP, switcher port 9910. VERIFY.
- Handshake: client sends a "hello" packet (flags 0x10, session id, 12-byte header + 8 bytes),
  switcher replies, client ACKs; then the switcher streams its full state. Every packet with the
  ACK-request flag must be ACKed within ~1 s or the switcher drops the session. VERIFY.
- Tally lives in commands `TlIn` (tally by input index: bit0 = program, bit1 = preview) and `TlSr`
  (tally by source id). ATEM Mini inputs 1-4 are indices 1-4. VERIFY against the SKAARHOJ protocol
  wiki and Tally Arbiter's atem source.
- Keep-alive: the switcher sends packets regularly; we ACK. If silence > 3 s, reconnect.
- Known good references to read before coding: SKAARHOJ "Blackmagic ATEM Switcher Protocol" page,
  Tally Arbiter (josephdadams) `sources/atem` in Node, `atem-connection` (SuperFly) in TypeScript,
  and the ESP32 ATEM libraries by Aron Hetlam (ATEMmin for Arduino). License check before reuse.

### vMix
- Transport: TCP, port 8099, plain text lines ending in CRLF. VERIFY.
- `TALLY\r\n` returns `TALLY OK 0120...` where each digit is an input: 0 off, 1 program, 2 preview.
- `SUBSCRIBE TALLY\r\n` makes vMix push `TALLY OK ...` on every change. Send `TALLY` once to seed.
- vMix must have the TCP API enabled (Settings > Web Controller/ TCP API). Document it on the page.

### OBS Studio
- obs-websocket v5 ships with OBS 28+. WebSocket, default port 4455, optional password
  (auth = SHA256 challenge/salt, base64). VERIFY.
- Identify (op 1) with `rpcVersion: 1` and event subscriptions (General | Scenes).
- Seed: request `GetCurrentProgramScene` and `GetCurrentPreviewScene` (studio mode only).
- Events: `CurrentProgramSceneChanged`, `CurrentPreviewSceneChanged`, `StudioModeStateChanged`.
- Mapping scenes to "cameras": user assigns light N to a scene name in the base's web page. If a
  scene contains the camera as a source, OBS does not tell us directly; keep v1 to scene names.

### Auto-detect order
1. If a switcher IP is configured, try ATEM (UDP 9910), then vMix (TCP 8099), then OBS (4455).
2. No IP configured: listen for nothing (no mDNS on ATEM); show "set switcher IP" on the web page.
3. Cue mode: no switcher; GPIO inputs drive lights 1 and 2 directly.

## Base <-> lights: ESP-NOW
- ESP-NOW works on Wi-Fi channel; base must stay on one channel. If the base also joins Wi-Fi
  (fallback mode), lights must follow the AP's channel: send the channel in the pairing record.
  With Ethernet as primary, keep Wi-Fi in STA mode, channel fixed (default 1), no AP join.
- Packet: 1-byte version, 1-byte seq, 4 bytes of state (one byte per light: 0 off, 1 program,
  2 preview, 3 both), 1 byte brightness, 1 byte flags. Broadcast every 250 ms even if unchanged,
  so a light that missed a packet converges in <= 250 ms and can show "lost link" after 2 s.
- Pairing: lights ship with the base's MAC burned in NVS. Pairing button on base + light: 10 s
  window, base broadcasts its MAC, light stores it. No screens.
- Range: ESP-NOW long-range mode (802.11 LR) is ESP32-only, NOT on C3 for peer links without
  both ends on C3... VERIFY: C3 supports LR mode per Espressif docs; test on the prototype.

## Light
- 6x addressable RGB (WS2812B-2020 or 5050) on one GPIO via RMT. Red program, green preview.
- Battery: read VBAT through a divider; show low battery as a slow amber blink on LED 6.
- Power: USB-C 5 V preferred; battery charges from USB when present (TP4056 family).
- Sleep: none in v1 (lights must react within a frame or two). Measure current first.

## Web page on the base
- ESPAsyncWebServer or the ESP-IDF httpd. One page: switcher IP, type (auto/ATEM/vMix/OBS),
  OBS password, light-to-input mapping (4 dropdowns), brightness, "pair" button, status.
- Persist in NVS. mDNS name `wren-tally.local` (ESP32 mDNS works over Wi-Fi; over W5500 verify).

## Build plan (first wake with the prototype on the bench)
1. Light firmware first: ESP-NOW receiver + LEDs. Test with a bare ESP32-C3 dev board as sender.
2. Base: OBS first (easiest to test with no hardware, OBS runs on this PC). Then vMix trial,
   then ATEM when a real unit is on Gabe's bench.
