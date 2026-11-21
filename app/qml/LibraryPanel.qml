import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    required property var library
    property bool active: true
    property bool requested: false
    readonly property var modes: [
        {id:"all",name:"Newest"},{id:"alphabeticalByName",name:"Albums A–Z"},{id:"alphabeticalByArtist",name:"Artists A–Z"},
        {id:"recent",name:"Recently played"},{id:"frequent",name:"Most played"},{id:"highest",name:"Top rated"},
        {id:"starred",name:"Favourites"},{id:"byGenre",name:"Genre"},{id:"byYear",name:"Decade"}
    ]
    readonly property var decades: {let values=[];for(let d=Math.floor(new Date().getFullYear()/10)*10;d>=1900;d-=10)values.push({id:String(d),name:d+"s"});return values}
    function load() { if(active && library.configured && !requested) { requested = true; library.refresh() } }
    function search() { searchField.focus=false;Qt.inputMethod.hide();panel.library.search(searchField.text);list.positionViewAtBeginning() }
    function browse(mode,value) { searchField.text="";searchField.focus=false;Qt.inputMethod.hide();panel.library.setMusicBrowse(mode,value||"");list.positionViewAtBeginning() }
    onActiveChanged: load()
    Component.onCompleted: load()
    Connections { target: panel.library; function onChanged() { Qt.callLater(list.maybeLoadMore) } }
    component Choice: ComboBox {
        id: choice
        implicitHeight: 58; font.pixelSize: 21; textRole: "name"; padding: 0; leftPadding: 0; rightPadding: 0
        indicator: UiIcon { name: "arrow"; rotation: 90; width: 18; height: 18; x: choice.width-width-10; y: (choice.height-height)/2; tint: Theme.muted }
        palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.base: Theme.surface; palette.window: Theme.surface; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
        contentItem: Text { text: choice.displayText; textFormat: Text.PlainText; color: Theme.ink; font: choice.font; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 30; elide: Text.ElideRight }
        background: Rectangle { color: Theme.raised; radius: 12; border.color: choice.activeFocus?Theme.accent:Theme.border }
        popup.width: Math.min(360,panel.width)
        popup.height: Math.min(460,Math.max(160,panel.height-70))
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Choice {
                objectName: "musicBrowseMode"; visible: !panel.library.inAlbum
                Layout.preferredWidth: 160; model: panel.modes
                currentIndex: {for(let i=0;i<model.length;i++)if(model[i].id===panel.library.section)return i;return 0}
                enabled: panel.library.configured && !panel.library.busy
                onActivated: {
                    const mode=model[currentIndex].id
                    if(mode==="byGenre")panel.browse(mode,panel.library.genres.length?panel.library.genres[0].id:"")
                    else panel.browse(mode,mode==="byYear"?panel.decades[0].id:"")
                }
                delegate: ItemDelegate {
                    id: modeOption
                    required property var modelData
                    width: parent ? parent.width : 320; height: 56
                    enabled: modelData.id!=="byGenre" || panel.library.genres.length>0
                    contentItem: Text { text: modelData.name; color: modeOption.enabled?Theme.ink:Theme.muted; font.pixelSize: 21; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: modeOption.highlighted?Theme.tint:Theme.surface }
                }
            }
            Choice {
                objectName: "musicGenre"; visible: !panel.library.inAlbum && panel.library.section==="byGenre"
                Layout.preferredWidth: 150; model: panel.library.genres
                enabled: !panel.library.busy
                currentIndex: {for(let i=0;i<model.length;i++)if(model[i].id===panel.library.genre)return i;return -1}
                onActivated: panel.browse("byGenre",model[currentIndex].id)
            }
            Choice {
                objectName: "musicDecade"; visible: !panel.library.inAlbum && panel.library.section==="byYear"
                Layout.preferredWidth: 125; model: panel.decades
                enabled: !panel.library.busy
                currentIndex: {for(let i=0;i<model.length;i++)if(Number(model[i].id)===panel.library.decade)return i;return 0}
                onActivated: panel.browse("byYear",model[currentIndex].id)
            }
            Text { visible: panel.library.inAlbum; text: panel.library.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 23; Layout.preferredWidth: 190; Layout.maximumWidth: 240; elide: Text.ElideRight }
            UiButton { objectName: "musicQueueAlbum"; visible: panel.library.inAlbum; text: "Queue album"; enabled: !panel.library.busy; onClicked: panel.library.enqueueAlbum() }
            TextField {
                id: searchField; objectName: "musicSearchInput"
                Layout.fillWidth: true; Layout.minimumWidth: 80; implicitHeight: 58; font.pixelSize: 21
                placeholderText: "Search music"; color: Theme.ink; placeholderTextColor: Theme.muted
                selectionColor: Theme.accent; selectedTextColor: Theme.accentInk
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: Theme.raised; radius: 12; border.color: searchField.activeFocus?Theme.accent:Theme.border }
                onAccepted: panel.search()
            }
            UiButton { objectName: "musicSearchButton"; text: "Search"; enabled: panel.library.configured && !panel.library.busy; onClicked: panel.search() }
            UiButton { objectName: "musicReloadButton"; iconName: "refresh"; description: "Reload music"; enabled: panel.library.configured && !panel.library.busy; onClicked: {searchField.focus=false;Qt.inputMethod.hide();panel.library.refresh();list.positionViewAtBeginning()} }
        }
        Text { visible: panel.library.problem.length>0 || panel.library.cached || panel.library.queueProblem.length>0; text: panel.library.queueProblem || panel.library.problem || panel.library.status; textFormat: Text.PlainText; color: panel.library.problem.length?Theme.danger:Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ListView {
            id: list; objectName: "musicLibraryList"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 9; model: panel.library.itemModel; cacheBuffer: 180
            function maybeLoadMore() {if(panel.active && !panel.library.busy && panel.library.canNext && count>0 && contentHeight-contentY-height<180)panel.library.loadMore()}
            onContentYChanged: maybeLoadMore()
            onHeightChanged: Qt.callLater(maybeLoadMore)
            onCountChanged: Qt.callLater(maybeLoadMore)
            ScrollBar.vertical: ScrollBar { }
            delegate: AbstractButton {
                id: row; objectName: "musicLibraryRow"+index
                required property var entry
                readonly property var modelData: entry
                required property int index
                width: list.width; height: 84; padding: 14; enabled: !panel.library.busy
                onClicked: panel.library.open(index)
                background: Rectangle { radius: 15; color: row.down ? Theme.border : Theme.raised }
                contentItem: RowLayout {
                    spacing: 15
                    UiIcon { name: "music"; tint: Theme.accent; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: row.modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: row.modelData.subtitle; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiIcon { name: row.modelData.type === "song" ? "add" : "arrow"; tint: Theme.faint; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                }
            }
            Text {
                visible: list.count === 0 && !panel.library.busy
                anchors.centerIn: parent; width: parent.width * 0.8
                text: panel.library.configured ? "No music in this selection." : "Add your music server in Settings → Pages & modules → Music → Edit."
                color: Theme.muted; font.pixelSize: 22; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
            }
            BusyIndicator { anchors.centerIn: parent; running: panel.library.busy; visible: running && list.count===0 }
            footer: Item { width: list.width; height: panel.library.busy && list.count>0?44:0; Text { anchors.centerIn: parent; visible: parent.height>0; text: "Loading…"; color: Theme.muted; font.pixelSize: 18 } }
        }
    }
}
