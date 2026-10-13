#!/usr/bin/env python3
"""Fake OBS Studio for bench-testing the Wren base's OBS client without installing OBS.

Speaks just enough obs-websocket v5 (port 4455): Hello (with the password challenge if you give
one), Identify/Identified, GetStudioModeEnabled / GetCurrentProgramScene / GetCurrentPreviewScene,
and the CurrentProgramSceneChanged / CurrentPreviewSceneChanged / StudioModeStateChanged events when
you type on the keyboard. Plain Python stdlib: the WebSocket server is hand-rolled (RFC 6455 text
frames, ping/pong, close). Written by Wren (2026-10-13) from the same protocol doc as
base/obs_client.h, so it only proves the two ends agree with each other, NOT that either matches a
real OBS. The real OBS test (README) is still the one that counts.

Run on a PC on the same Wi-Fi as the base:
    python fake_obs.py                 (auth off)
    python fake_obs.py --password abc  (auth on; put the same password on the base's setup page)
Then type:  p2 (program = "Camera 2")   v3 (preview = "Camera 3", only in studio mode)
            s  (toggle studio mode)      o (program = "Blank")   q (quit)
Scene names are "Camera 1".."Camera 4" and "Blank", matching the base's default light mapping.
"""
import base64, hashlib, json, os, socket, struct, sys, threading

PORT = 4455
SCENES = ["Camera 1", "Camera 2", "Camera 3", "Camera 4", "Blank"]
EVT_GENERAL, EVT_SCENES, EVT_UI = 1, 4, 1024
WS_GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

state = {"program": "Camera 1", "preview": "Camera 2", "studio": False}
clients = []        # [(sock, subscriptions)]
lock = threading.Lock()
PASSWORD = None


def b64sha256(s: str) -> str:
    return base64.b64encode(hashlib.sha256(s.encode()).digest()).decode()


# ---- minimal RFC 6455 server side ----
def ws_handshake(sock) -> bool:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            return False
        data += chunk
    head = data.split(b"\r\n\r\n", 1)[0].decode(errors="replace")
    key = None
    for line in head.split("\r\n")[1:]:
        if ":" in line:
            k, v = line.split(":", 1)
            if k.strip().lower() == "sec-websocket-key":
                key = v.strip()
    if not key:
        return False
    accept = base64.b64encode(hashlib.sha1(key.encode() + WS_GUID).digest()).decode()
    sock.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  f"Sec-WebSocket-Accept: {accept}\r\n\r\n").encode())
    return True


def ws_send(sock, text: str, opcode=0x1):
    payload = text.encode() if isinstance(text, str) else text
    n = len(payload)
    if n < 126:
        head = bytes([0x80 | opcode, n])
    elif n < 65536:
        head = bytes([0x80 | opcode, 126]) + struct.pack(">H", n)
    else:
        head = bytes([0x80 | opcode, 127]) + struct.pack(">Q", n)
    sock.sendall(head + payload)


def recv_exact(sock, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    return buf


def ws_recv(sock):
    """Returns (opcode, payload bytes). Client frames must be masked (RFC 6455 5.1)."""
    b0, b1 = recv_exact(sock, 2)
    opcode = b0 & 0x0F
    masked = b1 & 0x80
    n = b1 & 0x7F
    if n == 126:
        n = struct.unpack(">H", recv_exact(sock, 2))[0]
    elif n == 127:
        n = struct.unpack(">Q", recv_exact(sock, 8))[0]
    mask = recv_exact(sock, 4) if masked else b"\x00" * 4
    data = bytes(b ^ mask[i % 4] for i, b in enumerate(recv_exact(sock, n)))
    return opcode, data


# ---- obs-websocket v5 ----
def send_msg(sock, op, d):
    ws_send(sock, json.dumps({"op": op, "d": d}))


def event(sock, subs, kind, etype, data):
    if subs & kind:
        send_msg(sock, 5, {"eventType": etype, "eventIntent": kind, "eventData": data})


def broadcast(kind, etype, data):
    for sock, subs in list(clients):
        try:
            event(sock, subs, kind, etype, data)
        except OSError:
            pass


def handle_request(sock, d):
    rtype, rid = d.get("requestType"), d.get("requestId")
    ok = {"result": True, "code": 100}
    resp = {"requestType": rtype, "requestId": rid, "requestStatus": ok}
    if rtype == "GetStudioModeEnabled":
        resp["responseData"] = {"studioModeEnabled": state["studio"]}
    elif rtype == "GetCurrentProgramScene":
        resp["responseData"] = {"sceneName": state["program"], "currentProgramSceneName": state["program"],
                                "sceneUuid": "fake-" + state["program"]}
    elif rtype == "GetCurrentPreviewScene":
        if state["studio"]:
            resp["responseData"] = {"sceneName": state["preview"], "currentPreviewSceneName": state["preview"],
                                    "sceneUuid": "fake-" + state["preview"]}
        else:
            resp["requestStatus"] = {"result": False, "code": 506, "comment": "Studio mode is not enabled."}
    elif rtype == "GetSceneList":
        resp["responseData"] = {"currentProgramSceneName": state["program"],
                                "currentPreviewSceneName": state["preview"] if state["studio"] else None,
                                "scenes": [{"sceneName": s, "sceneIndex": i} for i, s in enumerate(SCENES)]}
    else:
        resp["requestStatus"] = {"result": False, "code": 204, "comment": f"fake_obs: unknown request {rtype}"}
    send_msg(sock, 7, resp)
    print(f"  request {rtype} -> {resp['requestStatus']['code']}")


def serve_client(sock, addr):
    subs = 0
    try:
        if not ws_handshake(sock):
            return
        hello = {"obsWebSocketVersion": "5.5.2", "rpcVersion": 1}
        salt = challenge = None
        if PASSWORD:
            salt = base64.b64encode(os.urandom(16)).decode()
            challenge = base64.b64encode(os.urandom(16)).decode()
            hello["authentication"] = {"challenge": challenge, "salt": salt}
        send_msg(sock, 0, hello)
        print(f"client {addr}: Hello sent (auth {'on' if PASSWORD else 'off'})")
        identified = False
        while True:
            opcode, data = ws_recv(sock)
            if opcode == 0x8:                       # close
                ws_send(sock, data[:2], 0x8)
                return
            if opcode == 0x9:                       # ping
                ws_send(sock, data, 0xA)
                continue
            if opcode != 0x1:
                continue
            msg = json.loads(data.decode())
            op, d = msg.get("op"), msg.get("d", {})
            if op == 1 and not identified:          # Identify
                if PASSWORD:
                    expect = b64sha256(b64sha256(PASSWORD + salt) + challenge)
                    if d.get("authentication") != expect:
                        print(f"client {addr}: BAD PASSWORD, closing 4009")
                        ws_send(sock, struct.pack(">H", 4009) + b"Authentication failed.", 0x8)
                        return
                subs = int(d.get("eventSubscriptions", 33))
                identified = True
                send_msg(sock, 2, {"negotiatedRpcVersion": 1})
                with lock:
                    clients.append((sock, subs))
                print(f"client {addr}: Identified (subscriptions {subs})")
            elif op == 6 and identified:
                with lock:
                    handle_request(sock, d)
            elif op == 3 and identified:            # Reidentify
                subs = int(d.get("eventSubscriptions", subs))
                send_msg(sock, 2, {"negotiatedRpcVersion": 1})
    except (OSError, ConnectionError, ValueError) as e:
        print(f"client {addr}: gone ({e})")
    finally:
        with lock:
            clients[:] = [c for c in clients if c[0] is not sock]
        sock.close()


def accept_loop(server):
    while True:
        sock, addr = server.accept()
        threading.Thread(target=serve_client, args=(sock, addr), daemon=True).start()


def main():
    global PASSWORD
    if "--password" in sys.argv:
        PASSWORD = sys.argv[sys.argv.index("--password") + 1]
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", PORT))
    server.listen(4)
    threading.Thread(target=accept_loop, args=(server,), daemon=True).start()
    print(f"fake OBS listening on ws://0.0.0.0:{PORT}/ ; on the base's setup page choose OBS and this PC's IP"
          + (f" (password {PASSWORD})" if PASSWORD else " (auth off)"))
    print("cmds: pN (program Camera N) vN (preview Camera N) s (studio mode) o (program Blank) q")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        c, n = line[0], line[1:]
        with lock:
            if c == "q":
                return
            if c == "s":
                state["studio"] = not state["studio"]
                broadcast(EVT_UI, "StudioModeStateChanged", {"studioModeEnabled": state["studio"]})
            elif c == "o":
                state["program"] = "Blank"
                broadcast(EVT_SCENES, "CurrentProgramSceneChanged", {"sceneName": "Blank", "sceneUuid": "fake-Blank"})
            elif c in "pv" and n.isdigit() and 1 <= int(n) <= 4:
                scene = f"Camera {int(n)}"
                if c == "p":
                    state["program"] = scene
                    broadcast(EVT_SCENES, "CurrentProgramSceneChanged", {"sceneName": scene, "sceneUuid": "fake-" + scene})
                elif not state["studio"]:
                    print("preview needs studio mode: type s first")
                    continue
                else:
                    state["preview"] = scene
                    broadcast(EVT_SCENES, "CurrentPreviewSceneChanged", {"sceneName": scene, "sceneUuid": "fake-" + scene})
            else:
                print("cmds: pN vN s o q")
                continue
            print(f"program={state['program']!r} preview={state['preview']!r} studio={state['studio']} clients={len(clients)}")


if __name__ == "__main__":
    main()
