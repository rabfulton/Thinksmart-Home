import QtQuick
import QtQuick.VirtualKeyboard.Styles
import "qrc:/qml"

KeyPanel {
    id: panel
    property string label: control ? (control.uppercased ? control.displayText.toUpperCase() : control.displayText) : ""
    property bool highlighted: false
    objectName: "virtualKey_" + label
    Rectangle {
        anchors.fill: parent; anchors.margins: Math.max(3, parent.height * 0.055)
        radius: 9; color: panel.control && panel.control.pressed ? Theme.border : panel.highlighted ? Theme.tint : Theme.raised
        border.color: panel.highlighted ? Theme.accent : Theme.border
        Text {
            anchors.centerIn: parent; text: panel.label; textFormat: Text.PlainText; color: panel.highlighted ? Theme.accent : Theme.ink
            font.family: Theme.font; font.pixelSize: Math.min(28, panel.height * 0.4)
        }
    }
}
