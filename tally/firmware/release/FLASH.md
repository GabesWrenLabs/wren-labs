# Wren Tally v0.6: flash kit (no toolchain needed)

Two files, one per board. Each is a complete image that flashes at address **0x0**:

| File | Goes on | Then it |
|---|---|---|
| `wren-tally-base-v0.6.bin` | board A (the base) | broadcasts light states; serial commands; setup web page |
| `wren-tally-light-v0.6.bin` | board B (a light) | drives 6 WS2812B LEDs from the base's broadcasts |

Boards: any **ESP32-C3** dev board with a USB-C/micro-USB port (ESP32-C3-DevKitM-1, "ESP32-C3 SuperMini",
Seeed XIAO ESP32C3 all fit: the firmware uses the chip's own USB for serial, GPIO4 for the LED strip,
GPIO9 = BOOT button). It is **not** built for the classic ESP32-WROOM or the S3; tell Wren the board you have
and a variant comes next wake.

## Option 1: flash from Chrome, nothing to install (~3 min per board)
1. Plug the board in. Open **https://espressif.github.io/esptool-js/** in Chrome or Edge.
2. Click **Connect**, pick the board's COM port.
   If the port never appears or it fails to sync: hold **BOOT**, tap **RESET**, release BOOT, then Connect again.
3. Flash address **0x0**, choose the `.bin` for that board, click **Program**. Wait for "Leaving...".
4. Press **RESET** on the board (or unplug/replug).

## Option 2: esptool from a command line (~1 min per board, if Python is around)
```
pip install esptool
esptool.py --chip esp32c3 --port COM5 write_flash 0x0 wren-tally-base-v0.6.bin
esptool.py --chip esp32c3 --port COM6 write_flash 0x0 wren-tally-light-v0.6.bin
```
(The four raw parts are here too if you prefer esptool's four-address form: bootloader 0x0, partitions 0x8000,
boot_app0 0xe000, firmware 0x10000.)

## Wiring the light (board B)
| Strip | Board |
|---|---|
| DIN | GPIO4 |
| 5V | 5V (USB) pin |
| GND | GND |
Any WS2812B strip or stick works; the firmware drives the first 6 LEDs. Light number: leave GPIO5 and GPIO6
open = light 1 (ground GPIO5 = light 2, ground GPIO6 = light 3, both = light 4).

## The 15-minute bench test (zero switchers)
1. Both boards flashed and powered. Open a serial monitor on **board A** at 115200 (Chrome:
   https://googlechromelabs.github.io/serial-terminal/ ; or Arduino IDE / PuTTY / `pio device monitor`).
   You should see a boot line with the base's MAC and `WrenTally-XXXX` Wi-Fi coming up.
2. Type `p1` + Enter: board B's LEDs go **red**. `v1`: **green**. `o`: off.
3. Unplug board A: within 2 s board B shows one **amber** LED blinking (lost link).
4. Plug A back in, walk away with B (on a power bank) until it loses link. That distance is the range answer.

What Wren needs back (a Discord message is enough):
- did each board boot (yes / no, and the first lines of serial if no)?
- did `p1` turn B red?
- range in metres / steps, and what was between the boards (walls? a hall?)
- one phone photo of the lit strip, if it's easy (it becomes the first real product photo)

Built 2026-10-22 from commit `dba3409` (firmware v0.6) with `python tools/make_release.py v0.6`.
Settings: ESP32-C3, 4 MB flash, DIO, 80 MHz. Never yet run on real hardware: the first boot is itself the test.
