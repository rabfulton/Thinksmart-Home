import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    signal back()
    property string genre: ""
    property string query: ""
    readonly property bool wide: width > 900
    readonly property var pins: radio.stations.filter(function(s) { return s.pinned })
    readonly property var matches: radio.stations.filter(function(s) {
        return !s.pinned && (!page.genre.length || s.genres.indexOf(page.genre) >= 0)
            && (!page.query.length || (s.name + " " + s.description + " " + s.genres.join(" ")).toLowerCase().indexOf(page.query.toLowerCase()) >= 0)
    })
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 14
        RowLayout {
            spacing: 20
            UiButton { iconName: "back"; description: "Back"; onClicked: page.back() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: "Radio"; color: Theme.ink; font.pixelSize: 29 }
                Text { text: !radio.busy && home.source === "radio" && home.state === "stopped" ? "Stopped · " + home.title : radio.status; color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            UiIcon { name: "radio"; tint: Theme.accent; width: 30; height: 30 }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Repeater {
                model: page.pins
                AbstractButton {
                    id: pin
                    required property var modelData
                    Layout.fillWidth: true; implicitHeight: 92; padding: 15
                    onClicked: radio.play(modelData.id)
                    background: Rectangle { color: pin.down || (home.source === "radio" && home.radioId === pin.modelData.id && home.state !== "stopped") ? Theme.tint : Theme.raised; radius: 16; border.color: Theme.border }
                    contentItem: ColumnLayout {
                        spacing: 6
                        Text { text: pin.modelData.name; color: Theme.ink; font.pixelSize: page.wide ? 23 : 19; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: pin.modelData.id === "radio4" ? "News & conversation" : "Live / latest sitting"; color: Theme.accent; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }
            }
        }
        GridLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            columns: page.wide ? 2 : 1; columnSpacing: 20; rowSpacing: 14
            ColumnLayout {
                Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: page.wide ? 260 : 1
                Layout.maximumWidth: page.wide ? 270 : Infinity
                spacing: 14
                Text { text: "Pick a mood · tap a genre for a random station"; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                GridLayout {
                    Layout.fillWidth: true; columns: page.wide ? 2 : 5; columnSpacing: 10; rowSpacing: 10
                    UiButton { text: "All stations"; implicitHeight: 56; Layout.columnSpan: page.wide ? 2 : 1; Layout.fillWidth: true; selected: !page.genre.length; onClicked: page.genre = "" }
                    Repeater {
                        model: ["folk","jazz","rock","classical"]
                        UiButton {
                            required property string modelData
                            text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                            implicitHeight: 56; Layout.fillWidth: true
                            selected: page.genre === modelData
                            onClicked: { page.genre = modelData; radio.random(modelData) }
                        }
                    }
                }
                Text { visible: page.wide; text: "Your internal station list. Parliament plays live or the latest recorded sitting."; color: Theme.faint; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; spacing: 12
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    TextField {
                        id: search
                        Layout.fillWidth: true; implicitHeight: 54; font.pixelSize: 19
                        placeholderText: "Filter station names or styles"; placeholderTextColor: Theme.faint; color: Theme.ink
                        background: Rectangle { color: Theme.background; radius: 12; border.color: Theme.border }
                        onTextChanged: page.query = text
                    }
                    UiButton { text: "Another pick"; implicitHeight: 56; enabled: page.genre.length > 0; onClicked: radio.random(page.genre) }
                }
        ListView {
            id: stationList
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8; model: page.matches
            ScrollBar.vertical: ScrollBar { }
            delegate: AbstractButton {
                id: row
                required property var modelData
                width: stationList.width; height: 76; padding: 14
                onClicked: radio.play(modelData.id)
                background: Rectangle { radius: 14; color: row.down || (home.source === "radio" && home.radioId === row.modelData.id && home.state !== "stopped") ? Theme.tint : Theme.raised }
                contentItem: RowLayout {
                    spacing: 16
                    UiIcon { name: "radio"; tint: Theme.accent; width: 28; height: 28 }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: row.modelData.name; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: row.modelData.description; color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiIcon { name: "play"; tint: Theme.accent }
                }
            }
            Text { visible: stationList.count === 0; anchors.centerIn: parent; text: "No matching stations"; color: Theme.muted; font.pixelSize: 21 }
        }
            }
        }
    }
}
