"""
hub.py - Central Pub/Sub IPC Router
=====================================
The hub implements a topic-based publish/subscribe broker over a
Unix Domain Socket. Publishers and subscribers are fully decoupled:
a publisher does not know who is listening, and a subscriber does
not know who is publishing.

Pub/Sub Model
-------------
                 publish("traffic_light", data)
    main1 ──────────────────────────────────────► HUB
                                                    │
                                         topic registry
                                         "traffic_light" -> [main2, main4]
                                         "vehicle_data"  -> [main4]
                                                    │
                         ┌──────────────────────────┘
                         │  deliver to each subscriber
                         ▼                    ▼
                       main2                main4

Wire Protocol
-------------
Client -> Hub:
    {"cmd": "register",  "name": "traffic_light_sim"}
    {"cmd": "subscribe", "topic": "traffic_light"}
    {"cmd": "publish",   "topic": "traffic_light",  "data": {"state": "RED"}}

Hub -> Client:
    {"ok": true, "name": "traffic_light_sim"}         register ack
    {"ok": true, "subscribed": "traffic_light"}       subscribe ack
    {"topic": "traffic_light", "data": {...},
     "from": "traffic_light_sim"}                     delivered frame

Kernel Syscalls Used
--------------------
    socket()   sys_socket()    Allocate a file descriptor
    bind()     sys_bind()      Bind it to a filesystem path
    listen()   sys_listen()    Mark it as a passive server socket
    accept()   sys_accept()    Accept an incoming client connection
    sendall()  sys_send()      Write bytes into the kernel buffer
    recv()     sys_recv()      Read bytes from the kernel buffer
"""

import socket
import threading
import json
import os

SOCKET_PATH = "/tmp/v2x_test.sock"

# Maps each client name to its connection object.
# Example: {"traffic_light_sim": <socket>, "v2p_camera": <socket>}
clients: dict[str, socket.socket] = {}

# Topic registry: maps topic name to the set of subscriber names.
# Example: {"traffic_light": {"v2p_camera", "adas"}, "vehicle_data": {"adas"}}
topics: dict[str, set[str]] = {}

lock = threading.Lock()


# ──────────────────────────────────────────────────────────────────────────────
def handle_client(conn: socket.socket) -> None:
    """
    Serve one connected client in a dedicated thread.

    Handles three commands:
        register  — store the name -> connection mapping
        subscribe — add this client to a topic's subscriber set
        publish   — fan the message out to every subscriber of the topic

    Parameters
    ----------
    conn : socket.socket
        The accepted connection for this client.
    """
    client_name: str | None = None
    buffer = ""

    try:
        while True:
            # sys_recv(): block until data arrives or connection closes
            data = conn.recv(4096)
            if not data:
                break

            buffer += data.decode("utf-8")

            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue

                msg = json.loads(line)
                cmd = msg.get("cmd")

                # ──────────────────────────────────────────────────────
                # register: remember name -> connection
                # ──────────────────────────────────────────────────────
                if cmd == "register":
                    client_name = msg["name"]
                    with lock:
                        clients[client_name] = conn
                    print(f"[HUB] registered  : {client_name}")
                    _send(conn, {"ok": True, "name": client_name})

                # ──────────────────────────────────────────────────────
                # subscribe: add client to the topic's subscriber set
                # ──────────────────────────────────────────────────────
                elif cmd == "subscribe":
                    topic = msg.get("topic", "")
                    with lock:
                        topics.setdefault(topic, set()).add(client_name)
                    print(f"[HUB] subscribe   : {client_name} -> topic '{topic}'")
                    _send(conn, {"ok": True, "subscribed": topic})

                # ──────────────────────────────────────────────────────
                # publish: fan out to every subscriber of the topic
                # ──────────────────────────────────────────────────────
                elif cmd == "publish":
                    topic = msg.get("topic", "")
                    data  = msg.get("data",  {})

                    with lock:
                        subscribers = set(topics.get(topic, set()))

                    if not subscribers:
                        print(f"[HUB] publish     : '{topic}' — no subscribers")
                        _send(conn, {"ok": True, "delivered_to": 0})
                        continue

                    frame = json.dumps({
                        "topic": topic,
                        "data":  data,
                        "from":  client_name,
                    }) + "\n"

                    dead = set()
                    for subscriber_name in subscribers:
                        with lock:
                            sub_conn = clients.get(subscriber_name)
                        if sub_conn:
                            try:
                                sub_conn.sendall(frame.encode("utf-8"))
                                print(
                                    f"[HUB] deliver     : "
                                    f"'{topic}' -> {subscriber_name}"
                                )
                            except (BrokenPipeError, OSError):
                                dead.add(subscriber_name)
                        else:
                            dead.add(subscriber_name)

                    # Prune any subscribers that have disconnected.
                    if dead:
                        with lock:
                            topics[topic] -= dead

                    _send(conn, {
                        "ok":          True,
                        "delivered_to": len(subscribers) - len(dead),
                    })

    except (ConnectionResetError, BrokenPipeError, json.JSONDecodeError) as exc:
        print(f"[HUB] client error ({client_name}): {exc}")
    finally:
        conn.close()
        if client_name:
            with lock:
                clients.pop(client_name, None)
                # Remove this client from every topic it subscribed to.
                for subscriber_set in topics.values():
                    subscriber_set.discard(client_name)
            print(f"[HUB] disconnected : {client_name}")


# ──────────────────────────────────────────────────────────────────────────────
def _send(conn: socket.socket, obj: dict) -> None:
    """
    Serialize *obj* to a newline-terminated JSON frame and write it.

    sendall() -> sys_send() in a loop — guarantees all bytes are written
    to the kernel buffer even if it was temporarily full.
    """
    try:
        conn.sendall((json.dumps(obj) + "\n").encode("utf-8"))
    except (BrokenPipeError, OSError):
        pass


# ──────────────────────────────────────────────────────────────────────────────
def main() -> None:
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    # sys_socket(): AF_UNIX = filesystem-based, SOCK_STREAM = ordered reliable
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    # sys_bind(): associate the socket with a filesystem path
    server.bind(SOCKET_PATH)

    # sys_listen(): enter passive mode, queue up to 10 pending connections
    server.listen(10)

    os.chmod(SOCKET_PATH, 0o777)

    print(f"[HUB] pub/sub broker started")
    print(f"[HUB] socket : {SOCKET_PATH}")
    print(f"[HUB] waiting for connections ...\n")

    try:
        while True:
            # sys_accept(): block until a client calls connect()
            conn, _ = server.accept()
            thread  = threading.Thread(
                target=handle_client,
                args=(conn,),
                daemon=True,
            )
            thread.start()

    except KeyboardInterrupt:
        print("\n[HUB] shutting down ...")
    finally:
        server.close()
        if os.path.exists(SOCKET_PATH):
            os.remove(SOCKET_PATH)


if __name__ == "__main__":
    main()
