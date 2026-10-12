#!/usr/bin/env python3
"""Fake ATEM switcher for bench-testing the Wren base's ATEM client without a real switcher.

Speaks just enough of the ATEM UDP protocol (port 9910): handshake, _pin, TlIn, InCm,
keepalives, and TlIn updates when you type on the keyboard. Written by Wren (2026-10-12) from the
same protocol notes as base/atem_client.h, so it only proves the two ends agree with each other,
NOT that either matches a real ATEM. A real ATEM test still has to happen on Gabe's bench.

Run on a PC on the same Wi-Fi as the base:   python fake_atem.py
Then type:  p2  (program on input 2)   v3 (preview on 3)   o (all off)   q (quit)
"""
import socket, struct, sys, threading, time

PORT = 9910
INPUTS = 4
FLAG_ACK_REQ, FLAG_SYN, FLAG_ACK = 0x01, 0x02, 0x10

tally = [0] * INPUTS          # bit0 program, bit1 preview
state = {"session": 0x8001, "pkt_id": 0, "client": None}
lock = threading.Lock()


def header(flags, length, session, ack_id=0, pkt_id=0):
    b0 = (flags << 3) | ((length >> 8) & 0x07)
    return bytes([b0, length & 0xFF]) + struct.pack(">HHHHH", session, ack_id, 0, 0, pkt_id)


def cmd(name, data):
    return struct.pack(">H", 8 + len(data)) + b"\x00\x00" + name.encode() + data


def send_cmds(sock, cmds):
    payload = b"".join(cmds)
    state["pkt_id"] = (state["pkt_id"] + 1) % 0x8000
    pkt = header(FLAG_ACK_REQ, 12 + len(payload), state["session"], 0, state["pkt_id"]) + payload
    sock.sendto(pkt, state["client"])


def tlin():
    return cmd("TlIn", struct.pack(">H", INPUTS) + bytes(tally))


SOCK = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
SOCK.bind(("0.0.0.0", PORT))
SOCK.settimeout(0.5)


def serve():
    sock = SOCK
    print(f"fake ATEM listening on UDP {PORT}; use this PC's IP as ATEM_HOST in base/main.cpp")
    last_keepalive = time.time()
    while True:
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            data = None
        with lock:
            if data and len(data) >= 12:
                flags = data[0] >> 3
                if flags & FLAG_SYN:
                    # Hello: answer SYN with the client's placeholder session, payload[0] = 0x02 (ok).
                    state["client"] = addr
                    client_session = struct.unpack(">H", data[2:4])[0]
                    reply = header(FLAG_SYN, 20, client_session, 0, 0) + bytes([0x02] + [0] * 7)
                    sock.sendto(reply, addr)
                    print(f"hello from {addr}, sent SYN-ok")
                elif flags & FLAG_ACK:
                    ack_id = struct.unpack(">H", data[4:6])[0]
                    if ack_id == 0 and state["client"] == addr and state["pkt_id"] == 0:
                        # ACK of the hello reply: send the initial state dump under our session id.
                        name = b"Fake ATEM Mini".ljust(40, b"\x00") + bytes([0, 0, 0, 0])
                        send_cmds(sock, [cmd("_pin", name), tlin(), cmd("InCm", b"\x01\x00\x00\x00")])
                        print("client acked hello; sent _pin + TlIn + InCm")
            if state["client"] and state["pkt_id"] and time.time() - last_keepalive > 1.0:
                send_cmds(sock, [])  # empty ACK-request packet as a keepalive
                last_keepalive = time.time()


def main():
    threading.Thread(target=serve, daemon=True).start()
    sock = SOCK  # same socket, so replies to the base come from port 9910
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        with lock:
            c, n = line[0], line[1:]
            if c == "q":
                return
            if c == "o":
                for i in range(INPUTS):
                    tally[i] = 0
            elif c in "pv" and n.isdigit() and 1 <= int(n) <= INPUTS:
                bit = 1 if c == "p" else 2
                for i in range(INPUTS):
                    tally[i] &= ~bit
                tally[int(n) - 1] |= bit
            else:
                print("cmds: pN vN o q")
                continue
            print("tally:", tally)
            if state["client"] and state["pkt_id"]:
                send_cmds(sock, [tlin()])


if __name__ == "__main__":
    main()
