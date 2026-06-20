#!/bin/bash
# =============================================================
# install_services.sh
# تثبيت وتفعيل جميع V2X systemd services على الـ Raspberry Pi
# شغّل بصلاحيات sudo:  sudo bash install_services.sh
# =============================================================

set -e  # وقف عند أي خطأ

# ── الألوان للـ output ────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}  V2X Services Installer — Raspberry Pi 5${NC}"
echo -e "${BLUE}=================================================${NC}\n"

# ── التحقق من صلاحيات sudo ───────────────────────────────────
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}❌  يجب تشغيل السكريبت بصلاحيات sudo${NC}"
    echo -e "    الحل:  sudo bash install_services.sh"
    exit 1
fi

# ── المسارات ─────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEMD_DIR="/etc/systemd/system"
V2X_DIR="/home/pi/v2x"
SERVICE_USER="pi"

# ── التحقق من وجود مجلد المشروع ──────────────────────────────
echo -e "${YELLOW}[1/5] التحقق من مجلد المشروع...${NC}"
if [ ! -d "$V2X_DIR" ]; then
    echo -e "${RED}❌  المجلد $V2X_DIR غير موجود!${NC}"
    echo -e "    تأكد إن الأكواد موجودة في:  $V2X_DIR"
    echo -e "    أو عدّل متغير V2X_DIR في هذا السكريبت"
    exit 1
fi

# التحقق من وجود الملفات الأساسية
REQUIRED_FILES=("hub.py" "traffic_light.py" "V2P_TEST.py" "v2v_uart_sender.py" "v2v_uart_receiver.py" "ipc_node.py")
for f in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$V2X_DIR/$f" ]; then
        echo -e "${RED}❌  الملف غير موجود: $V2X_DIR/$f${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✅  مجلد المشروع OK — كل الملفات موجودة${NC}"

# ── إضافة الـ pi user للـ groups المطلوبة ────────────────────
echo -e "\n${YELLOW}[2/5] تعيين الـ groups المطلوبة للـ user '$SERVICE_USER'...${NC}"
usermod -aG video   "$SERVICE_USER" && echo -e "  ${GREEN}✅  video   group → OK${NC}"
usermod -aG dialout "$SERVICE_USER" && echo -e "  ${GREEN}✅  dialout group → OK${NC}"

# ── نسخ الـ service files ─────────────────────────────────────
echo -e "\n${YELLOW}[3/5] نسخ الـ service files إلى $SYSTEMD_DIR ...${NC}"

SERVICES=(
    "v2x-hub.service"
    "v2x-traffic-light.service"
    "v2x-v2p.service"
    "v2x-v2v-sender.service"
    "v2x-v2v-receiver.service"
)

for svc in "${SERVICES[@]}"; do
    SRC="$SCRIPT_DIR/$svc"
    DST="$SYSTEMD_DIR/$svc"
    if [ ! -f "$SRC" ]; then
        echo -e "${RED}❌  Service file غير موجود: $SRC${NC}"
        exit 1
    fi
    cp "$SRC" "$DST"
    chmod 644 "$DST"
    echo -e "  ${GREEN}✅  $svc${NC}"
done

# ── إعادة تحميل systemd ──────────────────────────────────────
echo -e "\n${YELLOW}[4/5] إعادة تحميل systemd daemon...${NC}"
systemctl daemon-reload
echo -e "${GREEN}✅  daemon-reload OK${NC}"

# ── تفعيل وتشغيل الـ services ────────────────────────────────
echo -e "\n${YELLOW}[5/5] تفعيل الـ services...${NC}"

for svc in "${SERVICES[@]}"; do
    systemctl enable "$svc"
    echo -e "  ${GREEN}✅  enabled: $svc${NC}"
done

# ── تشغيل الـ services الآن (بدون reboot) ────────────────────
echo -e "\n${YELLOW}[+] تشغيل الـ services الآن...${NC}"

# Hub أول حاجة
systemctl start v2x-hub.service
sleep 2
echo -e "  ${GREEN}✅  started: v2x-hub${NC}"

# باقي الـ services
for svc in v2x-traffic-light.service v2x-v2v-receiver.service v2x-v2p.service v2x-v2v-sender.service; do
    systemctl start "$svc" || echo -e "  ${YELLOW}⚠️  $svc — check logs${NC}"
    sleep 1
    echo -e "  ${GREEN}✅  started: $svc${NC}"
done

# ── الحالة النهائية ───────────────────────────────────────────
echo -e "\n${BLUE}=================================================${NC}"
echo -e "${BLUE}  الحالة الحالية للـ Services${NC}"
echo -e "${BLUE}=================================================${NC}"

for svc in "${SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$svc" 2>/dev/null || echo "unknown")
    if [ "$STATUS" = "active" ]; then
        echo -e "  ${GREEN}● $svc → active (running)${NC}"
    else
        echo -e "  ${RED}✗ $svc → $STATUS${NC}"
    fi
done

echo -e "\n${GREEN}✅  التثبيت اكتمل!${NC}"
echo -e "\n${YELLOW}📋  أوامر مفيدة:${NC}"
echo -e "  sudo journalctl -u v2x-hub            -f    # لوج الـ hub"
echo -e "  sudo journalctl -u v2x-traffic-light  -f    # لوج الإشارة"
echo -e "  sudo journalctl -u v2x-v2p            -f    # لوج الكاميرا"
echo -e "  sudo journalctl -u v2x-v2v-sender     -f    # لوج الـ sender"
echo -e "  sudo journalctl -u v2x-v2v-receiver   -f    # لوج الـ receiver"
echo -e "  sudo systemctl status v2x-hub               # حالة service معينة"
echo -e "  sudo systemctl restart v2x-hub              # إعادة تشغيل"
echo -e "  sudo systemctl stop v2x-hub                 # إيقاف"
echo -e ""
