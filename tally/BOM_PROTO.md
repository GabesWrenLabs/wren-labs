# Wren Tally prototype BOM (1 base + 2 lights)

v0.2, 2026-10-10 (Wren). Purpose: the parts list behind the first `money` ask.
Prices USD. VERIFIED = seen on LCSC/JLCPCB on 2026-10-10 by my research helper (URLs in the
tables). Everything else is a candidate or a guess and says so.

## Headline: two prototype routes, and the cheap one goes first
| Route | What | Est. cost | Proves | Lead time |
|---|---|---|---|---|
| **A. Dev-board rig (first)** | ESP32-C3 dev boards + W5500 module + WS2812B sticks + TP4056 module, hand-wired on Gabe's bench | **~$45 (guess, Amazon)** | firmware for ATEM/vMix/OBS, ESP-NOW range, LED brightness, battery draw | 2-3 days |
| B. Custom boards (after E1 passes) | 2 JLCPCB economic-PCBA designs, 2 boards assembled each + 5 bare PCBs | **~$115 (estimate, breakdown below)** | assembly flow, footprints, enclosure fit | ~2 weeks |

Why A first: the risky questions are "does the firmware talk to all three switchers" and "does
ESP-NOW cross a church hall", not "can JLCPCB place a QFN". Route A answers those for a third of
the money and a tenth of the wait, and nothing in it is wasted: the dev boards become the firmware
test bench for every later revision. Route B's ~$43 of feeder fees is money we only spend once
E1 shows people want the kit.

## Route A: dev-board rig (prices are GUESSES until I check listings before the money ask)
| Item | Qty | Est. | Note |
|---|---|---|---|
| ESP32-C3 dev board (DevKitM-1 style, with USB-C) | 3 (1 base, 2 lights) | ~$18 (3-pack) | ESP-NOW + Wi-Fi + native USB flashing |
| W5500 Ethernet module (SPI, with magjack) | 1 | ~$9 | base's wired link |
| WS2812B 8-LED stick | 2 | ~$8 (5-pack) | light face; use 6 of 8 |
| TP4056 charger module w/ protection, USB-C | 2 | ~$7 (pack) | battery test; optional for first bench |
| 18650 cell + holder | 2 | $0 if Gabe has cells; else ~$12 | ask Gabe |
| Jumper wires, USB-C cables, breadboard | | $0 | Gabe's bench |
| **Total** | | **~$42-54** | |

## Route B: custom boards, verified parts (for the KiCad design; order after E1)
### Base board (qty 1 design, 2 assembled)
| # | Part | Qty | LCSC | Unit (lowest tier) | B/E | Status |
|---|---|---|---|---|---|---|
| B1 | ESP32-C3-MINI-1-N4, Espressif | 1 | C2838502 | $3.06 @1, $2.69 @10 | Extended | VERIFIED. **NRND**: Espressif points new designs to ESP32-C3-MINI-1-H4X (C-number not yet checked). Stock 3.5k. |
| B2 | W5500, WIZnet, **LQFP-48** (not QFN) | 1 | C32843 | $2.85 @1, $2.48 @10 | Extended | VERIFIED, stock 15.9k |
| B3 | HR911105A magjack w/ LEDs, THT, HanRun | 1 | C12074 | $1.73 @1 | likely Extended | VERIFIED price; stock reads 44 on the panel vs 34k in FAQ: have a fallback |
| B4 | 25 MHz 3225 20 pF crystal, JGHC | 1 | C390774 | $0.078 @10 (min 10) | unverified | no Basic 25 MHz/20 pF 3225 found |
| B5 | USB-C 16P SMD, TYPE-C-31-M-12 | 1 | C165948 | $0.173 @5 (min 5) | Extended | VERIFIED, stock 388k |
| B6 | AMS1117-3.3 SOT-223 | 1 | C6186 | $0.21 @5 (min 5) | Basic | VERIFIED |
| B7 | USB-UART bridge | 0 | | | | **Dropped.** ESP32-C3 has native USB Serial/JTAG on GPIO18/19 (Espressif docs). |
| B8 | 3.5 mm 2P screw terminal THT (Phoenix 1716933) | 2 | C6366685 | not read | assume Extended | candidate only |
| B9 | Tactile SMD, XKB TS-1187A-B-A-B (5.1 mm) | 2 | C318884 | ~$0.02 | Basic | from search snippet |
| B10 | Passives 0603, 5.1 k CC pulldowns, 10 uF, 100 nF | ~20 | Basic | ~$0.50 | Basic | estimate |
| B11 | Status LEDs 0805 x3 + resistors | 3 | Basic | ~$0.10 | Basic | estimate |
Base parts ~ $10 per board. Extended types: 6-7 (ESP32, W5500, magjack, crystal, USB-C, terminals).

### Light board (qty 1 design, 2 assembled; 4 per kit later)
| # | Part | Qty/board | LCSC | Unit (lowest tier) | B/E | Status |
|---|---|---|---|---|---|---|
| L1 | ESP32-C3-MINI-1-N4 | 1 | C2838502 | $3.06 @1 | Extended | VERIFIED (NRND, see B1) |
| L2 | WS2812B-2020, Worldsemi | 6 | C965555 | $0.092 @5 | Extended | VERIFIED price but panel says "Not available now". Candidates: WS2812B-2020-V6 C52917434, WS2812C-2020-V1 C2976072 (not priced) |
| L3 | TP4056-42-ESOP8, TopPower | 1 | C16581 | $0.186 @5 | Extended | VERIFIED, stock 47.7k |
| L4a | DW01A SOT-23-6, PUOLOP | 1 | C351410 | $0.025 @20 | unverified | |
| L4b | FS8205A SOT-23-6, FUXINSEMI | 1 | C908265 | $0.058 @10 | unverified | |
| L5 | 18650 holder 1-cell THT, MYOUNG BH-18650-A1AJ005 | 1 | C5290176 | ~$0.36 | assume Extended | snippet |
| L6 | USB-C 16P SMD | 1 | C165948 | $0.173 @5 | Extended | VERIFIED |
| L7 | ME6211C33M5G-N SOT-23-5, Microne (low Iq LDO) | 1 | C82942 | $0.062 @10 | Extended | VERIFIED |
| L8 | Tactile SMD XKB | 1 | C318884 | ~$0.02 | Basic | snippet |
| L9 | Passives | ~15 | Basic | ~$0.40 | Basic | estimate |
Light parts ~ $4.5 per board. Extended types: 7.

### JLCPCB economic PCBA fees (help page, "last updated Sep 09, 2026", VERIFIED)
| Fee | Amount |
|---|---|
| Setup | $8.18 per order/design |
| Stencil | $1.53 |
| SMT joints | $0.0016 each |
| **Extended part feeder fee** | **$3.07 per Extended part type** |
| Hand-solder (THT) labour | $3.58 per order + $0.0164/joint |
| Minimum assembled boards | 2 |
Sources: jlcpcb.com/help/article/pcb-assembly-price and jlcpcb.com/solutions/pcb-prototype-assembly.

### Route B order estimate (2 designs, 2 assembled each, 5 PCBs each)
| Line | Base | Light | Note |
|---|---|---|---|
| PCBs, 5 pcs 2-layer ~50x50 mm | ~$4 | ~$4 | guess (JLC 2-layer promo pricing) |
| Setup + stencil | $9.71 | $9.71 | verified |
| Extended feeders | ~$21 (7) | ~$21 (7) | verified rate x my count |
| Parts for 2 boards | ~$21 | ~$10 | lowest tiers; some have min 5-20 |
| Hand-solder THT | ~$4 | ~$4 | verified |
| Subtotal | ~$60 | ~$49 | |
| Shipping to Utah | ~$10-20 | | guess |
| **Total** | **~$115-130** | | over the $80 I wrote in COSTS.md: that figure was wrong |

Cut options for B if needed: merge base + light into one design (one setup, one feeder set, DNP
the W5500/magjack on light builds) saves ~$30 but makes the light board oversized; or swap
Extended parts for Basic equivalents where they exist (crystal, LDO, switches already Basic).

## Production note (per-kit, Route B parts only)
Feeder fees are per ORDER, not per board, so at 20 kits (20 base + 80 light boards) they add ~$2
per kit, not $43. The $18/light and $24/base in COSTS.md stay plausible. Re-check with a real
quote before the first batch.

## Open questions for Gabe (go in the money ask, not before)
- 18650 cells and a charger on the bench? (saves ~$12 and a day)
- Amazon is fastest for Route A. Does he prefer I list exact Amazon links, or will he pick his own?
- Anyone with an ATEM Mini for the bench test, or OBS-only first?
