# -*- coding: utf-8 -*-
"""
🚦 Traffic Light Controller + GUI (Scale-Proof Edition)
------------------------------------------------------------
Simulates a traffic light and broadcasts its real-time V2X data every second.
Optimized to fit perfectly on all screens regardless of Windows Display Scaling.
"""

import json
import tkinter as tk
from tkinter import font as tkfont
import paho.mqtt.client as mqtt
import ssl

# ============================================================
# 🌐 MQTT SERVER CONFIGURATION
# ============================================================
BROKER         = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT           = 8883
USERNAME       = "v2n_admin"
PASSWORD       = "V2n@2026!"
TRAFFIC_TOPIC  = "v2n/traffic/light/state"

# ============================================================
# ⏱️ STATE DURATIONS
# ============================================================
GREEN_DURATION  = 15
YELLOW_DURATION = 3
RED_DURATION    = 12

# ============================================================
# ⚙️ V2X NUMERIC TRANSITION CODES (التعديل المطلوب)
# ============================================================
TRANSITION_CODES = {
    ("RED", "YELLOW")    : 1,  
    ("YELLOW", "GREEN")  : 2,  
    ("GREEN", "YELLOW")  : 3,  
    ("YELLOW", "RED")    : 4  
}

# ============================================================
# 🎨 UI COLOR PALETTE
# ============================================================
BG_DARK       = "#1b1f27"
PANEL_DARK    = "#262b35"
BOX_BG        = "#11141a"
RED_ON        = "#ff1e1e"
RED_OFF       = "#4a1414"
YELLOW_ON     = "#ffd400"
YELLOW_OFF    = "#4a3f10"
GREEN_ON      = "#00e676"
GREEN_OFF     = "#0f3d24"
TEXT_COLOR    = "#f2f2f2"
SUBTEXT_COLOR = "#9aa0ac"
ACCENT        = "#0a84ff"


def round_rect(canvas, x1, y1, x2, y2, r=20, **kwargs):
    points = [
        x1 + r, y1, x2 - r, y1, x2, y1, x2, y1 + r,
        x2, y2 - r, x2, y2, x2 - r, y2, x1 + r, y2,
        x1, y2, x1, y2 - r, x1, y1 + r, x1, y1,
    ]
    return canvas.create_polygon(points, smooth=True, **kwargs)


class TrafficLightGUI:
    # التعديل المطلوب: التسلسل الجديد
    SIM_SEQUENCE = [("RED", RED_DURATION),
                    ("YELLOW", YELLOW_DURATION),
                    ("GREEN", GREEN_DURATION),
                    ("YELLOW", YELLOW_DURATION)]

    CANVAS_W   = 150
    CANVAS_H   = 370
    DIAMETER   = 100
    TOP_PAD    = 15
    GAP        = 15

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Traffic Light Controller - V2X")
        
        self.root.geometry("380x780")
        self.root.minsize(360, 720)
        self.root.configure(bg=BG_DARK)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        self.state = "RED"
        self.remaining_time = 0

        self.simulation_active = False
        self.sim_index = 0
        self.sim_after_id = None

        self.mqtt_client = None
        self.mqtt_connected = False

        self._build_ui()
        self._render()
        self._setup_mqtt()          
        self._start_simulation()    

    def _build_ui(self):
        title_font  = tkfont.Font(family="Segoe UI", size=17, weight="bold")
        sub_font    = tkfont.Font(family="Segoe UI", size=10)
        timer_font  = tkfont.Font(family="Segoe UI", size=42, weight="bold")
        unit_font   = tkfont.Font(family="Segoe UI", size=10)
        status_font = tkfont.Font(family="Segoe UI", size=16, weight="bold")
        btn_font    = tkfont.Font(family="Segoe UI", size=11, weight="bold")

        tk.Label(self.root, text="🚦 SMART TRAFFIC LIGHT", font=title_font, bg=BG_DARK, fg=TEXT_COLOR).pack(pady=(15, 2))
        tk.Label(self.root, text="V2X Node Real-time Transmission", font=sub_font, bg=BG_DARK, fg=SUBTEXT_COLOR).pack(pady=(0, 8))

        self.conn_label = tk.Label(self.root, text="🔄 Connecting to MQTT Server...", font=sub_font, bg=BG_DARK, fg="#ff9f0a")
        self.conn_label.pack(pady=(0, 10))

        w, h, d, top, gap = self.CANVAS_W, self.CANVAS_H, self.DIAMETER, self.TOP_PAD, self.GAP
        pad_x = (w - d) // 2

        self.canvas = tk.Canvas(self.root, width=w, height=h, bg=BG_DARK, highlightthickness=0)
        self.canvas.pack(pady=5)

        round_rect(self.canvas, 8, 8, w - 8, h - 8, r=20, fill=BOX_BG, outline="#3a4150", width=2)

        self.circle_red = self.canvas.create_oval(pad_x, top, pad_x + d, top + d, fill=RED_OFF, outline="#000000", width=2)
        self.circle_yellow = self.canvas.create_oval(pad_x, top + d + gap, pad_x + d, top + d + gap + d, fill=YELLOW_OFF, outline="#000000", width=2)
        self.circle_green = self.canvas.create_oval(pad_x, top + 2*(d + gap), pad_x + d, top + 2*(d + gap) + d, fill=GREEN_OFF, outline="#000000", width=2)

        self.status_label = tk.Label(self.root, text="", font=status_font, bg=BG_DARK, fg=TEXT_COLOR)
        self.status_label.pack(pady=(10, 2))

        self.code_label = tk.Label(self.root, text="Active Code: --", font=sub_font, bg=BG_DARK, fg=ACCENT)
        self.code_label.pack(pady=(0, 10))

        timer_frame = tk.Frame(self.root, bg=PANEL_DARK, bd=1, relief="solid")
        timer_frame.pack(pady=5, padx=30, fill="x")

        self.timer_label = tk.Label(timer_frame, text="0", font=timer_font, bg=PANEL_DARK, fg=TEXT_COLOR)
        self.timer_label.pack(pady=(5, 0))
        
        tk.Label(timer_frame, text="seconds left to change state", font=unit_font, bg=PANEL_DARK, fg=SUBTEXT_COLOR).pack(pady=(0, 5))

        self.sim_btn = tk.Button(self.root, text="⏸  Pause Simulation", font=btn_font, bg="#ff3b30", fg="white", relief="flat", padx=16, pady=8, command=self.toggle_simulation)
        self.sim_btn.pack(pady=10)

    def _setup_mqtt(self):
        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.mqtt_client.username_pw_set(USERNAME, PASSWORD)
        self.mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
        self.mqtt_client.on_connect = self._on_mqtt_connect
        self.mqtt_client.on_disconnect = self._on_mqtt_disconnect

        try:
            self.mqtt_client.connect(BROKER, PORT, 60)
            self.mqtt_client.loop_start()   
        except Exception as e:
            self.set_connection_status(False, f"⚠️ Connection Failed: {e}")

    def _on_mqtt_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            self.mqtt_connected = True
            self.set_connection_status(True)
            self._publish_state()        
        else:
            self.mqtt_connected = False
            self.set_connection_status(False, f"⚠️ Auth Failed (code {reason_code})")

    def _on_mqtt_disconnect(self, client, userdata, flags, reason_code=None, properties=None):
        self.mqtt_connected = False
        self.set_connection_status(False, "⚠️ Disconnected - Retrying...")

    def _get_next_state(self):
        # التعديل المطلوب: حساب الحالة القادمة بناءً على التسلسل الجديد
        next_index = (self.sim_index + 1) % len(self.SIM_SEQUENCE)
        return self.SIM_SEQUENCE[next_index][0]

    def _publish_state(self):
        if not self.mqtt_client or not self.mqtt_connected:
            return
        
        next_state = self._get_next_state()
        transition_code = TRANSITION_CODES.get((self.state, next_state), 0)

        self.code_label.config(text=f"Transition: {self.state} ➔ {next_state} | Sent Code: ({transition_code})")

        payload = json.dumps({
            "state": self.state,
            "next_state": next_state,
            "transition_code": transition_code,
            "remaining_time": self.remaining_time 
        })
        try:
            self.mqtt_client.publish(TRAFFIC_TOPIC, payload)
        except Exception as e:
            print(f"⚠️ MQTT publish failed: {e}")

    def set_connection_status(self, connected: bool, text: str = None):
        def _update():
            self.mqtt_connected = connected
            if connected:
                self.conn_label.config(text=text or "● Online - Broadcasting V2X Data", fg=GREEN_ON)
            else:
                self.conn_label.config(text=text or "● Offline - MQTT Disconnected", fg="#ff9f0a")
        self.root.after(0, _update)

    def _apply_update(self, state, remaining_time):
        self.state = (state or "RED").upper()
        try:
            self.remaining_time = int(remaining_time)
        except (TypeError, ValueError):
            self.remaining_time = 0
        self._render()
        self._publish_state()   

    def _render(self):
        is_red = self.state == "RED"
        is_yellow = self.state in ("YELLOW", "AMBER")
        is_green = self.state == "GREEN"

        self.canvas.itemconfig(self.circle_red, fill=RED_ON if is_red else RED_OFF)
        self.canvas.itemconfig(self.circle_yellow, fill=YELLOW_ON if is_yellow else YELLOW_OFF)
        self.canvas.itemconfig(self.circle_green, fill=GREEN_ON if is_green else GREEN_OFF)

        self.timer_label.config(text=str(max(0, int(self.remaining_time))))

        if is_red:
            text, color = "🔴 STOP - RED", RED_ON
        elif is_yellow:
            text, color = "🟡 CAUTION - YELLOW", YELLOW_ON
        elif is_green:
            text, color = "🟢 GO - GREEN", GREEN_ON
        else:
            text, color = f"⚪ UNKNOWN - {self.state}", TEXT_COLOR

        self.status_label.config(text=text, fg=color)

    def toggle_simulation(self):
        if self.simulation_active:
            self._stop_simulation()
        else:
            self._start_simulation()

    def _start_simulation(self):
        self.simulation_active = True
        self.sim_index = 0
        self.sim_btn.config(text="⏸  Pause Simulation", bg="#ff3b30")
        self._run_sim_step()

    def _stop_simulation(self):
        self.simulation_active = False
        self.sim_btn.config(text="▶  Start Simulation", bg="#34c759")
        if self.sim_after_id:
            self.root.after_cancel(self.sim_after_id)
            self.sim_after_id = None

    def _run_sim_step(self):
        if not self.simulation_active:
            return
        state, duration = self.SIM_SEQUENCE[self.sim_index % len(self.SIM_SEQUENCE)]
        self._apply_update(state, duration)
        self._sim_tick(duration)

    def _sim_tick(self, remaining):
        if not self.simulation_active:
            return
        self._apply_update(self.state, remaining)
        if remaining <= 0:
            self.sim_index += 1
            self.root.after(700, self._run_sim_step)
        else:
            self.root.after(1000, lambda: self._sim_tick(remaining - 1))

    def _on_close(self):
        self._stop_simulation()
        try:
            if self.mqtt_client:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
        except Exception:
            pass
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = TrafficLightGUI(root)
    root.mainloop()