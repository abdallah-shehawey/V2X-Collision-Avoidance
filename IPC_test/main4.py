"""
main4.py — يستقبل من main2 (ومش هتجيه حاجة من main1)
=========================================================
"""
import time
from ipc_node import IPCNode

node = IPCNode("main4")
node.connect()

def on_msg(frm, msg):
    print(f"[main4] 📩 received from {frm}: {msg!r}")

node.on_message = on_msg
node.start_listening()

print("[main4] 👂 listening...")
time.sleep(60)
