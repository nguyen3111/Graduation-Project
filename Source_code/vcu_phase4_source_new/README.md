# VCU IPC — Phase 4: VCU Software trên Raspberry Pi 4

## Tổng quan kiến trúc

```
┌─────────────────────────────────────────────────────────────────┐
│                    Raspberry Pi 4 (VCU + IPC)                   │
│                                                                 │
│  Thread 1: CanReceiver                                          │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  socket(AF_CAN) → read() [blocking 100ms timeout]       │   │
│  │  → parseFrame() → VehicleData::updateXxx()              │   │
│  │  → notifyDataReady() [Qt QueuedConnection]               │   │
│  └──────────────────────────────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │             VehicleData (Singleton + mutex)              │   │
│  │  EcuSnapshot: speed, batt, gear, inputs, odometer       │   │
│  │  VcuCommand:  motor_enable, pwm, state, fault           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                            │                                    │
│             ┌──────────────┼──────────────┐                     │
│             ▼              ▼              ▼                     │
│  Thread 2: FSM   Thread 3: CanSender   Qt Thread: VcuBridge    │
│  ┌─────────────┐ ┌───────────────────┐ ┌───────────────────┐   │
│  │ StateMachine│ │  write() 0x200    │ │  Q_PROPERTY       │   │
│  │  20ms cycle │ │  mỗi 20ms        │ │  emit signals     │   │
│  │  6 states   │ │                   │ │  → QML binding    │   │
│  └─────────────┘ └───────────────────┘ └───────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                            │ CAN Bus 500kbps
                            │ (MCP2515 + TJA1050)
┌─────────────────────────────────────────────────────────────────┐
│                    STM32F103C8T6 (ECU Node)                     │
│   0x100: speed + accel (20ms)    ← sensors                     │
│   0x101: battery (100ms)         ← ADC                         │
│   0x102: hardware inputs (40ms)  ← GPIO                        │
│   0x103: odometer (40ms)         ← encoder                     │
│   0x200: VCU command ← receive   → motor enable + PWM          │
└─────────────────────────────────────────────────────────────────┘
```

## Cấu trúc file

```
vcu_project/
├── vcu_ipc.pro              Qt6 project file
├── include/
│   ├── vehicle_types_pi.h   Mirror của vehicle_types.h (STM32)
│   ├── vehicle_data.h       Singleton shared data store
│   ├── can_receiver.h       SocketCAN RX thread
│   ├── can_sender.h         SocketCAN TX thread
│   ├── state_machine.h      FSM 6 trạng thái
│   └── vcu_bridge.h         Qt QObject → QML bridge
├── src/
│   ├── main.cpp             Entry point, khởi tạo tất cả
│   ├── vehicle_data.cpp
│   ├── can_receiver.cpp
│   ├── can_sender.cpp
│   ├── state_machine.cpp
│   └── vcu_bridge.cpp
├── qml/
│   ├── qml.qrc
│   └── main.qml             IPC HMI — Dashboard UI
└── scripts/
    └── setup_pi.sh          Cài đặt môi trường
```

## Bước 1: Cài đặt môi trường

```bash
chmod +x scripts/setup_pi.sh
./scripts/setup_pi.sh
sudo reboot
```

Sau reboot:
```bash
# Kiểm tra CAN interface
ip link show can0

# Test nhận frame từ STM32
candump can0
```

Expected output từ candump:
```
can0  100   [4]  XX XX XX XX   # Speed + Accel (20ms)
can0  101   [4]  XX XX XX XX   # Battery (100ms)
can0  102   [4]  XX XX XX XX   # Hardware inputs (40ms)
can0  103   [4]  XX XX XX XX   # Odometer (40ms)
```

## Bước 2: Build

```bash
mkdir build && cd build
qmake6 ../vcu_ipc.pro
make -j4
```

## Bước 3: Chạy

```bash
# Với Waveshare 7 inch qua HDMI (X11)
QT_QPA_PLATFORM=xcb ./vcu_ipc

# Nếu chạy không có X11 (bare framebuffer)
QT_QPA_PLATFORM=eglfs ./vcu_ipc

# Chạy fullscreen
QT_QPA_PLATFORM=xcb ./vcu_ipc -platform xcb -geometry 800x480
```

## Debug

```bash
# Theo dõi CAN bus
candump -ta can0

# Gửi frame giả để test UI (khi không có STM32)
cansend can0 100#20001432    # speed=32.0 km/h, accel=20%, pwm=50%
cansend can0 101#10210064    # batt=8.465V, soc=100%, no warning
cansend can0 102#03010001    # gear=D, mode=NORMAL, key_on
cansend can0 103#E8030000    # odometer=1000m

# Test fault injection
cansend can0 102#03010009    # key_on + fault_inject

# Test charging
cansend can0 102#03010005    # key_on + charging_detect
```

## CAN Protocol (tóm tắt)

| ID    | Hướng        | Period | DLC | Nội dung                         |
|-------|--------------|--------|-----|----------------------------------|
| 0x100 | STM32 → Pi   | 20ms   | 4   | speed_x10[2], accel[1], pwm[1]  |
| 0x101 | STM32 → Pi   | 100ms  | 4   | batt_mv[2], soc[1], flags[1]    |
| 0x102 | STM32 → Pi   | 40ms   | 4   | gear[1], mode[1], inputs[1], r[1]|
| 0x103 | STM32 → Pi   | 40ms   | 4   | odometer_m[2], r[2]             |
| 0x200 | Pi → STM32   | 20ms   | 4   | ctrl[1], pwm[1], state[1], flt[1]|

## State Machine

```
OFF ──(key_on)──► STANDBY ──(brake + gear≠P)──► READY ──(accel≥5%)──► DRIVING
                                                                            │
ANY ──(fault_inject || soc≤3%)──────────────────────────────────► FAULT    │
ANY ──(charging_detect)──────────────────────────────────────── ► CHARGING ◄┘

FAULT: reset bằng key OFF → key ON
CHARGING: thoát khi rút sạc → STANDBY
```

## Drive Mode PWM Limits

| Mode   | PWM tối đa |
|--------|-----------|
| ECO    | 50%       |
| NORMAL | 80%       |
| SPORT  | 100%      |
