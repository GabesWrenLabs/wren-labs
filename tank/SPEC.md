# Wren Tank Alert, spec v0.1 (2026-10-22)

A box at a remote stock tank that tells the rancher at the house when the water is low. One-time price, no
monthly fee, no cell contract, no app required. Everything below is a design intent; nothing has been built or
tested yet. Guesses are marked (guess).

## The promise
- Tank drops below the float: a buzzer and a big LED at the house within 1 minute, and a text if the house box
  has Wi-Fi.
- No cell signal needed at the tank. Radio link, tank to house, over open range and low hills.
- Runs on its own: small solar panel + battery, no wiring at the tank.
- Survives a Utah winter outdoors (guess on exact rating; target IP67 box, -20 C).

## Two parts
1. **Tank node.** Float switch on a cable (sump style) hung in the tank; LoRa radio (915 MHz, SX1262 class);
   ESP32-C3; 1-2 W solar panel + Li-ion cell + charger; IP67 box with a gland; 915 MHz antenna. Sends "level OK /
   LOW" plus battery voltage every 10 minutes and immediately on change. Sleeps between sends.
2. **House base.** Same radio + ESP32; loud buzzer, big LED, one button to silence; USB power. Optional Wi-Fi
   join (captive portal, like the tally base) so it can send a text/email. Raises an alarm if the tank node
   goes silent for 1 hour (dead battery, broken antenna) so silence is never mistaken for "fine".

Add-on: more tank nodes, one base. Each node has an ID; the base shows which tank.

## What has to be true for this to work (the unknowns, in order)
| # | Unknown | How we find out | Status |
|---|---|---|---|
| 1 | Is driving to check tanks a real, named cost for a rancher? | Farm-store counter call (ask #10), one rancher conversation via Gabe's network | open, 0 conversations so far |
| 2 | LoRa range tank-to-house over real Utah ground, no line of sight | Two Heltec LoRa 32 V3 boards (ask #11), range-test firmware, Gabe drives | open |
| 3 | Float switch survives ice, algae, cattle | Ask ranchers what floats they trust; test one over a winter | open |
| 4 | Power budget: does a 1 W panel keep a cell charged through December? | Measure sleep current on the prototype; math in COSTS.md | open |
| 5 | FCC: certified LoRa module + Part 15B self-declaration | Pick a module with its own FCC ID (Seeed Wio-SX1262 / RAK3172 class); budget the scan after demand | open |

Unknowns 1 and 2 come first. No custom PCB until 1 is answered by a human and 2 by a drive.

## Range-test firmware (next build, one wake)
Target: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 + 0.96" OLED). Same image on both boards. BOOT button at power-up
picks the role: held = "house" (listens, shows RSSI/SNR/packets lost), not held = "truck" (pings every 2 s with a
counter). 915 MHz, SF9-SF12 selectable later; start at SF10/125 kHz, 22 dBm. Deliverable: a merged .bin flashable
from the browser (same method as work/tally/firmware/release/FLASH.md) plus RANGE_TEST.md: where to park the house
board (window facing the range), what to note at each stop (distance, RSSI, lost count).

## Price and cost (from brain/COSTS.md section B, all guesses until quoted)
COGS ~$78 for 1 node + 1 base; all-in ~$98 at a $249 price (61% margin). Add-on node COGS ~$50, sell ~$129.
Competitors are satellite/cell units at $300-1,800 plus $99-365/yr. Our line: no monthly fee. Don't quote any price
to a rancher until a BOM is priced and Gabe's hour is counted.
