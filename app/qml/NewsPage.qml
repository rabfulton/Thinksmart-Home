import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property bool active: false
    property bool readingArticle: false
    function updateReadingState() {
        let reading=false
        for(let i=0;i<panels.count;i++){const loader=panels.itemAt(i);if(loader && loader.item && loader.item.readingArticle)reading=true}
        readingArticle=reading
    }
    readonly property bool readingDiscussion: hackerNews.selected.id !== undefined
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Text { visible: home.newsSettings.problem.length>0; text: home.newsSettings.problem; color: Theme.danger; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        GridLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            columns: page.width > page.height ? home.newsSettings.columns.length : 1
            columnSpacing: Theme.gap; rowSpacing: Theme.gap
            Repeater {
                id: panels
                model: home.newsSettings.columns
                Loader {
                    id: newsColumn
                    required property string modelData
                    required property int index
                    objectName: modelData === "HN" ? "hackerNewsPanel" : index===0 ? "headlinesPanel" : "headlinesPanel1"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 1; Layout.preferredHeight: 1; Layout.minimumWidth: 0; Layout.minimumHeight: 0
                    property var feedClient: modelData === "HN" ? hackerNews : index === 0 ? headlines : home.newsSettings.secondary
                    sourceComponent: modelData === "HN" ? discussionPanel : feedPanel
                    onLoaded: page.updateReadingState()
                    Connections { target: newsColumn.item; ignoreUnknownSignals: true; function onReadingArticleChanged() { page.updateReadingState() } }
                }
            }
            Component { id: feedPanel; NewsFeedPanel { client: parent.feedClient; keyPrefix: parent.index===0?"":"news2_" } }
            Component { id: discussionPanel; HackerNewsPanel { client: parent.feedClient } }
        }
    }
}
