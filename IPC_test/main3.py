"""
main3.py — يستقبل من main1 (ومش هتجيه حاجة من main2)
=========================================================
"""
import time
from ipc_node import IPCNode

node = IPCNode("main3")
node.connect()

def on_msg(frm, msg):
    print(f"[main3] 📩 وصل من {frm}: {msg!r}")
    # ممكن نرد على main1
    reply = f"وصل! شكراً يا main1 🙏"
    node.send(frm, reply)
    print(f"[main3] 📤 رديت على {frm}")

node.on_message = on_msg
node.start_listening()

print("[main3] 👂 مستنى رسائل...")
time.sleep(60)
