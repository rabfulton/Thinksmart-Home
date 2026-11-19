import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    property real keyboardHeight: 0
    readonly property var archive: home.tvArchive
    readonly property var selected: archive.selected
    readonly property bool inSeries: !!selected.id
    readonly property bool loading: home.source==="tvarchive"&&home.state==="connecting"
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function hideKeyboard(){page.forceActiveFocus();Qt.inputMethod.hide()}
    function clock(v){const n=Math.floor(v||0);return Math.floor(n/60)+":"+String(n%60).padStart(2,"0")}
    onActiveChanged: if(!active)hideKeyboard()
    Connections { target: page.archive; function onChanged(){if(!search.activeFocus&&search.text!==page.archive.query)search.text=page.archive.query} }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; anchors.bottomMargin: Math.max(20,page.keyboardHeight-110); spacing: 12
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            UiButton { objectName: "tvArchiveBack"; iconName: "back"; implicitHeight: 54; onClicked: {page.hideKeyboard();if(page.inSeries)archive.selectSeries("");else page.back()} }
            Text { text: page.selected.title||"TV Archive"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 27; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
            UiButton { objectName: "tvArchiveAutoplay"; text: "Autoplay"; selected: archive.autoplay; implicitHeight: 54; onClicked: archive.autoplay=!archive.autoplay }
            UiButton { objectName: "tvArchiveUpdate"; iconName: "refresh"; implicitHeight: 54; enabled: !archive.busy; onClicked: archive.refresh() }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            TextField {
                id: search; objectName: "tvArchiveSearch"; Layout.fillWidth: true; Layout.minimumWidth: 70; implicitHeight: 54; maximumLength: 100; font.pixelSize: 20; color: Theme.ink; placeholderTextColor: Theme.muted; placeholderText: page.inSeries?"Find an episode":"Find a series"; inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: archive.query=text
                onAccepted: page.hideKeyboard()
                background: Rectangle { color: Theme.raised; radius: 12; border.color: search.activeFocus?Theme.accent:Theme.border }
            }
            ComboBox {
                id: seasonChoice
                objectName: "tvArchiveSeason"; visible: page.inSeries&&archive.seasons.length>2; model: archive.seasons
                Layout.preferredWidth: 172; implicitHeight: 54; leftPadding: 14; rightPadding: 38
                font.family: Theme.font; font.pixelSize: 19
                function label(value){return value==="All episodes"?value:value==="0"?"Specials":"Season "+value}
                displayText: currentIndex<1?"All episodes":label(currentText)
                currentIndex: Math.max(0,model.indexOf(archive.season||"All episodes"))
                onActivated: i => archive.season=model[i]
                palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.button: Theme.raised; palette.base: Theme.raised; palette.window: Theme.raised; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
                contentItem: Text { text: seasonChoice.displayText; textFormat: Text.PlainText; font: seasonChoice.font; color: Theme.ink; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                indicator: UiIcon { name: "back"; rotation: -90; tint: Theme.ink; width: 20; height: 20; x: seasonChoice.width-width-14; y: (seasonChoice.height-height)/2 }
                background: Rectangle { color: Theme.raised; radius: 15; border.color: seasonChoice.activeFocus||seasonChoice.popup.visible?Theme.accent:Theme.border }
                delegate: ItemDelegate {
                    id: option
                    required property var modelData
                    required property int index
                    objectName: "tvArchiveSeasonOption"+index
                    width: seasonOptions.width; implicitHeight: 54; leftPadding: 12; rightPadding: 12; font: seasonChoice.font
                    highlighted: seasonChoice.highlightedIndex===index
                    contentItem: Text { text: seasonChoice.label(option.modelData); textFormat: Text.PlainText; font: option.font; color: option.highlighted?Theme.accent:Theme.ink; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    background: Rectangle { radius: 10; color: option.highlighted?Theme.tint:Theme.raised }
                }
                popup: Popup {
                    objectName: "tvArchiveSeasonPopup"; y: seasonChoice.height+6; width: seasonChoice.width; padding: 6
                    implicitHeight: Math.min(360,contentItem.implicitHeight+topPadding+bottomPadding)
                    contentItem: ListView {
                        id: seasonOptions
                        clip: true; implicitHeight: contentHeight
                        model: seasonChoice.popup.visible?seasonChoice.delegateModel:null
                        currentIndex: seasonChoice.highlightedIndex
                        ScrollIndicator.vertical: ScrollIndicator {}
                    }
                    background: Rectangle { color: Theme.raised; border.color: Theme.border; radius: 14 }
                }
            }
            UiButton { objectName: "tvArchiveContinue"; visible: page.inSeries||!!archive.current.id; text: page.loading?"Loading…":"Continue"; implicitHeight: 54; enabled: archive.ready&&!page.loading; onClicked: {page.hideKeyboard();archive.resume(page.selected.id||"")} }
            UiButton { objectName: "tvArchiveRandom"; text: "Random"; implicitHeight: 54; enabled: archive.ready&&!page.loading; onClicked: {page.hideKeyboard();archive.random(page.selected.id||"")} }
        }
        Text { visible: archive.problem.length>0; text: archive.problem; color: Theme.danger; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Text { text: archive.status; color: Theme.faint; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
        ListView {
            id: rows; objectName: "tvArchiveList"; model: page.inSeries?archive.episodes:archive.series
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            onModelChanged: positionViewAtBeginning()
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: row
                required property var entry
                required property int index
                objectName: "tvArchiveRow"+index
                width: rows.width; height: 86; padding: 14
                onClicked: { page.hideKeyboard();if(page.inSeries)archive.play(entry.id);else archive.selectSeries(entry.id) }
                background: Rectangle { color: row.down?Theme.tint:Theme.raised; radius: 14; border.color: row.entry.id===archive.current.id?Theme.accent:"transparent" }
                contentItem: RowLayout {
                    spacing: 14
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 6
                        Text { text: (page.inSeries&&row.entry.number?(row.entry.season?"S"+String(row.entry.season).padStart(2,"0"):"")+"E"+String(row.entry.number).padStart(2,"0")+" · ":"")+(row.entry.title||""); textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 24; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: page.inSeries?(row.entry.extra?"Extra · ":"")+(row.entry.completed?"Watched":row.entry.position?"Resume at "+page.clock(row.entry.position):row.entry.format+" · "+Math.round((row.entry.size||0)/1048576)+" MB")+(row.entry.duration?" · "+Math.round(row.entry.duration/60)+" min":""):row.entry.count+" episodes"; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    UiIcon { name: page.inSeries?"play":"back"; rotation: page.inSeries?0:180; tint: Theme.accent }
                }
            }
            Text { anchors.centerIn: parent; width: parent.width-48; visible: rows.count===0; text: archive.busy?"Loading the Archive catalogue…":archive.ready?"No matching episodes or series":"Tap Update to load classic British TV"; color: Theme.muted; font.pixelSize: 24; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        }
    }
}
