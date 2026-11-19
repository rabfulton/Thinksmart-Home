import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "weatherLocationDialog"
    required property var hostWindow
    property real keyboardHeight: 0
    parent: Overlay.overlay
    width: Math.min(hostWindow.width-40,720)
    height: Math.min(hostWindow.height-keyboardHeight-32,560)
    x: (hostWindow.width-width)/2; y: Math.max(12,(hostWindow.height-keyboardHeight-height)/2)
    modal: true; padding: 20
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Theme.scrim }
    function hideKeyboard() { hostWindow.contentItem.forceActiveFocus();Qt.inputMethod.hide() }
    function search() { hideKeyboard();weather.searchLocations(query.text) }
    onOpened: { query.text="";weather.cancelSearch() }
    onClosed: { hideKeyboard();weather.cancelSearch() }
    Connections { target: screenControl; function onChanged() { if(screenControl.sleeping)dialog.close() } }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Text { text: "Weather location"; color: Theme.ink; font.pixelSize: 26; Layout.fillWidth: true }
            UiButton { text: "Close"; onClicked: dialog.close() }
        }
        Text { text: "Current: " + weather.location; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; elide: Text.ElideRight }
        RowLayout {
            TextField {
                id: query; objectName: "weatherLocationQuery"; Layout.fillWidth: true; implicitHeight: 56; font.pixelSize: 21; color: Theme.ink
                placeholderText: "Town, city or postal code"; placeholderTextColor: Theme.faint; maximumLength: 100
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: Theme.raised; radius: 12; border.color: query.activeFocus ? Theme.accent : Theme.border }
                onAccepted: dialog.search()
            }
            UiButton { objectName: "weatherLocationSearch"; text: "Search"; enabled: query.text.trim().length>=2 && !weather.searching; onClicked: dialog.search() }
        }
        Text {
            text: weather.searching ? "Finding places…" : weather.searchProblem || "Search, then choose a place. Your location is saved on this device."
            color: weather.searchProblem ? Theme.accent : Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        ListView {
            objectName: "weatherLocationResults"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8; model: weather.locations
            ScrollBar.vertical: ScrollBar { }
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                objectName: "weatherLocationResult" + index
                width: ListView.view.width; height: 66
                background: Rectangle { color: Theme.raised; radius: 12; border.color: Theme.border }
                contentItem: Column {
                    spacing: 3
                    Text { text: modelData.label; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 19; width: parent.width; elide: Text.ElideRight }
                    Text { text: modelData.timezone; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14 }
                }
                onClicked: if(weather.selectLocation(index))dialog.close()
            }
        }
        UiButton { objectName: "weatherLocationReset"; text: "Use installed location"; onClicked: if(weather.resetLocation())dialog.close() }
    }
}
