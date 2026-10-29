/**
 * @file    vehicle_types.h
 * @brief   Shared data types, enums, and structs for EV ECU firmware
 *
 * Kiến trúc phân tán:
 *   STM32 (ECU Node) — thu thập sensor, thực thi lệnh motor
 *   RPi   (VCU)      — chạy state machine, ra lệnh điều khiển
 *
 * Luồng dữ liệu:
 *   STM32 → CAN 0x100~0x103 → Pi  (sensor data, hardware inputs)
 *   Pi    → CAN 0x200        → STM32 (VCU command: motor enable/pwm)
 *
 * @note  MISRA C:2012 compliant (basic rules)
 */

#ifndef VEHICLE_TYPES_H
#define VEHICLE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * SECTION 1: ENUMERATIONS
 * ========================================================= */

/**
 * @brief Trạng thái xe — encode để gửi qua CAN
 *        State machine thực sự chạy trên VCU (Raspberry Pi)
 */
typedef enum
{
    VEHICLE_STATE_OFF       = 0U,
    VEHICLE_STATE_STANDBY   = 1U,
    VEHICLE_STATE_READY     = 2U,
    VEHICLE_STATE_DRIVING   = 3U,
    VEHICLE_STATE_CHARGING  = 4U,
    VEHICLE_STATE_FAULT     = 5U
} VehicleState_t;

/**
 * @brief Vị trí cần số
 */
typedef enum
{
    GEAR_P = 0U,   /* Park    */
    GEAR_R = 1U,   /* Reverse */
    GEAR_N = 2U,   /* Neutral */
    GEAR_D = 3U    /* Drive   */
} GearPosition_t;

/**
 * @brief Chế độ lái
 */
typedef enum
{
    DRIVE_MODE_ECO    = 0U,
    DRIVE_MODE_NORMAL = 1U,
    DRIVE_MODE_SPORT  = 2U
} DriveMode_t;

/**
 * @brief Fault flags — bitmask, có thể set nhiều lỗi cùng lúc
 */
typedef enum
{
    FAULT_NONE         = 0x00U,
    FAULT_OVERCURRENT  = 0x01U,
    FAULT_OVERVOLTAGE  = 0x02U,
    FAULT_UNDERVOLTAGE = 0x04U,
    FAULT_OVERTEMP     = 0x08U,
    FAULT_MOTOR_STALL  = 0x10U,
    FAULT_CAN_TIMEOUT  = 0x20U,
    FAULT_INJECTED     = 0x40U
} FaultFlags_t;

/* =========================================================
 * SECTION 2: HARDWARE INPUT STRUCT
 * Đầu ra của InputManager — đi qua xInputQueue nội bộ STM32
 * ========================================================= */

/**
 * @brief Snapshot phần cứng tại 1 thời điểm — InputScan tạo mỗi 10ms
 */
typedef struct
{
    uint16_t        batt_raw;         /* ADC raw 0-4095, PA2              */
    uint8_t         accel_pct;        /* 0-100%, đã tính sẵn từ ADC raw  */
    bool            key_on;           /* PB13, đã debounce                */
    bool            brake_pressed;    /* PB12, đã debounce                */
    bool            charging_detect;  /* PB14, đã debounce                */
    bool            fault_inject;     /* PB15, đã debounce                */
    GearPosition_t  gear;             /* Cần số hiện tại                  */
    DriveMode_t     drive_mode;       /* Chế độ lái hiện tại              */
} HardwareInput_t;

/* =========================================================
 * SECTION 3: ECU DATA — STM32 sở hữu, gửi lên VCU qua CAN
 * Single source of truth trên STM32
 * Bảo vệ bởi xEcuMutex
 *
 * Ai ghi gì:
 *   InputScan    → accel_pct, key_on, brake_pressed,
 *                  charging_detect, fault_inject, gear, drive_mode
 *   BatteryMgr   → batt_voltage, soc_pct, batt_low_warning
 *   MotorControl → motor_pwm, speed_kmh, odometer_m
 * ========================================================= */

typedef struct
{
    /* Accelerator */
    uint8_t         accel_pct;          /* 0-100%                        */

    /* Battery */
    float           batt_voltage;       /* Volt thực sau chia áp         */
    uint8_t         soc_pct;            /* 0-100% State of Charge        */
    bool            batt_low_warning;   /* true khi soc_pct < threshold  */

    /* Motor / Speed */
    uint16_t        motor_pwm;          /* 0-1000 (0-100% duty)          */
    float           speed_kmh;          /* Tốc độ km/h từ encoder        */
    uint32_t        odometer_m;         /* Quãng đường tích lũy (m)      */

    /* Hardware inputs thô — VCU cần để chạy state machine */
    bool            key_on;
    bool            brake_pressed;
    bool            charging_detect;
    bool            fault_inject;
    GearPosition_t  gear;
    DriveMode_t     drive_mode;
} EcuData_t;

/* =========================================================
 * SECTION 4: VCU COMMAND — Pi gửi xuống STM32 qua CAN 0x200
 * STM32 nhận và thực thi, không tự quyết định
 * Bảo vệ bởi xVcuCmdMutex
 *
 * CAN ID 0x200, DLC = 4 bytes:
 *   Byte 0 bit0: motor_enable
 *   Byte 0 bit1: direction_fwd
 *   Byte 1:      pwm_target_pct (0-100)
 *   Byte 2:      vehicle_state (VehicleState_t từ VCU)
 *   Byte 3:      fault_flags   (FaultFlags_t từ VCU)
 * ========================================================= */

typedef struct
{
    bool     motor_enable;     /* VCU cho phép motor chạy   */
    bool     direction_fwd;    /* true=tiến / false=lùi     */
    uint8_t  pwm_target_pct;   /* 0-100%, VCU muốn đặt      */
    uint8_t  vehicle_state;    /* VehicleState_t hiện tại   */
    uint8_t  fault_flags;      /* FaultFlags_t từ VCU       */
} VcuCommand_t;

/* =========================================================
 * SECTION 5: CAN MESSAGE STRUCTS
 * packed — map trực tiếp vào CAN data bytes
 * ========================================================= */

/** CAN 0x100 — Speed + Accel (20ms), 4 bytes */
typedef struct __attribute__((packed))
{
    uint16_t speed_x10;      /* speed_kmh x10, tránh float */
    uint8_t  accel_pct;      /* 0-100                      */
    uint8_t  motor_pwm_pct;  /* 0-100                      */
} CanMsg100_t;

/** CAN 0x101 — Battery (100ms), 4 bytes */
typedef struct __attribute__((packed))
{
    uint16_t batt_mv;        /* Điện áp mV                 */
    uint8_t  soc_pct;        /* 0-100                      */
    uint8_t  batt_flags;     /* bit0 = batt_low_warning    */
} CanMsg101_t;

/**
 * @brief CAN 0x102 — Hardware inputs (40ms), 4 bytes
 *        VCU cần raw inputs này để chạy state machine
 */
typedef struct __attribute__((packed))
{
    uint8_t  gear;           /* GearPosition_t             */
    uint8_t  drive_mode;     /* DriveMode_t                */
    uint8_t  input_flags;    /* bit0=key, bit1=brake,
                                bit2=charging, bit3=fault  */
    uint8_t  reserved;
} CanMsg102_t;

/** CAN 0x103 — Odometer (40ms), 4 bytes */
typedef struct __attribute__((packed))
{
    uint16_t odometer_m;     /* Odometer (truncate 16-bit) */
    uint8_t  reserved0;
    uint8_t  reserved1;
} CanMsg103_t;

/** CAN 0x200 — VCU Command → STM32 (20ms), 4 bytes */
typedef struct __attribute__((packed))
{
    uint8_t  ctrl_flags;     /* bit0=motor_enable,
                                bit1=direction_fwd         */
    uint8_t  pwm_target_pct; /* 0-100                     */
    uint8_t  vehicle_state;  /* VehicleState_t từ VCU     */
    uint8_t  fault_flags;    /* FaultFlags_t từ VCU       */
} CanMsg200_t;

/* =========================================================
 * SECTION 6: CONSTANTS
 * ========================================================= */

/* ADC */
#define ADC_MAX_VALUE           (4095U)
#define ADC_VREF_MV             (3300U)
#define ADC_NUM_CHANNELS        (2U)
#define ADC_RANK_ACCEL          (0U)    /* PA4 — rank 1 CubeMX */
#define ADC_RANK_BATT           (1U)    /* PA2 — rank 2 CubeMX */

/* Voltage divider R1=22k, R2=10k → factor = 3.2 */
#define VDIV_FACTOR_X100        (320U)

/* Battery 2S 18650 */
#define BATT_FULL_MV            (8400U)
#define BATT_EMPTY_MV           (6400U)
#define BATT_LOW_WARNING_PCT    (20U)   /* Cảnh báo khi SoC < 20% */

/* Motor PWM — TIM1 ARR=999, resolution 1000 steps */
#define MOTOR_PWM_MAX           (1000U)

/* Encoder GA12-N20 */
#define ENCODER_PPR             (7U)    /* Pulses per revolution encoder shaft */
#define GEAR_RATIO              (100U)  /* Tỉ số hộp giảm tốc                  */
#define WHEEL_CIRCUMFERENCE_MM  (188U)  /* Chu vi bánh xe mô phỏng             */

/* Task periods (ms) */
#define TASK_PERIOD_INPUT_MS    (10U)
#define TASK_PERIOD_MOTOR_MS    (5U)
#define TASK_PERIOD_CAN_MS      (20U)
#define TASK_PERIOD_BATTERY_MS  (100U)

/* FreeRTOS stack sizes (words — 1 word = 4 bytes trên Cortex-M3) */
#define STACK_SIZE_INPUT        (256U)
#define STACK_SIZE_MOTOR        (256U)
#define STACK_SIZE_BATTERY      (256U)
#define STACK_SIZE_CAN_TX       (384U)
#define STACK_SIZE_CAN_RX       (256U)

/* FreeRTOS task priorities */
#define PRIORITY_INPUT          (3U)   /* Normal — đọc phần cứng          */
#define PRIORITY_MOTOR          (4U)   /* AboveNormal — điều khiển thực   */
#define PRIORITY_BATTERY        (2U)   /* BelowNormal — cập nhật chậm     */
#define PRIORITY_CAN_TX         (2U)   /* BelowNormal                     */
#define PRIORITY_CAN_RX         (3U)   /* Normal — phản hồi lệnh VCU      */

/* VCU watchdog — dừng motor nếu không nhận lệnh VCU quá thời gian này */
#define VCU_CMD_TIMEOUT_MS      (500U)   /* Tăng lên 2s để test thủ công với cansend */

#endif /* VEHICLE_TYPES_H */
