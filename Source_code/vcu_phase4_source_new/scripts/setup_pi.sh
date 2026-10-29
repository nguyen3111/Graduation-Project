#!/bin/bash
##############################################################
# setup_pi.sh — Cài đặt môi trường VCU IPC trên Raspberry Pi OS
#
# Chạy 1 lần sau khi flash OS:
#   chmod +x setup_pi.sh
#   ./setup_pi.sh
##############################################################

set -e  # Dừng nếu lệnh nào fail

echo "======================================================"
echo "  VCU IPC — Setup môi trường Raspberry Pi"
echo "======================================================"

# ── 1. Cập nhật package list ───────────────────────────────
echo "[1/6] Cập nhật apt..."
sudo apt update -y

# ── 2. Cài Qt6 và công cụ build ───────────────────────────
echo "[2/6] Cài Qt6..."
sudo apt install -y \
    qt6-base-dev       \
    qt6-declarative-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts  \
    cmake              \
    build-essential

# ── 3. Cài can-utils và SocketCAN tools ───────────────────
echo "[3/6] Cài can-utils..."
sudo apt install -y can-utils

# ── 4. Cấu hình MCP2515 trong /boot/config.txt ───────────
echo "[4/6] Cấu hình MCP2515..."
CONFIG="/boot/config.txt"

# Kiểm tra nếu chưa có dòng cấu hình
if ! grep -q "mcp2515-can0" "$CONFIG"; then
    echo "" | sudo tee -a "$CONFIG"
    echo "# MCP2515 CAN controller (8MHz crystal, INT=GPIO25)" | sudo tee -a "$CONFIG"
    echo "dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25" | sudo tee -a "$CONFIG"
    echo "dtoverlay=spi-bcm2835-overlay" | sudo tee -a "$CONFIG"
    echo "[MCP2515] Config thêm vào $CONFIG — reboot cần thiết"
else
    echo "[MCP2515] Đã có trong config — bỏ qua"
fi

# ── 5. Tạo systemd service để bring-up can0 tự động ───────
echo "[5/6] Tạo can0-up.service..."
sudo tee /etc/systemd/system/can0-up.service > /dev/null << 'EOF'
[Unit]
Description=Bring up CAN0 interface at 500kbps
After=network.target

[Service]
Type=oneshot
ExecStart=/sbin/ip link set can0 up type can bitrate 500000
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable can0-up.service
echo "[can0-up] Service enabled — sẽ bring up can0 khi boot"

# ── 6. Tạo thư mục build ──────────────────────────────────
echo "[6/6] Tạo thư mục build..."
mkdir -p build

echo ""
echo "======================================================"
echo "  Setup hoàn tất!"
echo ""
echo "  BƯỚC TIẾP THEO:"
echo "  1. Reboot: sudo reboot"
echo "  2. Sau reboot, kiểm tra CAN: ip link show can0"
echo "  3. Build project:"
echo "       cd build"
echo "       qmake6 ../vcu_ipc.pro"
echo "       make -j4"
echo "  4. Test CAN bus: candump can0"
echo "  5. Chạy app: QT_QPA_PLATFORM=xcb ./vcu_ipc"
echo "======================================================"
