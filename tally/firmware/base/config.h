// Wren Tally base: settings that survive a power cycle (ESP32 NVS via the core's Preferences).
// Written by Wren 2026-10-13. One switcher type at a time; the web page (web_config.h) edits these.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "wren_proto.h"

enum SwitcherType : uint8_t { SW_NONE = 0, SW_VMIX = 1, SW_ATEM = 2, SW_OBS = 3 };

struct WrenConfig {
  String ssid, pass;                 // Wi-Fi to join; empty = stand-alone (base makes its own AP)
  uint8_t type = SW_NONE;            // SwitcherType
  String host;                       // switcher IP or hostname
  uint16_t port = 0;                 // 0 = the switcher's default (vMix 8099, ATEM 9910, OBS 4455)
  String obsPass;                    // obs-websocket password; empty = auth off
  uint8_t inputMap[WREN_MAX_LIGHTS] = {1, 2, 3, 4};   // vMix/ATEM: light N shows input inputMap[N-1]
  String sceneMap[WREN_MAX_LIGHTS] = {"Camera 1", "Camera 2", "Camera 3", "Camera 4"};  // OBS
  uint8_t brightness = 160;
  String apPass = "wrentally";       // password of the base's own setup AP (8+ chars or WPA2 refuses it)

  uint16_t effectivePort() const {
    if (port) return port;
    return type == SW_VMIX ? 8099 : type == SW_ATEM ? 9910 : type == SW_OBS ? 4455 : 0;
  }
  const char *typeName() const {
    return type == SW_VMIX ? "vMix" : type == SW_ATEM ? "ATEM" : type == SW_OBS ? "OBS" : "none";
  }

  void load() {
    Preferences p;
    p.begin("wren", true);
    ssid = p.getString("ssid", "");
    pass = p.getString("pass", "");
    type = p.getUChar("type", SW_NONE);
    host = p.getString("host", "");
    port = p.getUShort("port", 0);
    obsPass = p.getString("obspass", "");
    brightness = p.getUChar("bright", 160);
    apPass = p.getString("appass", "wrentally");
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
      char k[8];
      snprintf(k, sizeof(k), "in%d", i + 1);
      inputMap[i] = p.getUChar(k, i + 1);
      snprintf(k, sizeof(k), "sc%d", i + 1);
      String def = String("Camera ") + (i + 1);
      sceneMap[i] = p.getString(k, def);
    }
    p.end();
    if (type > SW_OBS) type = SW_NONE;
    if (apPass.length() < 8) apPass = "wrentally";
  }

  void save() const {
    Preferences p;
    p.begin("wren", false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.putUChar("type", type);
    p.putString("host", host);
    p.putUShort("port", port);
    p.putString("obspass", obsPass);
    p.putUChar("bright", brightness);
    p.putString("appass", apPass);
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
      char k[8];
      snprintf(k, sizeof(k), "in%d", i + 1);
      p.putUChar(k, inputMap[i]);
      snprintf(k, sizeof(k), "sc%d", i + 1);
      p.putString(k, sceneMap[i]);
    }
    p.end();
  }

  static void factoryReset() {
    Preferences p;
    p.begin("wren", false);
    p.clear();
    p.end();
  }
};
