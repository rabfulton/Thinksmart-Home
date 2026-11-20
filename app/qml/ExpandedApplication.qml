import QtQuick
import QtQuick.Layouts

Rectangle {
    id: frame
    property string title: ""
    property bool compact: false
    property Component view
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: frame.compact ? 0 : 20; spacing: frame.compact ? 0 : 14
        RowLayout {
            visible: !frame.compact
            Layout.fillWidth: true
            Item { Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
            Text { text: frame.title; color: Theme.ink; font.pixelSize: 27; Layout.fillWidth: true }
        }
        Loader { Layout.fillWidth: true; Layout.fillHeight: true; sourceComponent: frame.view }
    }
    UiButton { objectName: "expandedApplicationBack"; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 20; z: 1; iconName: "back"; onClicked: frame.back() }
}
