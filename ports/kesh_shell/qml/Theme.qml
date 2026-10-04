pragma Singleton
import QtQuick

QtObject {
    // KDE Plasma 6 Breeze Dark Palette
    readonly property color bgDark: "#0d131d"
    readonly property color bgGradientStart: "#111824"
    readonly property color bgGradientEnd: "#070c12"

    readonly property color panelBg: "#ea141e2c"
    readonly property color panelBorder: "#34465e"

    readonly property color cardBg: "#de1a2638"
    readonly property color cardHover: "#f022324a"
    readonly property color cardBorder: "#3b4f6b"

    readonly property color accent: "#3daee9"        // KDE Breeze Blue
    readonly property color accentHover: "#56c2ff"
    readonly property color accentGlow: "#203daee9"

    readonly property color danger: "#e74c3c"
    readonly property color warning: "#f39c12"
    readonly property color success: "#2ecc71"

    readonly property color textMain: "#eff0f1"
    readonly property color textDim: "#8f9fb2"
    readonly property color textMuted: "#5e7085"

    readonly property int cornerRadius: 10
    readonly property int panelHeight: 48
    readonly property string fontFamily: "Open Sans"
}
