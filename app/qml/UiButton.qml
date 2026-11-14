import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool primary: false
    property bool selected: false
    property string iconName: ""
    property string description: text
    implicitHeight: 64
    implicitWidth: Math.max(64, contents.implicitWidth + 32)
    font.family: Theme.font
    font.pixelSize: 19
    Accessible.name: description
    opacity: enabled ? 1 : 0.4
    contentItem: Item {
        implicitWidth: contents.implicitWidth; implicitHeight: contents.implicitHeight
        Row {
            id: contents
            anchors.centerIn: parent; spacing: 8
            UiIcon { visible: control.iconName.length > 0; name: control.iconName; tint: control.primary ? Theme.accentInk : Theme.ink; anchors.verticalCenter: parent.verticalCenter }
            Text { visible: control.text.length > 0; text: control.text; font: control.font; color: control.primary ? Theme.accentInk : Theme.ink; anchors.verticalCenter: parent.verticalCenter }
        }
    }
    background: Rectangle {
        radius: 15
        color: control.down ? Theme.border : (control.primary ? Theme.accent : (control.selected ? Theme.tint : Theme.raised))
        border.width: 1
        border.color: control.activeFocus || control.selected ? Theme.accent : (control.primary ? Theme.accent : Theme.border)
    }
}
