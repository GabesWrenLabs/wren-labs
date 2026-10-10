# Wren Tally prototype BOM (1 base + 2 lights)

Draft v0.1, 2026-10-10 (Wren). Purpose: the parts list behind the first `money` ask (~$80 target).
Part numbers marked VERIFIED were seen on LCSC/JLCPCB with price and stock on the date given.
Everything else is a candidate. Prices in USD.

## What the prototype proves
1. ESP32-C3 base talks to an ATEM Mini, vMix and OBS over Ethernet (W5500) and Wi-Fi.
2. ESP-NOW from base to lights is reliable across a church-sized room (target: 30 m, through one wall).
3. Light-side battery life and brightness through a diffuser.
4. JLCPCB economic assembly works for our footprints (no hand-soldering of QFN parts by Gabe).

## Base board (qty 1)
| # | Part | Qty | LCSC | Unit | Ext | Basic/Ext | Status |
|---|---|---|---|---|---|---|---|
| B1 | ESP32-C3-MINI-1-N4 | 1 | | | | | pending helper |
| B2 | W5500 Ethernet (QFN-48) | 1 | | | | | pending helper |
| B3 | RJ45 magjack w/ magnetics + LEDs | 1 | | | | | pending helper |
| B4 | 25 MHz crystal 3225 | 1 | | | | | pending helper |
| B5 | USB-C receptacle 16-pin | 1 | | | | | pending helper |
| B6 | 3.3 V LDO 600 mA+ | 1 | | | | | pending helper |
| B7 | Screw terminal 3.5 mm 2-pin | 2 | | | | | pending helper |
| B8 | Tactile switch 6x6 | 2 | | | | | pending helper |
| B9 | Passives (0603 R/C, 5.1 k CC pulldowns, 10 uF, 100 nF) | ~20 | Basic | ~$0.50 | | Basic | estimate |
| B10 | Status LEDs 0805 x3 + resistors | 3 | Basic | ~$0.10 | | Basic | estimate |

## Light board (qty 2 for prototype; 4 per kit)
| # | Part | Qty/board | LCSC | Unit | Ext | Basic/Ext | Status |
|---|---|---|---|---|---|---|---|
| L1 | ESP32-C3-MINI-1-N4 | 1 | | | | | pending helper |
| L2 | Addressable RGB LED (WS2812B-2020 or 5050) | 6 | | | | | pending helper |
| L3 | Li-ion charger (TP4056 family) | 1 | | | | | pending helper |
| L4 | Battery protection (DW01A + FS8205A) | 1 | | | | | pending helper |
| L5 | 18650 holder, TH | 1 | | | | | pending helper |
| L6 | USB-C receptacle 16-pin | 1 | | | | | pending helper |
| L7 | 3.3 V low-Iq LDO | 1 | | | | | pending helper |
| L8 | Tactile switch 6x6 (brightness) | 1 | | | | | pending helper |
| L9 | Passives | ~15 | Basic | ~$0.40 | | Basic | estimate |

## Not on the boards (Gabe's bench / later)
- 18650 cells x2: Molicel P26A ~$5 each retail (COSTS.md). Gabe may have cells on hand: ask.
- Enclosures: 3D print (Gabe) for the prototype. $0 beyond filament.
- Diffuser: white PETG print or a slice of milky acrylic.
- Ethernet cable, USB-C cables: Gabe's bench.

## JLCPCB order (to be filled from helper's pricing page read)
| Item | Est. |
|---|---|
| 2 designs x 5 PCBs, 2-layer, ~50x50 mm | |
| Economic PCBA, 2 boards per design assembled (if min is 2) | |
| Extended-part fees | |
| Shipping to Utah (cheapest tracked) | |
| **Total** | **target <= $80** |

## Open questions for Gabe (go in the money ask, not before)
- Does he have 18650 cells and a charger on the bench?
- Does ShockForce have a JLCPCB account with saved address, or do I ask for a company one (`account`)?
- Does he know anyone with an ATEM Mini for the bench test, or should the prototype plan use OBS only first?
