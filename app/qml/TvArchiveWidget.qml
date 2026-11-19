import QtQuick
import QtQuick.Layouts

WidgetTile {
    id: card
    property bool running: false
    readonly property var archive: home.tvArchive
    readonly property var current: archive.current
    readonly property bool loading: home.source==="tvarchive"&&home.state==="connecting"
    heading: "TV Archive"; indexLabel: "CLASSIC BRITISH TELEVISION"
    ColumnLayout {
        anchors.fill: parent; spacing: 8
        Text { text: card.current.seriesTitle||"Classic British TV"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 24; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
        Text { text: card.current.title||"Browse series and episodes"; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            UiButton { objectName: "tvArchiveWidgetContinue"; Layout.fillWidth: true; implicitHeight: 48; text: card.loading?"Loading…":card.current.id?"Continue":"Browse"; enabled: !card.loading; onClicked: card.current.id?archive.resume():card.clicked() }
            UiButton { objectName: "tvArchiveWidgetRandom"; text: "Random"; implicitHeight: 48; enabled: archive.ready&&!card.loading; onClicked: archive.random() }
        }
    }
}
