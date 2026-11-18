import QtQuick
import QtQuick.Controls

ItemDelegate {
    id: row
    property string headline: ""
    property string source: ""
    property string byline: ""
    width: ListView.view ? ListView.view.width : 0
    implicitHeight: storyText.implicitHeight
    padding: 0
    background: Rectangle { color: row.down ? Theme.tint : "transparent" }
    contentItem: Column {
        id: storyText; spacing: 7; topPadding: 14; bottomPadding: 14
        Text { width: parent.width; text: row.source; color: Theme.accent; font.pixelSize: 13; textFormat: Text.PlainText; elide: Text.ElideRight }
        Text { width: parent.width; text: row.headline; color: Theme.ink; font.pixelSize: 21; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; lineHeight: 1.1 }
        Text { width: parent.width; text: row.byline; color: Theme.muted; font.pixelSize: 13; textFormat: Text.PlainText; elide: Text.ElideRight }
    }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border; opacity: 0.6 }
}
