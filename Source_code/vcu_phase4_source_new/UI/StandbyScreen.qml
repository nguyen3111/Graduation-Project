import QtQuick

Rectangle {
    id: standbyRoot
    anchors.fill: parent
    color: {
        if (vcuBridge.modeLabel === "ECO")
            return "#061a0e";
        if (vcuBridge.modeLabel === "SPORT")
            return "#1a0a0a";
        return "#0a0f1e";
    }

    Behavior on color {
        ColorAnimation {
            duration: 400
        }
    }

    // Thêm property selfTestRunning vào đầu file
    property bool selfTestRunning: false
    property real sweepValue: 0

    Component.onCompleted: {
        selfTestRunning = true;
        selfTestSequence.restart();
    }

    // Thêm 2 helper function
    function modeColor() {
        if (vcuBridge.modeLabel === "ECO")
            return "#00ff99";
        if (vcuBridge.modeLabel === "SPORT")
            return "#ff4444";
        return "#00aaff";
    }

    function modeMaxSpeed() {
        if (vcuBridge.modeLabel === "ECO")
            return 0.8;
        if (vcuBridge.modeLabel === "SPORT")
            return 1.6;
        return 1.3;
    }

    // animate maxSpeed khi đổi mode — kim trượt mượt
    property real animatedMaxSpeed: 1.3

    // khi driveMode đổi → trigger animation về maxSpeed mới
    Connections {
        target: vcuBridge
        function onDriveModeChanged() {
            standbyRoot.animatedMaxSpeed = standbyRoot.modeMaxSpeed();
            maxSpeedAnim.to = standbyRoot.modeMaxSpeed();
            maxSpeedAnim.restart();
        }
    }

    NumberAnimation {
        id: maxSpeedAnim
        target: standbyRoot
        property: "animatedMaxSpeed"
        duration: 400
        easing.type: Easing.InOutQuad
    }
    // khai báo id các vùng để anchor
    // topBar và bottomBar được khai báo trước
    // các vùng giữa anchor vào topBar.bottom và bottomBar.top

    // ── top bar container ──────────────────────────
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        color: "#080c18"

        // ── đường kẻ dưới top bar ──
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1a2030"
        }

        // ── tell-tales — góc trái ──────────────────
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // label WARNINGS phía trên — dùng Column bọc ngoài
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

                    // 🔑 key — active khi keyState = true
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: (selfTestRunning || vcuBridge.key) ? "#0a1a0a" : "#111118"
                        border.color: (selfTestRunning || vcuBridge.key) ? "#00cc66" : "#222233"
                        border.width: (selfTestRunning || vcuBridge.key) ? 1 : 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "🔑"
                            font.pixelSize: 14
                            color: (selfTestRunning || vcuBridge.key) ? "#00cc66" : "#222244"
                        }
                    }

                    // ⚡ soc low — active khi soc <= 20
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: (selfTestRunning || vcuBridge.socPercent <= 20) ? "#1a1100" : "#111118"
                        border.color: (selfTestRunning || vcuBridge.socPercent <= 20) ? "#ffaa00" : "#222233"
                        border.width: 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "⚡"
                            font.pixelSize: 14
                            color: (selfTestRunning || vcuBridge.socPercent <= 20) ? "#ffaa00" : "#222244"
                        }
                    }

                    // ⚠ fault — active khi faultStatus = true
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 4
                        color: (selfTestRunning || vcuBridge.hasFault) ? "#1a0000" : "#111118"
                        border.color: (selfTestRunning || vcuBridge.hasFault) ? "#ff4444" : "#222233"
                        border.width: 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "⚠"
                            font.pixelSize: 14
                            color: (selfTestRunning || vcuBridge.hasFault) ? "#ff4444" : "#222233"
                        }
                    }
                }
            }

            // ── voltage ────────────────────────────
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

        // ── vehicle state — giữa top bar ───────────
        Rectangle {
            anchors.centerIn: parent
            width: 400
            height: 34
            radius: 6
            color: "#0d1525"
            border.color: {
                if (vcuBridge.stateLabel === "READY")
                    return "#00ff99";
                if (vcuBridge.stateLabel === "DRIVING")
                    return "#00aaff";
                return "#1a2535";
            }
            border.width: vcuBridge.stateLabel === "STANDBY" ? 0.5 : 1

            Text {
                anchors.centerIn: parent
                text: "VEHICLE STATE: " + vcuBridge.stateLabel
                color: {
                    if (vcuBridge.stateLabel === "READY")
                        return "#00ff99";
                    if (vcuBridge.stateLabel === "DRIVING")
                        return "#00aaff";
                    return "#00aaff";
                }
                font.pixelSize: 13
                font.letterSpacing: 4
            }
        }

        // ── time — góc phải ────────────────────────
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

    // ── vùng trái ──────────────────────────────────
    // ── vùng trái ──────────────────────────────────
    Item {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.bottom: bottomBar.top
        width: 310

        // drive mode — neo lên trên
        Column {
            id: driveModeBlock
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "DRIVE MODE"
                color: "#444466"
                font.pixelSize: 10
                font.family: "monospace"
                font.letterSpacing: 3
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 160
                height: 32
                radius: 6
                color: "#0a1a2a"
                border.color: standbyRoot.modeColor()
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: vcuBridge.modeLabel
                    color: standbyRoot.modeColor()
                    font.pixelSize: 16
                    font.family: "monospace"
                    font.bold: true
                    font.letterSpacing: 3
                }
            }
        }

        // speed + km/h — neo xuống dưới
        Column {
            id: speedBlock
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: vcuBridge.speedKmh.toFixed(1)
                color: vcuBridge.stateLabel === "DRIVING" ? standbyRoot.modeColor() : "#ffffff"
                font.pixelSize: 64
                font.bold: true

                Behavior on color {
                    ColorAnimation {
                        duration: 300
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "km/h"
                color: "#445566"
                font.pixelSize: 14
                font.family: "monospace"
                font.letterSpacing: 2
            }
        }

        // ── không gian xe — giữa driveMode và speed ────
        Item {
            anchors.top: driveModeBlock.bottom
            anchors.bottom: speedBlock.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.bottomMargin: 8

            // 2 đường sọc — không nền, mờ dần lên trên
            Canvas {
                id: roadCanvas
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // điểm trên — giữ nguyên
                    var ltx = width * 0.38;
                    var rtx = width * 0.62;
                    var ty = 0;

                    // điểm dưới — gần hơn
                    var lbx = width * 0.2;
                    var rbx = width * 0.8;
                    var by = height;

                    // ── đường trái — 1 nét trắng, mờ dần lên trên ──
                    var gradL = ctx.createLinearGradient(ltx, ty, lbx, by);
                    gradL.addColorStop(0, "rgba(255,255,255,0.0)");
                    gradL.addColorStop(0.5, "rgba(255,255,255,0.5)");
                    gradL.addColorStop(1, "rgba(255,255,255,1.0)");

                    ctx.beginPath();
                    ctx.moveTo(ltx, ty);
                    ctx.lineTo(lbx, by);
                    ctx.strokeStyle = gradL;
                    ctx.lineWidth = 1.5;
                    ctx.globalAlpha = 1.0;
                    ctx.lineCap = "round";
                    ctx.stroke();

                    // ── đường phải — 1 nét trắng, mờ dần lên trên ──
                    var gradR = ctx.createLinearGradient(rtx, ty, rbx, by);
                    gradR.addColorStop(0, "rgba(255,255,255,0.0)");
                    gradR.addColorStop(0.5, "rgba(255,255,255,0.5)");
                    gradR.addColorStop(1, "rgba(255,255,255,1.0)");

                    ctx.beginPath();
                    ctx.moveTo(rtx, ty);
                    ctx.lineTo(rbx, by);
                    ctx.strokeStyle = gradR;
                    ctx.lineWidth = 1.5;
                    ctx.globalAlpha = 1.0;
                    ctx.lineCap = "round";
                    ctx.stroke();
                }
            }

            // ảnh xe — giữa theo chiều ngang, neo xuống dưới
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 0
                width: 70
                source: "qrc:/ui/car.png"
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    // divider dọc trái
    Rectangle {
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        x: 310
        width: 1
        color: "#1a2030"
    }

    // ── vùng giữa ──────────────────────────────────
    Item {
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        anchors.left: parent.left
        anchors.leftMargin: 311
        anchors.right: parent.right
        anchors.rightMargin: 311

        Column {
            anchors.centerIn: parent
            spacing: 20

            // ── gauge arc — Canvas ──────────────────
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 300
                height: 220

                // ── Canvas gauge ──────────────────────────────
                Canvas {
                    id: speedCanvas
                    anchors.fill: parent

                    // vẽ lại khi speed, mode, animatedMaxSpeed thay đổi
                    Connections {
                        target: vcuBridge
                        function onSpeedKmhChanged() {
                            speedCanvas.requestPaint();
                        }
                        function onDriveModeChanged() {
                            speedCanvas.requestPaint();
                        }
                    }

                    // vẽ lại khi animatedMaxSpeed thay đổi (animation kim)
                    Connections {
                        target: standbyRoot
                        function onAnimatedMaxSpeedChanged() {
                            speedCanvas.requestPaint();
                        }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        var cx = width / 2;
                        var cy = height - 50;    // tâm gauge gần đáy Item
                        var R = 110;            // bán kính

                        // ── định nghĩa arc 220° ──────────────────
                        // bắt đầu: 200° từ trục x dương (góc dưới trái)
                        // kết thúc: 340° (góc dưới phải)
                        // Math: đổi sang radian, tính từ trục x
                        var startDeg = 160;
                        var totalDeg = 220;
                        var startRad = (startDeg * Math.PI) / 180;
                        var endRad = ((startDeg + totalDeg) * Math.PI) / 180;

                        // ── track nền ───────────────────────────
                        ctx.beginPath();
                        ctx.arc(cx, cy, R, startRad, endRad);
                        ctx.strokeStyle = "#1a2535";
                        ctx.lineWidth = 10;
                        ctx.lineCap = "round";
                        ctx.stroke();

                        // ── arc giá trị ─────────────────────────
                        // dùng animatedMaxSpeed để kim animate mượt khi đổi mode
                        var maxSpd = standbyRoot.animatedMaxSpeed;
                        var ratio = Math.min(vcuBridge.speedKmh.toFixed(1) / maxSpd, 1.0);
                        var valEnd = startRad + ratio * (endRad - startRad);

                        // ── VẠCH CHIA THEO YÊU CẦU ───────────────────────────
                        var maxReal = standbyRoot.modeMaxSpeed();

                        var majorStep = 0.2;
                        var minorStep = 0.1;

                        var majorTicks = [];
                        var minorTicks = [];

                        for (var v = 0; v <= maxReal + 0.001; v += majorStep)
                            majorTicks.push(Number(v.toFixed(1)));

                        for (var m = minorStep; m <= maxReal + 0.001; m += minorStep)
                            if (Math.abs((m / majorStep) - Math.round(m / majorStep)) > 0.01)
                                minorTicks.push(Number(m.toFixed(1)));

                        // Vẽ vạch nhỏ
                        for (var j = 0; j < minorTicks.length; j++) {
                            var mVal = minorTicks[j];
                            if (mVal > maxReal)
                                continue;
                            var mRatio = mVal / maxReal;
                            var mAngle = startRad + mRatio * (endRad - startRad);

                            var mx1 = cx + Math.cos(mAngle) * (R - 10);
                            var my1 = cy + Math.sin(mAngle) * (R - 10);
                            var mx2 = cx + Math.cos(mAngle) * (R - 2);
                            var my2 = cy + Math.sin(mAngle) * (R - 2);

                            ctx.beginPath();
                            ctx.moveTo(mx1, my1);
                            ctx.lineTo(mx2, my2);
                            ctx.strokeStyle = "#334455";
                            ctx.lineWidth = 1.0;
                            ctx.stroke();
                        }

                        // Vẽ vạch lớn + Label
                        for (var i = 0; i < majorTicks.length; i++) {
                            var val = majorTicks[i];
                            if (val > maxReal)
                                continue;

                            var ratioTick = val / maxReal;
                            var angleTick = startRad + ratioTick * (endRad - startRad);

                            var x1 = cx + Math.cos(angleTick) * (R - 18);
                            var y1 = cy + Math.sin(angleTick) * (R - 18);
                            var x2 = cx + Math.cos(angleTick) * (R - 2);
                            var y2 = cy + Math.sin(angleTick) * (R - 2);

                            ctx.beginPath();
                            ctx.moveTo(x1, y1);
                            ctx.lineTo(x2, y2);
                            ctx.strokeStyle = "#aabbcc";
                            ctx.lineWidth = 2.5;
                            ctx.stroke();

                            // Label
                            var labelR = R - 32;
                            var lx = cx + Math.cos(angleTick) * labelR;
                            var ly = cy + Math.sin(angleTick) * labelR;

                            ctx.fillStyle = "#556677";
                            ctx.font = "bold 11px monospace";
                            ctx.textAlign = "center";
                            ctx.textBaseline = "middle";
                            ctx.fillText(val.toFixed(1), lx, ly);
                        }


                        // ── kim ─────────────────────────────────
                        // dùng animatedMaxSpeed để kim trượt mượt
                        var needleAngle = startRad + ratio * (endRad - startRad);
                        var needleLen = R - 15;
                        var tipX = cx + Math.cos(needleAngle) * needleLen;
                        var tipY = cy + Math.sin(needleAngle) * needleLen;

                        // thân kim — hình tam giác mỏng
                        var bw = 4;
                        var backLen = 18;
                        var leftAngle = needleAngle + Math.PI / 2;
                        var rightAngle = needleAngle - Math.PI / 2;

                        ctx.beginPath();
                        ctx.moveTo(tipX, tipY);
                        ctx.lineTo(cx + Math.cos(leftAngle) * bw, cy + Math.sin(leftAngle) * bw);
                        ctx.lineTo(cx + Math.cos(needleAngle + Math.PI) * backLen, cy + Math.sin(needleAngle + Math.PI) * backLen);
                        ctx.lineTo(cx + Math.cos(rightAngle) * bw, cy + Math.sin(rightAngle) * bw);
                        ctx.closePath();
                        ctx.fillStyle = standbyRoot.modeColor();
                        ctx.fill();

                        // tâm kim — vòng tròn nhỏ
                        ctx.beginPath();
                        ctx.arc(cx, cy, 9, 0, 2 * Math.PI);
                        ctx.fillStyle = "#0d1525";
                        ctx.fill();

                        ctx.beginPath();
                        ctx.arc(cx, cy, 5, 0, 2 * Math.PI);
                        ctx.fillStyle = standbyRoot.modeColor();
                        ctx.fill();
                    }
                }

                Canvas {
                    id: sweepCanvas
                    anchors.fill: parent
                    visible: standbyRoot.selfTestRunning
                    z: 10

                    Connections {
                        target: standbyRoot
                        function onSweepValueChanged() {
                            sweepCanvas.requestPaint();
                        }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        var cx = width / 2;
                        var cy = height - 50;
                        var R = 110;

                        var startRad = (160 * Math.PI) / 180;
                        var endRad = (380 * Math.PI) / 180;

                        // track nền
                        ctx.beginPath();
                        ctx.arc(cx, cy, R, startRad, endRad);
                        ctx.strokeStyle = "#1a2535";
                        ctx.lineWidth = 10;
                        ctx.lineCap = "round";
                        ctx.stroke();

                        // sweep arc
                        var maxSpd = standbyRoot.modeMaxSpeed();
                        var ratio = Math.min(standbyRoot.sweepValue / maxSpd, 1.0);
                        var valueEnd = startRad + ratio * (endRad - startRad);

                        if (ratio > 0) {
                            ctx.beginPath();
                            ctx.arc(cx, cy, R, startRad, valueEnd);
                            ctx.strokeStyle = standbyRoot.modeColor();
                            ctx.lineWidth = 10;
                            ctx.lineCap = "round";
                            ctx.stroke();
                        }

                        // kim sweep — luôn vẽ
                        var needleAngle = startRad + ratio * (endRad - startRad);
                        var needleLen = R - 15;
                        var tipX = cx + Math.cos(needleAngle) * needleLen;
                        var tipY = cy + Math.sin(needleAngle) * needleLen;
                        var bw = 4;
                        var backLen = 18;
                        var leftAngle = needleAngle + Math.PI / 2;
                        var rightAngle = needleAngle - Math.PI / 2;

                        ctx.beginPath();
                        ctx.moveTo(tipX, tipY);
                        ctx.lineTo(cx + Math.cos(leftAngle) * bw, cy + Math.sin(leftAngle) * bw);
                        ctx.lineTo(cx + Math.cos(needleAngle + Math.PI) * backLen, cy + Math.sin(needleAngle + Math.PI) * backLen);
                        ctx.lineTo(cx + Math.cos(rightAngle) * bw, cy + Math.sin(rightAngle) * bw);
                        ctx.closePath();
                        ctx.fillStyle = standbyRoot.modeColor();
                        ctx.fill();

                        // tâm kim
                        ctx.beginPath();
                        ctx.arc(cx, cy, 9, 0, 2 * Math.PI);
                        ctx.fillStyle = "#0d1525";
                        ctx.fill();

                        ctx.beginPath();
                        ctx.arc(cx, cy, 5, 0, 2 * Math.PI);
                        ctx.fillStyle = standbyRoot.modeColor();
                        ctx.fill();
                    }
                }

                // ── số speed giữa gauge ───────────────────────
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 20
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: vcuBridge.speedKmh.toFixed(1)
                        color: "#ffffff"
                        font.pixelSize: 42
                        font.family: "monospace"
                        font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "km/h"
                        color: "#445566"
                        font.pixelSize: 13
                        font.family: "monospace"
                    }
                }
            }

            // ── thanh accelerator ───────────────────
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "ACCELERATOR"
                    color: "#444466"
                    font.pixelSize: 10
                    font.family: "monospace"
                    font.letterSpacing: 3
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 284
                    height: 10
                    radius: 5
                    color: "#111122"
                    Rectangle {
                        width: parent.width * vcuBridge.accelPct / 100
                        height: parent.height
                        radius: 5
                        color: standbyRoot.modeColor()

                        Behavior on color {
                            ColorAnimation {
                                duration: 300
                            }
                        }
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: vcuBridge.accelPct + "%"
                    color: "#334455"
                    font.pixelSize: 11
                    font.family: "monospace"
                }
            }

            // ── gear selector ───────────────────────
            Column {
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
                            width: 56
                            height: 50
                            radius: 6
                            color: vcuBridge.gearLabel === modelData ? "#1a0a00" : "#111118"
                            border.color: vcuBridge.gearLabel === modelData ? "#00ff99" : "#1a1a2a"
                            border.width: vcuBridge.gearLabel === modelData ? 1.5 : 0.5
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: vcuBridge.gearLabel === modelData ? "#00ff99" : "#2a2a44"
                                font.pixelSize: 26
                                font.family: "monospace"
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // ── brake indicator ─────────────────────
            // dạng radio button: chấm đặc = PRESSED, rỗng = RELEASED
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "BRAKE"
                    color: "#444466"
                    font.pixelSize: 10
                    font.family: "monospace"
                    font.letterSpacing: 3
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 24

                    // RELEASED
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 16
                            height: 16
                            radius: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"
                            border.color: !vcuBridge.brake ? "#ff6600" : "#222233"
                            border.width: 1.5
                            // chấm đặc khi RELEASED được chọn
                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                radius: 4
                                color: "#ff6600"
                                visible: !vcuBridge.brake
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "RELEASED"
                            color: !vcuBridge.brake ? "#ff6600" : "#222233"
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                    }

                    // PRESSED
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 16
                            height: 16
                            radius: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"
                            border.color: vcuBridge.brake ? "#ff6600" : "#222233"
                            border.width: 1.5
                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                radius: 4
                                color: "#ff6600"
                                visible: vcuBridge.brake
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "PRESSED"
                            color: vcuBridge.brake ? "#ff6600" : "#222233"
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                    }
                }
            }
        }
    }

    // divider dọc phải
    Rectangle {
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        x: 713
        width: 1
        color: "#1a2030"
    }

    // ── vùng phải ──────────────────────────────────
    Item {
        anchors.top: topBar.bottom
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        width: 310

        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "ODOMETER"
                color: "#444466"
                font.pixelSize: 10
                font.family: "monospace"
                font.letterSpacing: 3
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: vcuBridge.odometerM
                color: "#334455"
                font.pixelSize: 42
                font.family: "monospace"
                font.bold: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "km"
                color: "#2a3545"
                font.pixelSize: 14
                font.family: "monospace"
            }
        }
    }

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

        // hint text — trái, height cố định
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: 400
            height: 20
            verticalAlignment: Text.AlignVCenter
            text: {
                if (vcuBridge.stateLabel === "READY")
                    return "Press ACCELERATOR to drive";
                if (vcuBridge.stateLabel === "STANDBY")
                    return "Press BRAKE + D or R to continue";
                return "";
            }
            color: {
                if (vcuBridge.stateLabel === "READY")
                    return "#444466";
                if (vcuBridge.stateLabel === "STANDBY")
                    return "#444466";
                return "transparent";
            }
            font.pixelSize: 12
            font.letterSpacing: 1
        }

        // % pin — anchor trực tiếp vào bottomBar
        Text {
            id: socText
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: 60
            height: 36
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
            text: vcuBridge.socPercent + "%"
            color: {
                if (vcuBridge.socPercent <= 10)
                    return "#ff4444";
                if (vcuBridge.socPercent <= 20)
                    return "#ffaa00";
                return "#00ff99";
            }
            font.pixelSize: 22
            font.family: "monospace"
            font.bold: true
        }

        // thân pin — anchor trực tiếp vào bottomBar
        Item {
            anchors.right: socText.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 160
            height: 36

            Rectangle {
                anchors.fill: parent
                radius: 6
                color: "#111122"
                border.color: "#1a2535"
                border.width: 1
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 2
                width: (parent.width - 4) * vcuBridge.socPercent / 100
                radius: 5
                color: {
                    if (vcuBridge.socPercent <= 20)
                        return "#ff4444";
                    if (vcuBridge.socPercent <= 40)
                        return "#ffaa00";
                    return "#00ff99";
                }
                opacity: 0.85

                SequentialAnimation on opacity {
                    running: vcuBridge.socPercent <= 20
                    loops: Animation.Infinite
                    NumberAnimation {
                        to: 0.4
                        duration: 500
                    }
                    NumberAnimation {
                        to: 0.85
                        duration: 500
                    }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: -8
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 20
                radius: 3
                color: "#1a2535"
            }
        }
    }

    // ── self-test sequence — tổng ~1.1s ────────────
    SequentialAnimation {
        id: selfTestSequence

        // tell-tales bật sáng 0.5s
        PauseAnimation {
            duration: 500
        }

        // gauge sweep 0 → 120

        NumberAnimation {
            target: standbyRoot
            property: "sweepValue"
            from: 0
            to: standbyRoot.modeMaxSpeed()
            duration: 300
            easing.type: Easing.InOutQuad
        }
        // gauge sweep 120 → 0
        NumberAnimation {
            target: standbyRoot
            property: "sweepValue"
            from: standbyRoot.modeMaxSpeed()
            to: 0
            duration: 300
            easing.type: Easing.InOutQuad
        }

        // self-test xong
        ScriptAction {
            script: {
                standbyRoot.selfTestRunning = false;
                standbyRoot.sweepValue = 0;
                speedCanvas.requestPaint();
            }
        }
    }
}
