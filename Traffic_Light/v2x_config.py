# -*- coding: utf-8 -*-
"""
Shared V2X configuration — single source of truth for every Traffic_Light MQTT node
(Intelligent_Gateway.py, Distance.py, dis&AI_camera.py, Traffic_light_GUI.py).

Values can be overridden through environment variables; the defaults keep the demo
working out of the box. SECURITY NOTE: the HiveMQ password lived in git history as a
plain literal — rotate it on HiveMQ and prefer setting V2X_MQTT_PASSWORD in the
environment (e.g. from a .env / secrets file) instead of relying on the default below.
"""

import os

# ------------------------------------------------------------
# MQTT broker (HiveMQ Cloud)
# ------------------------------------------------------------
BROKER   = os.environ.get("V2X_MQTT_BROKER",
                          "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud")
PORT     = int(os.environ.get("V2X_MQTT_PORT", "8883"))
USERNAME = os.environ.get("V2X_MQTT_USER", "v2n_admin")
PASSWORD = os.environ.get("V2X_MQTT_PASSWORD", "V2n@2026!")

# ------------------------------------------------------------
# Prioritized-vehicle identity — SINGLE source of truth.
# The cameras tag this plate as the ambulance and the Gateway matches against the
# same constant, so the two must never drift apart again (was "REX" vs "T4RR").
# ------------------------------------------------------------
AMBULANCE_ID = os.environ.get("V2X_AMBULANCE_ID", "T4RR")
