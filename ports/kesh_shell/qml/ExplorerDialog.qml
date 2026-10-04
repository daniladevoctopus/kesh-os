import QtQuick

WindowFrame {
    id: explorerRoot
    windowTitle: "Files - /cdrom/boot/apps"
    iconGlyph: "DIR"
    iconColor: "#2980b9"
    width: 640
    height: 420

    property var filesList: [
        { name: "about.elf", type: "App", size: "119 KB", color: "#3daee9", path: "/cdrom/boot/apps/about.elf" },
        { name: "notepad.elf", type: "App", size: "42 KB", color: "#34495e", path: "/cdrom/boot/apps/notepad.elf" },
        { name: "explorer.elf", type: "App", size: "61 KB", color: "#2980b9", path: "/cdrom/boot/apps/explorer.elf" },
        { name: "doom.kea", type: "Game", size: "4.2 MB", color: "#e74c3c", path: "/apps/doom.kea" },
        { name: "term.kea", type: "Shell", size: "84 KB", color: "#27ae60", path: "/apps/term.kea" },
        { name: "settings.elf", type: "App", size: "51 KB", color: "#f39c12", path: "/cdrom/boot/apps/settings.elf" },
        { name: "font_demo.elf", type: "Demo", size: "1.8 MB", color: "#9b59b6", path: "/cdrom/boot/apps/font_demo.elf" },
        { name: "gfx_demo.elf", type: "Demo", size: "1.0 MB", color: "#1abc9c", path: "/cdrom/boot/apps/gfx_demo.elf" }
    ]

    Column {
        anchors.fill: parent
        anchors.topMargin: 48
        anchors.margins: 14
        spacing: 10

        // Location bar
        Rectangle {
            width: parent.width
            height: 32
            radius: 8
            color: "#182333"
            border.width: 1
            border.color: "#2f425c"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text { text: "📁"; font.pixelSize: 14 }
                Text {
                    text: "/cdrom/boot/apps"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#3daee9"
                }
            }
        }

        // File Grid
        GridView {
            width: parent.width
            height: parent.height - 48
            cellWidth: 140
            cellHeight: 110
            clip: true
            model: explorerRoot.filesList

            delegate: Rectangle {
                width: 130
                height: 100
                radius: 10
                color: fileMouse.containsMouse ? "#27364a" : "transparent"
                border.width: fileMouse.containsMouse ? 1 : 0
                border.color: "#3daee9"

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        width: 44
                        height: 44
                        radius: 10
                        color: modelData.color
                        anchors.horizontalCenter: parent.horizontalCenter

                        Text {
                            anchors.centerIn: parent
                            text: (modelData.type === "App" || modelData.type === "Game") ? "⚙" : "📄"
                            font.pixelSize: 20
                            color: "#ffffff"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.name
                        font.family: "Open Sans"
                        font.bold: true
                        font.pixelSize: 11
                        color: "#f0f4f8"
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.size
                        font.family: "Open Sans"
                        font.pixelSize: 9
                        color: "#7e93ab"
                    }
                }

                MouseArea {
                    id: fileMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: {
                        if (typeof KeshOS !== "undefined") KeshOS.launch(modelData.path)
                    }
                }
            }
        }
    }
}
