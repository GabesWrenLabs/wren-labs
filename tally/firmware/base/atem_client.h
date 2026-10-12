// Wren Tally base: Blackmagic ATEM tally client (UDP 9910). Clean-room, written by Wren on
// 2026-10-12 from the wire format as documented by two independent projects:
//   - nrkno/sofie-atem-connection, src/lib/atemSocketChild.ts (hello bytes, header, flags, timing)
//   - docs.openswitcher.org (udptransport.html, fields/switcher.html: TlIn / TlSr / PrgI / PrvI)
// No code was copied from either. Not yet run against a real switcher (see README "Status").
//
// Wire format (all multi-byte values big-endian):
//   12-byte header: [0..1] flags(5 bits, top) | length(11 bits, whole datagram incl. header)
//                   [2..3] session id   [4..5] acked packet id (with ACK flag)
//                   [6..7] retransmit-from id   [8..9] unknown   [10..11] packet id
//   flags: 0x01 ACK-request ("reliable"), 0x02 SYN/new session, 0x04 is-retransmit,
//          0x08 retransmit-request, 0x10 ACK-reply.   Stored as (flags << 3) in byte 0.
//   payload: commands, each [u16 len][2 pad][4-char name][len-8 data bytes].
// Handshake: client sends the 20-byte hello (SYN, payload 01 00..); switcher answers SYN with
// payload[0] = 0x02 (ok) or 0x04 (busy/restart); client ACKs that packet id; the switcher then
// streams its full state as ACK-request packets ending with "InCm". Every ACK-request packet is
// ACKed with a 12-byte ACK-reply carrying the switcher's session id and the packet id.
// Silence > ATEM_TIMEOUT_MS -> reconnect.
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

class AtemClient {
 public:
  static const uint8_t MAX_INPUTS = 32;  // tally bits we keep (ATEM Mini has 4, Constellation more)

  // tally[i]: bit0 program, bit1 preview, for ATEM input index i+1 (TlIn is 0-based, inputs 1-based).
  uint8_t tally[MAX_INPUTS];
  uint8_t tallyCount = 0;
  bool connected = false;      // handshake done and InCm seen
  char productName[41] = {0};

  void begin(const char *host, uint16_t port = 9910) {
    _host = host; _port = port;
    memset(tally, 0, sizeof(tally));
    _udp.begin(0);  // any local port
    _restart();
  }

  // Call from loop(). Returns true if the tally changed since the last call.
  bool poll() {
    bool changed = false;
    uint32_t now = millis();

    if (!_synSent || (now - _lastRxMs > ATEM_TIMEOUT_MS)) {
      if (now - _lastTryMs >= ATEM_RETRY_MS) _restart();
      return false;
    }

    int len;
    while ((len = _udp.parsePacket()) > 0) {
      if (len > (int)sizeof(_buf)) { _udp.flush(); continue; }
      _udp.read(_buf, len);
      _lastRxMs = now;
      if (len < 12) continue;

      uint8_t flags = _buf[0] >> 3;
      uint16_t pktLen = ((_buf[0] & 0x07) << 8) | _buf[1];
      uint16_t session = (_buf[2] << 8) | _buf[3];
      uint16_t packetId = (_buf[10] << 8) | _buf[11];
      if (pktLen != len) continue;  // malformed

      if (flags & FLAG_SYN) {
        // Hello reply. payload[0]: 0x02 ok, 0x04 switcher full / restart later.
        if (len >= 13 && _buf[12] == 0x04) { _synSent = false; _lastTryMs = now; continue; }
        _session = session;
        _sendAck(packetId);
        _established = true;
        continue;
      }
      if (!_established) continue;
      _session = session;  // the switcher assigns the real session id after the handshake

      if (flags & FLAG_ACK_REQUEST) _sendAck(packetId);

      // Commands live in the payload after the 12-byte header.
      int pos = 12;
      while (pos + 8 <= len) {
        uint16_t cmdLen = (_buf[pos] << 8) | _buf[pos + 1];
        if (cmdLen < 8 || pos + cmdLen > len) break;
        const char *name = (const char *)&_buf[pos + 4];
        const uint8_t *data = &_buf[pos + 8];
        uint16_t dataLen = cmdLen - 8;
        if (_handle(name, data, dataLen)) changed = true;
        pos += cmdLen;
      }
    }
    return changed;
  }

  // Convenience: state for ATEM input number (1-based). 0 off, 1 program, 2 preview, 3 both.
  uint8_t stateForInput(uint8_t input) const {
    if (input < 1 || input > tallyCount) return 0;
    return tally[input - 1] & 0x03;
  }

 private:
  static const uint8_t FLAG_ACK_REQUEST = 0x01, FLAG_SYN = 0x02, FLAG_RETRANSMIT = 0x04,
                       FLAG_RETRANSMIT_REQ = 0x08, FLAG_ACK = 0x10;
  static const uint32_t ATEM_TIMEOUT_MS = 5000;  // atem-connection uses 5 s
  static const uint32_t ATEM_RETRY_MS = 1000;

  WiFiUDP _udp;
  const char *_host = "";
  uint16_t _port = 9910;
  uint16_t _session = 0;
  bool _synSent = false, _established = false;
  uint32_t _lastRxMs = 0, _lastTryMs = 0;
  uint8_t _buf[1500];

  void _restart() {
    _established = false; connected = false; tallyCount = 0;
    _lastTryMs = millis();
    _lastRxMs = _lastTryMs;
    // The hello packet as sent by atem-connection: SYN, len 20, session 0x53ab, byte 9 = 0x3a,
    // payload 01 00 00 00 00 00 00 00. The session id is a client-chosen placeholder.
    static const uint8_t hello[20] = {0x10, 0x14, 0x53, 0xab, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a,
                                      0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    _session = 0x53ab;
    _udp.beginPacket(_host, _port);
    _udp.write(hello, sizeof(hello));
    _udp.endPacket();
    _synSent = true;
    Serial.printf("ATEM: hello -> %s:%u\n", _host, _port);
  }

  void _sendAck(uint16_t packetId) {
    uint8_t ack[12] = {0};
    ack[0] = (FLAG_ACK << 3) | 0;  // flags | length high bits (12 < 256)
    ack[1] = 12;
    ack[2] = _session >> 8; ack[3] = _session & 0xff;
    ack[4] = packetId >> 8; ack[5] = packetId & 0xff;
    _udp.beginPacket(_host, _port);
    _udp.write(ack, sizeof(ack));
    _udp.endPacket();
  }

  static bool _is(const char *name, const char *want) { return memcmp(name, want, 4) == 0; }

  // Returns true if the tally picture changed.
  bool _handle(const char *name, const uint8_t *d, uint16_t n) {
    if (_is(name, "InCm")) {
      if (!connected) Serial.printf("ATEM: connected (%s), %u tally inputs\n", productName, tallyCount);
      connected = true;
      return true;
    }
    if (_is(name, "_pin")) {
      uint16_t c = n < 40 ? n : 40;
      memcpy(productName, d, c); productName[c] = 0;
      return false;
    }
    if (_is(name, "TlIn")) {
      // [0..1] u16 count, then count bytes: bit0 program, bit1 preview (index 0 = input 1).
      if (n < 2) return false;
      uint16_t count = (d[0] << 8) | d[1];
      if (count > MAX_INPUTS) count = MAX_INPUTS;
      if (2 + count > n) count = n - 2;
      bool changed = (count != tallyCount);
      for (uint16_t i = 0; i < count; i++) {
        uint8_t v = d[2 + i] & 0x03;
        if (tally[i] != v) changed = true;
        tally[i] = v;
      }
      tallyCount = count;
      return changed;
    }
    // TlSr (tally by source id), PrgI / PrvI (bus inputs per M/E) are also sent; TlIn alone is
    // enough for lights 1-4 on an ATEM Mini. Keep PrgI/PrvI for a later "by source" mapping.
    return false;
  }
};
