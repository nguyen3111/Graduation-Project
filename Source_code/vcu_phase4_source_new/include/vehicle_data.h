/**
 * @file    vehicle_data.h
 * @brief   Shared data store — source of truth của VCU
 *
 * VehicleData là "database" trung tâm của VCU phía Pi.
 * Dữ liệu được cập nhật bởi CanReceiver (nhận từ STM32) và
 * được đọc bởi StateMachine, CanSender, và VcuBridge (HMI).
 *
 * Thread-safety:
 *   Mọi truy cập phải qua getLocked() / setXxx() với mutex nội bộ.
 *   Không bao giờ truy cập thành viên trực tiếp từ bên ngoài.
 *
 * Pattern: Singleton — chỉ có 1 instance trong toàn ứng dụng.
 *
 * @note Tương đương với g_ecu_data + xEcuMutex trên STM32,
 *       nhưng phía Pi tách riêng ECU input và VCU output.
 */

#ifndef VEHICLE_DATA_H
#define VEHICLE_DATA_H

#include "vehicle_types_pi.h"
#include <mutex>
#include <cstdint>
#include <chrono>

/* ===========================================================
 * ECU Snapshot — dữ liệu nhận được từ STM32 qua CAN
 * =========================================================== */

/**
 * @brief Toàn bộ dữ liệu từ STM32, gộp từ 4 CAN frame
 *        Được cập nhật mỗi khi CanReceiver parse frame mới.
 */
struct EcuSnapshot
{
    /* Từ 0x100 — Speed + Accel (20ms) */
    float    speed_kmh     {0.0F};
    uint8_t  accel_pct     {0U};
    uint8_t  motor_pwm_pct {0U};

    /* Từ 0x101 — Battery (100ms) */
    float    batt_voltage  {0.0F};   /* mV → V */
    uint8_t  soc_pct       {100U};
    bool     batt_low_warning {false};

    /* Từ 0x102 — Hardware Inputs (40ms) */
    GearPosition gear       {GearPosition::P};
    DriveMode    drive_mode {DriveMode::NORMAL};
    bool         key_on          {false};
    bool         brake_pressed   {false};
    bool         charging_detect {false};
    bool         fault_inject    {false};

    /* Từ 0x103 — Odometer (40ms) */
    uint32_t  odometer_m    {0U};

    /* Timestamp lần cuối nhận frame — cho watchdog */
    std::chrono::steady_clock::time_point last_rx_time {};
};

/* ===========================================================
 * VCU Command — lệnh VCU gửi xuống STM32 qua CAN 0x200
 * =========================================================== */

/**
 * @brief Lệnh hiện tại từ VCU → STM32
 *        StateMachine cập nhật, CanSender đọc và gửi.
 */
struct VcuCommand
{
    bool         motor_enable   {false};
    bool         direction_fwd  {true};
    uint8_t      pwm_target_pct {0U};
    VehicleState vehicle_state  {VehicleState::OFF};
    uint8_t      fault_flags    {static_cast<uint8_t>(FaultFlags::NONE)};
};

/* ===========================================================
 * VehicleData — Singleton thread-safe data store
 * =========================================================== */

class VehicleData
{
public:
    /**
     * @brief Lấy instance singleton — gọi bất cứ đâu
     */
    static VehicleData& instance();

    /* ---- Cập nhật từ CanReceiver ---- */

    /**
     * @brief Cập nhật dữ liệu từ CAN 0x100
     * @param speed_x10  speed_kmh * 10
     * @param accel_pct  0–100
     * @param pwm_pct    0–100
     */
    void updateSpeedAccel(uint16_t speed_x10, uint8_t accel_pct, uint8_t pwm_pct);

    /**
     * @brief Cập nhật dữ liệu từ CAN 0x101
     */
    void updateBattery(uint16_t batt_mv, uint8_t soc_pct, uint8_t batt_flags);

    /**
     * @brief Cập nhật dữ liệu từ CAN 0x102
     */
    void updateHwInputs(uint8_t gear, uint8_t drive_mode, uint8_t input_flags);

    /**
     * @brief Cập nhật dữ liệu từ CAN 0x103
     */
    void updateOdometer(uint16_t odometer_m);

    /* ---- Đọc snapshot (thread-safe copy) ---- */

    /**
     * @brief Lấy bản sao EcuSnapshot — thread-safe
     *        Caller không cần giữ lock sau khi hàm trả về.
     * @return Copy của EcuSnapshot tại thời điểm gọi
     */
    EcuSnapshot getEcuSnapshot() const;

    /* ---- Cập nhật VcuCommand từ StateMachine ---- */

    /**
     * @brief StateMachine ghi lệnh mới vào đây
     */
    void setVcuCommand(const VcuCommand& cmd);

    /**
     * @brief CanSender đọc lệnh để gửi xuống STM32
     */
    VcuCommand getVcuCommand() const;

    /* ---- Watchdog helper ---- */

    /**
     * @brief Kiểm tra có nhận CAN frame trong vòng timeout_ms không
     * @param timeout_ms Ngưỡng timeout tính bằng ms
     * @return true nếu timeout (không nhận được gì)
     */
    bool isCanTimeout(uint32_t timeout_ms) const;

private:
    /* Singleton — không cho phép tạo ngoài */
    VehicleData()  = default;
    ~VehicleData() = default;
    VehicleData(const VehicleData&)            = delete;
    VehicleData& operator=(const VehicleData&) = delete;

    mutable std::mutex m_ecu_mutex;  /* Bảo vệ m_ecu */
    mutable std::mutex m_vcu_mutex;  /* Bảo vệ m_vcu */

    EcuSnapshot m_ecu {};
    VcuCommand  m_vcu {};
};

#endif /* VEHICLE_DATA_H */
