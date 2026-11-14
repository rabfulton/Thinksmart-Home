import QtQuick
import QtQuick.Layouts

Column {
    id: indicators
    required property var stats
    width: 84
    spacing: 16
    Repeater {
        model: [
            { label: "CPU", description: "CPU usage across all cores" },
            { label: "MEM", description: "Memory in use" },
            { label: "DISK", description: "Disk space in use" }
        ]
        Column {
            required property var modelData
            required property int index
            readonly property int value: index === 0 ? indicators.stats.cpu : index === 1 ? indicators.stats.memory : indicators.stats.disk
            width: indicators.width
            spacing: 6
            Accessible.role: Accessible.StaticText
            Accessible.name: modelData.description + ": " + (value < 0 ? "unavailable" : value + " percent")
            RowLayout {
                width: parent.width; spacing: 4
                Text { text: modelData.label; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 11; font.letterSpacing: 0.8 }
                Item { Layout.fillWidth: true }
                Text {
                    text: value < 0 ? "—" : value + "%"
                    color: Theme.muted; font.family: Theme.mono; font.pixelSize: 14
                }
            }
            Rectangle {
                width: parent.width; height: 2; radius: 1; color: Theme.border
                Rectangle {
                    width: parent.width * Math.max(0, value) / 100
                    height: parent.height; radius: 1; color: Theme.accent; opacity: 0.65
                    Behavior on width { NumberAnimation { duration: 250 } }
                }
            }
        }
    }
}
