import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    required property var library
    property bool active: true
    property bool requested: false
    function load() { if(active && library.configured && !requested) { requested = true; library.refresh() } }
    onActiveChanged: load()
    Component.onCompleted: load()
    ColumnLayout {
        anchors.fill: parent; spacing: 14
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: panel.library.title; color: Theme.ink; font.pixelSize: 24; Layout.fillWidth: true; elide: Text.ElideRight }
            UiButton { text: "Reload"; enabled: panel.library.configured && !panel.library.busy; onClicked: panel.library.refresh() }
        }
        Text { text: panel.library.status; color: panel.library.cached ? Theme.accent : Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: panel.library.problem.length > 0; text: panel.library.problem; color: Theme.danger; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            visible: panel.library.configured
            Layout.fillWidth: true
            TextField {
                id: searchField
                Layout.fillWidth: true; implicitHeight: 58; font.pixelSize: 20
                placeholderText: "Search your library"; color: Theme.ink
                background: Rectangle { color: Theme.background; radius: 14; border.color: Theme.border }
                onAccepted: panel.library.search(text)
            }
            UiButton { text: "Search"; enabled: !panel.library.busy; onClicked: panel.library.search(searchField.text) }
        }
        ListView {
            id: list
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 9; model: panel.library.items
            ScrollBar.vertical: ScrollBar { }
            delegate: AbstractButton {
                id: row
                required property var modelData
                required property int index
                width: list.width; height: 84; enabled: !panel.library.busy
                onClicked: panel.library.open(index)
                background: Rectangle { radius: 15; color: row.down ? Theme.border : Theme.raised }
                contentItem: RowLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 15
                    UiIcon { name: row.modelData.type === "song" ? "play" : (row.modelData.type === "album" ? "music" : "video"); tint: Theme.accent; width: 30; height: 30 }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: row.modelData.title; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text {
                            text: row.modelData.subtitle + (row.modelData.resume > 10 ? " · Resume at " + Math.floor(row.modelData.resume/60) + " min" : "")
                            color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight
                        }
                    }
                    UiIcon { name: "arrow"; tint: Theme.faint }
                }
            }
            Text {
                visible: list.count === 0 && !panel.library.busy
                anchors.centerIn: parent; width: parent.width * 0.8
                text: panel.library.configured ? "No library items are available yet." : "Your library will appear here when the server connection is added."
                color: Theme.muted; font.pixelSize: 22; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
            }
        }
        RowLayout {
            visible: panel.library.canPrevious || panel.library.canNext
            Layout.fillWidth: true
            UiButton { text: "Previous"; enabled: panel.library.canPrevious && !panel.library.busy; onClicked: panel.library.page(-1) }
            Item { Layout.fillWidth: true }
            UiButton { text: "Next"; enabled: panel.library.canNext && !panel.library.busy; onClicked: panel.library.page(1) }
        }
    }
}
