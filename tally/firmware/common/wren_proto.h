// Wren Tally: the packet that goes base -> lights over ESP-NOW.
// Matches PROTOCOLS.md "Base <-> lights". Keep this file identical on both ends.
#pragma once
#include <stdint.h>

#define WREN_MAX_LIGHTS 4

// Per-light state byte.
enum WrenState : uint8_t {
  WREN_OFF     = 0,
  WREN_PROGRAM = 1,  // red
  WREN_PREVIEW = 2,  // green
  WREN_BOTH    = 3,  // program wins on the LEDs; kept for completeness
};

// Flags byte.
#define WREN_FLAG_CUE_MODE   0x01  // cue mode: PROGRAM = "go", PREVIEW = "standby"
#define WREN_FLAG_SWITCHER   0x02  // base currently has a live switcher connection
#define WREN_FLAG_PAIRING    0x04  // base is in its 10 s pairing window

typedef struct __attribute__((packed)) {
  uint8_t version;                   // WREN_PROTO_VERSION
  uint8_t seq;                       // wraps; lights use it only for debugging
  uint8_t state[WREN_MAX_LIGHTS];    // one WrenState per light, index = light number - 1
  uint8_t brightness;                // 0..255
  uint8_t flags;                     // WREN_FLAG_*
  uint8_t channel;                   // Wi-Fi channel the base is transmitting on (1..13); v2+
} WrenPacket;                        // 9 bytes

// Timing (ms). Base broadcasts every WREN_TX_PERIOD even if nothing changed, so a light
// that missed a packet converges within one period and can show "lost link" after WREN_LINK_TIMEOUT.
#define WREN_TX_PERIOD     250
#define WREN_LINK_TIMEOUT  2000

// Channel-follow (light side). ESP-NOW only works when both ends sit on the same Wi-Fi channel,
// and the base's channel is whatever the venue router uses once it joins that Wi-Fi. So a light
// that has lost the link hops through channels 1..13, dwelling WREN_SCAN_DWELL on each (longer
// than one TX period, so a live base can't be missed), locks on the first valid packet, and
// remembers that channel in flash for the next power-up. Worst case to find the base: ~5 s.
#define WREN_SCAN_DWELL    400
#define WREN_CHANNEL_MIN   1
#define WREN_CHANNEL_MAX   13
#define WREN_CHANNEL_DEFAULT 1   // stand-alone base (no venue Wi-Fi) sits here

// Pairing (v0.6, PROTOCOLS.md "Pairing"). The base always broadcasts; a light decides whose
// packets to obey by the sender MAC. Base: a button / serial 'a' / web "Pair lights" opens a
// WREN_PAIR_WINDOW_MS window during which every packet carries WREN_FLAG_PAIRING. Light: with no
// base saved it follows any base and adopts the first one it hears with the flag set; once paired
// it ignores other bases. Holding the light's button WREN_PAIR_HOLD_MS forgets the base and opens
// the light's own window. The broadcast peer address:
static const uint8_t WREN_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define WREN_PAIR_WINDOW_MS 10000
#define WREN_PAIR_HOLD_MS   3000

// Kept for the base's stand-alone AP; the lights no longer depend on it (they scan).
#define WREN_WIFI_CHANNEL  WREN_CHANNEL_DEFAULT
