import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    property real keyboardHeight: 0
    readonly property var books: home.audiobooks
    readonly property var book: books.detail
    readonly property bool inspecting: !!book.id
    readonly property bool currentPlaying: home.source==="audiobooks"&&books.current.id===book.id
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function clock(v) {const n=Math.floor(v||0);return Math.floor(n/3600)?Math.floor(n/3600)+"h "+Math.floor(n%3600/60)+"m":Math.floor(n/60)+"m"}
    function hideKeyboard(){page.forceActiveFocus();Qt.inputMethod.hide()}
    function choose(tab){hideKeyboard();books.setTab(tab)}
    function searchNow(){hideKeyboard();books.search(searchText.text,searchType.currentIndex===1)}
    onActiveChanged: if(!active)hideKeyboard()
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        anchors.bottomMargin: Math.max(20,page.keyboardHeight-110)
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            UiButton { objectName: "audiobooksBack"; iconName: "back"; implicitHeight: 54; onClicked: { page.hideKeyboard();if(page.inspecting)books.closeDetails();else page.back() } }
            Text { text: "Audiobooks"; visible: page.width>1000; color: Theme.ink; font.pixelSize: 25 }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [{id:"mine",title:"My books"},{id:"top",title:"Top 100"},{id:"search",title:"Search"},{id:"picks",title:"Monthly picks"}]
                UiButton { required property var modelData; objectName: "audiobooksTab_"+modelData.id; text: modelData.title; selected: books.tab===modelData.id; implicitHeight: 54; font.pixelSize: 17; onClicked: page.choose(modelData.id) }
            }
        }
        Text { visible: books.problem.length>0; text: books.problem; color: Theme.danger; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            visible: !page.inspecting; Layout.fillWidth: true; spacing: 12
            ComboBox {
                id: genre; objectName: "audiobooksGenre"; visible: books.tab==="top"; model: books.genres; Layout.preferredWidth: Math.min(300,page.width*.4); implicitHeight: 52
                palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.button: Theme.raised; palette.base: Theme.raised; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
                onActivated: i => books.setGenre(model[i])
            }
            ComboBox {
                id: pickPeriod; objectName: "audiobooksPickPeriod"; visible: books.tab==="picks"; model: books.periods; textRole: "label"; valueRole: "id"; Layout.fillWidth: true; implicitHeight: 52
                currentIndex: {for(let i=0;i<model.length;i++)if(model[i].id===books.period)return i;return -1}
                palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.button: Theme.raised; palette.base: Theme.raised; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
                onActivated: i => books.setPeriod(model[i].id)
            }
            ComboBox { id: searchType; objectName: "audiobooksSearchType"; visible: books.tab==="search"; model: ["Title","Author"]; Layout.preferredWidth: 130; implicitHeight: 52; palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.button: Theme.raised; palette.base: Theme.raised }
            TextField {
                id: searchText; objectName: "audiobooksSearch"; visible: books.tab==="search"; Layout.fillWidth: true; implicitHeight: 52; maximumLength: 100; font.pixelSize: 21; color: Theme.ink; placeholderTextColor: Theme.muted; placeholderText: searchType.currentIndex===0?"Find a title":"Find an author"; inputMethodHints: Qt.ImhNoPredictiveText
                onAccepted: page.searchNow()
                background: Rectangle { color: Theme.raised; radius: 12; border.color: searchText.activeFocus?Theme.accent:Theme.border }
            }
            UiButton { objectName: "audiobooksSearchButton"; visible: books.tab==="search"; text: "Search"; implicitHeight: 52; enabled: !books.busy; onClicked: page.searchNow() }
            Item { visible: books.tab!=="search"&&books.tab!=="picks"; Layout.fillWidth: true }
            UiButton { visible: books.tab==="top"||books.tab==="picks"; text: "Update"; implicitHeight: 52; enabled: !books.busy; onClicked: books.refresh() }
        }
        Text { visible: !page.inspecting; text: books.status; color: Theme.faint; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
        ListView {
            id: list; objectName: "audiobooksList"; visible: !page.inspecting
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 9; model: books.items
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: row
                required property var entry
                required property int index
                objectName: "audiobookRow"+index
                width: list.width; height: 108; padding: 14
                onClicked: { page.hideKeyboard();books.inspect(entry.id) }
                background: Rectangle { radius: 14; color: row.down?Theme.tint:Theme.raised; border.color: entry.saved?Theme.border:"transparent" }
                contentItem: RowLayout {
                    spacing: 16
                    Rectangle { Layout.preferredWidth: 51; Layout.preferredHeight: 68; color: Theme.tint; radius: 6; UiIcon { anchors.centerIn: parent; name: "book"; width: 32; height: 32; tint: Theme.accent } }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 5
                        Text { text: (row.entry.rank?row.entry.rank+". ":"")+(row.entry.title||""); color: Theme.ink; font.pixelSize: 23; textFormat: Text.PlainText; Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
                        Text { text: row.entry.author || "LibriVox monthly selection"; color: Theme.muted; font.pixelSize: 17; textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: row.entry.completed?"Finished":row.entry.saved?"Part "+((row.entry.chapterIndex||0)+1)+" · "+page.clock(row.entry.position)+" listened":(row.entry.language?row.entry.language+" · ":"")+(row.entry.duration?page.clock(row.entry.duration):"Open for recording details"); color: Theme.faint; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiIcon { name: "arrow"; tint: Theme.faint }
                }
            }
            footer: UiButton { visible: books.tab==="search"&&books.hasMore; text: "More books"; width: list.width; enabled: !books.busy; onClicked: books.loadMore() }
            Text { anchors.centerIn: parent; width: parent.width-40; visible: list.count===0; text: books.busy?"Finding books…":books.tab==="search"?"Search by title or author, or browse Top 100 for inspiration.":books.tab==="mine"?"Books you start or save will appear here.":"No books available. Try Update."; color: Theme.muted; font.pixelSize: 23; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
        ColumnLayout {
            visible: page.inspecting; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                Rectangle {
                    Layout.preferredWidth: page.width>1000?110:82; Layout.preferredHeight: page.width>1000?150:112; color: Theme.tint; radius: 8; clip: true
                    UiIcon { anchors.centerIn: parent; name: "book"; width: 50; height: 50; tint: Theme.accent }
                    Image { anchors.fill: parent; source: page.active ? (page.book.coverLocal||"") : ""; fillMode: Image.PreserveAspectFit; asynchronous: true; sourceSize.width: 180 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 7
                    Text { text: page.book.title||""; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 29; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                    Text { text: (page.book.author||"")+(page.book.readers?" · Read by "+page.book.readers:""); textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: (page.book.language||"")+(page.book.duration?" · "+page.clock(page.book.duration):"")+(page.book.chapterCount?" · "+page.book.chapterCount+" parts":""); color: Theme.faint; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                UiButton { objectName: "audiobooksResume"; primary: true; Layout.fillWidth: true; text: books.preparing?"Preparing…":page.currentPlaying&&home.state==="playing"?"Pause":page.book.completed?"Play again":(page.book.position||page.book.chapterIndex||page.book.lastPlayed)?"Resume":"Start book"; enabled: !books.preparing&&books.chapters.length>0; onClicked: { if(page.currentPlaying&&home.state==="playing")home.pause();else if(page.currentPlaying&&home.state==="paused")home.play();else books.play(page.book.id) } }
                UiButton { objectName: "audiobooksSave"; text: page.book.saved?"In My books":"Save book"; enabled: !page.book.saved&&!books.busy; onClicked: books.saveBook(page.book.id) }
                UiButton { objectName: "audiobooksRestart"; text: "Restart"; enabled: books.chapters.length>0; onClicked: restartDialog.open() }
            }
            RowLayout {
                visible: page.currentPlaying; Layout.fillWidth: true
                UiButton { text: "−30s"; implicitHeight: 50; onClicked: home.seek(Math.max(0,home.position-30)) }
                Slider { id: seek; objectName: "audiobooksSeek"; Layout.fillWidth: true; from: 0; to: Math.max(1,home.duration); value: home.position; enabled: home.duration>0; onMoved: home.seek(value) }
                UiButton { text: "+30s"; implicitHeight: 50; onClicked: home.seek(home.position+30) }
            }
            Text { visible: (page.book.description||"").length>0; text: page.book.description||""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: page.width>1000?2:3; elide: Text.ElideRight }
            ListView {
                id: chapters; objectName: "audiobooksChapters"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: books.chapters
                ScrollBar.vertical: ScrollBar {}
                delegate: UiButton {
                    required property var modelData
                    required property int index
                    objectName: "audiobookChapter"+index
                    width: chapters.width; implicitHeight: 55; selected: (page.book.chapterIndex||0)===index
                    contentItem: RowLayout { Text { text: (index+1)+" · "+modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 19; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: page.clock(modelData.duration); color: Theme.faint; font.pixelSize: 16 } }
                    onClicked: books.playChapter(index)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "LibriVox · Position saved after pause/stop and during playback"; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                UiButton { objectName: "audiobooksAbout"; visible: !!page.book.description; text: "About"; implicitHeight: 44; onClicked: aboutDialog.open() }
                UiButton { visible: page.book.saved; text: "Remove"; implicitHeight: 44; onClicked: removeDialog.open() }
            }
        }
    }
    component Confirm: Dialog {
        parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(580,parent.width-48); modal: true; padding: 24
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
    }
    Confirm {
        id: aboutDialog
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: page.book.title||"About this book"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 25; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                id: aboutScroll; Layout.fillWidth: true; Layout.preferredHeight: Math.min(450,page.height*0.55); contentWidth: availableWidth; clip: true
                Text { width: aboutScroll.availableWidth; text: page.book.description||""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 21; wrapMode: Text.WordWrap }
            }
            UiButton { text: "Done"; Layout.fillWidth: true; onClicked: aboutDialog.close() }
        }
    }
    Confirm {
        id: restartDialog
        contentItem: ColumnLayout { spacing: 18; Text { text: "Restart this book from the beginning?"; color: Theme.ink; font.pixelSize: 24; wrapMode: Text.WordWrap; Layout.fillWidth: true } RowLayout { UiButton { text: "Cancel"; onClicked: restartDialog.close() } UiButton { text: "Restart book"; primary: true; onClicked: { restartDialog.close();books.play(page.book.id,true) } } } }
    }
    Confirm {
        id: removeDialog
        contentItem: ColumnLayout { spacing: 18; Text { text: "Remove this book and its saved listening position?"; color: Theme.ink; font.pixelSize: 24; wrapMode: Text.WordWrap; Layout.fillWidth: true } RowLayout { UiButton { text: "Cancel"; onClicked: removeDialog.close() } UiButton { text: "Remove book"; primary: true; onClicked: { removeDialog.close();books.removeBook(page.book.id) } } } }
    }
}
