/**
 * @file    vcu_bridge.h
 * @brief   Qt bridge giữa VCU C++ logic và QML HMI
 *
 * VcuBridge là QObject được expose sang QML qua setContextProperty().
 * QML bind trực tiếp vào các Q_PROPERTY của class này:
 *
 *   Text { text: vcuBridge.speedKmh.toFixed(1) + " km/h" }
 *   Text { text: vcuBridge.socPercent + "%" }
 *   Rectangle { color: vcuBridge.stateColor }
 *
 * Thread-safety:
 *   - VcuBridge nhận thông báo từ CanReceiver/StateMachine qua Qt::QueuedConnection
 *   - Tất cả property update xảy ra trên Qt main thread (event loop)
 *   - Không cần mutex thêm trong VcuBridge vì Qt signal/slot đã xử lý
 *
 * Cách hoạt động:
 *   CanReceiver thread  → emit frameReceived (queued) → refreshFromData() → emit propertyChanged
 *   StateMachine thread → callback → emit stateChanged (queued) → refreshState()
 *   QML binding        → auto re-render khi property thay đổi
 */

#ifndef VCU_BRIDGE_H
#define VCU_BRIDGE_H

#include "vehicle_types_pi.h"
#include <QObject>
#include <QString>
#include <QColor>
#include <QTimer>
#include <cstdint>

class VcuBridge : public QObject
{
    Q_OBJECT

    /* ── Speed + Accel ─────────────────────────────────── */
    Q_PROPERTY(double  speedKmh     READ speedKmh     NOTIFY speedKmhChanged)
    Q_PROPERTY(int     accelPct     READ accelPct     NOTIFY accelPctChanged)
    Q_PROPERTY(int     motorPwmPct  READ motorPwmPct  NOTIFY motorPwmPctChanged)

    /* ── Battery ────────────────────────────────────────── */
    Q_PROPERTY(double  battVoltage  READ battVoltage  NOTIFY battVoltageChanged)
    Q_PROPERTY(int     socPercent   READ socPercent   NOTIFY socPercentChanged)
    Q_PROPERTY(bool    battLow      READ battLow      NOTIFY battLowChanged)

    /* ── Inputs / Status ────────────────────────────────── */
    Q_PROPERTY(int     gear         READ gear         NOTIFY gearChanged)
    Q_PROPERTY(int     driveMode    READ driveMode    NOTIFY driveModeChanged)
    Q_PROPERTY(int     vehicleState READ vehicleState NOTIFY vehicleStateChanged)
    Q_PROPERTY(bool    key          READ key          NOTIFY keyChanged)
    Q_PROPERTY(bool    brake        READ brake        NOTIFY brakeChanged)

    /* ── Odometer ───────────────────────────────────────── */
    Q_PROPERTY(int     odometerM    READ odometerM    NOTIFY odometerMChanged)

    /* ── Fault ──────────────────────────────────────────── */
    Q_PROPERTY(int     faultFlags   READ faultFlags   NOTIFY faultFlagsChanged)
    Q_PROPERTY(bool    hasFault     READ hasFault     NOTIFY faultFlagsChanged)

    /* ── Computed display helpers (QML convenience) ─────── */
    Q_PROPERTY(QString stateLabel   READ stateLabel   NOTIFY vehicleStateChanged)
    Q_PROPERTY(QString gearLabel    READ gearLabel    NOTIFY gearChanged)
    Q_PROPERTY(QString modeLabel    READ modeLabel    NOTIFY driveModeChanged)
    Q_PROPERTY(QColor  stateColor   READ stateColor   NOTIFY vehicleStateChanged)

public:
    explicit VcuBridge(QObject* parent = nullptr);

    /* ── Property getters ── */
    double  speedKmh()     const;
    int     accelPct()     const;
    int     motorPwmPct()  const;
    double  battVoltage()  const;
    int     socPercent()   const;
    bool    battLow()      const;
    int     gear()         const;
    int     driveMode()    const;
    int     vehicleState() const;
    int     odometerM()    const;
    int     faultFlags()   const;
    bool    hasFault()     const;
    bool    key()          const;
    bool    brake()        const;

    /* ── Computed helpers ── */
    QString stateLabel()   const;
    QString gearLabel()    const;
    QString modeLabel()    const;
    QColor  stateColor()   const;

    /**
     * @brief Gọi từ CanReceiver callback để notify Qt event loop
     *        Thread-safe: postEvent → Qt main thread xử lý
     */
    void notifyDataReady();

    /**
     * @brief Gọi từ StateMachine callback khi state thay đổi
     */
    void notifyStateChanged(VehicleState new_state);

signals:
    void speedKmhChanged();
    void accelPctChanged();
    void motorPwmPctChanged();
    void battVoltageChanged();
    void socPercentChanged();
    void battLowChanged();
    void gearChanged();
    void driveModeChanged();
    void vehicleStateChanged();
    void odometerMChanged();
    void faultFlagsChanged();
    void keyChanged();
    void brakeChanged();

private slots:
    /**
     * @brief Đọc VehicleData và emit signals nếu có thay đổi
     *        Chỉ gọi trên Qt main thread (qua QTimer hoặc invokeMethod)
     */
    void refreshFromData();

private:
    /* Cache giá trị cũ — chỉ emit signal khi thực sự thay đổi */
    double   m_speed_kmh    {0.0};
    int      m_accel_pct    {0};
    int      m_motor_pwm    {0};
    double   m_batt_voltage {0.0};
    int      m_soc_pct      {100};
    bool     m_batt_low     {false};
    int      m_gear         {static_cast<int>(GearPosition::P)};
    int      m_drive_mode   {static_cast<int>(DriveMode::NORMAL)};
    int      m_vehicle_state{static_cast<int>(VehicleState::OFF)};
    int      m_odometer_m   {0};
    int      m_fault_flags  {0};
    bool     m_key          {false};
    bool     m_brake        {false};

    /* Polling timer — fallback nếu callback không được set */
    QTimer*  m_poll_timer   {nullptr};
};

#endif /* VCU_BRIDGE_H */
