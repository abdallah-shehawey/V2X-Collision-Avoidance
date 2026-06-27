#!/bin/bash
echo "========================================"
echo "V2P Project "
echo "========================================"
sudo apt update -y
sudo apt install python3-pip -y
sudo apt install python3-picamera2 -y
pip install --break-system-packages -r requirements.txt

echo ": python3 v2p_camera.py"