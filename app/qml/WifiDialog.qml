import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "wifiDialog"
    required property var hostWindow
    property real keyboardHeight: 0
    property string view: "available"
    property var selected: ({})
    property bool hiddenNetwork: false
    property string message: ""
    readonly property var wifi: home.wifi
    readonly property bool form: view === "join" || view === "profile"
    readonly property string security: hiddenNetwork ? securityChoice.currentValue || "wpa-psk" : selected.security || "open"
    parent: Overlay.overlay
    width: Math.min(hostWindow.width - 40, 860)
    height: Math.min(hostWindow.height - keyboardHeight - 40, form ? (view === "profile" ? 560 : hiddenNetwork ? 480 : 360) : 740)
    x: (hostWindow.width - width) / 2
    y: Math.max(12, (hostWindow.height - keyboardHeight - height) / 2)
    padding: 22; modal: true
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Theme.scrim }
    palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.windowText: Theme.ink
    palette.base: Theme.raised; palette.button: Theme.raised; palette.highlight: Theme.accent; palette.highlightedText: Theme.accentInk
    function hideKeyboard() { hostWindow.contentItem.forceActiveFocus(); Qt.inputMethod.hide() }
    function choose(row, saved) {
        hideKeyboard(); selected = row; hiddenNetwork = false; message = ""; password.text = ""; ssid.text = row.ssid || ""
        autoConnect.checked = row.autoconnect === undefined ? true : row.autoconnect
        priority.value = row.priority || 0
        view = saved ? "profile" : "join"
    }
    function back() { hideKeyboard(); password.text = ""; message = ""; view = "available" }
    function connectSelected() {
        hideKeyboard()
        const secret = password.text
        password.text = ""
        if(view === "profile") wifi.perform("connectSaved", { id: selected.id, password: secret })
        else wifi.perform("connect", { id: selected.id || "", ssid: ssid.text, password: secret, security: security, hidden: hiddenNetwork })
    }
    function revealFocused() {
        const item = hostWindow.activeFocusItem
        if(!item || !form)return
        const position = item.mapToItem(fields, 0, 0)
        if(position.y < editor.contentY)editor.contentY = Math.max(0, position.y - 8)
        else if(position.y + item.height > editor.contentY + editor.height)editor.contentY = Math.max(0, Math.min(editor.contentHeight - editor.height, position.y + item.height - editor.height + 12))
    }
    onKeyboardHeightChanged: Qt.callLater(revealFocused)
    onOpened: { view = "available"; message = ""; wifi.active = true }
    onClosed: { hideKeyboard(); password.text = ""; ssid.text = ""; selected = ({}); wifi.active = false }
    Connections { target: screenControl; function onChanged() { if(screenControl.sleeping)dialog.close() } }
    Connections { target: dialog.wifi; function onCompleted(ok, message) { dialog.message = message; if(ok && dialog.form)dialog.back() } }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            UiButton { visible: dialog.form; iconName: "back"; implicitWidth: 54; implicitHeight: 54; description: "Back to Wi-Fi networks"; onClicked: dialog.back() }
            Text {
                text: dialog.view === "profile" ? "Saved network" : dialog.view === "join" ? "Connect to Wi-Fi" : "Wi-Fi"
                color: Theme.ink; font.pixelSize: 28; Layout.fillWidth: true
            }
            UiButton { objectName: "wifiCloseButton"; text: "Close"; implicitHeight: 54; onClicked: dialog.close() }
        }
        Text {
            text: dialog.wifi.status + (dialog.wifi.addresses ? " · " + dialog.wifi.addresses : "")
            color: Theme.muted; font.pixelSize: 17; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Text {
            visible: (dialog.wifi.problem || dialog.message).length > 0
            text: dialog.wifi.problem || dialog.message; textFormat: Text.PlainText
            color: dialog.wifi.problem ? Theme.danger : Theme.accent; font.pixelSize: 16; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        RowLayout {
            visible: !dialog.form; Layout.fillWidth: true; spacing: 10
            UiButton { objectName: "wifiAvailableTab"; text: "Available"; selected: dialog.view === "available"; implicitHeight: 54; onClicked: dialog.view = "available" }
            UiButton { objectName: "wifiSavedTab"; text: "Saved"; selected: dialog.view === "saved"; implicitHeight: 54; onClicked: dialog.view = "saved" }
            Item { Layout.fillWidth: true }
            UiButton { objectName: "wifiScanButton"; text: dialog.wifi.busy ? "Wait…" : "Scan"; implicitHeight: 54; enabled: !dialog.wifi.busy; onClicked: dialog.wifi.scan() }
        }
        ListView {
            id: networks; objectName: "wifiNetworkList"
            visible: !dialog.form
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            model: dialog.view === "saved" ? dialog.wifi.saved : dialog.wifi.networks
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: networks.width; height: 88; radius: 14; color: Theme.raised; border.color: modelData.connected ? Theme.accent : Theme.border
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 12
                    UiIcon { name: "wifi"; tint: row.modelData.connected ? Theme.accent : Theme.muted; width: 28; height: 28 }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 5
                        Text { text: row.modelData.ssid || row.modelData.name; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text {
                            text: row.modelData.connected ? "Connected" : dialog.view === "saved" ? (row.modelData.autoconnect ? "Automatic" : "Manual") + " · priority " + row.modelData.priority : (row.modelData.security === "open" ? "Open" : row.modelData.security === "unsupported" ? "Enterprise / legacy security" : "Password protected") + " · " + row.modelData.strength + "%"
                            color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight
                        }
                    }
                    UiButton {
                        objectName: "wifiSelect" + row.index
                        text: dialog.view === "saved" ? "Manage" : row.modelData.connected ? "Saved" : "Connect"
                        implicitHeight: 54; enabled: !dialog.wifi.busy && (dialog.view === "saved" || row.modelData.security !== "unsupported")
                        onClicked: {
                            if(dialog.view === "saved")dialog.choose(row.modelData, true)
                            else if(row.modelData.savedId) {
                                const profile = dialog.wifi.saved.find(p => p.id === row.modelData.savedId)
                                if(profile)dialog.choose(profile, true)
                            } else dialog.choose(row.modelData, false)
                        }
                    }
                }
            }
            Text { visible: networks.count === 0; anchors.centerIn: parent; text: dialog.wifi.busy ? "Looking for networks…" : "No networks found"; color: Theme.muted; font.pixelSize: 20 }
        }
        Flickable {
            id: editor; visible: dialog.form
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentHeight: fields.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: fields; width: editor.width; spacing: 10
                Text { visible: !dialog.hiddenNetwork; text: dialog.selected.ssid || ""; textFormat: Text.PlainText; color: Theme.accent; font.pixelSize: 23; wrapMode: Text.Wrap; Layout.fillWidth: true }
                TextField {
                    id: ssid; objectName: "wifiSsidInput"; visible: dialog.hiddenNetwork
                    placeholderText: "Hidden network name"; font.pixelSize: 22; color: Theme.ink
                    maximumLength: 32
                    Layout.fillWidth: true; implicitHeight: 60; selectByMouse: true
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onActiveFocusChanged: if(activeFocus) { Qt.inputMethod.show(); Qt.callLater(dialog.revealFocused) }
                    background: Rectangle { color: Theme.raised; radius: 10; border.color: ssid.activeFocus ? Theme.accent : Theme.border }
                }
                ComboBox {
                    id: securityChoice; objectName: "wifiSecurityChoice"; visible: dialog.hiddenNetwork
                    Layout.fillWidth: true; implicitHeight: 56; font.pixelSize: 20
                    textRole: "label"; valueRole: "value"; model: [{label:"WPA / WPA2 personal",value:"wpa-psk"},{label:"Open · no password",value:"open"}]
                }
                RowLayout {
                    visible: dialog.security !== "open"
                    Layout.fillWidth: true
                    TextField {
                        id: password; objectName: "wifiPasswordInput"
                        placeholderText: dialog.view === "profile" ? "New password (optional)" : "Wi-Fi password"
                        maximumLength: 64
                        Layout.fillWidth: true; implicitHeight: 60; font.pixelSize: 22; color: Theme.ink; selectByMouse: true
                        echoMode: reveal.checked ? TextInput.Normal : TextInput.Password
                        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        onActiveFocusChanged: if(activeFocus) { Qt.inputMethod.show(); Qt.callLater(dialog.revealFocused) }
                        onAccepted: dialog.hideKeyboard()
                        background: Rectangle { color: Theme.raised; radius: 10; border.color: password.activeFocus ? Theme.accent : Theme.border }
                    }
                    UiButton { id: reveal; objectName: "wifiRevealPassword"; text: checked ? "Hide" : "Show"; checkable: true; implicitHeight: 60; focusPolicy: Qt.NoFocus }
                }
                RowLayout {
                    visible: dialog.view === "profile"; Layout.fillWidth: true
                    Text { text: "Connect automatically"; color: Theme.ink; font.pixelSize: 19; Layout.fillWidth: true }
                    Switch { id: autoConnect; objectName: "wifiAutoconnect" }
                }
                RowLayout {
                    visible: dialog.view === "profile"; Layout.fillWidth: true
                    Text { text: "Priority"; color: Theme.ink; font.pixelSize: 19; Layout.fillWidth: true }
                    SpinBox { id: priority; objectName: "wifiPriority"; from: -999; to: 999; stepSize: 10; editable: false; implicitHeight: 56; implicitWidth: 200; font.pixelSize: 20 }
                }
                UiButton {
                    objectName: "wifiSavePreferences"; text: "Save preferences"; visible: dialog.view === "profile"; enabled: !dialog.wifi.busy
                    Layout.fillWidth: true; implicitHeight: 54
                    onClicked: { dialog.hideKeyboard(); dialog.wifi.perform("preferences", {id:dialog.selected.id, autoconnect:autoConnect.checked, priority:priority.value}) }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            UiButton {
                objectName: "wifiHiddenButton"; visible: !dialog.form; text: "Hidden network"; implicitHeight: 56
                onClicked: { dialog.choose({},false); dialog.hiddenNetwork = true; ssid.text = ""; Qt.callLater(() => ssid.forceActiveFocus()) }
            }
            UiButton { objectName: "wifiDisconnectButton"; visible: !dialog.form; text: "Disconnect"; implicitHeight: 56; enabled: !dialog.wifi.busy; onClicked: dialog.wifi.perform("disconnect") }
            UiButton { objectName: "wifiForgetButton"; visible: dialog.view === "profile"; text: "Forget"; implicitHeight: 56; enabled: !dialog.wifi.busy; onClicked: forget.open() }
            Item { Layout.fillWidth: true }
            UiButton { objectName: "wifiConnectButton"; visible: dialog.form; text: "Connect"; primary: true; implicitHeight: 56; enabled: !dialog.wifi.busy && (!dialog.hiddenNetwork || ssid.text.length > 0); onClicked: dialog.connectSelected() }
        }
    }
    Dialog {
        id: forget; parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(dialog.hostWindow.width-48,520); padding: 24; modal: true
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 20
            Text { text: "Forget this network?"; color: Theme.ink; font.pixelSize: 26 }
            Text { text: "Its saved Wi-Fi settings and password will be removed."; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 19; Layout.fillWidth: true }
            RowLayout {
                UiButton { text: "Cancel"; onClicked: forget.close() }
                UiButton { objectName: "wifiConfirmForget"; text: "Forget"; onClicked: { forget.close(); dialog.wifi.perform("forget",{id:dialog.selected.id}) } }
            }
        }
    }
}
