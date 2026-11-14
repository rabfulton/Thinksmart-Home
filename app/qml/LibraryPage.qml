import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    required property var library
    property string heading: "Video"
    property bool active: true
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 18
        RowLayout {
            spacing: 20
            UiButton { iconName: "back"; description: page.library.canBack ? "Back in library" : "Back to dashboard"; onClicked: page.library.canBack ? page.library.back() : page.back() }
            Text { text: page.heading; color: Theme.ink; font.pixelSize: 29; Layout.fillWidth: true }
            UiIcon { name: "video"; tint: Theme.accent; width: 30; height: 30 }
        }
        LibraryPanel { library: page.library; active: page.active; Layout.fillWidth: true; Layout.fillHeight: true }
    }
}
