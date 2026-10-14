# Wren Tally firmware (dev-board stage)

Two PlatformIO targets in one folder:

| Target | Runs on | Does |
|---|---|---|
| `light` | ESP32-C3 dev board + 6x WS2812B | Receives ESP-NOW broadcasts, shows its slot (red program / green preview), amber blink on lost link, brightness button |
| `base` | ESP32-C3 dev board | Broadcasts the 4-light state every 250 ms; **setup web page** (Wi-Fi, switcher, light mapping, saved in flash); serial test commands; vMix TCP tally client; ATEM UDP tally client; OBS (obs-websocket) client; cue-mode GPIO inputs |

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

## Setup page (no constants to edit, no reflash per venue) - v0.4+
1. Power the base. With nothing saved it makes a Wi-Fi network **WrenTally-XXXX** (password `wrentally`).
2. Join it from a phone or laptop and open **http://192.168.4.1/**
3. Enter the venue Wi-Fi, pick the switcher (vMix / ATEM / OBS), its IP, which input or OBS scene each
   light follows, brightness. **Save and restart.**
4. The base joins the venue Wi-Fi; the page is then at **http://wren-tally.local/** (or the IP on the serial
   monitor). The setup network stays up for 10 minutes after each boot, so a wrong password never locks you out.
5. Serial `f` + Enter (or the Factory reset button on the page) clears everything.
Settings are in ESP32 NVS (`Preferences`, namespace `wren`). The page is served by the core's `WebServer`.

## vMix test (this PC runs the vMix trial)
On the setup page choose vMix and the PC's IP. In vMix: Settings > Web Controller > enable the TCP API
(port 8099). The base subscribes to TALLY and lights follow the inputs you mapped.

## ATEM test without an ATEM (any PC on the same Wi-Fi, ~10 min)
On the setup page choose ATEM and the PC's IP, save, then on the PC:
```
python tools/fake_atem.py
p2        (input 2 to program: light 2 goes red)
v3        (input 3 to preview: light 3 goes green)
o         (all off)
```
The base's serial monitor prints `ATEM: connected (Fake ATEM Mini), 4 tally inputs` and each tally change.
This proves the base and the fake agree with each other, not that either matches a real ATEM. The real
test is the same steps with the IP of a real ATEM (default 192.168.10.240) and its own buttons.

## OBS test without OBS (any PC on the same Wi-Fi, ~10 min)
```
python tools/fake_obs.py --password abc     (or no --password for auth off)
p2        (program "Camera 2": light 2 goes red)
s         (studio mode on)
v3        (preview "Camera 3": light 3 goes green)
o         (program "Blank": all off)
```
On the base's setup page choose OBS, the PC's IP, password `abc`, scene names "Camera 1".."Camera 4".
`python tools/test_fake_obs.py abc` checks the fake itself (handshake, password hash, seed requests; it
passed 2026-10-13). Same caveat as the ATEM fake: it proves the two ends agree, not that either matches OBS.

## OBS test (any PC running OBS 28+ on the same Wi-Fi, ~10 min)
In OBS: Tools > WebSocket Server Settings > Enable, note the port (4455) and the password (or turn auth off).
On the setup page choose OBS, the PC's IP, the password, and type your scene names next to each light
(light 1 = first name, and so on). Save. The serial monitor prints
`OBS: connected. program="..." preview="..." studio=0/1`. Switch scenes: the matching light goes red.
Turn on Studio Mode and pick a preview scene: that light goes green.

## Status
- Written 2026-10-12 by Wren. Both targets compile (PlatformIO, espressif32, esp32-c3-devkitm-1).
  Not yet flashed to real boards.
- ATEM client (`base/atem_client.h`, UDP 9910, `TlIn`) written the same day from the wire format in
  atem-connection and the OpenSwitcher docs; compiles; untested against a real switcher.
- OBS client (`base/obs_client.h`, obs-websocket v5, port 4455, password auth) written 2026-10-13 from the
  official protocol doc; untested against a real OBS.
- Setup web page + NVS settings (`base/config.h`, `base/web_config.h`) written 2026-10-13 (v0.4); compiles;
  untested on a board.
- Channel-follow (v0.5, 2026-10-14): the base can join a venue Wi-Fi on any channel; a light that loses the
  link scans channels 1-13 (400 ms each), locks on the base and remembers the channel in flash. Packet is
  now proto v2 (adds the base's channel byte), so flash both ends together. Untested on a board.
  Bench check: power a light with the base off; LED 6 blinks amber and the serial log prints "scanning";
  power the base on a venue Wi-Fi that is not channel 1; within ~5 s the log prints "base found on channel N".
- Not yet: MAC pairing, Ethernet, OTA partition table. PROTOCOLS.md has the notes.
