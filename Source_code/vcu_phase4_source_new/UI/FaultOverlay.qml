import QtQuick

Rectangle {
    anchors.fill: parent
    color: "#aa000000"
    z: 999

    Column {
        anchors.centerIn: parent
        spacing: 24

        // icon cảnh báo
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "⚠"
            color: "#ff4444"
            font.pixelSize: 80
        }

        // tiêu đề FAULT
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "FAULT DETECTED"
            color: "#ff4444"
            font.pixelSize: 36
            font.bold: true
            font.letterSpacing: 4
        }

        // message theo loại fault — thay vì text cứng
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: vcuBridge.faultFlags.toString(16).toUpperCase()
            color: "#ff8888"
            font.pixelSize: 26
            font.bold: true
        }

        // animation nhấp nháy
        SequentialAnimation on opacity {
            running: true
            loops: Animation.Infinite
            NumberAnimation {
                to: 0.7
                duration: 500
            }
            NumberAnimation {
                to: 1.0
                duration: 500
            }
        }
    }
}
