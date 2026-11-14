import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    required property var library
    property string heading: "Video"
    property bool active: false
    property bool requested: false
    property bool showingDetail: library.detail.id !== undefined
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function load() { if(active && library.configured && !requested) { requested = true; library.refresh() } }
    onActiveChanged: load()
    Component.onCompleted: load()
    Connections { target: page.library; function onChanged() { Qt.callLater(posters.maybeLoadMore) } }
    component Choice: ComboBox {
        id: choice
        palette.text: Theme.ink; palette.buttonText: Theme.muted; palette.base: Theme.surface; palette.window: Theme.surface; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
        implicitHeight: 54; font.pixelSize: 18
        contentItem: Text { text: choice.displayText; color: Theme.ink; font: choice.font; verticalAlignment: Text.AlignVCenter; leftPadding: 14; rightPadding: 28; elide: Text.ElideRight }
        background: Rectangle { color: Theme.raised; radius: 13; border.color: Theme.border }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            UiButton { iconName: "back"; description: "Back"; onClicked: page.showingDetail ? page.library.closeDetails() : (page.library.canBack ? page.library.back() : page.back()) }
            Text { visible: page.showingDetail; text: "Film details"; color: Theme.ink; font.pixelSize: 29; Layout.fillWidth: true }
            Choice {
                visible: !page.showingDetail; Layout.preferredWidth: page.width > 900 ? 190 : 155
                model: ["All titles", "Films", "TV series", "Continue watching", "Recently added", "Collections"]
                currentIndex: Math.max(0,["all","movies","series","resume","recent","collections"].indexOf(page.library.section))
                onActivated: page.library.section = ["all","movies","series","resume","recent","collections"][currentIndex]
                enabled: !page.library.busy && page.library.configured
            }
            Choice {
                visible: !page.showingDetail; Layout.fillWidth: true; Layout.minimumWidth: 96
                model: [{name: "All genres"}].concat(page.library.genres); textRole: "name"
                currentIndex: { for(let i=1;i<model.length;i++)if(model[i].name===page.library.genre)return i; return 0 }
                onActivated: page.library.genre = currentIndex === 0 ? "" : model[currentIndex].name
                enabled: !page.library.busy && page.library.configured
            }
            Choice {
                visible: !page.showingDetail; Layout.preferredWidth: page.width > 900 ? 200 : 160
                model: { let values=[{name:"All decades",value:0}]; for(let d=Math.floor(new Date().getFullYear()/10)*10;d>=1900;d-=10)values.push({name:d+"s",value:d}); return values }
                textRole: "name"
                currentIndex: { for(let i=0;i<model.length;i++)if(model[i].value===page.library.decade)return i; return 0 }
                onActivated: page.library.decade = model[currentIndex].value
                enabled: !page.library.busy && page.library.configured
            }
            Choice {
                visible: !page.showingDetail && page.library.libraries.length > 1
                Layout.preferredWidth: 130; model: [{name: "All libraries",id:""}].concat(page.library.libraries); textRole: "name"
                onActivated: page.library.setLibrary(model[currentIndex].id)
                enabled: !page.library.busy && page.library.configured
            }
        }
        RowLayout {
            visible: page.library.problem.length > 0; Layout.fillWidth: true
            Text { text: page.library.problem; color: Theme.danger; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            UiButton { text: "Retry"; enabled: !page.library.busy; onClicked: page.library.refresh() }
        }
        ColumnLayout {
            visible: !page.showingDetail
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            GridView {
                id: posters
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; cacheBuffer: 320
                readonly property int columns: Math.max(2,Math.floor(width / 150))
                cellWidth: width / columns; cellHeight: Math.round((cellWidth - 18) * 1.5) + 52
                function maybeLoadMore() {
                    if(page.active && !page.showingDetail && !page.library.busy && page.library.canNext && count > 0 && contentHeight - contentY - height < cellHeight * 2) page.library.loadMore()
                }
                onContentYChanged: maybeLoadMore()
                onHeightChanged: maybeLoadMore()
                onCountChanged: Qt.callLater(maybeLoadMore)
                model: page.library.itemModel
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: poster
                    required property var entry
                    readonly property var modelData: entry
                    required property int index
                    width: posters.cellWidth - 18; height: posters.cellHeight - 16
                    enabled: !page.library.busy
                    onClicked: page.library.inspect(index)
                    background: Item {}
                    contentItem: Column {
                        spacing: 8
                        Rectangle {
                            width: poster.width; height: Math.round(width * 1.5); radius: 12; color: Theme.raised; clip: true
                            UiIcon { anchors.centerIn: parent; name: "video"; width: 46; height: 46; tint: Theme.faint }
                            Image { anchors.fill: parent; source: poster.modelData.poster || ""; asynchronous: true; fillMode: Image.PreserveAspectCrop; sourceSize.width: 320; visible: status === Image.Ready }
                            Rectangle { visible: poster.modelData.resume > 10; height: 5; anchors.left: parent.left; anchors.bottom: parent.bottom; width: parent.width * Math.min(1,poster.modelData.resume / Math.max(1,poster.modelData.duration)); color: Theme.accent }
                            Rectangle { visible: poster.down; anchors.fill: parent; color: "#40000000" }
                        }
                        Text { text: poster.modelData.title; width: poster.width; color: Theme.ink; font.pixelSize: 17; elide: Text.ElideRight }
                        Text { text: poster.modelData.subtitle; width: poster.width; color: Theme.muted; font.pixelSize: 14; elide: Text.ElideRight }
                    }
                }
                Text { visible: posters.count === 0 && !page.library.busy; anchors.centerIn: parent; width: parent.width * .8; text: page.library.configured ? "No titles in this category." : "Add your Jellyfin connection to browse films and series."; color: Theme.muted; font.pixelSize: 22; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                BusyIndicator { anchors.centerIn: parent; running: page.library.busy; visible: running }
            }

        }
        RowLayout {
            visible: page.showingDetail; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 26
            Rectangle {
                Layout.preferredWidth: Math.min(260,page.width * .28); Layout.fillHeight: true; color: Theme.background; radius: 14
                Image { anchors.fill: parent; source: page.library.detail.poster || ""; fillMode: Image.PreserveAspectFit; asynchronous: true; sourceSize.width: 420 }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                Text { text: page.library.detail.title || ""; color: Theme.ink; font.pixelSize: 30; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: (page.library.detail.year || "") + "  ·  " + Math.round((page.library.detail.duration || 0) / 60) + " min"; color: Theme.accent; font.pixelSize: 18 }
                Text { text: (page.library.detail.genres || []).join(" · "); color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Flickable {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentHeight: synopsis.height
                    Text { id: synopsis; width: parent.width; text: page.library.detail.overview || "No synopsis available."; color: Theme.muted; font.pixelSize: 20; wrapMode: Text.WordWrap }
                    ScrollBar.vertical: ScrollBar {}
                }
                RowLayout {
                    UiButton { text: page.library.detail.resume > 10 ? "Resume" : "Play"; iconName: "play"; primary: true; enabled: !page.library.busy; onClicked: page.library.playSelection() }
                    UiButton { visible: page.library.detail.resume > 10; text: "Start over"; enabled: !page.library.busy; onClicked: page.library.playSelection(true) }
                }
            }
        }
    }
}
