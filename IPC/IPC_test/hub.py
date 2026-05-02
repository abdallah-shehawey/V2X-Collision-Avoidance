"""
hub.py — الـ Router المركزي
============================

ده أول process بيشتغل. بيعمل الـ Unix Domain Socket ويستنى
الـ processes تتوصل بيه. لما main1 يبعت "to: main3"، الـ hub
يبعتها لـ main3 بس — مش لـ main2 ولا main4.

--- ليه Unix Domain Socket؟ ---
الـ Unix Domain Socket بيتكلم مع الـ kernel مباشرة عبر:
  socket()   → sys_socket()    - ينشئ file descriptor
  bind()     → sys_bind()      - يربطه بـ path في الـ filesystem
  listen()   → sys_listen()    - يحوله لـ server
  accept()   → sys_accept()    - يقبل connection جديدة
  send()     → sys_send()      - يبعت bytes
  recv()     → sys_recv()      - يستقبل bytes

لو كنا بنستخدم TCP، الـ data بتروح من process → kernel → network stack
→ loopback → kernel → process. Unix Domain بيختصر الرحلة:
process → kernel buffer → process  (أسرع وأبسط)

--- الـ Protocol ---
كل message هي سطر JSON (ينتهي بـ \n):
  {"cmd": "register", "name": "main1"}          - تسجيل الاسم
  {"cmd": "send", "to": "main3", "msg": "Hi!"}  - إرسال
  
Hub بيرد:
  {"ok": true}
  {"from": "main1", "msg": "Hi!"}   (للـ subscriber)
"""

import socket
import threading
import json
import os
import sys

# ─── الإعداد ──────────────────────────────────────────────
SOCKET_PATH = "/tmp/v2x_ipc.sock"

# القاموس ده بيخزن: اسم الـ process → connection object
# مثال: {"main1": <socket>, "main3": <socket>}
clients = {}

# lock عشان الـ dict ده بيتعدّل من threads مختلفة في نفس الوقت
lock = threading.Lock()


# ═══════════════════════════════════════════════════════════
def handle_client(conn):
    """
    دالة بتشتغل في thread منفصل لكل client متصل.
    
    conn = الـ socket object للـ connection دي
    """
    client_name = None   # هنعرفه لما يبعت register
    buffer = ""          # بنجمع فيه البيانات الجاية

    try:
        while True:
            # ── recv() = sys_recv() kernel syscall ──────────────
            # بيستقبل max 1024 byte من الـ kernel buffer
            # لو رجع b"" معناه الـ client قفل الاتصال
            data = conn.recv(1024)
            if not data:
                break   # الـ client قفل → نخرج من الـ loop

            # حوّل bytes → string وضيفها للـ buffer
            buffer += data.decode("utf-8")

            # كل message بتنتهي بـ \n — نعمل parse لما نلاقيها
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue

                # ── parse الـ JSON ───────────────────────────────
                msg = json.loads(line)
                cmd = msg.get("cmd")

                # ─── أمر register ─────────────────────────────
                if cmd == "register":
                    client_name = msg["name"]
                    with lock:
                        clients[client_name] = conn
                    print(f"[HUB] ✅ registered: {client_name}")
                    _send(conn, {"ok": True, "name": client_name})

                # ─── أمر send (routing هنا) ───────────────────
                elif cmd == "send":
                    to_name  = msg.get("to")    # مين المرسل إليه؟
                    msg_body = msg.get("msg")   # إيه الرسالة؟
                    
                    print(f"[HUB] 📨 {client_name} → {to_name}: {msg_body!r}")

                    with lock:
                        dest_conn = clients.get(to_name)  # ابحث عن المستقبل

                    if dest_conn:
                        # ── بعت فقط للـ destination المطلوبة ──
                        _send(dest_conn, {
                            "from": client_name,
                            "to":   to_name,
                            "msg":  msg_body
                        })
                        _send(conn, {"ok": True, "delivered": True})
                    else:
                        # الـ process المطلوبة مش متوصلة
                        _send(conn, {
                            "ok":    False,
                            "error": f"{to_name!r} not connected yet"
                        })

    except (ConnectionResetError, BrokenPipeError, json.JSONDecodeError) as e:
        print(f"[HUB] ⚠️  {client_name}: {e}")
    finally:
        # الـ client بيغل أو بـ crash → نشيله من القاموس
        conn.close()
        if client_name:
            with lock:
                clients.pop(client_name, None)
            print(f"[HUB] 🔌 disconnected: {client_name}")


# ═══════════════════════════════════════════════════════════
def _send(conn, obj: dict):
    """بيبعت JSON + newline. لو فشل بيتجاهل الـ error."""
    try:
        # sendall() = sys_send() - بيضمن إن كل الـ bytes اتبعتت
        conn.sendall((json.dumps(obj) + "\n").encode("utf-8"))
    except (BrokenPipeError, OSError):
        pass


# ═══════════════════════════════════════════════════════════
def main():
    # امسح الـ socket القديم لو كان موجود
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    # ── socket() = sys_socket() ──────────────────────────────
    # AF_UNIX      = Unix Domain (مش network)
    # SOCK_STREAM  = stream مرتب (زي TCP) مش datagrams
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    # ── bind() = sys_bind() ──────────────────────────────────
    # بيربط الـ socket بـ file path في الـ filesystem
    # بعد كده الـ clients يعملوا connect() على المسار ده
    server.bind(SOCKET_PATH)

    # ── listen() = sys_listen() ──────────────────────────────
    # 5 = حجم قائمة الانتظار لو جم clients كتير في نفس الوقت
    server.listen(5)

    # بدّل الصلاحيات عشان أي user على نفس الجهاز يقدر يتوصل
    os.chmod(SOCKET_PATH, 0o777)

    print(f"[HUB] 🚀 running on {SOCKET_PATH}")
    print(f"[HUB] waiting for connections...\n")

    try:
        while True:
            # ── accept() = sys_accept() ──────────────────────
            # blocking — بيستنى لما client يعمل connect()
            # بيرجع (conn, addr) — الـ addr بيبقى "" في Unix sockets
            conn, _ = server.accept()
            print(f"[HUB] 🔗 new connection")

            # كل client في thread منفصل عشان نقدر نستقبل الكل في نفس الوقت
            t = threading.Thread(target=handle_client, args=(conn,), daemon=True)
            t.start()

    except KeyboardInterrupt:
        print("\n[HUB] shutting down...")
    finally:
        server.close()
        if os.path.exists(SOCKET_PATH):
            os.remove(SOCKET_PATH)


if __name__ == "__main__":
    main()
