import QtQuick

Rectangle {
    id: kickoffRoot
    width: 440
    height: 460
    radius: 22
    color: "#f2141c27"
    border.width: 1
    border.color: "#2c3e54"
    clip: true

    property string filterText: ""
    signal appLaunched(string path, string name)
    signal openAboutDialog()
    signal openExplorerDialog()

    property var defaultApps: [
        { name: "Files", path: "/cdrom/boot/apps/explorer.elf", color: "#2980b9", glyph: "📁", isSpecial: "explorer" },
        { name: "About KeshOS", path: "/apps/about.elf", color: "#3daee9", glyph: "✦", isSpecial: "about" },
        { name: "Terminal", path: "/apps/term.kea", color: "#27ae60", glyph: ">_", isSpecial: "" },
        { name: "Notepad", path: "/cdrom/boot/apps/notepad.elf", color: "#34495e", glyph: "TXT", isSpecial: "" },
        { name: "Settings", path: "/cdrom/boot/apps/settings.elf", color: "#f39c12", glyph: "⚙", isSpecial: "" },
        { name: "Doom", path: "/apps/doom.kea", color: "#e74c3c", glyph: "DOOM", isSpecial: "" },
        { name: "Vector Demo", path: "/cdrom/boot/apps/font_demo.elf", color: "#9b59b6", glyph: "FON", isSpecial: "" },
        { name: "2D Canvas", path: "/cdrom/boot/apps/gfx_demo.elf", color: "#1abc9c", glyph: "GFX", isSpecial: "" }
    ]

    Column {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        // Search Bar (Like in reference)
        Rectangle {
            width: parent.width
            height: 42
            radius: 21
            color: "#1c2736"
            border.width: 1
            border.color: searchInput.activeFocus ? "#3daee9" : "#2f425c"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Text {
                    text: "🔍"
                    font.pixelSize: 13
                    opacity: 0.7
                    anchors.verticalCenter: parent.verticalCenter
                }

                TextInput {
                    id: searchInput
                    width: kickoffRoot.width - 90
                    font.family: "Open Sans"
                    font.pixelSize: 13
                    color: "#f0f4f8"
                    verticalAlignment: TextInput.AlignVCenter
                    onTextChanged: kickoffRoot.filterText = text.toLowerCase()

                    Text {
                        text: "Search apps, files..."
                        font.family: "Open Sans"
                        font.pixelSize: 13
                        color: "#6b8096"
                        visible: !searchInput.text && !searchInput.activeFocus
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // 4x2 Grid of Circular Icons (ChromiumOS Style)
        Grid {
            columns: 4
            spacing: 14
            width: parent.width
            horizontalItemAlignment: Grid.AlignHCenter

            Repeater {
                model: kickoffRoot.defaultApps

                Rectangle {
                    width: 86
                    height: 80
                    radius: 12
                    visible: kickoffRoot.filterText === "" ||
                             modelData.name.toLowerCase().indexOf(kickoffRoot.filterText) !== -1

                    color: gridMouse.containsMouse ? "#27364a" : "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        // Circular Icon
                        Rectangle {
                            width: 44
                            height: 44
                            radius: 22
                            color: modelData.color
                            anchors.horizontalCenter: parent.horizontalCenter
                            border.width: 1
                            border.color: Qt.lighter(modelData.color, 1.25)

                            Text {
                                anchors.centerIn: parent
                                text: modelData.glyph
                                font.family: "Open Sans"
                                font.bold: true
                                font.pixelSize: 12
                                color: "#ffffff"
                            }
                        }

                        // App Label
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.name
                            font.family: "Open Sans"
                            font.bold: true
                            font.pixelSize: 11
                            color: "#eff4f9"
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: gridMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData.isSpecial === "about") {
                                kickoffRoot.openAboutDialog()
                            } else if (modelData.isSpecial === "explorer") {
                                kickoffRoot.openExplorerDialog()
                            } else {
                                kickoffRoot.appLaunched(modelData.path, modelData.name)
                                if (typeof KeshOS !== "undefined") KeshOS.launch(modelData.path)
                            }
                        }
                    }
                }
            }
        }

        // Bottom Power Controls
        Row {
            width: parent.width
            spacing: 12

            Rectangle {
                width: (parent.width - 12) / 2
                height: 36
                radius: 10
                color: rMouse.containsMouse ? "#28384d" : "#192433"
                border.width: 1
                border.color: "#f39c12"

                Text {
                    anchors.centerIn: parent
                    text: "↻  Restart"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 11
                    color: "#f39c12"
                }

                MouseArea {
                    id: rMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof KeshOS !== "undefined") KeshOS.reboot()
                    }
                }
            }

            Rectangle {
                width: (parent.width - 12) / 2
                height: 36
                radius: 10
                color: pMouse.containsMouse ? "#28384d" : "#192433"
                border.width: 1
                border.color: "#e74c3c"

                Text {
                    anchors.centerIn: parent
                    text: "⏻  Shut Down"
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 11
                    color: "#e74c3c"
                }

                MouseArea {
                    id: pMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof KeshOS !== "undefined") KeshOS.poweroff()
                    }
                }
            }
        }
    }
}
