import QtQuick

WindowFrame {
    id: aboutRoot
    windowTitle: "About KeshOS"
    iconGlyph: "KESH"
    iconColor: "#3daee9"
    width: 520
    height: 380

    Column {
        anchors.fill: parent
        anchors.topMargin: 56
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        spacing: 16

        Row {
            spacing: 18
            Rectangle {
                width: 64
                height: 64
                radius: 18
                color: "#3daee9"
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    anchors.centerIn: parent
                    text: "✦"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 34
                    color: "#ffffff"
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: "KeshOS 1.0"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 22
                    color: "#ffffff"
                }
                Text {
                    text: "Next-Generation Modular Operating System"
                    font.family: "Open Sans"
                    font.pixelSize: 12
                    color: "#8fa3ba"
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "#28384d"
        }

        Grid {
            columns: 2
            spacing: 12
            width: parent.width

            Text { text: "Kernel Architecture:"; color: "#7a8fa6"; font.pixelSize: 12; font.family: "Open Sans"; font.bold: true }
            Text { text: "x86_64 Preemptive Multiprocess (Ring 0)"; color: "#dbe5f0"; font.pixelSize: 12; font.family: "Open Sans" }

            Text { text: "Windowing System:"; color: "#7a8fa6"; font.pixelSize: 12; font.family: "Open Sans"; font.bold: true }
            Text { text: "Wayland IPC + Qt Quick Scene Graph"; color: "#dbe5f0"; font.pixelSize: 12; font.family: "Open Sans" }

            Text { text: "Shell Environment:"; color: "#7a8fa6"; font.pixelSize: 12; font.family: "Open Sans"; font.bold: true }
            Text { text: "KeshShell 1.0 (Modular Userspace)"; color: "#3daee9"; font.pixelSize: 12; font.family: "Open Sans"; font.bold: true }

            Text { text: "C Standard Library:"; color: "#7a8fa6"; font.pixelSize: 12; font.family: "Open Sans"; font.bold: true }
            Text { text: "musl libc 1.2.5 Static Runtime"; color: "#dbe5f0"; font.pixelSize: 12; font.family: "Open Sans" }
        }

        Item { height: 10; width: 1 }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 120
            height: 34
            radius: 8
            color: closeMouse.containsMouse ? "#4db8f2" : "#3daee9"

            Text {
                anchors.centerIn: parent
                text: "OK"
                font.family: "Open Sans"
                font.bold: true
                font.pixelSize: 12
                color: "#ffffff"
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: aboutRoot.visible = false
            }
        }
    }
}
