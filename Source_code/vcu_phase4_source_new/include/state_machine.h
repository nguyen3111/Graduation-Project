/**
 * @file    state_machine.h
 * @brief   VCU State Machine — 6 trạng thái xe điện
 *
 * FSM chạy trong thread riêng, chu kỳ 20ms.
 * Đọc EcuSnapshot từ VehicleData, quyết định chuyển state,
 * tính toán VcuCommand và ghi lại vào VehicleData.
 *
 * Sơ đồ chuyển trạng thái:
 *
 *   OFF ──(key_on)──► STANDBY ──(brake+gear≠P)──► READY ──(accel>0)──► DRIVING
 *    ▲                                                                       │
 *    │ (key off reset)                                                       │
 *    │                                                                       │
 *   FAULT ◄──────────────── ANY (fault_inject || soc≤3%)                    │
 *    │                                                                       │
 *   CHARGING ◄─────────────── ANY (charging_detect)  ◄────────────────────  │
 *
 * Pattern Entry/Run/Exit — giống state_machine.c trên STM32:
 *   Entry: gọi 1 lần khi vào state, khởi tạo
 *   Run:   gọi mỗi 20ms, kiểm tra điều kiện chuyển
 *   Exit:  gọi 1 lần khi rời state, cleanup
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "vehicle_types_pi.h"
#include "vehicle_data.h"
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>

class StateMachine
{
public:
    /**
     * @brief Callback khi state thay đổi — VcuBridge dùng để emit signal
     * @param new_state  Trạng thái mới
     */
    using StateChangeCallback = std::function<void(VehicleState)>;

    StateMachine();
    ~StateMachine();

    /**
     * @brief Khởi động FSM thread
     */
    bool start();

    /**
     * @brief Dừng FSM thread
     */
    void stop();

    /**
     * @brief Đăng ký callback khi state thay đổi
     */
    void setOnStateChange(StateChangeCallback cb);

    /**
     * @brief Lấy state hiện tại — thread-safe
     */
    VehicleState currentState() const;

    /**
     * @brief Lấy fault flags hiện tại
     */
    uint8_t faultFlags() const;

private:
    /* ---- Thread ---- */
    void threadFunc();

    /* ---- FSM core ---- */
    void transition(VehicleState new_state);
    bool checkFaultCondition(const EcuSnapshot& ecu);
    bool checkChargingCondition(const EcuSnapshot& ecu);

    /* ---- State handlers — Entry/Run pattern ---- */
    void stateOff_Entry();
    void stateOff_Run(const EcuSnapshot& ecu);

    void stateStandby_Entry();
    void stateStandby_Run(const EcuSnapshot& ecu);

    void stateReady_Entry();
    void stateReady_Run(const EcuSnapshot& ecu);

    void stateDriving_Entry();
    void stateDriving_Run(const EcuSnapshot& ecu);

    void stateCharging_Entry();
    void stateCharging_Run(const EcuSnapshot& ecu);

    void stateFault_Entry();
    void stateFault_Run(const EcuSnapshot& ecu);

    /* ---- Tính toán VcuCommand từ state + EcuSnapshot ---- */
    VcuCommand computeCommand(const EcuSnapshot& ecu) const;

    /* ---- Compute PWM theo Drive Mode ---- */
    static uint8_t applyDriveMode(uint8_t accel_pct, DriveMode mode);

    /* Thread */
    std::thread       m_thread;
    std::atomic<bool> m_running {false};

    /* State (atomic để đọc từ ngoài thread không cần mutex) */
    std::atomic<uint8_t> m_state {static_cast<uint8_t>(VehicleState::OFF)};
    std::atomic<uint8_t> m_fault_flags {static_cast<uint8_t>(FaultFlags::NONE)};

    /* Callback */
    StateChangeCallback m_on_state_change;

    /* Chu kỳ task FSM */
    static constexpr uint32_t TASK_PERIOD_MS = 20U;

    /* Ngưỡng accel để chuyển READY → DRIVING */
    static constexpr uint8_t ACCEL_MIN_THRESHOLD = 5U;  /* 5% */

    /* PWM limits theo drive mode */
    static constexpr uint8_t PWM_MAX_ECO    = 50U;
    static constexpr uint8_t PWM_MAX_NORMAL = 80U;
    static constexpr uint8_t PWM_MAX_SPORT  = 100U;
};

#endif /* STATE_MACHINE_H */
