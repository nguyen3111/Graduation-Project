import QtQuick

Rectangle {
    id: chargingStopRoot
    anchors.fill: parent
    z: 100
    visible: false
    color: "transparent"

    signal finished

    function play() {
        overlayRect.opacity = 0.0;
        stopLabel.opacity = 0.0;
        subLabel.opacity = 0.0;
        chargingStopRoot.visible = true;
        stopSequence.restart();
    }

    Rectangle {
        id: overlayRect
        anchors.fill: parent
        color: "#000000"
        opacity: 0.0
    }

    Text {
        id: stopLabel
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -14
        text: "CHARGING STOPPED"
        color: "#00ff99"
        font.pixelSize: 20
        font.letterSpacing: 6
        opacity: 0.0
    }

    Text {
        id: subLabel
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 14
        text: vcuBridge.socPercent + "%  —  " + vcuBridge.battVoltage.toFixed(2) + "V"
        color: "#446655"
        font.pixelSize: 13
        font.family: "monospace"
        font.letterSpacing: 3
        opacity: 0.0
    }

    SequentialAnimation {
        id: stopSequence

        // Phase 1 — overlay phủ nhanh
        NumberAnimation {
            target: overlayRect
            property: "opacity"
            from: 0.0
            to: 0.85
            duration: 250
            easing.type: Easing.InQuad
        }

        // Phase 2 — hiện chữ
        ParallelAnimation {
            NumberAnimation {
                target: stopLabel
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 300
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: subLabel
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 300
                easing.type: Easing.OutQuad
            }
        }

        // giữ 1s
        PauseAnimation {
            duration: 1000
        }

        // Phase 3 — fade out toàn bộ
        ParallelAnimation {
            NumberAnimation {
                target: stopLabel
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 400
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: subLabel
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 400
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: overlayRect
                property: "opacity"
                from: 0.85
                to: 1.0
                duration: 400
                easing.type: Easing.InQuad
            }
        }

        ScriptAction {
            script: chargingStopRoot.finished()
        }
    }
}
