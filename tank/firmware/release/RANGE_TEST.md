# Wren Tank Alert: LoRa range test (firmware v0.1)

One file, both boards. `wren-range-v0.1.bin` flashes at address **0x0** on a **Heltec WiFi LoRa 32 V3** (the ESP32-S3
one with the 0.96" screen and the SX1262 radio, 915 MHz version). Screw the antenna on **before** powering either
board; the radio can damage itself transmitting into nothing.

## Flash (~3 min per board, nothing to install)
1. Plug the board in. Open **https://espressif.github.io/esptool-js/** in Chrome or Edge.
2. **Connect**, pick the board's COM port. If it won't sync: hold **PRG**, tap **RST**, release PRG, Connect again.
3. Flash address **0x0**, choose `wren-range-v0.1.bin`, **Program**, wait for "Leaving...".
4. Unplug and replug.

Command-line form, if Python is around: `pip install esptool` then
`esptool.py --chip esp32s3 --port COM5 write_flash 0x0 wren-range-v0.1.bin`
(raw parts are in this folder too: bootloader 0x0, partitions 0x8000, boot_app0 0xe000, firmware 0x10000).

## Pick the roles (at power-up)
| Board | How | Screen says |
|---|---|---|
| **HOUSE** | hold **PRG** while plugging in (or while tapping RST), let go after a second | `HOUSE` top-left, then `waiting` |
| **TRUCK** | just plug in | `TRUCK` top-left, then `pinging` |

Both boards side by side on the bench first: the truck should say **LINK OK** within 2 s and the house should show
`got N lost 0`. If a board shows `radio err -2` the radio didn't answer (wrong board or bad solder); tell Wren the number.

Power: USB power bank for the truck; the house can hang off a phone charger. The Heltec battery plug (a small 2-pin
JST) also works if you have a LiPo; the truck shows its battery in the serial log.

## The drive
1. **House board:** at the house (or wherever the real base would live), **inside, at a window facing the range**,
   antenna vertical. Higher is better: an upstairs window beats the kitchen counter.
2. **Truck board:** on the dash or held out the window, antenna vertical. The screen updates every 2 s:
   - `LINK OK` = the house answered the last ping
   - `LOST 12s` = no answer for 12 s
   - `here -112 dBm  -8.5` = how loud the house sounded at the truck, and the signal-to-noise
   - `house -110 dBm -7.0` = how loud the truck sounded at the house
   - `sent 120 ok 118 lost 2` = pings sent / answered / unanswered since power-up
3. Drive away along the road a tank would be on. **Stop every half mile or so**, wait 10 s, and note one line. Keep
   going until it says LOST for 30 s straight, then back up until it comes back; that spot is the edge.
4. If you can, do one stop with a hill or a building between the two boards. That's the number that decides the
   product.

What to write down at each stop (a phone photo of the truck screen is fine instead):

| Stop | Miles from house | What's between (open / trees / low hill / building) | here dBm | house dBm | lost count |
|---|---|---|---|---|---|
| 1 | 0.5 | | | | |

Rules of thumb (SF10, 125 kHz): -120 dBm with SNR above -12 is still solid; the link usually dies around -130.
Anything under 1 mile over open ground means something's wrong with an antenna, not with LoRa.

## What Wren needs back
- did both boards boot and show LINK OK on the bench? (if not: the first screen line / serial first lines)
- the stop table or the screen photos
- the distance where it died, and whether line-of-sight was open there
- a guess at how far your nearest "out of sight" tank would be from the house

Built 2026-10-22 from firmware v0.1 (`python tools/make_release.py v0.1` after `pio run`). Settings: 915.0 MHz,
BW 125 kHz, SF10, CR 4/5, 22 dBm, CRC on. Never yet run on real hardware: the bench LINK OK is itself the first test.
Battery readout assumes Heltec's 390k/100k divider (x4.9, guess); if it reads nonsense, ignore it, it changes nothing.
