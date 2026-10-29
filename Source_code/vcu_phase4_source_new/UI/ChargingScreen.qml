import QtQuick

Rectangle {
    id: chargingRoot
    anchors.fill: parent
    color: "#050d14"

    // ── TOP BAR — giống StandbyScreen ──────────────
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        color: "#080c18"

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1a2030"
        }

        // tell-tales — góc trái
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Column {
                spacing: 6
                Text {
                    text: "WARNINGS"
                    color: "#444466"
                    font.pixelSize: 10
                    font.family: "monospace"
                    font.letterSpacing: 2
                }
                Row {
                    spacing: 6

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: vcuBridge.key ? "#0a1a0a" : "#111118"
                        border.color: vcuBridge.key ? "#00cc66" : "#222233"
                        border.width: vcuBridge.key ? 1 : 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "🔑"
                            font.pixelSize: 14
                            color: vcuBridge.key ? "#00cc66" : "#222244"
                        }
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: vcuBridge.socPercent <= 20 ? "#1a1100" : "#111118"
                        border.color: vcuBridge.socPercent <= 20 ? "#ffaa00" : "#222233"
                        border.width: 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "⚡"
                            font.pixelSize: 14
                            color: vcuBridge.socPercent <= 20 ? "#ffaa00" : "#222244"
                        }
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: vcuBridge.hasFault ? "#1a0000" : "#111118"
                        border.color: vcuBridge.hasFault ? "#ff4444" : "#222233"
                        border.width: 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "⚠"
                            font.pixelSize: 14
                            color: vcuBridge.hasFault ? "#ff4444" : "#222233"
                        }
                    }
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 16
                spacing: 4
                Text {
                    text: "VOLTAGE"
                    color: "#444466"
                    font.pixelSize: 10
                    font.family: "monospace"
                    font.letterSpacing: 2
                }
                Text {
                    text: vcuBridge.battVoltage.toFixed(2) + "V"
                    color: "#4488aa"
                    font.pixelSize: 22
                    font.family: "monospace"
                    font.bold: true
                }
            }
        }

        // vehicle state badge — giữa
        Rectangle {
            anchors.centerIn: parent
            width: 400
            height: 34
            radius: 6
            color: "#0d1525"
            border.color: "#00aaff"
            border.width: 1
            Text {
                anchors.centerIn: parent
                text: "VEHICLE STATE: CHARGING"
                color: "#00aaff"
                font.pixelSize: 13
                font.letterSpacing: 4
            }
        }

        // time — phải
        Column {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "TIME"
                color: "#444466"
                font.pixelSize: 10
                font.family: "monospace"
                font.letterSpacing: 2
            }
            Text {
                text: "--:--"
                color: "#556677"
                font.pixelSize: 28
                font.family: "monospace"
                font.bold: true
            }
        }
    }

    // ── BOTTOM BAR — giống StandbyScreen ───────────
    Rectangle {
        id: bottomBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        color: "#080c18"

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1a2030"
        }

        // hint text
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: 400
            height: 20
            verticalAlignment: Text.AlignVCenter
            text: "Unplug to stop charging"
            color: "#444466"
            font.pixelSize: 12
            font.letterSpacing: 1
        }
    }

    // ── divider dọc trái ───────────────────────────
    Rectangle {
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        x: 310
        width: 1
        color: "#1a2030"
    }

    // ── VÙNG TRÁI ──────────────────────────────────
    Item {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.bottom: bottomBar.top
        width: 310

        // charging status — trên cùng
        Column {
            anchors.top: parent.top
            anchors.topMargin: 14
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "CHARGING STATUS"
                color: "#444466"
                font.pixelSize: 10
                font.family: "monospace"
                font.letterSpacing: 3
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 150
                height: 34
                radius: 6
                color: "#0a1a2a"
                border.color: "#00aaff"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "PLUGGED IN"
                    color: "#00aaff"
                    font.pixelSize: 13
                    font.family: "monospace"
                    font.bold: true
                    font.letterSpacing: 2
                }
            }
        }

        // xe cắm sạc — giữa
        Item {
            id: carLeftBlock
            anchors.top: parent.top
            anchors.topMargin: 90
            anchors.horizontalCenter: parent.horizontalCenter
            width: 210
            height: 220
            // xe top view placeholder
            Image {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 20
                width: 180
                source: "qrc:/ui/car.png"
                fillMode: Image.PreserveAspectFit

                onStatusChanged: {
                    if (status === Image.Error)
                        console.log("Image load error:", source);
                    if (status === Image.Ready)
                        console.log("Image loaded OK:", source);
                }
            }

            Canvas {
                id: cableCanvas
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: -5
                height: parent.height + 5

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // ── dây sạc ──────────────────────────────
                    ctx.beginPath();
                    ctx.moveTo(0, 18);
                    ctx.bezierCurveTo(13, 18, 18, 26, 18, 36);
                    ctx.lineTo(18, 51);
                    ctx.bezierCurveTo(18, 57, 22, 59, 27, 59);
                    ctx.strokeStyle = "#00ff99";
                    ctx.lineWidth = 2.0;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";
                    ctx.globalAlpha = 1.0;
                    ctx.stroke();

                    // ── đầu cắm sạc ──────────────────────────
                    ctx.globalAlpha = 0.92;
                    ctx.beginPath();
                    ctx.moveTo(27, 56);
                    ctx.lineTo(46, 55);
                    ctx.lineTo(46, 63);
                    ctx.lineTo(27, 62);
                    ctx.bezierCurveTo(26, 62, 26, 56, 27, 56);
                    ctx.closePath();
                    ctx.fillStyle = "#00ff99";
                    ctx.fill();

                    // viền đầu cắm
                    ctx.beginPath();
                    ctx.moveTo(27, 57);
                    ctx.lineTo(46, 56);
                    ctx.lineTo(46, 62);
                    ctx.lineTo(27, 61);
                    ctx.strokeStyle = "#00aa55";
                    ctx.lineWidth = 0.6;
                    ctx.globalAlpha = 0.5;
                    ctx.stroke();

                    // đầu phải bo tròn
                    ctx.globalAlpha = 0.85;
                    ctx.beginPath();
                    ctx.roundRect(45, 56, 3, 7, 1.5);
                    ctx.fillStyle = "#00bb66";
                    ctx.fill();

                    // glow
                    ctx.globalAlpha = 0.2;
                    ctx.beginPath();
                    ctx.roundRect(26, 55, 22, 9, 3);
                    ctx.strokeStyle = "#00ff99";
                    ctx.lineWidth = 0.8;
                    ctx.stroke();

                    // điểm đầu dây
                    ctx.globalAlpha = 0.6;
                    ctx.beginPath();
                    ctx.arc(0, 18, 2.5, 0, 2 * Math.PI);
                    ctx.fillStyle = "#00ff99";
                    ctx.fill();
                }
            }
        }

        // gear selector — dưới cùng
        Column {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "GEAR"
                color: "#444466"
                font.pixelSize: 10
                font.family: "monospace"
                font.letterSpacing: 3
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Repeater {
                    model: ["P", "R", "N", "D"]
                    Rectangle {
                        width: 50
                        height: 44
                        radius: 6
                        color: vcuBridge.gearLabel === modelData ? "#0a1e0a" : "#111118"
                        border.color: vcuBridge.gearLabel === modelData ? "#00ff99" : "#1a1a2a"
                        border.width: vcuBridge.gearLabel === modelData ? 1.5 : 0.5
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: vcuBridge.gearLabel === modelData ? "#00ff99" : "#2a2a44"
                            font.pixelSize: 22
                            font.family: "monospace"
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    // ── VÙNG PHẢI ──────────────────────────────────
    Item {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.leftMargin: 311
        anchors.right: parent.right
        anchors.bottom: bottomBar.top

        // 2 card thông tin — trên cùng, căn đều
        Row {
            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

            // thời gian còn lại
            Rectangle {
                width: 310
                height: 100
                radius: 8
                color: "#0d1525"
                border.color: "#1a2535"
                border.width: 0.5

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "THỜI GIAN CÒN LẠI"
                        color: "#444466"
                        font.pixelSize: 10
                        font.family: "monospace"
                        font.letterSpacing: 2
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "--:--"
                        color: "#00aaff"
                        font.pixelSize: 42
                        font.family: "monospace"
                        font.bold: true
                    }
                }
            }

            // % pin hiện tại
            Rectangle {
                width: 310
                height: 100
                radius: 8
                color: "#0d1525"
                border.color: "#1a2535"
                border.width: 0.5

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "PIN HIỆN TẠI"
                        color: "#444466"
                        font.pixelSize: 10
                        font.family: "monospace"
                        font.letterSpacing: 2
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: vcuBridge.socPercent + "%"
                        color: {
                            if (vcuBridge.socPercent <= 20)
                                return "#ff6633";
                            return "#00ff99";
                        }
                        font.pixelSize: 42
                        font.family: "monospace"
                        font.bold: true
                    }
                }
            }
        }

        // pin lớn — giữa vùng phải
        Item {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 30
            width: 400
            height: 80

            // ── thân pin ──────────────────────────────
            Rectangle {
                id: battBody
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - 20
                radius: 10
                color: "#111122"
                border.color: {
                    if (vcuBridge.socPercent <= 20)
                        return "#ff6633";
                    return "#00ff99";
                }
                border.width: 1.5

                // fill màu theo SoC
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 4
                    width: Math.max(0, (parent.width - 8) * vcuBridge.socPercent / 100)
                    radius: 7
                    color: {
                        if (vcuBridge.socPercent <= 20)
                            return "#ff6633";
                        return "#00ff99";
                    }

                    Behavior on width {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuad
                        }
                    }

                    opacity: vcuBridge.socPercent === 100 ? 0.85 : 0.85

                    SequentialAnimation on opacity {
                        running: vcuBridge.socPercent <= 20
                        loops: Animation.Infinite
                        NumberAnimation {
                            to: 0.3
                            duration: 300
                        }
                        NumberAnimation {
                            to: 0.85
                            duration: 300
                        }
                    }
                    SequentialAnimation on opacity {
                        running: vcuBridge.socPercent > 20 && vcuBridge.socPercent <= 80
                        loops: Animation.Infinite
                        NumberAnimation {
                            to: 0.5
                            duration: 400
                        }
                        NumberAnimation {
                            to: 0.85
                            duration: 400
                        }
                    }
                    SequentialAnimation on opacity {
                        running: vcuBridge.socPercent > 80 && vcuBridge.socPercent < 100
                        loops: Animation.Infinite
                        NumberAnimation {
                            to: 0.3
                            duration: 1200
                        }
                        NumberAnimation {
                            to: 0.7
                            duration: 1200
                        }
                    }
                }

                // ── ⚡ nhấp nháy ──────────────────────
                Text {
                    anchors.centerIn: parent
                    text: "⚡"
                    font.pixelSize: 36
                    color: "#00ff99"
                    SequentialAnimation on opacity {
                        running: true
                        loops: Animation. Infinite
                        NumberAnimation {
                            to: 0.2
                            duration: 600
                        }
                        NumberAnimation {
                            to: 1.0
                            duration: 600
                        }
                    }
                }
            }

            // ── đầu cực + ─────────────────────────────
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: parent.height * 0.45
                radius: 4
                color: "#334455"
                border.color: "#445566"
                border.width: 0.5
            }
        }
    }
}
