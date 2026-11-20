import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: dialogs
    required property var hostWindow
    property real keyboardHeight: 0
    property string confirmation: "shutdown"
    readonly property color ink: Theme.ink
    readonly property color quiet: Theme.muted
    readonly property color gold: Theme.accent
    component ActionButton: UiButton { implicitHeight: 72; font.pixelSize: 22 }
    component Sheet: Dialog {
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(dialogs.hostWindow.width - 48, 630)
        padding: 28
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        palette.window: Theme.surface
        palette.windowText: dialogs.ink
        palette.text: dialogs.ink
        palette.base: Theme.raised
        palette.button: Theme.raised
        palette.buttonText: dialogs.ink
        palette.highlight: dialogs.gold
        palette.highlightedText: Theme.accentInk
        background: Rectangle { color: Theme.surface; radius: 28; border.color: Theme.border }
        Overlay.modal: Rectangle { color: Theme.scrim }
    }
    Connections {
        target: screenControl
        function onPowerMenuRequested() { powerSheet.open() }
        function onSettingsRequested() { settingsSheet.open() }
    }
    Sheet {
        id: powerSheet
        objectName: "powerSheet"
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: "Device settings"; color: dialogs.ink; font.pixelSize: 30; Layout.bottomMargin: 8 }
            ActionButton { objectName: "wifiSettingsButton"; text: "Wi-Fi settings"; Layout.fillWidth: true; onClicked: { powerSheet.close(); wifiSheet.open() } }
            ActionButton { objectName: "displaySettingsButton"; text: "Display settings"; Layout.fillWidth: true; onClicked: { powerSheet.close(); settingsSheet.open() } }
            ActionButton { objectName: "modulesSettingsButton"; text: "Pages & modules"; Layout.fillWidth: true; onClicked: { powerSheet.close(); home.editLayout() } }
            ActionButton { text: "Restart"; Layout.fillWidth: true; onClicked: { dialogs.confirmation = "restart"; powerSheet.close(); confirmSheet.open() } }
            ActionButton { objectName: "shutdownMenuButton"; text: "Shut down for unplugging"; Layout.fillWidth: true; onClicked: { dialogs.confirmation = "shutdown"; powerSheet.close(); confirmSheet.open() } }
            ActionButton { text: "Close"; Layout.fillWidth: true; onClicked: powerSheet.close() }
        }
    }
    WifiDialog { id: wifiSheet; hostWindow: dialogs.hostWindow; keyboardHeight: dialogs.keyboardHeight }
    Connections { target: home.wifi; function onOpenRequested() { powerSheet.close(); wifiSheet.open() } }
    Sheet {
        id: settingsSheet
        objectName: "settingsSheet"
        property int previousBrightness: 50
        property string previousTheme: ""
        property bool saved: false
        property bool validTimers: offChoice.currentValue === 0 || dimChoice.currentValue === 0 || dimChoice.currentValue < offChoice.currentValue
        onOpened: {
            previousBrightness = screenControl.brightness
            saved = false
            previousTheme = home.themeName
            dimChoice.currentIndex = dimChoice.indexOfValue(screenControl.dimSeconds)
            offChoice.currentIndex = offChoice.indexOfValue(screenControl.sleepSeconds)
        }
        onClosed: { if(!saved) { screenControl.setBrightness(previousBrightness); home.setTheme(previousTheme) } }
        contentItem: ColumnLayout {
            spacing: 18
            Text { text: "Display settings"; color: dialogs.ink; font.pixelSize: 30 }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Theme"; color: dialogs.ink; font.pixelSize: 22; Layout.fillWidth: true }
                ActionButton {
                    objectName: "themeButton"; implicitWidth: 240; implicitHeight: 60
                    iconName: "palette"; text: Theme.title
                    onClicked: home.setTheme(Theme.names[(Theme.names.indexOf(home.themeName) + 1) % Theme.names.length])
                }
            }
            RowLayout {
                Text { text: "Brightness"; color: dialogs.quiet; font.pixelSize: 22 }
                Item { Layout.fillWidth: true }
                Text { text: screenControl.brightness + "%"; color: dialogs.ink; font.pixelSize: 22 }
            }
            Slider {
                Layout.fillWidth: true; implicitHeight: 64
                from: 35; to: 100; stepSize: 1; value: screenControl.brightness
                onMoved: screenControl.setBrightness(Math.round(value))
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Dim after"; color: dialogs.ink; font.pixelSize: 22; Layout.fillWidth: true }
                ComboBox {
                    id: dimChoice; objectName: "dimChoice"; implicitWidth: 240; implicitHeight: 60; font.pixelSize: 22
                    textRole: "label"; valueRole: "seconds"
                    model: [ {label: "Never", seconds: 0}, {label: "30 seconds", seconds: 30}, {label: "1 minute", seconds: 60}, {label: "2 minutes", seconds: 120}, {label: "5 minutes", seconds: 300}, {label: "10 minutes", seconds: 600} ]
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Screen off after"; color: dialogs.ink; font.pixelSize: 22; Layout.fillWidth: true }
                ComboBox {
                    id: offChoice; objectName: "offChoice"; implicitWidth: 240; implicitHeight: 60; font.pixelSize: 22
                    textRole: "label"; valueRole: "seconds"
                    model: [ {label: "Never", seconds: 0}, {label: "15 seconds", seconds: 15}, {label: "30 seconds", seconds: 30}, {label: "1 minute", seconds: 60}, {label: "2 minutes", seconds: 120}, {label: "5 minutes", seconds: 300}, {label: "10 minutes", seconds: 600}, {label: "30 minutes", seconds: 1800} ]
                }
            }
            Text { text: "Touch or press a volume button to wake. Audio continues with the screen off."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: dialogs.quiet; font.pixelSize: 19 }
            Text { visible: !settingsSheet.validTimers; text: "Choose an earlier dim time, or set Dim after to Never."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; font.pixelSize: 19 }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                ActionButton { objectName: "settingsCancelButton"; text: "Cancel"; Layout.fillWidth: true; onClicked: settingsSheet.close() }
                ActionButton { objectName: "settingsSaveButton"; text: "Save"; primary: true; enabled: settingsSheet.validTimers; opacity: enabled ? 1 : 0.4; Layout.fillWidth: true; onClicked: { if(screenControl.setIdleTimers(dimChoice.currentValue, offChoice.currentValue)) { settingsSheet.saved = true; settingsSheet.close() } } }
            }
        }
    }
    Sheet {
        id: confirmSheet
        objectName: "confirmSheet"
        contentItem: ColumnLayout {
            spacing: 24
            Text { text: dialogs.confirmation === "restart" ? "Restart device?" : "Shut down for unplugging?"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: dialogs.ink; font.pixelSize: 30 }
            Text {
                text: dialogs.confirmation === "restart" ? "Playback will stop while the device restarts. It will return with the screen asleep." : "Stops the device so you can unplug it. After the screen goes dark, wait 15 seconds before disconnecting power. Leave it unplugged for about 30 seconds before reconnecting to turn it on again."
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: dialogs.quiet; font.pixelSize: 22
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                ActionButton { objectName: "confirmCancelButton"; text: "Cancel"; Layout.fillWidth: true; onClicked: confirmSheet.close() }
                ActionButton {
                    objectName: "confirmPowerButton"
                    text: dialogs.confirmation === "restart" ? "Restart" : "Shut down"; primary: true; Layout.fillWidth: true
                    onClicked: { confirmSheet.close(); if(dialogs.confirmation === "restart") screenControl.restart(); else screenControl.shutdown() }
                }
            }
        }
    }
}
