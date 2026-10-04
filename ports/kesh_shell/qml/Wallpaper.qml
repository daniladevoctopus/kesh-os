import QtQuick

Item {
    id: wallpaperRoot
    anchors.fill: parent

    // Base deep gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgGradientStart }
            GradientStop { position: 0.6; color: Theme.bgDark }
            GradientStop { position: 1.0; color: Theme.bgGradientEnd }
        }
    }

    // Plasma Breeze geometric light accent #1 (top right)
    Rectangle {
        width: Math.max(parent.width * 0.55, 500)
        height: width
        radius: width / 2
        x: parent.width - width * 0.65
        y: -height * 0.4
        color: "#1c5d94"
        opacity: 0.32

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { to: 0.22; duration: 4000; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 0.36; duration: 4000; easing.type: Easing.InOutQuad }
        }
    }

    // Plasma Breeze geometric light accent #2 (bottom left)
    Rectangle {
        width: Math.max(parent.width * 0.45, 400)
        height: width
        radius: width / 2
        x: -width * 0.35
        y: parent.height - height * 0.55
        color: "#106368"
        opacity: 0.25
    }

    // Modern angular polygon element (KDE style)
    Canvas {
        anchors.fill: parent
        opacity: 0.12
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            ctx.fillStyle = "#3daee9";
            ctx.beginPath();
            ctx.moveTo(width * 0.7, 0);
            ctx.lineTo(width, 0);
            ctx.lineTo(width, height * 0.45);
            ctx.lineTo(width * 0.45, height * 0.9);
            ctx.closePath();
            ctx.fill();

            ctx.fillStyle = "#27ae60";
            ctx.beginPath();
            ctx.moveTo(0, height * 0.6);
            ctx.lineTo(width * 0.35, height);
            ctx.lineTo(0, height);
            ctx.closePath();
            ctx.fill();
        }
    }
}
