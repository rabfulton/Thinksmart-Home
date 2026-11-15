import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Styles
import "qrc:/qml"

KeyboardStyle {
    keyboardDesignWidth: 1000
    keyboardDesignHeight: 310
    keyboardRelativeLeftMargin: 0.012
    keyboardRelativeRightMargin: 0.012
    keyboardRelativeTopMargin: 0.035
    keyboardRelativeBottomMargin: 0.035
    keyboardBackground: Rectangle { color: Theme.surface; border.color: Theme.border }
    keyPanel: ThemedKey {}
    backspaceKeyPanel: ThemedKey { objectName: "virtualBackspaceKey"; label: "⌫" }
    shiftKeyPanel: ThemedKey { objectName: "virtualShiftKey"; label: "⇧"; highlighted: InputContext.shift || InputContext.capsLock }
    spaceKeyPanel: ThemedKey { label: "Space" }
    enterKeyPanel: ThemedKey { objectName: "virtualEnterKey"; label: "Done"; highlighted: true }
    symbolKeyPanel: ThemedKey { objectName: "virtualSymbolKey"; label: control ? control.displayText : "" }
    hideKeyPanel: ThemedKey { objectName: "virtualHideKey"; label: "⌄" }
    languageKeyPanel: ThemedKey { label: "EN" }
    modeKeyPanel: ThemedKey { label: control ? control.displayText : "" }
    selectionListHeight: 0
    selectionListDelegate: SelectionListItem { Text { text: parent.display; color: Theme.ink } }
    selectionListBackground: Rectangle { color: Theme.surface }
    selectionListHighlight: Rectangle { color: Theme.tint }
    characterPreviewDelegate: Item { property string text: ""; visible: false }
    alternateKeysListItemWidth: 54
    alternateKeysListItemHeight: 64
    alternateKeysListBackground: Rectangle { color: Theme.raised; radius: 10; border.color: Theme.accent }
    alternateKeysListDelegate: Text { text: model.text; color: Theme.ink; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    alternateKeysListHighlight: Rectangle { color: Theme.tint; radius: 8 }
}
