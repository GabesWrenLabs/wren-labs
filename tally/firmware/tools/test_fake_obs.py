#!/usr/bin/env python3
"""Self-test for fake_obs.py: a raw-socket obs-websocket v5 client that does what base/obs_client.h
does (Hello -> Identify with the password hash -> three seed requests). Run fake_obs.py first:
    python fake_obs.py --password abc      (another terminal)
    python test_fake_obs.py abc
Exit code 0 = handshake, auth and the three seed responses all came back as expected."""
import base64, hashlib, json, os, socket, struct, sys

HOST, PORT = "127.0.0.1", 4455
password = sys.argv[1] if len(sys.argv) > 1 else ""


def b64sha256(s):
    return base64.b64encode(hashlib.sha256(s.encode()).digest()).decode()


def send(sock, obj):
    payload = json.dumps(obj).encode()
    mask = os.urandom(4)
    n = len(payload)
    head = bytes([0x81, 0x80 | n]) if n < 126 else bytes([0x81, 0x80 | 126]) + struct.pack(">H", n)
    sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))


def recv(sock):
    def exact(n):
        buf = b""
        while len(buf) < n:
            c = sock.recv(n - len(buf))
            if not c:
                raise ConnectionError("closed")
            buf += c
        return buf
    b0, b1 = exact(2)
    n = b1 & 0x7F
    if n == 126:
        n = struct.unpack(">H", exact(2))[0]
    data = exact(n)
    if (b0 & 0x0F) == 0x8:
        raise ConnectionError(f"server closed: code {struct.unpack('>H', data[:2])[0]} {data[2:].decode()}")
    return json.loads(data.decode())


s = socket.create_connection((HOST, PORT), timeout=5)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall((f"GET / HTTP/1.1\r\nHost: {HOST}:{PORT}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
resp = b""
while b"\r\n\r\n" not in resp:
    resp += s.recv(1024)
assert resp.startswith(b"HTTP/1.1 101"), resp
hello = recv(s)
assert hello["op"] == 0, hello
ident = {"op": 1, "d": {"rpcVersion": 1, "eventSubscriptions": 1 | 4 | 1024}}
if "authentication" in hello["d"]:
    a = hello["d"]["authentication"]
    ident["d"]["authentication"] = b64sha256(b64sha256(password + a["salt"]) + a["challenge"])
send(s, ident)
identified = recv(s)
assert identified["op"] == 2, identified
results = {}
for rt, rid in [("GetStudioModeEnabled", "seed-studio"), ("GetCurrentProgramScene", "seed-program"),
                ("GetCurrentPreviewScene", "seed-preview")]:
    send(s, {"op": 6, "d": {"requestType": rt, "requestId": rid}})
    r = recv(s)
    assert r["op"] == 7 and r["d"]["requestId"] == rid, r
    results[rid] = (r["d"]["requestStatus"]["code"], r["d"].get("responseData"))
print("auth:", "on" if "authentication" in hello["d"] else "off")
for k, v in results.items():
    print(f"{k}: code {v[0]} data {v[1]}")
assert results["seed-studio"][0] == 100 and results["seed-program"][0] == 100
assert results["seed-preview"][0] == 506, "preview should fail with 506 while studio mode is off"
print("PASS")
