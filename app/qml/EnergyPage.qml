import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    readonly property var fields: [
        {key:"soc",label:"Battery charge"},{key:"batteryPower",label:"Battery power"},{key:"solar",label:"Solar (DC)"},
        {key:"voltage",label:"Battery voltage"},{key:"current",label:"Battery current"},{key:"dcLoad",label:"DC load"},
        {key:"acLoad",label:"AC output"},{key:"yieldToday",label:"Yield on reported day"},{key:"yieldYesterday",label:"Previous day's yield"}
    ]
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 16
        RowLayout {
            spacing: 20
            UiButton { objectName: "energyBackButton"; iconName: "back"; description: "Back to dashboard"; onClicked: page.back() }
            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                Text { text: "Energy"; color: Theme.ink; font.pixelSize: 29; font.weight: Font.Medium }
                Text { text: energy.siteName; color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            UiButton { text: energy.refreshing ? "Checking…" : "Refresh"; enabled: energy.configured && !energy.refreshing; onClicked: energy.refresh() }
        }
        ComboBox {
            visible: energy.installations.length > 1
            Layout.fillWidth: true; implicitHeight: 60
            model: energy.installations; textRole: "name"; valueRole: "id"
            palette.button: Theme.raised; palette.buttonText: Theme.ink
            onActivated: energy.selectInstallation(currentValue)
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: reportColumn.implicitHeight + 28
            color: Theme.raised; radius: 14
            ColumnLayout {
                id: reportColumn
                anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 16
                anchors.verticalCenter: parent.verticalCenter; spacing: 5
                Text { text: energy.status; color: energy.stale ? Theme.accent : Theme.ink; font.pixelSize: 21; font.weight: Font.Medium }
                Text { text: energy.lastReport; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { visible: energy.problem.length > 0; text: energy.problem; color: Theme.danger; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: scroll.availableWidth; spacing: 14
                GridLayout {
                    Layout.fillWidth: true
                    columns: page.width > 950 ? 3 : 2
                    columnSpacing: 12; rowSpacing: 12
                    Repeater {
                        model: page.fields
                        Rectangle {
                            required property var modelData
                            readonly property var reading: energy.metrics[modelData.key]
                            Layout.fillWidth: true; Layout.preferredWidth: 1
                            implicitHeight: 140; color: Theme.background; radius: 16
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 18; spacing: 6
                                Text { text: modelData.label; color: Theme.muted; font.pixelSize: 16 }
                                Text { text: reading.text; color: Theme.ink; font.pixelSize: 30; font.weight: Font.Medium }
                                Item { Layout.fillHeight: true }
                                Text { text: reading.age; color: reading.stale && reading.available ? Theme.accent : Theme.faint; font.pixelSize: 14 }
                                Text { visible: reading.available; text: reading.when; color: Theme.faint; font.pixelSize: 12 }
                            }
                        }
                    }
                }
                Text { text: "Each reading keeps its own measurement time. Power is positive while charging and negative while discharging."; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: "Yield totals belong to the reported day, not necessarily today. Missing readings are shown as —."; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: energy.lastCheck; color: Theme.faint; font.pixelSize: 13; Layout.fillWidth: true; Layout.bottomMargin: 8 }
            }
        }
    }
}
