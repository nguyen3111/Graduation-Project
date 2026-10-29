/**
 * @file    vehicle_data.cpp
 * @brief   Implementation của VehicleData singleton
 *
 * Tất cả phương thức đều acquire mutex trước khi đọc/ghi,
 * sau đó release ngay — tránh deadlock và giữ thời gian lock ngắn nhất.
 */

#include "vehicle_data.h"
#include <cstring>

/* ============================================================
 * Singleton implementation
 * ============================================================ */

VehicleData& VehicleData::instance()
{
    /* Static local — guaranteed thread-safe initialization (C++11) */
    static VehicleData s_instance;
    return s_instance;
}

/* ============================================================
 * Update methods — gọi từ CanReceiver thread
 * ============================================================ */

void VehicleData::updateSpeedAccel(uint16_t speed_x10,
                                   uint8_t  accel_pct,
                                   uint8_t  pwm_pct)
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);

    /* speed_x10 = speed_kmh * 10 → chuyển về float */
    m_ecu.speed_kmh     = static_cast<float>(speed_x10) / 10.0F;
    m_ecu.accel_pct     = accel_pct;
    m_ecu.motor_pwm_pct = pwm_pct;
    m_ecu.last_rx_time  = std::chrono::steady_clock::now();
}

void VehicleData::updateBattery(uint16_t batt_mv,
                                uint8_t  soc_pct,
                                uint8_t  batt_flags)
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);

    /* batt_mv là mV → chuyển về Volt */
    m_ecu.batt_voltage     = static_cast<float>(batt_mv) / 1000.0F;
    m_ecu.soc_pct          = soc_pct;
    /* bit0 của batt_flags = batt_low_warning */
    m_ecu.batt_low_warning = ((batt_flags & 0x01U) != 0U);
    m_ecu.last_rx_time     = std::chrono::steady_clock::now();
}

void VehicleData::updateHwInputs(uint8_t gear,
                                 uint8_t drive_mode,
                                 uint8_t input_flags)
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);

    /* Parse gear — clamp về P nếu giá trị không hợp lệ */
    m_ecu.gear = (gear <= static_cast<uint8_t>(GearPosition::D))
                     ? static_cast<GearPosition>(gear)
                     : GearPosition::P;

    /* Parse drive mode — clamp về NORMAL nếu không hợp lệ */
    m_ecu.drive_mode = (drive_mode <= static_cast<uint8_t>(DriveMode::SPORT))
                           ? static_cast<DriveMode>(drive_mode)
                           : DriveMode::NORMAL;

    /* Parse input_flags bitmask */
    m_ecu.key_on          = ((input_flags & INPUT_FLAG_KEY_ON)   != 0U);
    m_ecu.brake_pressed   = ((input_flags & INPUT_FLAG_BRAKE)    != 0U);
    m_ecu.charging_detect = ((input_flags & INPUT_FLAG_CHARGING) != 0U);
    m_ecu.fault_inject    = ((input_flags & INPUT_FLAG_FAULT)    != 0U);
    m_ecu.last_rx_time    = std::chrono::steady_clock::now();
}

void VehicleData::updateOdometer(uint16_t odometer_m)
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);
    m_ecu.odometer_m   = static_cast<uint32_t>(odometer_m);
    m_ecu.last_rx_time = std::chrono::steady_clock::now();
}

/* ============================================================
 * Read methods — gọi từ StateMachine, CanSender, VcuBridge
 * ============================================================ */

EcuSnapshot VehicleData::getEcuSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);
    return m_ecu;  /* Return by value — copy while holding lock */
}

/* ============================================================
 * VCU Command R/W
 * ============================================================ */

void VehicleData::setVcuCommand(const VcuCommand& cmd)
{
    std::lock_guard<std::mutex> lock(m_vcu_mutex);
    m_vcu = cmd;
}

VcuCommand VehicleData::getVcuCommand() const
{
    std::lock_guard<std::mutex> lock(m_vcu_mutex);
    return m_vcu;
}

/* ============================================================
 * Watchdog helper
 * ============================================================ */

bool VehicleData::isCanTimeout(uint32_t timeout_ms) const
{
    std::lock_guard<std::mutex> lock(m_ecu_mutex);

    /* Nếu chưa từng nhận frame nào (time_point == epoch) */
    if (m_ecu.last_rx_time.time_since_epoch().count() == 0)
    {
        return false;  /* Chưa bắt đầu — không coi là timeout */
    }

    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - m_ecu.last_rx_time).count();

    return (elapsed > static_cast<long long>(timeout_ms));
}
