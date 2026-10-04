import QtQuick

Window {
    id: root
    visible: true
    visibility: Window.FullScreen
    color: "#0c1019"
    title: "QML on KeshOS"
    Shortcut { sequence: "Escape"; onActivated: Qt.quit() }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#101725" }
            GradientStop { position: 1.0; color: "#07101b" }
        }
    }

    Rectangle {
        id: glow
        width: 420
        height: 420
        radius: 210
        color: "#195b92"
        opacity: 0.35
        x: root.width - width * 0.72
        y: -height * 0.35

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { to: 0.18; duration: 1600; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 0.42; duration: 1600; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        width: Math.min(parent.width - 100, 820)
        height: 430
        anchors.centerIn: parent
        radius: 24
        color: "#df182131"
        border.width: 1
        border.color: "#52657d"

        Column {
            anchors.fill: parent
            anchors.margins: 42
            spacing: 22

            Text {
                text: "KeshOS + Qt Quick"
                color: "#8bd5ff"
                font.family: "Open Sans"
                font.pixelSize: 42
                font.bold: true
            }

            Text {
                text: "QML engine and software scene graph are running"
                color: "#d8e5f5"
                font.family: "Open Sans"
                font.pixelSize: 20
            }

            Rectangle {
                width: parent.width
                height: 74
                radius: 14
                color: "#182b2737"
                border.color: "#4ecb8f"

                Text {
                    anchors.centerIn: parent
                    text: "Phase 4: Qt Declarative 6.11.2"
                    color: "#9ef0bf"
                    font.family: "Open Sans"
                    font.pixelSize: 22
                }
            }

            Rectangle {
                width: 210
                height: 58
                radius: 14
                color: closeArea.pressed ? "#9ef0bf" : "#47b8ff"

                Text {
                    anchors.centerIn: parent
                    text: "Close QML demo"
                    color: "#08111c"
                    font.family: "Open Sans"
                    font.pixelSize: 18
                    font.bold: true
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    onClicked: Qt.quit()
                }
            }
        }
    }

}
