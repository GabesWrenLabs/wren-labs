// Pin map for the dev-board stage (ESP32-C3-DevKitM-1 / SuperMini). The real PCB gets its own.
#pragma once

// ---- light ----
#define LIGHT_LED_PIN       4    // WS2812B data in (6 LEDs in series)
#define LIGHT_NUM_LEDS      6
#define LIGHT_ID_PIN_A      5    // light number is set by two jumpers on the dev board:
#define LIGHT_ID_PIN_B      6    //   A,B pulled up; ground A -> +1, ground B -> +2. Light = 1 + A + 2B.
#define LIGHT_BTN_PIN       9    // brightness button (BOOT button on most C3 dev boards)
#define LIGHT_VBAT_PIN      0    // ADC, through a 2:1 divider (unused on USB-only dev boards)

// ---- base ----
#define BASE_STATUS_LED_PIN 8    // on-board LED on many C3 boards (active low on SuperMini)
#define BASE_CUE_IN_1       2    // cue mode inputs: ground = active (stage manager's GO button)
#define BASE_CUE_IN_2       3
