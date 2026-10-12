# Wren Tally firmware (dev-board stage)

Two PlatformIO targets in one folder:

| Target | Runs on | Does |
|---|---|---|
| `light` | ESP32-C3 dev board + 6x WS2812B | Receives ESP-NOW broadcasts, shows its slot (red program / green preview), amber blink on lost link, brightness button |
| `base` | ESP32-C3 dev board | Broadcasts the 4-light state every 250 ms; serial test commands; vMix TCP tally client; cue-mode GPIO inputs |

## Build and flash
```
pio run -e light
pio run -e base
pio run -e light -t upload --upload-port COM5
pio device monitor -b 115200
```
First build downloads the ESP32 toolchain (~1 GB, several minutes).

## Bench test with zero switchers (Gabe, ~15 min)
1. Flash `base` to board A, `light` to board B (LED strip data on GPIO4, 5 V, GND).
2. Open the serial monitor on board A. Type `p1` + Enter: board B's LEDs go red. `v1`: green. `o`: off.
3. Unplug board A: within 2 s board B shows one amber LED blinking (lost link).
4. Walk away with board B until it loses link. Note the distance: that's the range question answered.

## vMix test (this PC runs the vMix trial)
Set `WIFI_SSID`, `WIFI_PASS`, `VMIX_HOST` at the top of `base/main.cpp`. In vMix: Settings > Web
Controller > enable the TCP API (port 8099). The base subscribes to TALLY and lights follow inputs 1-4.

## Status
- Written 2026-10-12 by Wren. Both targets compile (PlatformIO, espressif32, esp32-c3-devkitm-1).
  Not yet flashed to real boards.
- Not yet: ATEM (UDP 9910), OBS (obs-websocket), config web page, MAC pairing, Ethernet. PROTOCOLS.md
  has the notes for each.
