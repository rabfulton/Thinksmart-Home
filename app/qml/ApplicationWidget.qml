import QtQuick
import QtQuick.Layouts

WidgetTile {
    id: card
    property string mainText: ""
    property string detailText: ""
    property string piece: ""
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Item {
            Layout.fillWidth: true; Layout.preferredHeight: 82
            UiIcon { visible: !card.piece.length; anchors.verticalCenter: parent.verticalCenter; name: card.iconName; tint: Theme.accent; strokeWidth: 0.65; width: 100; height: 100 }
            Rectangle {
                visible: card.piece.length; anchors.verticalCenter: parent.verticalCenter
                width: 92; height: 92; color: Theme.chessDark; radius: 12
                Image { anchors.fill: parent; anchors.margins: 8; source: card.piece; sourceSize: Qt.size(width*2,height*2); fillMode: Image.PreserveAspectFit }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 24; spacing: 12
            Text { text: card.mainText; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: card.detailText; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; elide: Text.ElideRight }
        }
        Item { Layout.fillHeight: true }
    }
}
