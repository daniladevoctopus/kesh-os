import QtQuick

Window {
    id: rootWindow
    visible: true
    visibility: Window.FullScreen
    color: "#0a0e16"
    title: "KeshOS Desktop Shell"

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (kickoffMenu.visible) kickoffMenu.visible = false;
        }
    }

    // 1. Wallpaper
    Wallpaper {
        anchors.fill: parent
    }

    // 2. Desktop shortcuts
    DesktopView {
        anchors.fill: parent

        onAppSelected: (path, name) => {
            toast.show("Launching " + name + "...")
            kickoffMenu.visible = false
        }
    }

    // Dismiss Kickoff menu on empty desktop click
    MouseArea {
        anchors.fill: parent
        z: 70
        visible: kickoffMenu.visible
        onClicked: kickoffMenu.visible = false
    }

    // 3. Native Window Dialog: Explorer
    ExplorerDialog {
        id: explorerWin
        z: 85
        visible: false
        x: Math.max(40, (rootWindow.width - width) / 2)
        y: Math.max(40, (rootWindow.height - height) / 2 - 30)
    }

    // 4. Native Window Dialog: About KeshOS
    AboutDialog {
        id: aboutWin
        z: 86
        visible: false
        x: Math.max(60, (rootWindow.width - width) / 2)
        y: Math.max(60, (rootWindow.height - height) / 2 - 30)
    }

    // 5. Kickoff Start Menu (Floats above panel)
    KickoffMenu {
        id: kickoffMenu
        z: 100
        anchors.left: parent.left
        anchors.bottom: bottomPanel.top
        anchors.leftMargin: 16
        anchors.bottomMargin: 10
        visible: false

        onAppLaunched: (path, name) => {
            toast.show("Launching " + name + "...")
            kickoffMenu.visible = false
        }

        onOpenAboutDialog: {
            aboutWin.visible = true
            aboutWin.z = 90
            kickoffMenu.visible = false
        }

        onOpenExplorerDialog: {
            explorerWin.visible = true
            explorerWin.z = 90
            kickoffMenu.visible = false
        }
    }

    // 6. Floating ChromiumOS-style Bottom Shelf
    Panel {
        id: bottomPanel
        z: 95
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 32, 1080)
        isStartMenuOpen: kickoffMenu.visible

        onToggleStartMenu: {
            kickoffMenu.visible = !kickoffMenu.visible;
        }

        onAppQuickLaunch: (path, name) => {
            toast.show("Launching " + name + "...")
            kickoffMenu.visible = false
        }

        onOpenAboutDialog: {
            aboutWin.visible = true
            aboutWin.z = 90
            kickoffMenu.visible = false
        }

        onOpenExplorerDialog: {
            explorerWin.visible = true
            explorerWin.z = 90
            kickoffMenu.visible = false
        }
    }

    // 7. Toast Notification Pill
    Rectangle {
        id: toast
        z: 110
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 18
        width: toastText.implicitWidth + 36
        height: 38
        radius: 19
        color: "#f2141c27"
        border.width: 1
        border.color: "#3daee9"
        opacity: 0

        function show(msg) {
            toastText.text = msg
            toastAnim.restart()
        }

        Text {
            id: toastText
            anchors.centerIn: parent
            font.family: "Open Sans"
            font.bold: true
            font.pixelSize: 12
            color: "#3daee9"
        }

        SequentialAnimation on opacity {
            id: toastAnim
            running: false
            NumberAnimation { to: 1.0; duration: 160 }
            PauseAnimation { duration: 2200 }
            NumberAnimation { to: 0.0; duration: 350 }
        }
    }
}

