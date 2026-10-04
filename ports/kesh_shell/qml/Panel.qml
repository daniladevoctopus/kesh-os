import QtQuick

Rectangle {
    id: panelRoot
    height: 52
    anchors.bottomMargin: 8
    radius: 18
    color: "#d9121a26"
    border.width: 1
    border.color: "#2a394d"

    property bool isStartMenuOpen: false
    signal toggleStartMenu()
    signal appQuickLaunch(string path, string name)
    signal openAboutDialog()
    signal openExplorerDialog()

    // 1. Left Section: Circular Launcher Button
    Rectangle {
        id: startBtn
        width: 38
        height: 38
        radius: 19
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        color: panelRoot.isStartMenuOpen ? "#3daee9" : (startMouse.containsMouse ? "#27364a" : "#192433")
        border.width: 1
        border.color: panelRoot.isStartMenuOpen ? "#63c7ff" : "#354861"

        // Concentric launcher circle like ChromiumOS
        Rectangle {
            width: 12
            height: 12
            radius: 6
            anchors.centerIn: parent
            color: panelRoot.isStartMenuOpen ? "#ffffff" : "#3daee9"
        }

        MouseArea {
            id: startMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: panelRoot.toggleStartMenu()
        }
    }

    // 2. Center Section: Pinned Circular Icons
    Row {
        id: centerApps
        anchors.centerIn: parent
        spacing: 12

        Repeater {
            model: [
                { name: "Files", path: "/cdrom/boot/apps/explorer.elf", color: "#2980b9", glyph: "📁", isSpecial: "explorer" },
                { name: "About KeshOS", path: "/apps/about.elf", color: "#3daee9", glyph: "✦", isSpecial: "about" },
                { name: "Terminal", path: "/apps/term.kea", color: "#27ae60", glyph: ">_", isSpecial: "" },
                { name: "Notepad", path: "/cdrom/boot/apps/notepad.elf", color: "#34495e", glyph: "TXT", isSpecial: "" },
                { name: "Doom", path: "/apps/doom.kea", color: "#e74c3c", glyph: "DOOM", isSpecial: "" },
                { name: "Settings", path: "/cdrom/boot/apps/settings.elf", color: "#f39c12", glyph: "⚙", isSpecial: "" }
            ]

            Rectangle {
                width: 40
                height: 40
                radius: 20
                color: appMouse.containsMouse ? "#2d3e54" : "transparent"

                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    anchors.centerIn: parent
                    color: modelData.color
                    border.width: 1
                    border.color: Qt.lighter(modelData.color, 1.25)

                    Text {
                        anchors.centerIn: parent
                        text: modelData.glyph
                        font.family: "Open Sans"
                        font.bold: true
                        font.pixelSize: 11
                        color: "#ffffff"
                    }
                }

                // Active dot indicator (like in ChromeOS)
                Rectangle {
                    width: 4
                    height: 4
                    radius: 2
                    color: "#3daee9"
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                MouseArea {
                    id: appMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (modelData.isSpecial === "about") {
                            panelRoot.openAboutDialog()
                        } else if (modelData.isSpecial === "explorer") {
                            panelRoot.openExplorerDialog()
                        } else {
                            panelRoot.appQuickLaunch(modelData.path, modelData.name)
                            if (typeof KeshOS !== "undefined") KeshOS.launch(modelData.path)
                        }
                    }
                }
            }
        }
    }

    // 3. Right Section: Modern ChromeOS-style Status Pill
    Rectangle {
        id: statusPill
        height: 38
        width: pillRow.width + 20
        radius: 19
        color: "#182333"
        border.width: 1
        border.color: "#2c3d52"
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 12

            // User avatar dot
            Rectangle {
                width: 22
                height: 22
                radius: 11
                color: "#3daee9"
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    anchors.centerIn: parent
                    text: "D"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 10
                    color: "#ffffff"
                }
            }

            // Time & Date
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: (typeof KeshOS !== "undefined") ? KeshOS.currentTime : "10:05:00"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#eff4f9"
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: (typeof KeshOS !== "undefined") ? KeshOS.currentDate : "Oct 4"
                    font.family: "Open Sans"
                    font.pixelSize: 10
                    color: "#7e93ab"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
