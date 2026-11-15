import QtQuick
import QtQuick.Controls
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

InputPanel {
    id: panel
    objectName: "virtualKeyboard"
    required property var hostWindow
    readonly property real visibleHeight: visible ? height : 0
    parent: Overlay.overlay
    x: 0; y: hostWindow.height - height; width: hostWindow.width
    z: 1000
    visible: active && !screenControl.sleeping && !screenControl.powerPending
    Component.onCompleted: {
        VirtualKeyboardSettings.styleName = "thinksmart"
        VirtualKeyboardSettings.activeLocales = ["en_GB"]
        VirtualKeyboardSettings.locale = "en_GB"
    }
    Connections { target: screenControl; function onChanged() { if(screenControl.sleeping || screenControl.powerPending) Qt.inputMethod.hide() } }
}
