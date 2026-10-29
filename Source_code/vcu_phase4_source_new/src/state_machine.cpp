/**
 * @file    state_machine.cpp
 * @brief   VCU State Machine implementation
 *
 * Mỗi vòng lặp 20ms:
 *   1. getEcuSnapshot() — lấy bản sao data từ VehicleData
 *   2. checkFaultCondition() — kiểm tra fault (ưu tiên cao nhất)
 *   3. checkChargingCondition() — kiểm tra sạc (ưu tiên thứ hai)
 *   4. Chạy stateXxx_Run() của state hiện tại
 *   5. computeCommand() — tính VcuCommand từ state + ecu data
 *   6. setVcuCommand() — ghi lệnh vào VehicleData cho CanSender
 */

#include "state_machine.h"

#include <chrono>
#include <thread>
#include <cstdio>
#include <algorithm>

/* ============================================================
 * Constructor / Destructor
 * ============================================================ */

StateMachine::StateMachine()
{
    /* Bắt đầu ở OFF — an toàn nhất */
    m_state.store(static_cast<uint8_t>(VehicleState::OFF));
}

StateMachine::~StateMachine()
{
    stop();
}

/* ============================================================
 * Public: start() / stop()
 * ============================================================ */

bool StateMachine::start()
{
    m_running = true;
    m_thread  = std::thread(&StateMachine::threadFunc, this);
    fprintf(stdout, "[FSM] Started\n");
    return true;
}

void StateMachine::stop()
{
    m_running = false;
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    fprintf(stdout, "[FSM] Stopped\n");
}

void StateMachine::setOnStateChange(StateChangeCallback cb)
{
    m_on_state_change = std::move(cb);
}

VehicleState StateMachine::currentState() const
{
    return static_cast<VehicleState>(m_state.load());
}

uint8_t StateMachine::faultFlags() const
{
    return m_fault_flags.load();
}

/* ============================================================
 * Private: threadFunc() — vòng lặp FSM 20ms
 * ============================================================ */

void StateMachine::threadFunc()
{
    /* Gọi Entry của state khởi đầu */
    stateOff_Entry();

    auto next_tick = std::chrono::steady_clock::now();

    while (m_running.load())
    {
        /* ---- Lấy snapshot dữ liệu từ CAN ---- */
        EcuSnapshot ecu = VehicleData::instance().getEcuSnapshot();

        /* ---- Kiểm tra CAN timeout (watchdog) ---- */
        if (VehicleData::instance().isCanTimeout(VCU_WATCHDOG_MS))
        {
            if (currentState() != VehicleState::OFF &&
                currentState() != VehicleState::FAULT)
            {
                fprintf(stderr, "[FSM] CAN TIMEOUT — transitioning to FAULT\n");
                m_fault_flags.fetch_or(static_cast<uint8_t>(FaultFlags::CAN_TIMEOUT));
                transition(VehicleState::FAULT);
            }
        }
        else
        {
            /* ---- Kiểm tra Fault (ưu tiên cao nhất) ---- */
            if (checkFaultCondition(ecu) &&
                currentState() != VehicleState::FAULT)
            {
                transition(VehicleState::FAULT);
            }
            /* ---- Kiểm tra Charging (ưu tiên thứ hai) ---- */
            else if (checkChargingCondition(ecu) &&
                     currentState() != VehicleState::CHARGING &&
                     currentState() != VehicleState::FAULT)
            {
                transition(VehicleState::CHARGING);
            }
            else
            {
                /* ---- Chạy handler của state hiện tại ---- */
                switch (currentState())
                {
                    case VehicleState::OFF:
                        stateOff_Run(ecu);
                        break;
                    case VehicleState::STANDBY:
                        stateStandby_Run(ecu);
                        break;
                    case VehicleState::READY:
                        stateReady_Run(ecu);
                        break;
                    case VehicleState::DRIVING:
                        stateDriving_Run(ecu);
                        break;
                    case VehicleState::CHARGING:
                        stateCharging_Run(ecu);
                        break;
                    case VehicleState::FAULT:
                        stateFault_Run(ecu);
                        break;
                    default:
                        /* MISRA: default bắt buộc */
                        break;
                }
            }
        }

        /* ---- Tính và ghi VcuCommand ---- */
        VcuCommand cmd = computeCommand(ecu);
        VehicleData::instance().setVcuCommand(cmd);

        /* ---- Sleep đến tick tiếp theo (20ms period) ---- */
        next_tick += std::chrono::milliseconds(TASK_PERIOD_MS);
        std::this_thread::sleep_until(next_tick);
    }

    fprintf(stdout, "[FSM] Thread exited\n");
}

/* ============================================================
 * Private: transition() — chuyển state + gọi Entry
 * ============================================================ */

void StateMachine::transition(VehicleState new_state)
{
    VehicleState old_state = currentState();

    if (old_state == new_state) { return; }  /* Không chuyển nếu đang ở đó rồi */

    fprintf(stdout, "[FSM] %u → %u\n",
            static_cast<unsigned>(old_state),
            static_cast<unsigned>(new_state));

    /* Cập nhật state nguyên tử */
    m_state.store(static_cast<uint8_t>(new_state));

    /* Gọi Entry handler của state mới */
    switch (new_state)
    {
        case VehicleState::OFF:      stateOff_Entry();      break;
        case VehicleState::STANDBY:  stateStandby_Entry();  break;
        case VehicleState::READY:    stateReady_Entry();    break;
        case VehicleState::DRIVING:  stateDriving_Entry();  break;
        case VehicleState::CHARGING: stateCharging_Entry(); break;
        case VehicleState::FAULT:    stateFault_Entry();    break;
        default:                                            break;
    }

    /* Notify VcuBridge → QML */
    if (m_on_state_change)
    {
        m_on_state_change(new_state);
    }
}

/* ============================================================
 * Private: checkFaultCondition()
 *
 * Fault khi:
 *   - fault_inject button được nhấn (phần cứng)
 *   - SoC ≤ SOC_FAULT_THRESHOLD (pin cạn kiệt)
 * ============================================================ */

bool StateMachine::checkFaultCondition(const EcuSnapshot& ecu)
{
    bool fault = false;

    if (ecu.fault_inject)
    {
        m_fault_flags.fetch_or(static_cast<uint8_t>(FaultFlags::INJECTED));
        fault = true;
        fprintf(stdout, "[FSM] Fault inject detected\n");
    }

    if (ecu.soc_pct <= SOC_FAULT_THRESHOLD)
    {
        m_fault_flags.fetch_or(static_cast<uint8_t>(FaultFlags::UNDERVOLTAGE));
        fault = true;
        fprintf(stdout, "[FSM] Undervoltage fault: SoC=%u%%\n", ecu.soc_pct);
    }

    return fault;
}

/* ============================================================
 * Private: checkChargingCondition()
 * ============================================================ */

bool StateMachine::checkChargingCondition(const EcuSnapshot& ecu)
{
    return ecu.charging_detect;
}

/* ============================================================
 * STATE HANDLERS — Entry + Run
 * ============================================================ */

/* ---- OFF ---- */
void StateMachine::stateOff_Entry()
{
    /* Reset fault khi về OFF */
    m_fault_flags.store(static_cast<uint8_t>(FaultFlags::NONE));
    fprintf(stdout, "[FSM] → OFF\n");
}

void StateMachine::stateOff_Run(const EcuSnapshot& ecu)
{
    /* Chuyển STANDBY khi key_on */
    if (ecu.key_on)
    {
        transition(VehicleState::STANDBY);
    }
}

/* ---- STANDBY ---- */
void StateMachine::stateStandby_Entry()
{
    fprintf(stdout, "[FSM] → STANDBY (Key ON, chờ sẵn sàng)\n");
}

void StateMachine::stateStandby_Run(const EcuSnapshot& ecu)
{
    /* Key tắt → về OFF */
    if (!ecu.key_on)
    {
        transition(VehicleState::OFF);
        return;
    }

    /* Brake + Gear ≠ P → READY */
    if (ecu.brake_pressed && (ecu.gear == GearPosition::D || ecu.gear == GearPosition::R))
    {
        transition(VehicleState::READY);
    }
}

/* ---- READY ---- */
void StateMachine::stateReady_Entry()
{
    fprintf(stdout, "[FSM] → READY (Motor cho phép, chờ ga)\n");
}

void StateMachine::stateReady_Run(const EcuSnapshot& ecu)
{
    if (!ecu.key_on)
    {
        transition(VehicleState::OFF);
        return;
    }

    /* Về P → STANDBY */
    if (ecu.gear != GearPosition::D && ecu.gear != GearPosition::R)
    {
        transition(VehicleState::STANDBY);
        return;
    }

    /* Vặn ga vượt ngưỡng → DRIVING */
    if (ecu.accel_pct >= ACCEL_MIN_THRESHOLD)
    {
        transition(VehicleState::DRIVING);
    }
}

/* ---- DRIVING ---- */
void StateMachine::stateDriving_Entry()
{
    fprintf(stdout, "[FSM] → DRIVING\n");
}

void StateMachine::stateDriving_Run(const EcuSnapshot& ecu)
{
    if (!ecu.key_on)
    {
        transition(VehicleState::OFF);
        return;
    }

    // xử lý gear khi đang chạy
    if (ecu.gear == GearPosition::P) {
        if (ecu.speed_kmh == 0.0) {
            // xe đã dừng hẳn → cho phép về P → STANDBY
            transition(VehicleState::STANDBY);
        }
        // currentSpeed > 0 → chặn, bỏ qua lệnh P
        // giữ nguyên DRIVING, FakeECU tự giảm speed về 0
        return;
    }

    if (ecu.gear == GearPosition::N)
    {
        transition(VehicleState::STANDBY);
        return;
    }

    /* Ga về 0 → READY (idle) */
    if (ecu.speed_kmh == 0.0)
    {
        transition(VehicleState::READY);
    }
}

/* ---- CHARGING ---- */
void StateMachine::stateCharging_Entry()
{
    fprintf(stdout, "[FSM] → CHARGING (Motor bị khoá)\n");
}

void StateMachine::stateCharging_Run(const EcuSnapshot& ecu)
{
    if (!ecu.charging_detect) {
        // rút sạc — kiểm tra key
        if (ecu.key_on) {
            transition(VehicleState::STANDBY); // key ON → STANDBY
        } else {
            transition(VehicleState::OFF);     // key OFF → OFF
        }
        return;
    }
}

/* ---- FAULT ---- */
void StateMachine::stateFault_Entry()
{
    fprintf(stdout, "[FSM] → FAULT (fault_flags=0x%02X)\n",
            m_fault_flags.load());
}

void StateMachine::stateFault_Run(const EcuSnapshot& ecu)
{
    /* Thoát FAULT: tắt key rồi bật lại → OFF (reset) */
    if (!ecu.key_on)
    {
        m_fault_flags.store(static_cast<uint8_t>(FaultFlags::NONE));
        transition(VehicleState::OFF);
    } else {
        m_fault_flags.store(static_cast<uint8_t>(FaultFlags::NONE));
        transition(VehicleState::STANDBY);
    }
}

/* ============================================================
 * Private: computeCommand() — tính VcuCommand từ state + ecu
 *
 * Đây là nơi duy nhất quyết định motor_enable và pwm_target.
 * STM32 chỉ thực thi lệnh này, không tự quyết định.
 * ============================================================ */

VcuCommand StateMachine::computeCommand(const EcuSnapshot& ecu) const
{
    VcuCommand cmd;
    cmd.vehicle_state = currentState();
    cmd.fault_flags   = m_fault_flags.load();

    switch (currentState())
    {
        case VehicleState::DRIVING:
        {
            /* Motor enable — hướng phụ thuộc gear */
            cmd.motor_enable  = true;
            cmd.direction_fwd = (ecu.gear != GearPosition::R);

            /* PWM giới hạn theo drive mode */
            cmd.pwm_target_pct = applyDriveMode(ecu.accel_pct, ecu.drive_mode);
            break;
        }

        case VehicleState::FAULT:
        case VehicleState::CHARGING:
        case VehicleState::OFF:
        {
            /* Motor cứng tắt — an toàn */
            cmd.motor_enable   = false;
            cmd.direction_fwd  = true;
            cmd.pwm_target_pct = 0U;
            break;
        }

        case VehicleState::STANDBY:
        case VehicleState::READY:
        default:
        {
            /* Motor không chạy nhưng chưa fault */
            cmd.motor_enable   = false;
            cmd.direction_fwd  = true;
            cmd.pwm_target_pct = 0U;
            break;
        }
    }

    return cmd;
}

/* ============================================================
 * Private: applyDriveMode() — tính PWM % với giới hạn mode
 *
 * ECO   : PWM tối đa 50% — tiết kiệm pin
 * NORMAL: PWM tối đa 80%
 * SPORT : PWM tối đa 100% — không giới hạn
 * ============================================================ */

uint8_t StateMachine::applyDriveMode(uint8_t accel_pct, DriveMode mode)
{
    uint8_t max_pwm = PWM_MAX_NORMAL;

    switch (mode)
    {
        case DriveMode::ECO:    max_pwm = PWM_MAX_ECO;    break;
        case DriveMode::NORMAL: max_pwm = PWM_MAX_NORMAL; break;
        case DriveMode::SPORT:  max_pwm = PWM_MAX_SPORT;  break;
        default:                max_pwm = PWM_MAX_NORMAL;  break;
    }

    /* Scale tuyến tính: pwm = accel * max_pwm / 100 */
    uint32_t pwm = (static_cast<uint32_t>(accel_pct) * max_pwm) / 100U;

    /* Clamp về [0, max_pwm] */
    if (pwm > max_pwm) { pwm = max_pwm; }

    return static_cast<uint8_t>(pwm);
}
