pragma Singleton
import QtQuick

QtObject {
    property string paletteName: "ember"
    property var names: ["ember", "tide", "dawn", "autumn", "spring", "dusk"]
    readonly property string title: paletteName.charAt(0).toUpperCase() + paletteName.slice(1)
    property var palette: ({
        background: "#10161c", surface: "#181f27", raised: "#222c36", border: "#303d49",
        ink: "#edf2f4", muted: "#a3b1bc", faint: "#788996", accent: "#e8b568",
        secondary: "#8ed5cc", tint: "#373027", accentInk: "#131c22", danger: "#ef9b8e"
    })
    readonly property color background: palette.background
    readonly property color surface: palette.surface
    readonly property color raised: palette.raised
    readonly property color border: palette.border
    readonly property color ink: palette.ink
    readonly property color muted: palette.muted
    readonly property color faint: palette.faint
    readonly property color accent: palette.accent
    readonly property color tint: palette.tint
    readonly property color accentInk: palette.accentInk
    readonly property color secondary: palette.secondary
    readonly property color danger: palette.danger
    readonly property color chessLight: Qt.tint("#f0eee7", Qt.rgba(accent.r,accent.g,accent.b,0.18))
    readonly property color chessDark: Qt.tint("#66756d", Qt.rgba(accent.r,accent.g,accent.b,0.60))
    readonly property color scrim: "#99000000"
    readonly property int radius: 22
    readonly property int gap: 16
    readonly property string font: "DejaVu Sans"
    readonly property string mono: "DejaVu Sans Mono"
}
