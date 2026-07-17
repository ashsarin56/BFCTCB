#!/usr/bin/env python3
# echo backend for testing
import sys
import socket
import threading


def handle_client(conn):
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            conn.sendall(b"ECHO:" + data)
    finally:
        conn.close()


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <port>", file=sys.stderr)
        sys.exit(1)

    port = int(sys.argv[1])
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(128)
    print(f"Echo backend listening on port {port}")

    while True:
        conn, addr = srv.accept()
        threading.Thread(target=handle_client, args=(conn,), daemon=True).start()


if __name__ == "__main__":
    main()
