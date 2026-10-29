##############################################################
# vcu_ipc.pro — Qt6 project file cho VCU + IPC trên Pi 4
#
# Build:
#   qmake vcu_ipc.pro
#   make -j4
#
# Yêu cầu:
#   Qt6 Core, Gui, Quick, Qml, Concurrent
#   SocketCAN (linux/can.h) — sẵn có trên Raspberry Pi OS
##############################################################

QT += core gui quick qml concurrent

# C++17 cho std::mutex, std::atomic, std::thread
CONFIG += c++17

# Tên binary output
TARGET = vcu_ipc

# Không dùng app bundle (chỉ cần trên macOS)
CONFIG -= app_bundle

# ── Source files ──────────────────────────────────────────
SOURCES += \
    src/main.cpp            \
    src/vehicle_data.cpp    \
    src/can_receiver.cpp    \
    src/can_sender.cpp      \
    src/state_machine.cpp   \
    src/vcu_bridge.cpp

# ── Header files ──────────────────────────────────────────
HEADERS += \
    include/vehicle_types_pi.h  \
    include/vehicle_data.h      \
    include/can_receiver.h      \
    include/can_sender.h        \
    include/state_machine.h     \
    include/vcu_bridge.h

# ── QML resources ─────────────────────────────────────────
RESOURCES += qml/qml.qrc

# ── Include path ──────────────────────────────────────────
INCLUDEPATH += include

# ── Compiler warnings — MISRA-like strictness ─────────────
QMAKE_CXXFLAGS += -Wall -Wextra -Wshadow

# ── SocketCAN — chỉ build trên Linux ──────────────────────
linux {
    # Không cần link thêm library — AF_CAN là built-in kernel
    QMAKE_CXXFLAGS += -DPLATFORM_LINUX
}
