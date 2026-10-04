import QtQuick

Item {
    id: desktopRoot
    anchors.fill: parent

    signal appSelected(string path, string name)

    property var defaultApps: [
        { name: "Notepad", path: "/cdrom/boot/apps/notepad.elf", iconColor: "#3daee9", glyph: "TXT", description: "Text Editor" },
        { name: "Doom", path: "/apps/doom.kea", iconColor: "#e74c3c", glyph: "DOOM", description: "Classic 3D Action" },
        { name: "Terminal", path: "/apps/term.kea", iconColor: "#27ae60", glyph: ">_", description: "Kesh Command Shell" },
        { name: "Settings", path: "/cdrom/boot/apps/settings.elf", iconColor: "#f39c12", glyph: "CFG", description: "System Settings" },
        { name: "Vector Demo", path: "/cdrom/boot/apps/font_demo.elf", iconColor: "#9b59b6", glyph: "FON", description: "FreeType2 Fonts" },
        { name: "2D Canvas", path: "/cdrom/boot/apps/gfx_demo.elf", iconColor: "#1abc9c", glyph: "GFX", description: "Pixman Graphics" },
        { name: "POSIX Hello", path: "/cdrom/boot/apps/hello.elf", iconColor: "#34495e", glyph: "CLI", description: "musl libc test" }
    ]

    property var appList: (typeof KeshOS !== "undefined" && KeshOS.installedApps) ? KeshOS.installedApps : defaultApps

    // Left Side Desktop Grid Icons
    Flow {
        id: iconFlow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 28
        width: 140
        spacing: 16
        flow: Flow.TopToBottom

        Repeater {
            model: desktopRoot.appList

            Rectangle {
                width: 110
                height: 100
                radius: 12
                color: mouseArea.containsMouse ? "#3026364a" : "transparent"
                border.width: mouseArea.containsMouse ? 1 : 0
                border.color: "#3daee9"

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 48
                        height: 48
                        radius: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: modelData.iconColor
                        border.width: 1
                        border.color: Qt.lighter(modelData.iconColor, 1.2)

                        Text {
                            anchors.centerIn: parent
                            text: modelData.glyph
                            font.family: "Open Sans"
                            font.bold: true
                            font.pixelSize: 13
                            color: "#ffffff"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.name
                        font.family: "Open Sans"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#f0f4f8"
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        desktopRoot.appSelected(modelData.path, modelData.name)
                        if (typeof KeshOS !== "undefined") KeshOS.launch(modelData.path)
                    }
                }
            }
        }
    }

    // Top-Right Modern Plasma System Widget
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 28
        width: 320
        height: 110
        radius: 14
        color: "#d9162232"
        border.width: 1
        border.color: "#31455e"

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Row {
                spacing: 10
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: "#3daee9"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "KeshOS 1.0 (Plasma Shell)"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 14
                    color: "#f0f4f8"
                }
            }

            Text {
                text: "Modular Ring 3 Qt Quick Environment"
                font.family: "Open Sans"
                font.pixelSize: 11
                color: "#99abbf"
            }

            Row {
                spacing: 8
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#2ecc71"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Architecture: Wayland IPC + Modular Userspace"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 11
                    color: "#3daee9"
                }
            }
        }
    }
}
