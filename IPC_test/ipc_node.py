"""
ipc_node.py — الـ Client Library المشتركة
==========================================

كل process (main1..main4) بتعمل import من الملف ده.
بدل ما كل واحد يكتب الـ socket code من أوله، هيستخدم
الـ class دي اللي بتعمل كل حاجة.

--- الـ Kernel Syscalls المستخدمة ---
connect() = sys_connect()  - يتوصل بالـ hub
send()    = sys_send()     - يبعت message
recv()    = sys_recv()     - يستقبل messages في background thread
"""

import socket
import threading
import json
import time

SOCKET_PATH = "/tmp/v2x_ipc.sock"


class IPCNode:
    """
    Client بسيط للتواصل مع الـ hub.
    
    مثال:
        node = IPCNode("main1")
        node.connect()
        node.on_message = lambda frm, msg: print(f"{frm}: {msg}")
        node.start_listening()
        node.send("main3", "مرحبا!")
    """

    def __init__(self, name: str):
        self.name   = name
        self.sock   = None
        self.on_message = None   # callback: fn(from_name, msg)

    # ─────────────────────────────────────────────────────
    def connect(self, retries: int = 8) -> bool:
        """
        بيعمل connect() للـ hub ويبعت register.
        بيحاول retries مرة لو الـ hub مش جاهز لسه.
        """
        for attempt in range(1, retries + 1):
            try:
                # ── socket() = sys_socket() ──────────────
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

                # ── connect() = sys_connect() ─────────────
                # kernel بيبعت SYN-equivalent للـ hub server
                self.sock.connect(SOCKET_PATH)

                # بعد الـ connect، بعت الاسم عشان الـ hub يعرفنا
                self._send_raw({"cmd": "register", "name": self.name})
                resp = self._recv_one()

                if resp and resp.get("ok"):
                    print(f"[{self.name}] ✅ connected to hub")
                    return True

            except (FileNotFoundError, ConnectionRefusedError):
                print(f"[{self.name}] hub not ready, retry {attempt}/{retries}...")
                time.sleep(0.5)

        print(f"[{self.name}] ❌ failed to connect")
        return False

    # ─────────────────────────────────────────────────────
    def send(self, to: str, msg: str):
        """
        بيبعت message لـ process معينة بالاسم.
        الـ hub هو اللي بيعمل الـ routing — احنا بس بنقوله "to".
        """
        self._send_raw({"cmd": "send", "to": to, "msg": msg})
        # اقرأ الـ ack بس متوقفش الـ listener thread
        # (الـ ack هيجي على الـ sock ده بس الـ listener thread مش شاغله)

    # ─────────────────────────────────────────────────────
    def start_listening(self):
        """
        يشغل background thread يستقبل messages.
        لما تيجي message، بيطلع on_message callback.
        """
        t = threading.Thread(target=self._recv_loop, daemon=True)
        t.start()

    def _recv_loop(self):
        """الـ loop اللي بيشتغل في background thread."""
        buf = ""
        while True:
            try:
                # ── recv() = sys_recv() ───────────────────
                # blocking — بيستنى لحد ما يجي data
                data = self.sock.recv(1024)
                if not data:
                    print(f"[{self.name}] hub disconnected")
                    break
                buf += data.decode("utf-8")

                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    msg = json.loads(line)

                    # لو في "from" معناها دي رسالة من process تانية
                    if "from" in msg and self.on_message:
                        self.on_message(msg["from"], msg["msg"])
                    # غير كده هي ack أو error من الـ hub، نطبعها
                    elif "error" in msg:
                        print(f"[{self.name}] ⚠️  hub error: {msg['error']}")

            except (ConnectionResetError, OSError):
                break

    # ─────────────────────────────────────────────────────
    def _send_raw(self, obj: dict):
        """بيبعت dict كـ JSON line."""
        self.sock.sendall((json.dumps(obj) + "\n").encode("utf-8"))

    def _recv_one(self, timeout: float = 3.0) -> dict | None:
        """بيستقبل response واحدة بـ timeout."""
        self.sock.settimeout(timeout)
        try:
            buf = b""
            while b"\n" not in buf:
                chunk = self.sock.recv(512)
                if not chunk:
                    return None
                buf += chunk
            self.sock.settimeout(None)   # ارجع blocking
            return json.loads(buf.split(b"\n")[0])
        except (socket.timeout, json.JSONDecodeError):
            return None
