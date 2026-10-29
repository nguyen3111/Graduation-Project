import QtQuick
import QtQuick.Window
import "UI"

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "EV IPC Simulation"
    color: "#000000"

    // ── lưu state trước để detect transition ───────────
    property string previousVehicleState: "OFF"

    // ── helper: map vehicle_state → đường dẫn QML file ──
    function sourceForState(state) {
        if (state === "STANDBY")
            return "qrc:/UI/StandbyScreen.qml";
        if (state === "READY")
            return "qrc:/UI/StandbyScreen.qml";
        if (state === "DRIVING")
            return "qrc:/UI/StandbyScreen.qml";
        if (state === "CHARGING")
            return "qrc:/UI/ChargingScreen.qml";
        return "qrc:/UI/OffScreen.qml";
    }

    // ── lắng nghe vehicle_state thay đổi ────────────────
    Connections {
        target: vcuBridge

        function onVehicleStateChanged() {
            var prev = root.previousVehicleState;
            var curr = vcuBridge.stateLabel;

            // [1] FAULT — ưu tiên tuyệt đối
            // dừng mọi animation đang chạy ngay lập tức
            // FaultOverlay Loader sẽ tự hiện vì active: faultStatus
            if (curr === "FAULT") {
                bootAnim.visible = false;
                shutdownAnim.visible = false;
                root.previousVehicleState = curr;
                return;
            }

            // [2] OFF → STANDBY — chạy boot animation
            // load StandbyScreen trước (ẩn dưới bootAnim)
            // bootAnim phủ lên trên, chạy 2.5s rồi ẩn đi
            if (prev === "OFF" && curr === "STANDBY") {
                screenLoader.source = "qrc:/UI/StandbyScreen.qml";
                bootAnim.visible = true;
                bootAnim.play();
                root.previousVehicleState = curr;
                return;
            }

            // [5] CHARGING → OFF hoặc STANDBY — chạy charging stop animation
            if (prev === "CHARGING" && (curr === "OFF" || curr === "STANDBY")) {
                chargingStopAnim.visible = true;
                chargingStopAnim.play();
                root.previousVehicleState = curr;
                return;
            }
            // [3] bất kỳ → OFF — chạy shutdown animation
            // shutdownAnim phủ lên screen hiện tại
            // sau khi xong mới swap screenLoader về OffScreen
            if (curr === "OFF") {
                shutdownAnim.visible = true;
                shutdownAnim.play();
                root.previousVehicleState = curr;
                return;
            }

            // [4] các transition thường — swap + fade 300ms
            screenLoader.source = sourceForState(curr);
            root.previousVehicleState = curr;
        }
    }

    Item {
        id: screenRoot
        anchors.fill: parent

        // ─────────────────────────────────────────
        // Screen Loader
        // ─────────────────────────────────────────
        Loader {
            id: screenLoader
            anchors.fill: parent
            source: "qrc:/UI/OffScreen.qml"

            // fade 300ms mỗi khi swap screen thường
            onSourceChanged: {
                screenLoader.opacity = 0.0;
                fadeIn.restart();
            }
            NumberAnimation {
                id: fadeIn
                target: screenLoader
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 300
                easing.type: Easing.OutQuad
            }
        }

        // ─────────────────────────────────────────
        // Fault Overlay
        // ─────────────────────────────────────────
        Loader {
            anchors.fill: parent
            source: "qrc:/UI/FaultOverlay.qml"
            active: vcuBridge.hasFault
            z: 999
        }

        // ── boot animation overlay ─────────────────────
        // z: 100 — trên screenLoader, dưới FaultOverlay
        // visible được control bởi onVehicleStateChanged
        BootAnimation {
            id: bootAnim
            anchors.fill: parent
            z: 100

            onFinished: {
                // animation xong → ẩn overlay
                // StandbyScreen bên dưới tự lộ ra
                bootAnim.visible = false;
            }
        }

        // ── shutdown animation overlay ─────────────────
        // z: 100 — trên screenLoader, dưới FaultOverlay
        ShutdownAnimation {
            id: shutdownAnim
            anchors.fill: parent
            z: 100

            onFinished: {
                // animation xong → swap về OffScreen → ẩn overlay
                screenLoader.source = "qrc:/UI/OffScreen.qml";
                shutdownAnim.visible = false;
            }
        }

        ChargingStopAnimation {
            id: chargingStopAnim
            anchors.fill: parent
            z: 100

            onFinished: {
                screenLoader.source = sourceForState(vcuBridge.stateLabel);
                chargingStopAnim.visible = false;
            }
        }
    }
}
