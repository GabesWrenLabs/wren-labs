# Wren Tally: wireless tally + cue lights for ATEM Mini, vMix and OBS

Product spec v0.1 (2026-10-09, Wren). Working name: **Wren Tally**. Everything here is a target
until the prototype proves it. Costs: brain/COSTS.md. Demand evidence: brain/RESEARCH.md wake 3.

## The wedge (why this and not a $600 kit)
- The Blackmagic ATEM Mini (the common church switcher) has no tally port. Every $500+ radio kit
  needs a $559 GPI box or doesn't work with it. We talk to the switcher over the network instead.
- Nothing on sale meets all of: under $300, base + 4 lights, no app, no flashing, works with
  ATEM Mini + vMix + OBS.
- "No weekly charging" is unserved. Our lights run from USB-C (camera rigs have power) and take an
  optional battery.

## Kit (retail $199: base + 4 lights; extra light $39)
### Base
- ESP32-C3-MINI-1 (FCC-certified module) + W5500 Ethernet with RJ45 magjack. Wi-Fi as fallback.
- Talks to: ATEM (Blackmagic TCP/UDP protocol, same one Tally Arbiter uses), vMix (TCP tally API,
  port 8099), OBS (obs-websocket v5). Auto-detects which one answers on the network.
- 2 screw-terminal GPIO inputs: plain "cue switch" mode for theatre. Close a contact, a light goes
  red. No switcher needed.
- Setup: plug in Ethernet, open the base's web page from any phone or laptop, type the switcher's IP,
  assign light 1-4 to camera 1-4. Settings persist. No app store, no Arduino IDE.
- Power: USB-C, 5 V. Hammond 1591XX-size ABS box. Status LEDs: link, switcher, lights.

### Light (x4)
- ESP32-C3-MINI-1. Receives from the base over ESP-NOW (no router, no pairing screens: lights ship
  paired to their base).
- Front: 6 RGB LEDs behind a diffuser, operator side. Back: 1 small LED (talent side), optional.
- Red = program, green = preview, off = idle. Brightness button. Cue mode: red standby, green go.
- Power: USB-C 5 V in. Optional 18650 inside (holder + charger IC); target 2600 mAh = ~25
  three-hour services per charge (guess until measured).
- Mount: 1/4"-20 insert + cold-shoe adapter + a velcro strip.
- Enclosure: 3D print (Gabe) for the first 20, then Hammond 1551 or a custom print service.

## Not in v1
- Talent-side big light, camera-number display, DMX, NDI tally, Roland/TriCaster. Add when asked.

## Regulatory
- Certified radio module avoids intentional-radiator testing. Part 15 Subpart B (SDoC) for the host
  still applies; budget $1-3k once demand is proven. Ask Gabe how ShockForce handled it.

## Milestones
1. Preorder page live + forum posts (experiment E1, $0). Pass line in brain/EXPERIMENTS.md.
2. Prototype: 1 base + 2 lights on JLCPCB, ~$80. Firmware: ATEM + vMix + OBS + ESP-NOW.
3. Gabe's bench test with a real ATEM Mini (ask him; or borrow one from a church contact).
4. First 10 kits, hand-assembled.
