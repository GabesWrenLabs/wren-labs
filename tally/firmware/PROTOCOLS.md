# Wren Tally firmware: switcher protocols and radio link (notes for the build)

v0.1, 2026-10-10 (Wren). From memory, so each line is marked VERIFY until a fetch of the primary
source confirms it. Target: ESP32-C3-MINI-1, Arduino framework under PlatformIO (fallback ESP-IDF).

## Base: switcher side
### Blackmagic ATEM (ATEM Mini and up)
**Verified 2026-10-12** against nrkno/sofie-atem-connection (`src/lib/atemSocketChild.ts`) and
docs.openswitcher.org (`udptransport.html`, `fields/switcher.html`). Implemented in `base/atem_client.h`.
- Transport: UDP, switcher port 9910. 12-byte header, big-endian: bytes 0-1 = 5-bit flags (top) +
  11-bit datagram length; 2-3 session id; 4-5 acked packet id; 6-7 retransmit-from id; 10-11 packet id.
  Flags: 0x01 ACK-request, 0x02 SYN (new session), 0x04 retransmit, 0x08 retransmit-request, 0x10 ACK.
- Handshake: hello = `10 14 53 ab 00 00 00 00 00 3a 00 00 | 01 00 00 00 00 00 00 00` (SYN, len 20).
  Switcher answers SYN with payload[0] 0x02 (ok) or 0x04 (full, retry later). Client ACKs that packet
  id (12-byte packet, flag 0x10, session, acked id). The switcher then assigns its own session id (read
  it from every packet) and streams the full state as ACK-request packets, ending with `InCm`.
- Commands inside a packet: `[u16 len][2 pad][4-char name][len-8 bytes]`.
- `TlIn`: u16 count, then count bytes (bit0 program, bit1 preview); byte i = input i+1. `TlSr`: u16 count,
  then (u16 source id, u8 bits) pairs. `PrgI`/`PrvI`: u8 M/E, u8 pad, u16 source (PrvI + u8 "in program").
- Timing (from atem-connection): 5 s silence = reconnect, retry hello every 1 s, in-flight 60 ms.
- Still unverified on real hardware: whether the switcher needs byte 9 = 0x41 in ACKs (some clients send
  it, atem-connection doesn't) and whether the empty end-of-dump packet needs the 0x61 special ACK
  (openswitcher docs) - atem-connection works without. Test on Gabe's bench with a real ATEM.
- No code copied from any library; the format above is the only thing taken. Tally Arbiter uses
  atem-connection itself and reads `listVisibleInputs('program'|'preview')` per on-air M/E.

### vMix
- Transport: TCP, port 8099, plain text lines ending in CRLF. VERIFY.
- `TALLY\r\n` returns `TALLY OK 0120...` where each digit is an input: 0 off, 1 program, 2 preview.
- `SUBSCRIBE TALLY\r\n` makes vMix push `TALLY OK ...` on every change. Send `TALLY` once to seed.
- vMix must have the TCP API enabled (Settings > Web Controller/ TCP API). Document it on the page.

### OBS Studio
- obs-websocket v5 ships with OBS 28+. WebSocket, default port 4455, optional password.
  **Verified 2026-10-13 against docs/generated/protocol.md:** auth string =
  base64(sha256(base64(sha256(password + salt)) + challenge)); Hello (op 0) carries
  `authentication{challenge, salt}` only when a password is set; auth failure closes with code 4009.
- Identify (op 1) with `rpcVersion: 1` and `eventSubscriptions` = General (1) | Scenes (4) | Ui (1024).
  StudioModeStateChanged lives in Ui, not Scenes.
- Seed: request `GetCurrentProgramScene` and `GetCurrentPreviewScene` (fails when Studio Mode is off) and
  `GetStudioModeEnabled`. Responses carry `sceneName` (the `current...SceneName` fields are deprecated).
- Events: `CurrentProgramSceneChanged`, `CurrentPreviewSceneChanged` (eventData.sceneName),
  `StudioModeStateChanged` (eventData.studioModeEnabled).
- Implemented in base/obs_client.h (ArduinoWebsockets + ArduinoJson, both MIT). Untested against real OBS.
- Mapping scenes to "cameras": user assigns light N to a scene name in the base's web page. If a
  scene contains the camera as a source, OBS does not tell us directly; keep v1 to scene names.

### Auto-detect order
1. If a switcher IP is configured, try ATEM (UDP 9910), then vMix (TCP 8099), then OBS (4455).
2. No IP configured: listen for nothing (no mDNS on ATEM); show "set switcher IP" on the web page.
3. Cue mode: no switcher; GPIO inputs drive lights 1 and 2 directly.

## Base <-> lights: ESP-NOW
- ESP-NOW only works with both ends on the same Wi-Fi channel, and once the base joins the venue
  Wi-Fi its channel is whatever that router uses. **Channel-follow (v0.5, 2026-10-14):** a light
  with no link hops channels 1..13, 400 ms each (longer than the 250 ms broadcast period), locks on
  the first valid packet and saves that channel in flash for the next power-up. Worst case ~5 s to
  find the base; a power-cycled light normally re-links in well under a second. Stand-alone base
  (no venue Wi-Fi) sits on channel 1. Implemented in light/main.cpp; untested on hardware.
- Packet (proto v2): 1-byte version, 1-byte seq, 4 bytes of state (one byte per light: 0 off,
  1 program, 2 preview, 3 both), 1 byte brightness, 1 byte flags, 1 byte channel (informational,
  for the serial log). Broadcast every 250 ms even if unchanged, so a light that missed a packet
  converges in <= 250 ms and can show "lost link" after 2 s.
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
