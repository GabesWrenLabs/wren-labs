// Wren Tally base: Wi-Fi bring-up + the one-page setup site (port 80).
// Written by Wren 2026-10-13. Uses the ESP32 Arduino core's WebServer (same LGPL-2.1 as the core
// itself, so no new licence exposure; D18 stays satisfied: nothing GPL).
//
// How a user sets it up, no app, no flashing:
//   1. Power the base. If it has no Wi-Fi saved (or can't join within 20 s) it makes its own network
//      "WrenTally-XXXX" (password: wrentally). Join it, open http://192.168.4.1
//   2. Pick the switcher, type its IP, choose which input (or OBS scene) each light follows, Save.
//      The base reboots, joins the venue Wi-Fi and the page is then at http://wren-tally.local
//      (mDNS) or the IP shown on the serial monitor.
//   The setup AP stays up for 10 min after boot even when the venue Wi-Fi works, so a wrong
//   password never locks anyone out. Hold the status/boot button at power-up to factory-reset
//   (handled in main.cpp via the serial 'f' command for the dev-board stage).
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"

typedef String (*StatusFn)();

class WebConfig {
 public:
  bool apUp = false;

  void begin(WrenConfig *cfg, StatusFn status, uint8_t espNowChannel) {
    _cfg = cfg; _status = status; _channel = espNowChannel;
    _bootMs = millis();
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(_apName, sizeof(_apName), "WrenTally-%02X%02X", mac[4], mac[5]);

    if (_cfg->ssid.length()) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.setHostname("wren-tally");
      WiFi.begin(_cfg->ssid.c_str(), _cfg->pass.c_str());
      Serial.printf("wifi: joining %s\n", _cfg->ssid.c_str());
    } else {
      WiFi.mode(WIFI_AP_STA);
    }
    _startAp();

    _server.on("/", HTTP_GET, [this]() { _page(""); });
    _server.on("/save", HTTP_POST, [this]() { _save(); });
    _server.on("/status", HTTP_GET, [this]() { _server.send(200, "text/plain", _status ? _status() : ""); });
    _server.on("/reset", HTTP_POST, [this]() {
      WrenConfig::factoryReset();
      _server.send(200, "text/html", "<p>Settings cleared. Rebooting.</p>");
      delay(300);
      ESP.restart();
    });
    _server.onNotFound([this]() { _server.sendHeader("Location", "/"); _server.send(302); });
    _server.begin();
    MDNS.begin("wren-tally");
  }

  void loop() {
    _server.handleClient();
    if (_cfg->ssid.length() && WiFi.status() != WL_CONNECTED && !_staGaveUp && millis() - _bootMs > 20000) {
      _staGaveUp = true;
      Serial.printf("wifi: could not join %s; setup AP %s stays up\n", _cfg->ssid.c_str(), _apName);
    }
    if (_cfg->ssid.length() && WiFi.status() == WL_CONNECTED && !_staReported) {
      _staReported = true;
      Serial.printf("wifi: joined %s, ip %s, setup page http://%s/ or http://wren-tally.local/\n",
                    _cfg->ssid.c_str(), WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
    }
    // Drop the setup AP 10 min after boot once the venue Wi-Fi works (less RF noise, less exposure).
    if (apUp && WiFi.status() == WL_CONNECTED && millis() - _bootMs > 600000) {
      WiFi.softAPdisconnect(true);
      apUp = false;
      Serial.println("wifi: setup AP closed (venue Wi-Fi is up)");
    }
  }

 private:
  WebServer _server{80};
  WrenConfig *_cfg = nullptr;
  StatusFn _status = nullptr;
  uint8_t _channel = 1;
  uint32_t _bootMs = 0;
  bool _staGaveUp = false, _staReported = false;
  char _apName[20] = "WrenTally";

  void _startAp() {
    // Stand-alone: AP on the default ESP-NOW channel. With a venue STA link the AP follows the
    // STA's channel (one radio); the lights scan for whichever channel we end up on.
    apUp = WiFi.softAP(_apName, _cfg->apPass.c_str(), _cfg->ssid.length() ? 1 : _channel);
    Serial.printf("wifi: setup AP %s (pw %s) at http://%s/\n", _apName, _cfg->apPass.c_str(),
                  WiFi.softAPIP().toString().c_str());
  }

  static String _esc(const String &s) {
    String o;
    o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
      else if (c == '"') o += "&quot;"; else o += c;
    }
    return o;
  }

  static String _opt(uint8_t v, uint8_t cur, const char *label) {
    String s = "<option value=\"" + String(v) + "\"" + (v == cur ? " selected" : "") + ">" + label + "</option>";
    return s;
  }

  void _page(const String &msg) {
    WrenConfig &c = *_cfg;
    String h;
    h.reserve(4096);
    h += F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
           "<title>Wren Tally setup</title><style>body{font:16px system-ui,sans-serif;max-width:34em;margin:1.5em auto;padding:0 1em;color:#222}"
           "label{display:block;margin:.8em 0 .2em;font-weight:600}input,select{width:100%;padding:.45em;font-size:1em;box-sizing:border-box}"
           "fieldset{border:1px solid #ccc;border-radius:6px;margin:1em 0}legend{font-weight:700}.row{display:flex;gap:.6em}.row>div{flex:1}"
           "button{font-size:1em;padding:.6em 1.2em;margin-top:1em}.ok{background:#e6f7e6;padding:.6em;border-radius:6px}"
           "small{color:#666}pre{background:#f4f4f4;padding:.6em;border-radius:6px;white-space:pre-wrap}</style></head><body>"
           "<h1>Wren Tally</h1>");
    if (msg.length()) h += "<p class=ok>" + msg + "</p>";
    h += "<pre>" + _esc(_status ? _status() : "") + "</pre>";
    h += F("<form method=post action=/save><fieldset><legend>Venue Wi-Fi</legend>"
           "<label>Network name (SSID)</label><input name=ssid maxlength=32 value=\"");
    h += _esc(c.ssid);
    h += F("\"><label>Password</label><input name=pass type=password maxlength=63 value=\"");
    h += _esc(c.pass);
    h += F("\"><small>Leave both empty to run stand-alone (serial / cue mode only).</small></fieldset>"
           "<fieldset><legend>Switcher</legend><label>Type</label><select name=type>");
    h += _opt(SW_NONE, c.type, "None (cue mode / bench)");
    h += _opt(SW_VMIX, c.type, "vMix (TCP API, port 8099)");
    h += _opt(SW_ATEM, c.type, "Blackmagic ATEM (port 9910)");
    h += _opt(SW_OBS, c.type, "OBS Studio (obs-websocket, port 4455)");
    h += F("</select><div class=row><div><label>IP address / hostname</label><input name=host maxlength=63 value=\"");
    h += _esc(c.host);
    h += F("\"></div><div><label>Port <small>(0 = default)</small></label><input name=port type=number min=0 max=65535 value=\"");
    h += String(c.port);
    h += F("\"></div></div><label>OBS WebSocket password <small>(OBS only)</small></label><input name=obspass type=password maxlength=63 value=\"");
    h += _esc(c.obsPass);
    h += F("\"></fieldset><fieldset><legend>Lights</legend>"
           "<small>vMix / ATEM: the input number each light follows. OBS: the scene name.</small>");
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
      h += "<div class=row><div><label>Light " + String(i + 1) + ": input</label><input name=in" + String(i + 1) +
           " type=number min=1 max=99 value=\"" + String(c.inputMap[i]) + "\"></div><div><label>Light " + String(i + 1) +
           ": OBS scene</label><input name=sc" + String(i + 1) + " maxlength=63 value=\"" + _esc(c.sceneMap[i]) + "\"></div></div>";
    }
    h += F("<label>Brightness (1-255)</label><input name=bright type=number min=1 max=255 value=\"");
    h += String(c.brightness);
    h += F("\"></fieldset><fieldset><legend>This base</legend><label>Setup network password <small>(8+ characters)</small></label>"
           "<input name=appass maxlength=63 value=\"");
    h += _esc(c.apPass);
    h += F("\"></fieldset><button type=submit>Save and restart</button></form>"
           "<form method=post action=/reset onsubmit=\"return confirm('Clear all settings?')\"><button type=submit>Factory reset</button></form>"
           "<p><small>Wren Tally base, firmware v0.5 (dev-board stage). Made by Wren Labs.</small></p></body></html>");
    _server.send(200, "text/html", h);
  }

  void _save() {
    WrenConfig &c = *_cfg;
    c.ssid = _server.arg("ssid"); c.ssid.trim();
    c.pass = _server.arg("pass");
    int t = _server.arg("type").toInt();
    c.type = (t >= SW_NONE && t <= SW_OBS) ? (uint8_t)t : SW_NONE;
    c.host = _server.arg("host"); c.host.trim();
    int port = _server.arg("port").toInt();
    c.port = (port > 0 && port <= 65535) ? (uint16_t)port : 0;
    c.obsPass = _server.arg("obspass");
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
      int in = _server.arg("in" + String(i + 1)).toInt();
      c.inputMap[i] = (in >= 1 && in <= 99) ? (uint8_t)in : (uint8_t)(i + 1);
      c.sceneMap[i] = _server.arg("sc" + String(i + 1));
      c.sceneMap[i].trim();
    }
    int b = _server.arg("bright").toInt();
    c.brightness = (b >= 1 && b <= 255) ? (uint8_t)b : 160;
    String ap = _server.arg("appass");
    if (ap.length() >= 8) c.apPass = ap;
    c.save();
    _server.send(200, "text/html",
                 F("<!doctype html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
                   "<body style=\"font:16px system-ui;max-width:34em;margin:2em auto;padding:0 1em\"><h1>Saved</h1>"
                   "<p>The base is restarting. If you set a venue Wi-Fi, reconnect your phone to it and open "
                   "<a href=\"http://wren-tally.local/\">http://wren-tally.local/</a> (or the IP on the serial monitor). "
                   "Otherwise rejoin the WrenTally network and <a href=\"/\">reload</a>.</p></body>"));
    delay(500);
    ESP.restart();
  }
};
