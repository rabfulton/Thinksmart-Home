pragma Singleton
import QtQuick

QtObject {
    property string paletteName: "ember"
    readonly property var names: ["ember", "tide", "dawn", "autumn", "spring", "dusk"]
    readonly property string title: paletteName.charAt(0).toUpperCase() + paletteName.slice(1)
    readonly property var palette: paletteName === "autumn" ? ({
        background: "#1c1512", surface: "#281e19", raised: "#362922", border: "#503c30",
        ink: "#f5ebe0", muted: "#c6afa0", faint: "#a58b79", accent: "#efa66d", tint: "#493123", accentInk: "#24170f", secondary: "#c9cf8c", danger: "#f29c91"
    }) : paletteName === "spring" ? ({
        background: "#101c18", surface: "#182922", raised: "#23392e", border: "#365044",
        ink: "#edf4e7", muted: "#afc4b6", faint: "#849e8c", accent: "#b5d98a", tint: "#30452c", accentInk: "#172414", secondary: "#e5b4ce", danger: "#efa49e"
    }) : paletteName === "dusk" ? ({
        background: "#191421", surface: "#241e30", raised: "#332a42", border: "#493c59",
        ink: "#f2edf7", muted: "#bfb0cf", faint: "#9887ab", accent: "#c8aff0", tint: "#3b2c50", accentInk: "#241930", secondary: "#eab5a1", danger: "#f0a1af"
    }) : paletteName === "dawn" ? ({
        background: "#e8e9e3", surface: "#f5f5ef", raised: "#dde2da", border: "#c6cfc5",
        ink: "#20312f", muted: "#5e706b", faint: "#71847c", accent: "#357e70", tint: "#d9ebe1", accentInk: "#ffffff", secondary: "#517db1", danger: "#a74840"
    }) : ({
        background: "#10161c", surface: "#181f27", raised: "#222c36", border: "#303d49",
        ink: "#edf2f4", muted: "#a3b1bc", faint: "#788996",
        accent: paletteName === "tide" ? "#6bd0bf" : "#e8b568",
        secondary: paletteName === "tide" ? "#9cbcff" : "#8ed5cc",
        tint: paletteName === "tide" ? "#203a37" : "#373027", accentInk: "#131c22", danger: "#ef9b8e"
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
