import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    property real keyboardHeight: 0
    property string category: "favourites"
    property string query: ""
    readonly property var categories: [{id:"favourites",title:"Favourites"},{id:"all",title:"All stations"},{id:"folk",title:"Folk"},{id:"jazz",title:"Jazz"},{id:"rock",title:"Rock"},{id:"classical",title:"Classical"}]
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function hideKeyboard() { page.forceActiveFocus(); Qt.inputMethod.hide() }
    function searchNow() {
        searchDelay.stop()
        if(!active)return
        if(category === "favourites") { radio.cancelSearch();return }
        radio.searchStations(query, category === "all" ? "" : category)
    }
    function choose(id) { hideKeyboard();category=id;query="";search.text="";searchNow() }
    onActiveChanged: {
        if(active) { category="favourites";query="";search.text="";radio.cancelSearch() }
        else { searchDelay.stop();radio.cancelSearch();hideKeyboard() }
    }
    Timer { id: searchDelay; interval: 600; onTriggered: page.searchNow() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        anchors.bottomMargin: Math.max(20, page.keyboardHeight - 110)
        RowLayout {
            spacing: 12
            UiButton { objectName: "radioBackButton"; iconName: "back"; implicitHeight: 54; onClicked: { page.hideKeyboard();page.back() } }
            TextField {
                id: search; objectName: "radioSearch"; Layout.fillWidth: true; implicitHeight: 54; font.pixelSize: 21
                placeholderText: "Search worldwide stations"; color: Theme.ink; placeholderTextColor: Theme.muted
                leftPadding: 14; rightPadding: 14; maximumLength: 100; inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: Theme.raised; radius: 12; border.color: search.activeFocus ? Theme.accent : Theme.border }
                onTextChanged: { page.query=text;if(text.trim().length && page.category === "favourites")page.category="all";if(page.active&&page.category!=="favourites")searchDelay.restart() }
                onAccepted: { page.hideKeyboard();page.searchNow() }
            }
            UiButton { objectName: "radioSearchButton"; text: "Search"; implicitHeight: 54; onClicked: { page.hideKeyboard();if(page.category==="favourites")page.category="all";page.searchNow() } }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
            ColumnLayout {
                Layout.preferredWidth: page.width > 900 ? 190 : 145; Layout.minimumWidth: Layout.preferredWidth; Layout.maximumWidth: Layout.preferredWidth; Layout.alignment: Qt.AlignTop; spacing: 9
                Repeater {
                    model: page.categories
                    UiButton {
                        required property var modelData
                        objectName: "radioCategory_" + modelData.id
                        text: modelData.title; selected: page.category === modelData.id; implicitHeight: 54; Layout.fillWidth: true
                        onClicked: page.choose(modelData.id)
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1; Layout.fillHeight: true; spacing: 10
                Text { visible: radio.searchProblem.length > 0; text: radio.searchProblem; color: Theme.danger; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                ListView {
                    id: stationList; objectName: "radioResults"; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true; clip: true; spacing: 8
                    model: page.category === "favourites" ? radio.favouriteModel : radio.resultModel
                    ScrollBar.vertical: ScrollBar {}
                    delegate: AbstractButton {
                        id: row
                        required property var entry
                        required property int index
                        objectName: "radioStation" + index
                        width: stationList.width; height: 82; padding: 12
                        Accessible.name: entry.name
                        onClicked: { page.hideKeyboard();radio.play(entry.id) }
                        background: Rectangle { radius: 14; color: row.down || (home.source === "radio" && home.radioId === row.entry.id && home.state !== "stopped") ? Theme.tint : Theme.raised }
                        contentItem: RowLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 5
                                Text { text: row.entry.name; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                                Text { text: row.entry.description || ""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                            UiButton { objectName: "radioFavourite" + row.index; iconName: "star"; selected: row.entry.saved; implicitWidth: 54; implicitHeight: 54; description: row.entry.saved ? "Remove favourite" : "Save favourite"; onClicked: radio.favourite(row.entry.id,!row.entry.saved) }
                            UiIcon { name: "play"; tint: Theme.accent; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                        }
                    }
                    footer: UiButton { width: stationList.width; height: visible ? 54 : 0; visible: page.category!=="favourites" && radio.hasMore; text: radio.searchBusy ? "Searching…" : "More stations"; enabled: !radio.searchBusy; onClicked: radio.loadMore() }
                    BusyIndicator { anchors.centerIn: parent; running: radio.searchBusy; visible: running }
                    Text { visible: stationList.count===0&&!radio.searchBusy; anchors.centerIn: parent; width: parent.width-24; text: page.category==="favourites" ? "Search for stations and save them with the star." : "No matching stations"; color: Theme.muted; font.pixelSize: 21; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }
    }
}
