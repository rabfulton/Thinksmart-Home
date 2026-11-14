import QtQuick
import QtQuick.Controls

AbstractButton {
    id: tile
    property bool sampleData: false
    property string badge: sampleData ? "Sample" : ""
    property color badgeColor: Theme.faint
    property string heading: ""
    property string iconName: ""
    property string indexLabel: ""
    default property alias body: bodyItem.data
    hoverEnabled: true
    Accessible.name: heading
    contentItem: Item {
        Text { x: 20; y: 18; text: tile.heading; color: Theme.muted; font.family: Theme.font; font.pixelSize: 18 }
        Text { visible: tile.badge.length > 0; text: tile.badge; color: tile.badgeColor; font.pixelSize: 13; anchors.right: parent.right; anchors.rightMargin: 20; y: 22 }
        Item { id: bodyItem; anchors.fill: parent; anchors.topMargin: 54; anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.bottomMargin: 42 }
        Text { x: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 17; text: tile.indexLabel; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 12 }
        UiIcon { name: "arrow"; tint: Theme.faint; width: 19; height: 19; anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 15 }
    }
    background: Rectangle {
        radius: Theme.radius
        color: tile.down ? Theme.raised : Theme.surface
        border.width: 1
        border.color: tile.activeFocus || tile.hovered ? Theme.accent : Theme.border
    }
}
