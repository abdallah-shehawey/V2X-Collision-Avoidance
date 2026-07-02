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

# Maps each client name to (connection, write_lock). The per-connection lock serializes
# every write to that socket — its own acks and pub frames delivered by other threads —
# so concurrent writes can never interleave and corrupt a client's JSON stream.
# Example: {"traffic_light_sim": (<socket>, <Lock>), "v2p_camera": (<socket>, <Lock>)}
clients: dict[str, tuple[socket.socket, threading.Lock]] = {}

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
    # One write-lock for THIS connection, shared with every other thread that may
    # deliver a frame to it (stored alongside the socket in `clients`).
    conn_lock = threading.Lock()

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
                        clients[client_name] = (conn, conn_lock)
                    print(f"[HUB] registered  : {client_name}")
                    _send(conn, {"ok": True, "name": client_name}, conn_lock)

                # ──────────────────────────────────────────────────────
                # subscribe: add client to the topic's subscriber set
                # ──────────────────────────────────────────────────────
                elif cmd == "subscribe":
                    topic = msg.get("topic", "")
                    with lock:
                        topics.setdefault(topic, set()).add(client_name)
                    print(f"[HUB] subscribe   : {client_name} -> topic '{topic}'")
                    _send(conn, {"ok": True, "subscribed": topic}, conn_lock)

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
                        _send(conn, {"ok": True, "delivered_to": 0}, conn_lock)
                        continue

                    frame_bytes = (json.dumps({
                        "topic": topic,
                        "data":  data,
                        "from":  client_name,
                    }) + "\n").encode("utf-8")

                    dead = set()
                    for subscriber_name in subscribers:
                        with lock:
                            entry = clients.get(subscriber_name)
                        if entry:
                            sub_conn, sub_lock = entry
                            try:
                                # Serialize writes to this subscriber's socket so a frame
                                # from another publisher thread cannot interleave with ours.
                                with sub_lock:
                                    sub_conn.sendall(frame_bytes)
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
                    }, conn_lock)

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
def _send(conn: socket.socket, obj: dict,
          wlock: "threading.Lock | None" = None) -> None:
    """
    Serialize *obj* to a newline-terminated JSON frame and write it.

    sendall() -> sys_send() in a loop — guarantees all bytes are written
    to the kernel buffer even if it was temporarily full. When *wlock* (the
    connection's write-lock) is provided, the write is serialized against
    concurrent deliveries to the same socket.
    """
    data = (json.dumps(obj) + "\n").encode("utf-8")
    try:
        if wlock is not None:
            with wlock:
                conn.sendall(data)
        else:
            conn.sendall(data)
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
