import QtQuick
import QtQuick.Layouts

Rectangle {
    id: frame
    property string title: ""
    property Component view
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 14
        RowLayout {
            Layout.fillWidth: true
            UiButton { objectName: "expandedApplicationBack"; iconName: "back"; onClicked: frame.back() }
            Text { text: frame.title; color: Theme.ink; font.pixelSize: 27; Layout.fillWidth: true }
        }
        Loader { Layout.fillWidth: true; Layout.fillHeight: true; sourceComponent: frame.view }
    }
}
