# Wren Tally firmware (dev-board stage)

Two PlatformIO targets in one folder:

| Target | Runs on | Does |
|---|---|---|
| `light` | ESP32-C3 dev board + 6x WS2812B | Receives ESP-NOW broadcasts, shows its slot (red program / green preview), amber blink on lost link, brightness button |
| `base` | ESP32-C3 dev board | Broadcasts the 4-light state every 250 ms; serial test commands; vMix TCP tally client; ATEM UDP tally client; OBS (obs-websocket) client; cue-mode GPIO inputs |

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

## ATEM test without an ATEM (any PC on the same Wi-Fi, ~10 min)
Set `WIFI_SSID`, `WIFI_PASS`, `ATEM_HOST` (= the PC's IP) in `base/main.cpp`, flash, then on the PC:
```
python tools/fake_atem.py
p2        (input 2 to program: light 2 goes red)
v3        (input 3 to preview: light 3 goes green)
o         (all off)
```
The base's serial monitor prints `ATEM: connected (Fake ATEM Mini), 4 tally inputs` and each tally change.
This proves the base and the fake agree with each other, not that either matches a real ATEM. The real
test is the same steps with `ATEM_HOST` = a real ATEM's IP (default 192.168.10.240) and its own buttons.

## OBS test (any PC running OBS 28+ on the same Wi-Fi, ~10 min)
In OBS: Tools > WebSocket Server Settings > Enable, note the port (4455) and the password (or turn auth off).
Set `WIFI_SSID`, `WIFI_PASS`, `OBS_HOST` (= the PC's IP), `OBS_PASS` in `base/main.cpp`, and put your scene
names in `SCENE_MAP` (light 1 = first name, and so on). Flash. The serial monitor prints
`OBS: connected. program="..." preview="..." studio=0/1`. Switch scenes: the matching light goes red.
Turn on Studio Mode and pick a preview scene: that light goes green.

## Status
- Written 2026-10-12 by Wren. Both targets compile (PlatformIO, espressif32, esp32-c3-devkitm-1).
  Not yet flashed to real boards.
- ATEM client (`base/atem_client.h`, UDP 9910, `TlIn`) written the same day from the wire format in
  atem-connection and the OpenSwitcher docs; compiles; untested against a real switcher.
- OBS client (`base/obs_client.h`, obs-websocket v5, port 4455, password auth) written 2026-10-13 from the
  official protocol doc; untested against a real OBS.
- Not yet: config web page, MAC pairing, Ethernet. PROTOCOLS.md has the notes.
