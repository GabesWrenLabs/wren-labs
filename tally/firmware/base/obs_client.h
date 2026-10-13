// Wren Tally base: OBS Studio tally client (obs-websocket v5, default port 4455).
// Written by Wren on 2026-10-13 from the obs-websocket protocol doc
// (obsproject/obs-websocket, docs/generated/protocol.md, read the same day). No code copied.
// Libraries: gilmaimon/ArduinoWebsockets (MIT), bblanchon/ArduinoJson (MIT), mbedtls (Apache, in the core).
//
// Flow (all JSON text frames):
//   server Hello (op 0)   d: rpcVersion, authentication{challenge, salt} (only if a password is set)
//   client Identify (op 1) d: rpcVersion 1, authentication, eventSubscriptions (General 1 | Scenes 4 | Ui 1024)
//   server Identified (op 2)
//   client Request (op 6)  GetCurrentProgramScene, GetCurrentPreviewScene, GetStudioModeEnabled
//   server RequestResponse (op 7) d: requestType, requestId, requestStatus{result, code}, responseData
//   server Event (op 5)   d: eventType, eventData  (CurrentProgramSceneChanged / CurrentPreviewSceneChanged
//                          carry sceneName; StudioModeStateChanged carries studioModeEnabled)
// Auth string: base64(sha256(base64(sha256(password + salt)) + challenge)).
// Auth failure: the server closes with code 4009.
//
// OBS has scenes, not numbered inputs, so the user maps each light to a scene name. Preview only
// exists in Studio Mode; outside it the preview light stays off.
#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"

class ObsClient {
 public:
  bool connected = false;      // Identified and the seed requests answered
  bool studioMode = false;
  String programScene;
  String previewScene;

  void begin(const char *host, uint16_t port, const char *password) {
    _host = host; _port = port; _password = password;
    _ws.onMessage([this](websockets::WebsocketsMessage msg) { _onMessage(msg.data()); });
    _ws.onEvent([this](websockets::WebsocketsEvent ev, String data) {
      if (ev == websockets::WebsocketsEvent::ConnectionClosed) {
        if (_identified) Serial.println("OBS: connection closed");
        _reset();
      }
    });
    _lastTryMs = 0;
  }

  // Call from loop(). Returns true if program/preview/studio-mode changed since the last call.
  bool poll() {
    if (!_ws.available()) {
      if (millis() - _lastTryMs < OBS_RETRY_MS) return false;
      _lastTryMs = millis();
      _reset();
      Serial.printf("OBS: connecting to %s:%u\n", _host, _port);
      _ws.connect(_host, _port, "/");
      return false;
    }
    _ws.poll();
    bool c = _changed;
    _changed = false;
    return c;
  }

  // 0 off, 1 program, 2 preview, 3 both; for a scene name (empty name = never lit).
  uint8_t stateForScene(const char *scene) const {
    if (!connected || !scene || !scene[0]) return 0;
    uint8_t s = 0;
    if (programScene == scene) s |= 1;
    if (studioMode && previewScene == scene) s |= 2;
    return s;
  }

 private:
  static const uint32_t OBS_RETRY_MS = 3000;
  static const uint32_t SUB_GENERAL = 1, SUB_SCENES = 4, SUB_UI = 1024;

  websockets::WebsocketsClient _ws;
  const char *_host = "";
  uint16_t _port = 4455;
  const char *_password = "";
  uint32_t _lastTryMs = 0;
  bool _identified = false, _changed = false;
  uint8_t _pendingSeeds = 0;

  void _reset() {
    connected = false; _identified = false; _pendingSeeds = 0;
    programScene = ""; previewScene = ""; studioMode = false;
  }

  static String _sha256b64(const String &in) {
    uint8_t hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const uint8_t *)in.c_str(), in.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    uint8_t out[64]; size_t olen = 0;
    mbedtls_base64_encode(out, sizeof(out), &olen, hash, sizeof(hash));
    out[olen] = 0;
    return String((const char *)out);
  }

  void _send(const JsonDocument &doc) {
    String s; serializeJson(doc, s);
    _ws.send(s);
  }

  void _request(const char *type, const char *id) {
    JsonDocument doc;
    doc["op"] = 6;
    doc["d"]["requestType"] = type;
    doc["d"]["requestId"] = id;
    _send(doc);
  }

  void _onMessage(const String &text) {
    JsonDocument doc;
    if (deserializeJson(doc, text)) return;
    int op = doc["op"] | -1;
    JsonObject d = doc["d"];

    if (op == 0) {  // Hello
      JsonDocument id;
      id["op"] = 1;
      id["d"]["rpcVersion"] = 1;
      id["d"]["eventSubscriptions"] = SUB_GENERAL | SUB_SCENES | SUB_UI;
      if (!d["authentication"].isNull()) {
        const char *challenge = d["authentication"]["challenge"] | "";
        const char *salt = d["authentication"]["salt"] | "";
        String secret = _sha256b64(String(_password) + salt);
        id["d"]["authentication"] = _sha256b64(secret + challenge);
      }
      _send(id);
      return;
    }
    if (op == 2) {  // Identified
      _identified = true;
      _pendingSeeds = 3;
      _request("GetStudioModeEnabled", "seed-studio");
      _request("GetCurrentProgramScene", "seed-program");
      _request("GetCurrentPreviewScene", "seed-preview");
      return;
    }
    if (op == 7) {  // RequestResponse
      const char *reqId = d["requestId"] | "";
      bool ok = d["requestStatus"]["result"] | false;
      JsonObject r = d["responseData"];
      if (strcmp(reqId, "seed-studio") == 0 && ok) _setStudio(r["studioModeEnabled"] | false);
      else if (strcmp(reqId, "seed-program") == 0 && ok) _setProgram(r["sceneName"] | r["currentProgramSceneName"] | "");
      else if (strcmp(reqId, "seed-preview") == 0) {
        // Fails (code 506) when Studio Mode is off; that's fine, preview stays empty.
        if (ok) _setPreview(r["sceneName"] | r["currentPreviewSceneName"] | "");
      }
      if (_pendingSeeds && --_pendingSeeds == 0 && !connected) {
        connected = true; _changed = true;
        Serial.printf("OBS: connected. program=\"%s\" preview=\"%s\" studio=%d\n",
                      programScene.c_str(), previewScene.c_str(), studioMode);
      }
      return;
    }
    if (op == 5) {  // Event
      const char *type = d["eventType"] | "";
      JsonObject e = d["eventData"];
      if (strcmp(type, "CurrentProgramSceneChanged") == 0) _setProgram(e["sceneName"] | "");
      else if (strcmp(type, "CurrentPreviewSceneChanged") == 0) _setPreview(e["sceneName"] | "");
      else if (strcmp(type, "StudioModeStateChanged") == 0) {
        _setStudio(e["studioModeEnabled"] | false);
        if (studioMode) _request("GetCurrentPreviewScene", "seed-preview");
      }
      return;
    }
  }

  void _setProgram(const char *s) { if (programScene != s) { programScene = s; _changed = true; } }
  void _setPreview(const char *s) { if (previewScene != s) { previewScene = s; _changed = true; } }
  void _setStudio(bool on) { if (studioMode != on) { studioMode = on; if (!on) previewScene = ""; _changed = true; } }
};
