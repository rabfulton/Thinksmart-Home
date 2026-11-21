import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    required property var library
    ColumnLayout {
        anchors.fill: parent; spacing: 12
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: page.library.queueCount+" tracks"+(page.library.queueFinished?" · Finished":""); color: Theme.muted; font.pixelSize: 20; Layout.fillWidth: true }
            UiButton { objectName: "musicQueuePlay"; text: "Play"; iconName: "play"; enabled: page.library.queueCount>0; onClicked: page.library.playQueue() }
            UiButton { objectName: "musicQueueReplay"; text: "Play again"; enabled: page.library.queueCount>0; onClicked: page.library.replayQueue() }
            UiButton { objectName: "musicQueueClear"; text: "Clear"; enabled: page.library.queueCount>0; onClicked: page.library.clearQueue() }
        }
        Text { visible: page.library.queueProblem.length>0; text: page.library.queueProblem; textFormat: Text.PlainText; color: Theme.danger; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ListView {
            id: list; objectName: "musicQueueList"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 9; cacheBuffer: 180
            model: page.library.queueModel
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: row; objectName: "musicQueueRow"+index
                required property var entry
                required property int index
                width: list.width; height: 84; padding: 14
                onClicked: page.library.playQueue(index)
                background: Rectangle { color: row.down?Theme.border:row.index===page.library.queueIndex?Theme.tint:Theme.raised; radius: 15; border.color: row.index===page.library.queueIndex?Theme.accent:Theme.border }
                contentItem: RowLayout {
                    spacing: 15
                    Text { text: row.index+1; color: Theme.accent; font.pixelSize: 20; Layout.preferredWidth: 36; horizontalAlignment: Text.AlignHCenter }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 5
                        Text { text: row.entry.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: row.entry.subtitle; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiIcon { name: "play"; tint: row.index===page.library.queueIndex?Theme.accent:Theme.muted; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                }
            }
            Text { visible: page.library.queueCount===0; anchors.centerIn: parent; width: parent.width*.8; text: "Tap tracks in Library to add them here.\nPlayback starts when you add a track."; color: Theme.muted; font.pixelSize: 22; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
    }
}
