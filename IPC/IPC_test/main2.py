"""
main2.py — يبعت رسائل لـ main4 بس
=====================================
"""
import time
from ipc_node import IPCNode

node = IPCNode("main2")
node.connect()

def on_msg(frm, msg):
    print(f"[main2] 📩 جاي من {frm}: {msg!r}")

node.on_message = on_msg
node.start_listening()

for i in range(1, 4):
    text = f"hey main4 — message #{i} from main2"
    print(f"[main2] 📤 sending to main4: {text!r}")
    node.send("main4", text)
    time.sleep(2)

print("[main2] ✅ done")
time.sleep(60)
