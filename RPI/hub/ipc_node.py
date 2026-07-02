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

        # Serializes writes: some clients publish from two threads (MQTT + IPC), and
        # sendall() can split a frame into several writes — without this lock two frames
        # can interleave on the wire and the hub's json.loads() fails.
        self._send_lock = threading.Lock()

        # Bytes read by _recv_one() past the first newline are kept here so the
        # background _recv_loop() starts from them instead of dropping a delivered frame
        # that shared a TCP segment with an ack.
        self._rx_buf = b""

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
        frame = {"cmd": "publish", "topic": topic, "data": data}
        try:
            self._send_raw(frame)
        except (BrokenPipeError, OSError, AttributeError):
            # Hub gone (crashed/restarted). Try one quick reconnect + re-subscribe so a
            # hub restart degrades gracefully (drop this frame) instead of raising
            # BrokenPipeError up into the caller's main loop and killing the process.
            if self.connect(retries=1):
                for t in list(self._callbacks):
                    try:
                        self._send_raw({"cmd": "subscribe", "topic": t})
                    except OSError:
                        pass
                self.start_listening()
                try:
                    self._send_raw(frame)
                except OSError:
                    pass

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

    def _recv_loop(self) -> None:
        """
        Background receive loop.

        Reads raw bytes via sys_recv(), assembles complete JSON lines
        from the stream, and dispatches them to registered callbacks.
        Exits cleanly when the hub closes the connection.
        """
        # Start from any bytes _recv_one() read past the last ack's newline.
        buf = self._rx_buf.decode("utf-8", errors="ignore")
        self._rx_buf = b""
        while True:
            try:
                # sys_recv(): block in kernel until data is available
                raw = self.sock.recv(4096)
                if not raw:
                    print(f"[{self.name}] hub closed the connection")
                    break

                buf += raw.decode("utf-8")

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

            except (ConnectionResetError, OSError):
                break

    def _send_raw(self, obj: dict) -> None:
        """
        Serialize *obj* and write it as a newline-terminated frame.

        sendall() -> sys_send() loop — all bytes are guaranteed to reach
        the kernel buffer. The write is serialized by _send_lock so frames
        from concurrent publisher threads cannot interleave on the wire.
        """
        data = (json.dumps(obj) + "\n").encode("utf-8")
        with self._send_lock:
            self.sock.sendall(data)

    def _recv_one(self, timeout: float = 3.0) -> dict | None:
        """
        Read exactly one JSON frame synchronously (used during connect
        and subscribe to wait for the ack before continuing).

        Any bytes received after the frame's newline are retained in
        self._rx_buf so a frame that shared a TCP segment with the ack is
        not thrown away.
        """
        self.sock.settimeout(timeout)
        try:
            while b"\n" not in self._rx_buf:
                chunk = self.sock.recv(512)
                if not chunk:
                    return None
                self._rx_buf += chunk
            self.sock.settimeout(None)
            line, self._rx_buf = self._rx_buf.split(b"\n", 1)   # keep the remainder
            return json.loads(line)
        except (socket.timeout, json.JSONDecodeError):
            return None
