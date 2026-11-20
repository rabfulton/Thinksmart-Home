import QtQuick
import QtQuick.Layouts

WidgetTile {
    id: tile
    heading: "Recipes"; indexLabel: "ASK SHEILA · COOK AT YOUR PACE"
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Item {
            Layout.fillWidth: true; Layout.preferredHeight: 82
            UiIcon { anchors.verticalCenter: parent.verticalCenter; name: "book"; tint: Theme.accent; width: 100; height: 100 }
        }
        Text { text: home.recipes.busy ? "Preparing…" : home.recipes.current.title || "What shall we cook?"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 25; wrapMode: Text.WordWrap; Layout.fillWidth: true; maximumLineCount: 2; elide: Text.ElideRight }
        Text { text: "Ingredients · Method"; color: Theme.muted; font.pixelSize: 17 }
        Item { Layout.fillHeight: true }
    }
}
