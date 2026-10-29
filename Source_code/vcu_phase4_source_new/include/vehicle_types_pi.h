/**
 * @file    vehicle_types_pi.h
 * @brief   Mirror của vehicle_types.h (STM32) cho phía Raspberry Pi VCU
 *
 * QUAN TRỌNG: File này phải được giữ đồng bộ với vehicle_types.h
 * trên STM32. Mọi thay đổi CAN format ở STM32 phải được cập nhật
 * đồng thời tại đây.
 *
 * CAN Protocol (500kbps, Standard Frame 11-bit ID):
 *
 *  STM32 → Pi (sensor data):
 *    0x100 (20ms)  : speed_x10[2], accel_pct[1], motor_pwm_pct[1]
 *    0x101 (100ms) : batt_mv[2], soc_pct[1], batt_flags[1]
 *    0x102 (40ms)  : gear[1], drive_mode[1], input_flags[1], rsv[1]
 *    0x103 (40ms)  : odometer_m[2], rsv[2]
 *
 *  Pi → STM32 (VCU command):
 *    0x200 (20ms)  : ctrl_flags[1], pwm_pct[1], vehicle_state[1], fault_flags[1]
 *
 * @note Dùng #pragma pack hoặc __attribute__((packed)) để đảm bảo
 *       layout byte chính xác khi memcpy từ CAN data buffer.
 *
 * @author  [Tên bạn]
 * @date    2025
 */

#ifndef VEHICLE_TYPES_PI_H
#define VEHICLE_TYPES_PI_H

#include <cstdint>

/* ===========================================================
 * SECTION 1: ENUMERATIONS — giống hệt STM32 side
 * =========================================================== */

/**
 * @brief Trạng thái xe — 6 states, encode vào CAN byte
 */
enum class VehicleState : uint8_t
{
    OFF      = 0U,
    STANDBY  = 1U,
    READY    = 2U,
    DRIVING  = 3U,
    CHARGING = 4U,
    FAULT    = 5U
};

/**
 * @brief Vị trí cần số
 */
enum class GearPosition : uint8_t
{
    P = 0U,   /* Park    */
    R = 1U,   /* Reverse */
    N = 2U,   /* Neutral */
    D = 3U    /* Drive   */
};

/**
 * @brief Chế độ lái
 */
enum class DriveMode : uint8_t
{
    ECO    = 0U,
    NORMAL = 1U,
    SPORT  = 2U
};

/**
 * @brief Fault flags — bitmask, có thể OR nhiều lỗi
 */
enum class FaultFlags : uint8_t
{
    NONE         = 0x00U,
    OVERCURRENT  = 0x01U,
    OVERVOLTAGE  = 0x02U,
    UNDERVOLTAGE = 0x04U,
    OVERTEMP     = 0x08U,
    MOTOR_STALL  = 0x10U,
    CAN_TIMEOUT  = 0x20U,
    INJECTED     = 0x40U
};

/* ===========================================================
 * SECTION 2: CAN MESSAGE STRUCTS (packed — map vào CAN bytes)
 *
 * LUẬT: Mọi struct này dùng __attribute__((packed)) để đảm bảo
 * compiler không chèn padding giữa các field. Bắt buộc khi
 * memcpy trực tiếp từ can_frame.data[].
 * =========================================================== */

/**
 * @brief CAN 0x100 — Speed + Accel (20ms), DLC = 4 bytes
 *
 * Byte layout:
 *   [0..1] speed_x10  : speed_kmh * 10, uint16_t little-endian
 *   [2]    accel_pct  : 0–100%
 *   [3]    motor_pwm_pct : 0–100%
 */
struct __attribute__((packed)) CanMsg100_t
{
    uint16_t speed_x10;       /* speed_kmh × 10 để tránh float */
    uint8_t  accel_pct;       /* 0–100 */
    uint8_t  motor_pwm_pct;   /* 0–100 */
};
static_assert(sizeof(CanMsg100_t) == 4U, "CanMsg100_t size mismatch");

/**
 * @brief CAN 0x101 — Battery (100ms), DLC = 4 bytes
 *
 * Byte layout:
 *   [0..1] batt_mv   : điện áp mV, uint16_t
 *   [2]    soc_pct   : 0–100%
 *   [3]    batt_flags: bit0 = batt_low_warning
 */
struct __attribute__((packed)) CanMsg101_t
{
    uint16_t batt_mv;
    uint8_t  soc_pct;
    uint8_t  batt_flags;   /* bit0 = low warning */
};
static_assert(sizeof(CanMsg101_t) == 4U, "CanMsg101_t size mismatch");

/**
 * @brief CAN 0x102 — Hardware inputs (40ms), DLC = 4 bytes
 *
 * Byte layout:
 *   [0] gear        : GearPosition (0=P,1=R,2=N,3=D)
 *   [1] drive_mode  : DriveMode (0=ECO,1=NORMAL,2=SPORT)
 *   [2] input_flags : bit0=key_on, bit1=brake, bit2=charging, bit3=fault_inject
 *   [3] reserved
 */
struct __attribute__((packed)) CanMsg102_t
{
    uint8_t  gear;
    uint8_t  drive_mode;
    uint8_t  input_flags;  /* bit0=key, bit1=brake, bit2=chg, bit3=fault */
    uint8_t  reserved;
};
static_assert(sizeof(CanMsg102_t) == 4U, "CanMsg102_t size mismatch");

/**
 * @brief CAN 0x103 — Odometer (40ms), DLC = 4 bytes
 */
struct __attribute__((packed)) CanMsg103_t
{
    uint16_t odometer_m;   /* Quãng đường mét (truncate 16-bit) */
    uint8_t  reserved0;
    uint8_t  reserved1;
};
static_assert(sizeof(CanMsg103_t) == 4U, "CanMsg103_t size mismatch");

/**
 * @brief CAN 0x200 — VCU Command → STM32 (20ms), DLC = 4 bytes
 *
 * Byte layout:
 *   [0] ctrl_flags   : bit0=motor_enable, bit1=direction_fwd
 *   [1] pwm_target   : 0–100%
 *   [2] vehicle_state: VehicleState enum
 *   [3] fault_flags  : FaultFlags bitmask
 */
struct __attribute__((packed)) CanMsg200_t
{
    uint8_t ctrl_flags;      /* bit0=enable, bit1=fwd */
    uint8_t pwm_target_pct;  /* 0–100 */
    uint8_t vehicle_state;   /* VehicleState */
    uint8_t fault_flags;     /* FaultFlags   */
};
static_assert(sizeof(CanMsg200_t) == 4U, "CanMsg200_t size mismatch");

/* ===========================================================
 * SECTION 3: INPUT FLAG BIT POSITIONS (cho CanMsg102_t)
 * =========================================================== */
static constexpr uint8_t INPUT_FLAG_KEY_ON   = 0x01U; /* bit 0 */
static constexpr uint8_t INPUT_FLAG_BRAKE    = 0x02U; /* bit 1 */
static constexpr uint8_t INPUT_FLAG_CHARGING = 0x04U; /* bit 2 */
static constexpr uint8_t INPUT_FLAG_FAULT    = 0x08U; /* bit 3 */

/* ===========================================================
 * SECTION 4: CAN IDs
 * =========================================================== */
static constexpr uint32_t CAN_ID_SPEED_ACCEL  = 0x100U;
static constexpr uint32_t CAN_ID_BATTERY      = 0x101U;
static constexpr uint32_t CAN_ID_HW_INPUTS    = 0x102U;
static constexpr uint32_t CAN_ID_ODOMETER     = 0x103U;
static constexpr uint32_t CAN_ID_VCU_COMMAND  = 0x200U;

/* ===========================================================
 * SECTION 5: SAFETY THRESHOLDS
 * =========================================================== */
static constexpr uint8_t  SOC_LOW_THRESHOLD    = 10U;   /* % → cảnh báo */
static constexpr uint8_t  SOC_FAULT_THRESHOLD  = 3U;    /* % → FAULT    */
static constexpr uint32_t VCU_TX_PERIOD_MS     = 20U;   /* ms           */
static constexpr uint32_t VCU_WATCHDOG_MS      = 2000U; /* ms timeout   */

#endif /* VEHICLE_TYPES_PI_H */
