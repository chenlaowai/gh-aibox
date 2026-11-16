#!/usr/bin/env python3
# Simple TCP pass-through to the ISC host, used only because 10.10.206.68
# is not on the ISC allow-list. Run from the project directory on .25.
import socket, threading, select, sys

LISTEN = ('0.0.0.0', 10443)
TARGET = ('10.10.69.250', 443)

def pipe(a, b):
    try:
        while True:
            r, _, _ = select.select([a, b], [], [], 30)
            if not r:
                break
            for s in r:
                data = s.recv(65536)
                if not data:
                    return
                (b if s is a else a).sendall(data)
    except Exception:
        pass
    finally:
        for s in (a, b):
            try: s.close()
            except Exception: pass

def handle(c):
    try:
        u = socket.create_connection(TARGET, timeout=10)
        u.settimeout(None)
        threading.Thread(target=pipe, args=(c, u), daemon=True).start()
    except Exception as e:
        print('connect failed', e, flush=True)
        try: c.close()
        except Exception: pass

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(LISTEN)
srv.listen(64)
print('isc proxy listening', LISTEN, '->', TARGET, flush=True)
while True:
    c, _ = srv.accept()
    threading.Thread(target=handle, args=(c,), daemon=True).start()
