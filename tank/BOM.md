# Stock-tank alert: priced bill of materials (v0.1, 2026-10-24)

One tank node + one base, costed at about 20-25 units from listed vendor prices. "L" = page read live on
24 Oct 2026; "S" = the search engine's copy of the vendor page (date unknown); no mark = our guess. Nothing has been
ordered or built from this list yet. Target retail is $249-299 for base + one tank, about $149 per extra tank.

## Tank node (~$77)

| Item | Cost | Source |
|---|---|---|
| ESP32-C3-MINI-1 (use the H4X; the N4 is marked not-recommended-for-new-designs) | $2.50 | DigiKey N4 $3.26 @1, $2.20 @650 (S) |
| LoRa module Seeed Wio-SX1262. FCC ID Z4T-WIO-SX1262, single modular grant 2024-10-16, 902.3-914.9 MHz | $4.50 | seeedstudio.com $4.29 @10+ (L, in stock); DigiKey 114993390 $5.95 @1 (S). Alternative RAK3172, FCC ID 2AF6B-RAK3172, $5.99 at the RAK store (L) |
| Tethered float switch, 10 ft lead, polypropylene (Sump Alarm SA-2359-3) | $27 | Zoro $26.89 (S). Biggest single line. Vertical mini float (Madison M8000, 1/8" NPT) $12.59 (S) if the tank has a fitting |
| Enclosure Hammond 1554 polycarbonate, clear lid | $9 | 1554CPL DigiKey $6.91 @25 (L); larger 1554F2GY $15 @100 (S) |
| PG7 nylon cable gland | $1.50 | Essentra CG-PG7-1-BK DigiKey $1.46 (S) |
| 915 MHz-band antenna 2 dBi SMA + u.FL pigtail | $16 | ALFA ARS-915P Rokland $11.97 (L); Adafruit 851 pigtail $3.95 (S). A PCB or LCSC antenna would cut ~$12 (unverified) |
| 3x AA holder + 3 Energizer L91 lithium cells | $10.80 | holder ~$1.81 (S, part number unverified); L91 $2.85-3.00 per cell (S) |
| Solar panel + charger | $0 in v1 | Adafruit 6 V 1 W $19.95 (L, out of stock) + bq24074 $14.95 (L) = $35: dropped from v1 |
| PCB + assembly, passives, regulator, connectors | $6 | JLCPCB economic PCBA fees (L, help page dated 22 Oct 2026): setup $8.18, stencil $1.53, $0.0016 per joint, $3.07 per Extended part type, $3.58 hand-solder if any through-hole part. About $45 fixed per order at ~10 Extended types |

## Base (~$44)

| Item | Cost | Source |
|---|---|---|
| ESP32-C3 + Wio-SX1262 | $7 | as above |
| Antenna + pigtail | $16 | as above |
| Buzzer CUI CMI-1275-05TH 85 dB + driver | $2 | Mouser $0.99 @1, ~$0.70 at breaks (S) |
| 0.96" OLED, I2C | $2 | LCSC HS96L03W2C03 ~$1.43 (S); DigiKey $4.99 (S) |
| Hammond 1591XXSSBK ABS box | $7 | DigiKey $9.09 @1, $5.56 @100 (S) |
| USB-C receptacle + PCB/assembly + passives | $6 | LCSC C165948 ~$0.09 (S), JLC basic part |
| USB wall adapter | $5 | guess |

## Totals

| | |
|---|---|
| COGS (node + base + ~$5 packaging) | ~$126 |
| Shipping (guess) | ~$12 |
| Payment fees | ~$8 @ $249, ~$9 @ $299 |
| All-in at $249 | ~$146 (margin ~$103, 41%) |
| All-in at $299 | ~$147 (margin ~$152, 51%) |

## Notes

- The radio in the product must run inside 902.3-914.9 MHz (for example 906.5 MHz), the range both module grants
  cover. The host must carry "Contains FCC ID: Z4T-WIO-SX1262" (or the RAK one). The host still needs a Part 15B
  unintentional-radiator self-declaration; budget that scan after demand is shown.
- The Ebyte E22-900M22S is off the list: no FCC ID string anywhere, out of stock at Ebyte's store, LCSC page gone.
- Stock today: Adafruit 1 W panel out, Molicel P26A 18650 sold out at 18650batterystore, so AA lithium it is.
- The two levers to get COGS under $100: a cheaper float switch (offshore tethered floats are ~$8-10, unverified) and a
  PCB or low-cost antenna instead of the $16 retail whip + pigtail.
