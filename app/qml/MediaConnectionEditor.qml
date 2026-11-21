import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    objectName: "mediaConnectionEditor"
    property var library: null
    property string provider: "navidrome"
    property real keyboardHeight: 0
    property var saved: ({})
    readonly property bool music: provider==="navidrome"
    readonly property bool busy: library ? library.connectionBusy : false
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function begin(service, client) {
        if(library)library.cancelConnectionCheck()
        provider=service;library=client;saved=library.connectionSettings()
        server.text=saved.url || "";username.text=saved.username || "";secret.text=""
        tokenMode.checked=saved.authentication==="token";allowHttp.checked=!!saved.allowHttp
        fields.contentY=0
    }
    function hideKeyboard() { page.forceActiveFocus();Qt.inputMethod.hide() }
    function close() { hideKeyboard();if(library)library.cancelConnectionCheck();secret.text="";page.back() }
    function check(save) {
        hideKeyboard()
        library.checkConnection({url:server.text,username:username.text,secret:secret.text,authentication:tokenMode.checked?"token":"account",allowHttp:allowHttp.checked},save)
    }
    function revealFocused() {
        let item=page.Window.window ? page.Window.window.activeFocusItem : null
        while(item && item.parent!==form)item=item.parent
        if(!item)return
        const position=item.mapToItem(form,0,0)
        if(position.y<fields.contentY)fields.contentY=Math.max(0,position.y-10)
        else if(position.y+item.height>fields.contentY+fields.height)fields.contentY=Math.max(0,Math.min(fields.contentHeight-fields.height,position.y+item.height-fields.height+10))
    }
    onKeyboardHeightChanged: Qt.callLater(revealFocused)
    onHeightChanged: Qt.callLater(revealFocused)
    onVisibleChanged: if(!visible){if(library)library.cancelConnectionCheck();secret.text=""}
    component Label: Text { color: Theme.ink; font.pixelSize: 20; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    component Field: TextField {
        Layout.fillWidth: true; implicitHeight: 60; font.pixelSize: 22; color: Theme.ink; placeholderTextColor: Theme.muted
        selectionColor: Theme.accent; selectedTextColor: Theme.accentInk; selectByMouse: true; maximumLength: 8192
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        enabled: !page.busy
        background: Rectangle { color: Theme.raised; radius: 12; border.color: parent.activeFocus?Theme.accent:Theme.border }
        onAccepted: page.hideKeyboard()
        onActiveFocusChanged: if(activeFocus)Qt.callLater(page.revealFocused)
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; anchors.bottomMargin: 24+page.keyboardHeight; spacing: 14
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            UiButton { objectName: "mediaSettingsBack"; iconName: "back"; onClicked: page.close() }
            Label { text: page.music?"Music · Navidrome":"Video · Jellyfin"; font.pixelSize: 28 }
        }
        Flickable {
            id: fields; objectName: "mediaConnectionFields"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentHeight: form.implicitHeight
            onHeightChanged: Qt.callLater(page.revealFocused)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: form; width: fields.width; spacing: 12
                Label { text: "Server address" }
                Field { id: server; objectName: "mediaServerUrl"; maximumLength: 2048; placeholderText: page.music?"https://music.example.net":"http://server:8096"; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    UiButton { objectName: "mediaAccountMode"; text: "Account"; selected: !tokenMode.checked; enabled: !page.busy; onClicked: {tokenMode.checked=false;secret.text=""} }
                    UiButton { id: tokenMode; objectName: "mediaTokenMode"; checked: false; text: page.music?"API key":"Access token"; selected: checked; enabled: !page.busy; onClicked: {checked=true;secret.text=""} }
                }
                Label { visible: !tokenMode.checked; text: "Username" }
                Field { id: username; objectName: "mediaUsername"; visible: !tokenMode.checked; maximumLength: 256; placeholderText: "Username" }
                Label { text: tokenMode.checked?(page.music?"API key":"Access token"):"Password" }
                Field {
                    id: secret; objectName: "mediaSecret"; echoMode: TextInput.Password
                    placeholderText: page.saved.hasSecret && page.saved.authentication===(tokenMode.checked?"token":"account")?"Saved · leave blank to keep":"Enter credential"
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                }
                CheckDelegate {
                    id: allowHttp; objectName: "mediaAllowHttp"; Layout.fillWidth: true; implicitHeight: 60; enabled: !page.busy
                    text: "Allow HTTP on a private network"; font.pixelSize: 20; palette.text: Theme.ink
                    contentItem: Text { text: allowHttp.text; color: Theme.ink; font: allowHttp.font; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap; rightPadding: 50 }
                    indicator: Rectangle {
                        width: 28; height: 28; x: allowHttp.width-width-14; y: (allowHttp.height-height)/2; radius: 5
                        color: allowHttp.checked?Theme.accent:Theme.surface; border.color: allowHttp.checked?Theme.accent:Theme.muted
                        Text { anchors.centerIn: parent; visible: allowHttp.checked; text: "✓"; color: Theme.accentInk; font.pixelSize: 22 }
                    }
                    background: Rectangle { color: Theme.raised; radius: 12 }
                }
                Label { text: "Saved secrets stay hidden. Changing server or account requires new credentials."; color: Theme.muted; font.pixelSize: 17 }
                Label {
                    objectName: "mediaConnectionMessage"
                    visible: text.length>0
                    text: page.library ? page.library.connectionMessage : ""
                    color: page.library && page.library.connectionSucceeded?Theme.accent:Theme.muted; font.pixelSize: 18
                }
                Label { text: "Save checks the connection, then restarts the dashboard and stops playback or conversation. Other connections and your layout are kept."; color: Theme.muted; font.pixelSize: 17 }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            UiButton { objectName: "mediaTestConnection"; text: page.busy?"Checking…":"Test connection"; Layout.fillWidth: true; enabled: !page.busy; onClicked: page.check(false) }
            UiButton { objectName: "mediaSaveConnection"; text: "Save & restart"; primary: true; Layout.fillWidth: true; enabled: !page.busy; onClicked: page.check(true) }
        }
    }
}
