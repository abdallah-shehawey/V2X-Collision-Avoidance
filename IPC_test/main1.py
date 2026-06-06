"""
main1.py — يبعت رسائل لـ main3 بس
=====================================
"""
import time
from ipc_node import IPCNode

node = IPCNode("main1")
node.connect()

# main1 ممكن يستقبل رسائل كمان (مثلاً ردود من main3)
def on_msg(frm, msg):
    print(f"[main1] 📩 جاي من {frm}: {msg!r}")

node.on_message = on_msg
node.start_listening()

# ابعت 3 رسائل لـ main3 وبس
for i in range(1, 4):
    text = f"مرحبا main3 — رسالة رقم {i}"
    print(f"[main1] 📤 بعتت لـ main3: {text!r}")
    node.send("main3", text)
    time.sleep(2)

print("[main1] ✅ خلصت")
time.sleep(60)   # افضل شغال عشان تستقبل ردود
