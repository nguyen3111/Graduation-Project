import QtQuick

Rectangle {
    id: bootRoot
    anchors.fill: parent
    z: 100
    visible: false
    color: "#000000"

    signal finished

    function play() {
        bootRoot.opacity = 1.0;
        bgRect.opacity = 0.0;
        logoText.opacity = 0.0;
        logoText.scale = 0.85;
        taglineText.opacity = 0.0;
        contentColumn.opacity = 1.0;
        bootSequence.restart();
    }

    Rectangle {
        id: bgRect
        anchors.fill: parent
        opacity: 0.0
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: "#0a1628"
            }
            GradientStop {
                position: 1.0
                color: "#050d1a"
            }
        }
    }

    Column {
        id: contentColumn
        anchors.centerIn: parent
        spacing: 12
        opacity: 1.0

        Text {
            id: logoText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "EVSYS"
            color: "#ffffff"
            font.pixelSize: 64
            font.bold: true
            font.letterSpacing: 12
            opacity: 0.0
            scale: 0.85
            transformOrigin: Item.Center
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 140
            height: 1
            color: "#00aaff"
            opacity: 0.6
        }

        Text {
            id: taglineText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "ELECTRIC VEHICLE SYSTEM"
            color: "#00aaff"
            font.pixelSize: 13
            font.letterSpacing: 4
            opacity: 0.0
        }
    }

    // ── tổng 0.9s ──────────────────────────────
    SequentialAnimation {
        id: bootSequence

        // Phase 1 — 0.0s đến 0.2s
        // bg + logo cùng lúc
        ParallelAnimation {
            NumberAnimation {
                target: bgRect
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: logoText
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: logoText
                property: "scale"
                from: 0.85
                to: 1.0
                duration: 200
                easing.type: Easing.OutQuad
            }
        }

        // Phase 2 — 0.2s đến 0.5s
        // tagline fade in
        NumberAnimation {
            target: taglineText
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 300
            easing.type: Easing.OutQuad
        }

        // giữ 0.1s
        PauseAnimation {
            duration: 100
        }

        // Phase 3 — 0.6s đến 0.9s
        NumberAnimation {
            target: bootRoot
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 300
            easing.type: Easing.InOutQuad
        }

        ScriptAction {
            script: bootRoot.finished()
        }
    }
}
