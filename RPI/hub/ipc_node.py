"""
ipc_node.py - Pub/Sub IPC Client Library
==========================================
Every process imports this module to participate in the pub/sub system.
IPCNode wraps the kernel syscalls each client needs and exposes two
high-level primitives:

    publish(topic, data)    Broadcast a message to all subscribers of a topic.
    subscribe(topic, cb)    Register a callback for messages on a topic.

The hub handles all fan-out logic; clients only declare intent.

Wire Protocol
-------------
Outgoing (client -> hub):
    {"cmd": "register",  "name": "<name>"}
    {"cmd": "subscribe", "topic": "<topic>"}
    {"cmd": "publish",   "topic": "<topic>", "data": <dict>}

Incoming (hub -> client):
    {"ok": true,  "name": "<name>"}                  register ack
    {"ok": true,  "subscribed": "<topic>"}            subscribe ack
    {"topic": "<topic>", "data": <dict>,
     "from": "<publisher_name>"}                      delivered frame

Kernel Syscalls
---------------
    connect()   sys_connect()    Establish the connection to the hub
    sendall()   sys_send()       Write a framed message (guaranteed delivery)
    recv()      sys_recv()       Block until bytes arrive (background thread)

Usage
-----
    node = IPCNode("v2p_camera")
    node.connect()

    def on_traffic(topic, data, sender):
        print(f"[{topic}] state={data['state']} from {sender}")

    node.subscribe("traffic_light", on_traffic)
    node.start_listening()          # non-blocking — starts a daemon thread

    # Publisher side:
    node.publish("vehicle_data", {"speed_kmh": 45.0, "brake": False})
"""

import socket
import threading
import json
import time

SOCKET_PATH = "/tmp/v2x_test.sock"


class IPCNode:
    """
    Pub/Sub IPC client for communicating with the central hub.

    Attributes
    ----------
    name : str
        Unique name this process registers under.
    """

    def __init__(self, name: str) -> None:
        self.name = name
        self.sock = None

        # Maps topic -> list of callbacks registered for that topic.
        # Multiple callbacks per topic are supported.
        self._callbacks: dict[str, list] = {}

        # Guards writes to the socket: sendall() is not atomic across threads,
        # so any node that ever calls publish()/subscribe() from more than one
        # thread (e.g. a callback thread alongside the main thread) can
        # otherwise interleave partial frames on the wire.
        self._send_lock = threading.Lock()

        # Bytes read past the first frame during the synchronous _recv_one()
        # calls (connect/subscribe acks), carried over so _recv_loop() doesn't
        # silently lose a message that arrived in the same recv() chunk.
        self._leftover = b""

    # ──────────────────────────────────────────────────────────────────────
    def connect(self, retries: int = 8) -> bool:
        """
        Open the Unix Domain Socket connection to the hub and register.

        Retries up to *retries* times with a short delay to tolerate the
        hub not yet being ready when this process starts.

        Returns True on success, False if all attempts fail.

        Syscall sequence
        ----------------
            socket(AF_UNIX, SOCK_STREAM, 0)   allocate a file descriptor
            connect(fd, SOCKET_PATH)          ask the kernel to complete
                                              the handshake with the hub
            sendall(fd, register_frame)       identify ourselves
            recv(fd, buf, 512)               wait for the ack
        """
        for attempt in range(1, retries + 1):
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(SOCKET_PATH)

                self._send_raw({"cmd": "register", "name": self.name})
                resp = self._recv_one()

                if resp and resp.get("ok"):
                    print(f"[{self.name}] connected to hub")
                    return True

            except (FileNotFoundError, ConnectionRefusedError):
                print(f"[{self.name}] hub not ready — retry {attempt}/{retries}")
                time.sleep(0.5)

        print(f"[{self.name}] ERROR: could not connect after {retries} attempts")
        return False

    # ──────────────────────────────────────────────────────────────────────
    def subscribe(self, topic: str, callback) -> None:
        """
        Subscribe to *topic* and register *callback* for incoming messages.

        The hub adds this client to the topic's subscriber set. Every time
        any process publishes to *topic*, the hub delivers the frame here
        and the background thread fires *callback*.

        Multiple calls with the same topic register multiple callbacks;
        all of them will be invoked in registration order.

        Parameters
        ----------
        topic    : str
            Name of the topic to subscribe to (e.g. "traffic_light").
        callback : callable
            Function with signature: callback(topic: str, data: dict,
            sender: str) -> None
        """
        self._callbacks.setdefault(topic, []).append(callback)
        self._send_raw({"cmd": "subscribe", "topic": topic})

        # Read the subscribe ack synchronously so the subscription is
        # confirmed before the caller proceeds.
        ack = self._recv_one()
        if ack and ack.get("ok"):
            print(f"[{self.name}] subscribed to '{topic}'")
        else:
            print(f"[{self.name}] WARNING: unexpected subscribe response: {ack}")

    # ──────────────────────────────────────────────────────────────────────
    def publish(self, topic: str, data: dict) -> None:
        """
        Publish *data* to *topic*.

        The hub immediately fans the message out to every subscriber of
        *topic*. This call is fire-and-forget from the publisher's
        perspective — delivery confirmation is logged but not returned.

        Parameters
        ----------
        topic : str    Name of the topic (e.g. "vehicle_data").
        data  : dict   Payload — any JSON-serializable dictionary.
        """
        self._send_raw({"cmd": "publish", "topic": topic, "data": data})

    # ──────────────────────────────────────────────────────────────────────
    def start_listening(self) -> None:
        """
        Start a daemon thread that continuously reads from the socket.

        The thread invokes the appropriate callbacks for every incoming
        message frame and logs hub-level errors. Does not block the
        calling thread.
        """
        thread = threading.Thread(target=self._recv_loop, daemon=True)
        thread.start()

    # ──────────────────────────────────────────────────────────────────────
    # Internal helpers
    # ──────────────────────────────────────────────────────────────────────

    def _dispatch_lines(self, buf: str) -> str:
        """
        Parses and dispatches every complete newline-terminated JSON frame
        currently in *buf*, returning whatever incomplete tail remains.
        Shared by _recv_loop() and the leftover bytes _recv_one() may have
        already read past the handshake ack, so neither path can silently
        drop a frame that arrived in the same read as the previous one.
        """
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue

            frame = json.loads(line)

            # Delivered pub/sub message from the hub.
            if "topic" in frame:
                topic  = frame["topic"]
                data   = frame["data"]
                sender = frame.get("from", "unknown")

                for cb in self._callbacks.get(topic, []):
                    try:
                        cb(topic, data, sender)
                    except Exception as exc:
                        print(
                            f"[{self.name}] callback error "
                            f"on topic '{topic}': {exc}"
                        )

            # Ack or error from the hub (publish confirmation, etc.)
            elif not frame.get("ok") and "error" in frame:
                print(f"[{self.name}] hub error: {frame['error']}")
        return buf

    def _recv_loop(self) -> None:
        """
        Background receive loop.

        Reads raw bytes via sys_recv(), assembles complete JSON lines
        from the stream, and dispatches them to registered callbacks.
        Exits cleanly when the hub closes the connection.
        """
        # Anything _recv_one() already read past the connect/subscribe ack's
        # newline belongs to this stream too - dispatch it before blocking
        # on the next recv(), instead of discarding it.
        buf = self._leftover.decode("utf-8", errors="ignore")
        self._leftover = b""
        buf = self._dispatch_lines(buf)

        while True:
            try:
                # sys_recv(): block in kernel until data is available
                raw = self.sock.recv(4096)
                if not raw:
                    print(f"[{self.name}] hub closed the connection")
                    break

                buf += raw.decode("utf-8")
                buf = self._dispatch_lines(buf)

            except (ConnectionResetError, OSError):
                break

    def _send_raw(self, obj: dict) -> None:
        """
        Serialize *obj* and write it as a newline-terminated frame.

        sendall() -> sys_send() loop — all bytes are guaranteed to reach
        the kernel buffer. Guarded by _send_lock since sendall() is not
        atomic across concurrent threads on the same socket.
        """
        data = (json.dumps(obj) + "\n").encode("utf-8")
        with self._send_lock:
            self.sock.sendall(data)

    def _recv_one(self, timeout: float = 3.0) -> dict | None:
        """
        Read exactly one JSON frame synchronously (used during connect
        and subscribe to wait for the ack before continuing). Any bytes
        read past the first frame's newline are stashed in self._leftover
        instead of discarded, since a fast-arriving pub/sub message can
        share a recv() chunk with the ack.
        """
        self.sock.settimeout(timeout)
        try:
            buf = self._leftover
            self._leftover = b""
            while b"\n" not in buf:
                chunk = self.sock.recv(512)
                if not chunk:
                    return None
                buf += chunk
            self.sock.settimeout(None)
            line, rest = buf.split(b"\n", 1)
            self._leftover = rest
            return json.loads(line)
        except (socket.timeout, json.JSONDecodeError):
            return None
