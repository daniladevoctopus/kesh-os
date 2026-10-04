import QtQuick

Rectangle {
    id: windowRoot
    width: 680
    height: 440
    radius: isMaximized ? 0 : 16
    color: "#f2141c27"
    border.width: isMaximized ? 0 : 1
    border.color: "#354863"
    clip: true

    property string windowTitle: "Application"
    property string iconGlyph: "APP"
    property color iconColor: "#3daee9"
    property bool isMaximized: false
    property rect prevGeometry: Qt.rect(100, 80, 680, 440)

    signal windowClosed()

    // Titlebar
    Rectangle {
        id: titleBar
        width: parent.width
        height: 42
        color: "#182232"
        border.width: 1
        border.color: "#2b3b50"

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            // App Icon Circle
            Rectangle {
                width: 26
                height: 26
                radius: 13
                color: windowRoot.iconColor
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: windowRoot.iconGlyph
                    font.family: "Open Sans"
                    font.bold: true
                    font.pixelSize: 8
                    color: "#ffffff"
                }
            }

            // Window Title Text
            Text {
                text: windowRoot.windowTitle
                font.family: "Open Sans"
                font.bold: true
                font.pixelSize: 13
                color: "#e8eff7"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Window Control Buttons (Minimize, Maximize, Close)
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // Minimize
            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: "#f39c12"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: windowRoot.visible = false
                }
            }

            // Maximize / Restore
            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: "#27ae60"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!windowRoot.isMaximized) {
                            windowRoot.prevGeometry = Qt.rect(windowRoot.x, windowRoot.y, windowRoot.width, windowRoot.height)
                            windowRoot.x = 0
                            windowRoot.y = 0
                            windowRoot.width = parent.parent.parent.width
                            windowRoot.height = parent.parent.parent.height - 60
                            windowRoot.isMaximized = true
                        } else {
                            windowRoot.x = windowRoot.prevGeometry.x
                            windowRoot.y = windowRoot.prevGeometry.y
                            windowRoot.width = windowRoot.prevGeometry.width
                            windowRoot.height = windowRoot.prevGeometry.height
                            windowRoot.isMaximized = false
                        }
                    }
                }
            }

            // Close
            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: "#e74c3c"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        windowRoot.visible = false
                        windowRoot.windowClosed()
                    }
                }
            }
        }

        // Drag to move window
        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 90
            cursorShape: Qt.SizeAllCursor

            property point pressPos
            onPressed: (mouse) => {
                pressPos = Qt.point(mouse.x, mouse.y)
                windowRoot.z = 50 // Bring window to front
            }
            onPositionChanged: (mouse) => {
                if (pressed && !windowRoot.isMaximized) {
                    var dx = mouse.x - pressPos.x
                    var dy = mouse.y - pressPos.y
                    windowRoot.x += dx
                    windowRoot.y += dy
                }
            }
        }
    }

    // Window Inner Content Container
    Item {
        id: clientArea
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
