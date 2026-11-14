import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    property bool active: true
    property bool phoneTab: home.modules.bluetooth && (!home.modules.navidrome || !musicLibrary.configured)
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 18
        RowLayout {
            spacing: 20
            UiButton { iconName: "back"; description: "Back"; onClicked: !page.phoneTab && musicLibrary.canBack ? musicLibrary.back() : page.back() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: "Music"; color: Theme.ink; font.pixelSize: 29 }
                Text { text: "Your library and audio from your phone"; color: Theme.muted; font.pixelSize: 16 }
            }
            UiButton { visible: home.modules.navidrome; text: "Library"; selected: !page.phoneTab; onClicked: page.phoneTab = false }
            UiButton { visible: home.modules.bluetooth; text: "Phone"; selected: page.phoneTab; onClicked: page.phoneTab = true }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: page.phoneTab ? 1 : 0
            LibraryPanel { library: musicLibrary; active: page.active && !page.phoneTab }
            ScrollView {
                id: scroll
                contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: scroll.availableWidth; spacing: 18
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 148; radius: 20; color: Theme.raised
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 22; spacing: 10
                            RowLayout {
                                UiIcon { name: "bluetooth"; tint: Theme.accent; width: 32; height: 32 }
                                Text { text: "ThinkSmart Speaker"; color: Theme.ink; font.pixelSize: 26; Layout.fillWidth: true }
                            }
                            Text { text: phoneAudio.status; color: Theme.muted; font.pixelSize: 19; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                    RowLayout {
                        spacing: 12
                        UiButton { text: home.source === "bluetooth" && phoneAudio.active ? "Phone audio selected" : "Use phone audio"; primary: true; enabled: phoneAudio.available; onClicked: home.useBluetooth() }
                        UiButton {
                            text: phoneAudio.pairing ? "Stop pairing" : "Pair a phone"; enabled: phoneAudio.available
                            onClicked: { if(phoneAudio.pairing) phoneAudio.endPairing(); else { home.useBluetooth(); phoneAudio.startPairing() } }
                        }
                        UiButton { text: "Disconnect"; visible: phoneAudio.connected; onClicked: phoneAudio.disconnectPhone() }
                    }
                    Text { text: "Choose ThinkSmart Speaker in your phone's Bluetooth settings. Confirm the matching code on both devices, then play audio from your phone."; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Phone audio and radio use the same speaker. Selecting a source stops the previous one."; color: Theme.faint; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { visible: phoneAudio.devices.length > 0; text: "Paired phones"; color: Theme.ink; font.pixelSize: 23 }
                    Repeater {
                        model: phoneAudio.devices
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text { text: modelData.name + (modelData.connected ? " · Connected" : ""); color: Theme.ink; font.pixelSize: 20; Layout.fillWidth: true; elide: Text.ElideRight }
                            UiButton { text: "Connect"; visible: !modelData.connected; onClicked: phoneAudio.connectPhone(modelData.path) }
                            UiButton { text: "Forget"; onClicked: phoneAudio.forgetPhone(modelData.path) }
                        }
                    }
                    Item { Layout.preferredHeight: 8 }
                }
            }
        }
    }
    Connections {
        target: phoneAudio
        function onChanged() { if(phoneAudio.confirmationPending) { if(!pairDialog.visible) pairDialog.open() } else pairDialog.close() }
    }
    Dialog {
        id: pairDialog
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(600, page.Window.width - 48); padding: 28
        modal: true; closePolicy: Popup.NoAutoClose
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 22
            Text { text: "Pair with " + phoneAudio.confirmationName + "?"; color: Theme.ink; font.pixelSize: 28; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { visible: phoneAudio.confirmationCode.length > 0; text: phoneAudio.confirmationCode; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 44; font.letterSpacing: 6; Layout.alignment: Qt.AlignHCenter }
            Text { text: phoneAudio.confirmationCode.length ? "Check that this code matches the one on your phone." : "Allow this phone to pair for audio playback."; color: Theme.muted; font.pixelSize: 20; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: phoneAudio.confirmPairing(false) }
                UiButton { text: "Pair"; primary: true; Layout.fillWidth: true; onClicked: phoneAudio.confirmPairing(true) }
            }
        }
    }
}
