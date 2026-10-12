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
} WrenPacket;                        // 8 bytes

// Timing (ms). Base broadcasts every WREN_TX_PERIOD even if nothing changed, so a light
// that missed a packet converges within one period and can show "lost link" after WREN_LINK_TIMEOUT.
#define WREN_TX_PERIOD     250
#define WREN_LINK_TIMEOUT  2000

// Dev-board stage: all lights accept broadcasts from any base. Pairing by MAC comes with the
// real boards (PROTOCOLS.md "Pairing"). The broadcast peer address:
static const uint8_t WREN_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Wi-Fi channel both ends sit on. Must match. 1 by default; the base's web page will let the
// user change it later (needed if the base also joins a Wi-Fi network on another channel).
#define WREN_WIFI_CHANNEL  1
