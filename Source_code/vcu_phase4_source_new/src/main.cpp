/**
 * @file    main.cpp
 * @brief   VCU IPC Application — entry point
 *
 * Khởi tạo theo thứ tự:
 *   1. QGuiApplication + QQmlApplicationEngine (Qt event loop)
 *   2. VcuBridge — expose sang QML
 *   3. CanReceiver — khởi động receive thread
 *   4. StateMachine — khởi động FSM thread
 *   5. CanSender — khởi động transmit thread
 *   6. Kết nối callback: CanReceiver → VcuBridge
 *                        StateMachine → VcuBridge
 *   7. Load QML UI
 *   8. Chạy Qt event loop (app.exec())
 *
 * Shutdown:
 *   Khi cửa sổ đóng, Qt gọi lastWindowClosed signal → app.quit()
 *   → destructor lần lượt stop() các thread → join → sạch sẽ.
 *
 * @note  Trên Raspberry Pi với Waveshare 7 inch, chạy với:
 *          QT_QPA_PLATFORM=xcb ./vcu_ipc
 *        hoặc nếu dùng eglfs (không có X11):
 *          QT_QPA_PLATFORM=eglfs ./vcu_ipc
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "vcu_bridge.h"
#include "can_receiver.h"
#include "can_sender.h"
#include "state_machine.h"

int main(int argc, char* argv[])
{
    /* ---- 1. Qt Application ---- */
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VCU IPC"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    /* ---- 2. VcuBridge — tạo trước khi load QML ---- */
    VcuBridge vcu_bridge;

    /* ---- 3. CAN Receiver ---- */
    CanReceiver can_rx("can0");

    /* Kết nối callback: mỗi CAN frame → notify Qt để update UI */
    can_rx.setOnFrameReceived([&vcu_bridge]()
    {
        vcu_bridge.notifyDataReady();
    });

    if (!can_rx.start())
    {
        fprintf(stderr, "[main] WARNING: CanReceiver failed — chạy ở chế độ demo\n");
        /* Không exit — cho phép UI chạy với giá trị mặc định */
    }

    /* ---- 4. State Machine ---- */
    StateMachine fsm;

    fsm.setOnStateChange([&vcu_bridge](VehicleState new_state)
    {
        vcu_bridge.notifyStateChanged(new_state);
    });

    (void)fsm.start();

    /* ---- 5. CAN Sender ---- */
    CanSender can_tx("can0");
    if (!can_tx.start())
    {
        fprintf(stderr, "[main] WARNING: CanSender failed\n");
    }

    /* ---- 6. QML Engine ---- */
    QQmlApplicationEngine engine;

    /* Expose VcuBridge sang QML với tên "vcuBridge" */
    engine.rootContext()->setContextProperty(
        QStringLiteral("vcuBridge"), &vcu_bridge);

    /* Load main QML file */
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty())
    {
        fprintf(stderr, "[main] ERROR: QML load failed\n");
        return 1;
    }

    fprintf(stdout, "[main] VCU IPC started successfully\n");

    /* ---- 7. Qt event loop ---- */
    int ret = app.exec();

    /* ---- 8. Cleanup — thứ tự ngược lại ---- */
    can_tx.stop();
    fsm.stop();
    can_rx.stop();

    fprintf(stdout, "[main] VCU IPC exited cleanly\n");
    return ret;
}
