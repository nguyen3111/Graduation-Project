/**
 * @file    vcu_bridge.cpp
 * @brief   VcuBridge implementation — Qt C++ ↔ QML data bridge
 *
 * Nguyên tắc Qt thread-safety:
 *   - Mọi signal emission PHẢI xảy ra trên Qt main thread
 *   - notifyDataReady() được gọi từ CanReceiver thread
 *     → dùng QMetaObject::invokeMethod với Qt::QueuedConnection
 *     → hàm refreshFromData() chạy trên main thread (safe)
 */

#include "vcu_bridge.h"
#include "vehicle_data.h"

#include <QMetaObject>
#include <QTimer>
#include <cmath>

/* ============================================================
 * Constructor
 * ============================================================ */

VcuBridge::VcuBridge(QObject* parent)
    : QObject(parent)
{
    /* Polling timer 50ms — fallback để luôn refresh UI */
    m_poll_timer = new QTimer(this);
    m_poll_timer->setInterval(50);  /* 50ms — 20fps là đủ cho UI */
    connect(m_poll_timer, &QTimer::timeout, this, &VcuBridge::refreshFromData);
    m_poll_timer->start();
}

/* ============================================================
 * notifyDataReady() — gọi từ CanReceiver thread
 *
 * Dùng Qt::QueuedConnection để marshal sang main thread.
 * QMetaObject::invokeMethod là cách an toàn duy nhất để
 * gọi slot Qt từ non-Qt thread.
 * ============================================================ */

void VcuBridge::notifyDataReady()
{
    QMetaObject::invokeMethod(this,
                              "refreshFromData",
                              Qt::QueuedConnection);
}

void VcuBridge::notifyStateChanged(VehicleState /*new_state*/)
{
    /* State sẽ được đọc trong refreshFromData() */
    QMetaObject::invokeMethod(this,
                              "refreshFromData",
                              Qt::QueuedConnection);
}

/* ============================================================
 * refreshFromData() — chạy trên Qt main thread
 *
 * So sánh với cache, chỉ emit signal khi giá trị thực sự thay đổi.
 * Tránh re-render QML không cần thiết → cải thiện hiệu năng.
 * ============================================================ */

void VcuBridge::refreshFromData()
{
    /* Lấy bản sao thread-safe của EcuSnapshot */
    EcuSnapshot ecu = VehicleData::instance().getEcuSnapshot();
    VcuCommand  cmd = VehicleData::instance().getVcuCommand();

    /* ---- Speed ---- */
    if (std::abs(ecu.speed_kmh - m_speed_kmh) > 0.05)
    {
        m_speed_kmh = static_cast<double>(ecu.speed_kmh);
        emit speedKmhChanged();
    }

    /* ---- Accel ---- */
    int accel = static_cast<int>(ecu.accel_pct);
    if (accel != m_accel_pct)
    {
        m_accel_pct = accel;
        emit accelPctChanged();
    }

    /* ---- Motor PWM ---- */
    int pwm = static_cast<int>(ecu.motor_pwm_pct);
    if (pwm != m_motor_pwm)
    {
        m_motor_pwm = pwm;
        emit motorPwmPctChanged();
    }

    /* ---- Battery voltage ---- */
    if (std::abs(ecu.batt_voltage - m_batt_voltage) > 0.005)
    {
        m_batt_voltage = static_cast<double>(ecu.batt_voltage);
        emit battVoltageChanged();
    }

    /* ---- SoC ---- */
    int soc = static_cast<int>(ecu.soc_pct);
    if (soc != m_soc_pct)
    {
        m_soc_pct = soc;
        emit socPercentChanged();
    }

    /* ---- Battery low warning ---- */
    if (ecu.batt_low_warning != m_batt_low)
    {
        m_batt_low = ecu.batt_low_warning;
        emit battLowChanged();
    }

    /* ---- Gear ---- */
    int gear_val = static_cast<int>(ecu.gear);
    if (gear_val != m_gear)
    {
        m_gear = gear_val;
        emit gearChanged();
    }

    /* ---- Drive Mode ---- */
    int mode_val = static_cast<int>(ecu.drive_mode);
    if (mode_val != m_drive_mode)
    {
        m_drive_mode = mode_val;
        emit driveModeChanged();
    }

    /* ---- Vehicle State (từ VcuCommand, vì FSM là nguồn truth) ---- */
    int state_val = static_cast<int>(cmd.vehicle_state);
    if (state_val != m_vehicle_state)
    {
        m_vehicle_state = state_val;
        emit vehicleStateChanged();
    }

    /* ---- Odometer ---- */
    int odo = static_cast<int>(ecu.odometer_m);
    if (odo != m_odometer_m)
    {
        m_odometer_m = odo;
        emit odometerMChanged();
    }

    /* ---- Fault flags ---- */
    int fault = static_cast<int>(cmd.fault_flags);
    if (fault != m_fault_flags)
    {
        m_fault_flags = fault;
        emit faultFlagsChanged();
    }

    /* ---- Key ---- */
    if (ecu.key_on != m_key)
    {
        m_key = ecu.key_on;
        emit keyChanged();
    }

    /* ---- Brake ---- */
    if (ecu.brake_pressed != m_brake)
    {
        m_brake = ecu.brake_pressed;
        emit brakeChanged();
    }
}

/* ============================================================
 * Property getters
 * ============================================================ */

double  VcuBridge::speedKmh()     const { return m_speed_kmh;     }
int     VcuBridge::accelPct()     const { return m_accel_pct;     }
int     VcuBridge::motorPwmPct()  const { return m_motor_pwm;     }
double  VcuBridge::battVoltage()  const { return m_batt_voltage;  }
int     VcuBridge::socPercent()   const { return m_soc_pct;       }
bool    VcuBridge::battLow()      const { return m_batt_low;      }
int     VcuBridge::gear()         const { return m_gear;          }
int     VcuBridge::driveMode()    const { return m_drive_mode;    }
int     VcuBridge::vehicleState() const { return m_vehicle_state; }
int     VcuBridge::odometerM()    const { return m_odometer_m;   }
int     VcuBridge::faultFlags()   const { return m_fault_flags;   }
bool    VcuBridge::hasFault()     const { return m_fault_flags != 0; }
bool    VcuBridge::key()          const { return m_key            }
bool    VcuBridge::brake()        const { return m_brake          }

/* ============================================================
 * Computed display helpers — QML dùng trực tiếp
 * ============================================================ */

QString VcuBridge::stateLabel() const
{
    switch (static_cast<VehicleState>(m_vehicle_state))
    {
        case VehicleState::OFF:      return QStringLiteral("OFF");
        case VehicleState::STANDBY:  return QStringLiteral("STANDBY");
        case VehicleState::READY:    return QStringLiteral("READY");
        case VehicleState::DRIVING:  return QStringLiteral("DRIVING");
        case VehicleState::CHARGING: return QStringLiteral("CHARGING");
        case VehicleState::FAULT:    return QStringLiteral("FAULT");
        default:                     return QStringLiteral("UNKNOWN");
    }
}

QString VcuBridge::gearLabel() const
{
    switch (static_cast<GearPosition>(m_gear))
    {
        case GearPosition::P: return QStringLiteral("P");
        case GearPosition::R: return QStringLiteral("R");
        case GearPosition::N: return QStringLiteral("N");
        case GearPosition::D: return QStringLiteral("D");
        default:              return QStringLiteral("?");
    }
}

QString VcuBridge::modeLabel() const
{
    switch (static_cast<DriveMode>(m_drive_mode))
    {
        case DriveMode::ECO:    return QStringLiteral("ECO");
        case DriveMode::NORMAL: return QStringLiteral("NORMAL");
        case DriveMode::SPORT:  return QStringLiteral("SPORT");
        default:                return QStringLiteral("?");
    }
}

QColor VcuBridge::stateColor() const
{
    switch (static_cast<VehicleState>(m_vehicle_state))
    {
        case VehicleState::OFF:      return QColor(80, 80, 80);       /* Dark gray */
        case VehicleState::STANDBY:  return QColor(255, 165, 0);      /* Orange    */
        case VehicleState::READY:    return QColor(0, 200, 255);      /* Cyan      */
        case VehicleState::DRIVING:  return QColor(0, 220, 100);      /* Green     */
        case VehicleState::CHARGING: return QColor(100, 100, 255);    /* Blue      */
        case VehicleState::FAULT:    return QColor(255, 50, 50);      /* Red       */
        default:                     return QColor(80, 80, 80);
    }
}
