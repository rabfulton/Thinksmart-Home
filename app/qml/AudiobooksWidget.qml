import QtQuick
import QtQuick.Layouts

WidgetTile {
    id: card
    property bool running: false
    readonly property var books: home.audiobooks
    readonly property var book: books.current
    heading: "Audiobooks"; indexLabel: book.completed ? "FINISHED · READY TO READ AGAIN" : "LIBRIVOX · YOUR LISTENING PLACE"
    function time(v) { const n=Math.floor(v||0);return Math.floor(n/60)+":"+String(n%60).padStart(2,"0") }
    readonly property bool canSeek: home.source==="audiobooks"&&(home.state==="playing"||home.state==="paused")&&home.duration>0
    ColumnLayout {
        anchors.fill: parent; spacing: 6
        Text { text: card.book.title || "Choose a good book"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 22; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
        Text { text: card.book.author || "Top 100 and monthly picks"; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: card.book.id ? "Part "+((card.book.chapterIndex||0)+1)+" · "+card.time(card.book.position) : ""; color: Theme.faint; font.pixelSize: 13 }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            UiButton { objectName: "audiobookWidgetBack30"; text: "−30s"; implicitWidth: 68; implicitHeight: 48; font.pixelSize: 16; enabled: card.canSeek; onClicked: home.seek(Math.max(0,home.position-30)) }
            UiButton {
                objectName: "audiobookWidgetResume"
                implicitHeight: 48; font.pixelSize: 16; Layout.fillWidth: true; Layout.minimumWidth: 0
                readonly property bool playing: home.source==="audiobooks"&&home.state==="playing"
                text: books.preparing?"Preparing…":playing?"Pause":card.book.completed?"Play again":(card.book.position||card.book.chapterIndex)?"Resume":card.book.id==="lv:753"?"Start Moby-Dick":"Start book"
                enabled: !books.preparing
                onClicked: playing?home.pause():home.source==="audiobooks"&&home.state==="paused"?home.play():books.resume()
            }
            UiButton { objectName: "audiobookWidgetForward30"; text: "+30s"; implicitWidth: 68; implicitHeight: 48; font.pixelSize: 16; enabled: card.canSeek; onClicked: home.seek(Math.min(home.duration,home.position+30)) }
        }
    }
}
