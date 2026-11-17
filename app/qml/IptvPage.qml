import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    property real keyboardHeight: 0
    property string tab: "suggested"
    property string query: ""
    property string countryFilter: ""
    property string categoryFilter: ""
    readonly property var client: home.iptv
    readonly property var rows: tab === "saved" ? client.savedModel : tab === "suggested" ? client.suggestedModel : client.channelModel
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    Component.onCompleted: if(client.favourites.length)tab = "saved"
    onTabChanged: applyFilters()
    onActiveChanged: if(active)applyFilters()
    function applyFilters() { if(active)client.filter(query,countryFilter,categoryFilter) }
    component Choice: ComboBox {
        id: choice
        implicitHeight: 54; font.pixelSize: 18; textRole: "title"; valueRole: "id"
        palette.button: Theme.raised; palette.buttonText: Theme.ink; palette.base: Theme.raised; palette.text: Theme.ink; palette.window: Theme.raised
        palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
        contentItem: Text { text: choice.displayText; color: Theme.ink; font: choice.font; verticalAlignment: Text.AlignVCenter; leftPadding: 14; rightPadding: 34; elide: Text.ElideRight }
        indicator: UiIcon { name: "back"; rotation: -90; tint: Theme.muted; width: 18; height: 18; x: choice.width-width-10; y: (choice.height-height)/2 }
        background: Rectangle { color: Theme.raised; radius: 12; border.color: choice.activeFocus ? Theme.accent : Theme.border }
    }
    component Input: TextField {
        implicitHeight: 56; font.pixelSize: 21; color: Theme.ink; placeholderTextColor: Theme.muted; leftPadding: 14; rightPadding: 14
        background: Rectangle { radius: 12; color: Theme.raised; border.color: parent.activeFocus ? Theme.accent : Theme.border }
    }
    Timer { id: filterDelay; interval: 250; onTriggered: page.applyFilters() }
    function hideKeyboard() { page.forceActiveFocus(); Qt.inputMethod.hide() }
    function saveChannel(id, saved) { page.client.favourite(id, saved) }
    function moreChannels() { page.client.loadMore() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; anchors.bottomMargin: Math.max(20, page.keyboardHeight); spacing: 12
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            UiButton { objectName: "iptvBackButton"; iconName: "back"; implicitHeight: 54; implicitWidth: 54; onClicked: { page.client.cancelPlayback(); page.hideKeyboard(); page.back() } }
            Choice {
                objectName: "iptvView"; Layout.preferredWidth: page.width > 900 ? 170 : 155
                model: [{id:"saved",title:"Saved"},{id:"suggested",title:"Suggested"},{id:"directory",title:"All channels"}]
                currentIndex: Math.max(0,model.findIndex(x => x.id === page.tab))
                onActivated: { page.hideKeyboard(); page.tab = currentValue }
            }
            Input {
                id: search; objectName: "iptvSearch"; Layout.fillWidth: true; Layout.minimumWidth: 120; implicitHeight: 54
                placeholderText: "Search TV channels"; inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: { page.query = text; if(text.trim().length)page.tab = "directory"; filterDelay.restart() }
                onAccepted: { filterDelay.stop(); page.applyFilters(); page.hideKeyboard() }
            }
            UiButton { objectName: "iptvFiltersButton"; iconName: "filter"; implicitWidth: 54; implicitHeight: 54; selected: page.countryFilter.length > 0 || page.categoryFilter.length > 0; description: "Filter by country or category"; onClicked: { page.hideKeyboard(); filters.open() } }
            UiButton { objectName: "iptvOptionsButton"; iconName: "more"; implicitWidth: 54; implicitHeight: 54; description: "TV options"; onClicked: { page.hideKeyboard(); options.open() } }
        }
        Text { visible: page.client.problem.length > 0; text: page.client.problem; color: Theme.danger; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Text { visible: home.source === "iptv" && home.state === "error"; text: home.message; color: Theme.danger; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ListView {
            id: list; objectName: "iptvChannels"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            model: page.rows
            BusyIndicator { anchors.centerIn: parent; running: page.client.preparing; visible: running }
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: channelRow
                objectName: "iptvPlay" + index
                required property var entry
                readonly property var modelData: entry
                required property int index
                width: list.width; height: 82; padding: 12
                Accessible.name: modelData.name
                onClicked: { page.hideKeyboard(); page.client.play(modelData.id) }
                background: Rectangle { radius: 14; color: channelRow.down ? Theme.tint : Theme.raised; border.color: Theme.border }
                contentItem: RowLayout {
                    spacing: 14
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: channelRow.modelData.name; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: [channelRow.modelData.country || "",channelRow.modelData.quality || "",(channelRow.modelData.labels || []).join(" · ")].filter(x => x).join(" · "); textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiButton { objectName: "iptvSave" + channelRow.index; iconName: "star"; selected: channelRow.modelData.saved; implicitWidth: 54; implicitHeight: 54; description: channelRow.modelData.saved ? "Remove saved channel" : "Save channel"; onClicked: page.saveChannel(channelRow.modelData.id,!channelRow.modelData.saved) }
                    UiIcon { name: "play"; tint: Theme.accent; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                }
            }
            footer: UiButton { visible: page.tab === "directory" && page.client.hasMore; height: visible ? 54 : 0; width: list.width; text: "More channels"; onClicked: page.moreChannels() }
            Text { visible: list.count === 0; anchors.centerIn: parent; text: page.tab === "saved" ? "Save channels with the star, or add your own stream." : page.client.busy ? "Updating the directory…" : "No matching channels"; color: Theme.muted; font.pixelSize: 21; width: parent.width-40; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        }
    }
    Popup {
        id: filters; objectName: "iptvFiltersPopup"; parent: page; modal: true; padding: 20
        width: Math.min(page.width-40,420); x: page.width-width-20; y: 84
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Filter channels"; color: Theme.ink; font.pixelSize: 25 }
            Choice {
                objectName: "iptvCountry"; Layout.fillWidth: true; model: page.client.countries
                currentIndex: { const entries=page.client.countries;return Math.max(0,entries.findIndex(x => x.id === page.countryFilter)) }
                onActivated: { page.tab="directory";page.countryFilter=currentValue;page.applyFilters() }
            }
            Choice {
                objectName: "iptvCategory"; Layout.fillWidth: true; model: page.client.categories
                currentIndex: { const entries=page.client.categories;return Math.max(0,entries.findIndex(x => x.id === page.categoryFilter)) }
                onActivated: { page.tab="directory";page.categoryFilter=currentValue;page.applyFilters() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                UiButton { text: "Clear"; Layout.fillWidth: true; onClicked: { page.countryFilter="";page.categoryFilter="";page.applyFilters() } }
                UiButton { objectName: "iptvFiltersDone"; text: "Done"; primary: true; Layout.fillWidth: true; onClicked: filters.close() }
            }
        }
    }
    Popup {
        id: options; objectName: "iptvOptionsPopup"; parent: page; modal: true; padding: 20
        width: Math.min(page.width-40,360); x: page.width-width-20; y: 84
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 14
            UiButton { objectName: "iptvAddStream"; text: "Add stream"; Layout.fillWidth: true; onClicked: { options.close(); custom.open() } }
            UiButton { objectName: "iptvRefresh"; text: page.client.busy ? "Updating…" : "Refresh directory"; Layout.fillWidth: true; enabled: !page.client.busy; onClicked: { page.client.refresh(); options.close() } }
            Text { text: page.client.status; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    Dialog {
        id: custom; objectName: "iptvCustomDialog"; parent: Overlay.overlay; modal: true; padding: 24
        width: Math.min(parent.width-48,720); height: Math.min(parent.height-page.keyboardHeight-40,390)
        x: (parent.width-width)/2; y: Math.max(12,(parent.height-page.keyboardHeight-height)/2)
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        onOpened: nameField.forceActiveFocus()
        onClosed: { Qt.inputMethod.hide(); nameField.text=""; urlField.text=""; page.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Save a stream"; font.pixelSize: 27; color: Theme.ink }
            Input { id: nameField; objectName: "iptvCustomName"; Layout.fillWidth: true; maximumLength: 120; placeholderText: "Channel name"; inputMethodHints: Qt.ImhNoPredictiveText }
            Input { id: urlField; objectName: "iptvCustomUrl"; Layout.fillWidth: true; maximumLength: 4096; placeholderText: "Direct HTTP(S) stream URL"; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData }
            Text { visible: page.client.problem.length > 0; text: page.client.problem; color: Theme.danger; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: custom.close() }
                UiButton { objectName: "iptvCustomSave"; text: "Save"; primary: true; Layout.fillWidth: true; onClicked: { if(page.client.addCustom(nameField.text,urlField.text)){page.tab="saved";custom.close()} } }
            }
        }
    }
}
