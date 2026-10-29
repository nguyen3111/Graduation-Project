import QtQuick

Rectangle {
    anchors.fill: parent
    color: "#000000"

    Column {
        anchors.centerIn: parent
        spacing: 20

        // logo text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "EV SYSTEM"
            color: "#ffffff"
            font.pixelSize: 60
            font.bold: true
            font.letterSpacing: 8
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "POWERED OFF"
            color: "#444444"
            font.pixelSize: 20
            font.letterSpacing: 4
        }
    }
}