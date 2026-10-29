import QtQuick

Rectangle {
    id: shutdownRoot
    anchors.fill: parent
    z: 100
    visible: false
    color: "transparent"

    // ── signal báo Main.qml khi animation xong ─────────
    signal finished

    // ── Main.qml gọi hàm này để bắt đầu ───────────────
    function play() {
        // reset về trạng thái ban đầu
        overlayRect.opacity = 0.0;
        shutdownLabel.opacity = 0.0;
        shutdownSequence.restart();
    }

    // ─────────────────────────────────────────────────────
    // Layer 1: overlay đen
    // Phase 1 fade nhanh từ 0 → 0.85 để che screen cũ
    // Phase 3 fade tiếp từ 0.85 → 1.0 để về đen hoàn toàn
    // ─────────────────────────────────────────────────────
    Rectangle {
        id: overlayRect
        anchors.fill: parent
        color: "#000000"
        opacity: 0.0
    }

    // ─────────────────────────────────────────────────────
    // Layer 2: chữ POWERING OFF
    // xuất hiện trên nền tối, rồi mờ dần
    // ─────────────────────────────────────────────────────
    Text {
        id: shutdownLabel
        anchors.centerIn: parent
        text: "POWERING OFF"
        color: "#666666"
        font.pixelSize: 18
        font.letterSpacing: 6
        opacity: 0.0
    }

    // ─────────────────────────────────────────────────────
    // Animation Sequence — tổng 1.5s
    // ─────────────────────────────────────────────────────
    SequentialAnimation {
        id: shutdownSequence

        // Phase 1 — 0.0s đến 0.3s
        // overlay đen phủ nhanh lên screen cũ
        NumberAnimation {
            target: overlayRect
            property: "opacity"
            from: 0.0
            to: 0.85
            duration: 300
            easing.type: Easing.InQuad
        }

        // Phase 2 — 0.3s đến 0.8s
        // chữ POWERING OFF fade in trên nền tối
        NumberAnimation {
            target: shutdownLabel
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 500
            easing.type: Easing.OutQuad
        }

        // giữ 0.4s
        PauseAnimation {
            duration: 400
        }

        // Phase 3 — 1.2s đến 1.5s
        // tất cả fade to black hoàn toàn
        ParallelAnimation {
            NumberAnimation {
                target: shutdownLabel
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 300
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: overlayRect
                property: "opacity"
                from: 0.85
                to: 1.0
                duration: 300
                easing.type: Easing.InQuad
            }
        }

        // kết thúc — báo Main.qml
        ScriptAction {
            script: shutdownRoot.finished()
        }
    }
}
