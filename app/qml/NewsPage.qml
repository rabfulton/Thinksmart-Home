import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property bool active: false
    property var article: ({})
    readonly property bool readingArticle: article.id !== undefined
    readonly property bool readingDiscussion: hackerNews.selected.id !== undefined
    Binding { target: headlines; property: "active"; value: page.active }
    Binding { target: hackerNews; property: "active"; value: page.active }

    component SmallButton: UiButton { implicitHeight: 44; font.pixelSize: 15 }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    component FeedRow: ItemDelegate {
        id: row
        property string headline: ""
        property string source: ""
        property string byline: ""
        width: ListView.view ? ListView.view.width : 0
        implicitHeight: storyText.implicitHeight
        padding: 0
        background: Rectangle { color: row.down ? Theme.tint : "transparent" }
        contentItem: Column {
            id: storyText; spacing: 7; topPadding: 14; bottomPadding: 14
            Text { width: parent.width; text: row.source; color: Theme.accent; font.pixelSize: 13; textFormat: Text.PlainText; elide: Text.ElideRight }
            Text { width: parent.width; text: row.headline; color: Theme.ink; font.pixelSize: 21; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; lineHeight: 1.1 }
            Text { width: parent.width; text: row.byline; color: Theme.muted; font.pixelSize: 13; textFormat: Text.PlainText; elide: Text.ElideRight }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border; opacity: 0.6 }
    }

    GridLayout {
        anchors.fill: parent
        columns: page.width > page.height ? 2 : 1
        columnSpacing: Theme.gap; rowSpacing: Theme.gap
        Rectangle {
            objectName: "headlinesPanel"
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.preferredWidth: 1; Layout.preferredHeight: 1
            color: Theme.surface; radius: Theme.radius; border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 10
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    SmallButton { objectName: "headlinesBackButton"; visible: page.readingArticle; iconName: "back"; text: "News"; onClicked: page.article = ({}) }
                    Text { visible: !page.readingArticle; text: "News"; color: Theme.ink; font.pixelSize: 26 }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        id: sectionChoice; objectName: "newsSectionChoice"
                        visible: !page.readingArticle
                        model: headlines.sections; textRole: "label"
                        Layout.preferredWidth: Math.min(230, page.width > page.height ? page.width * .2 : page.width * .45)
                        implicitHeight: 44; font.pixelSize: 15
                        palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.base: Theme.surface; palette.window: Theme.surface; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
                        currentIndex: { for(let i=0;i<model.length;i++)if(model[i].id===headlines.section)return i; return 0 }
                        onActivated: { headlines.section = model[currentIndex].id; headlineList.positionViewAtBeginning() }
                        contentItem: Text { text: sectionChoice.displayText; color: Theme.ink; font: sectionChoice.font; verticalAlignment: Text.AlignVCenter; leftPadding: 10; rightPadding: 25; elide: Text.ElideRight }
                        background: Rectangle { color: Theme.raised; radius: 12; border.color: Theme.border }
                    }
                    SmallButton { visible: !page.readingArticle; text: "Update"; enabled: !headlines.busy; onClicked: headlines.refresh() }
                }
                Text { visible: !page.readingArticle; text: headlines.status + " · Google News"; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { visible: !page.readingArticle && headlines.problem.length > 0; text: headlines.problem; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Divider {}
                ListView {
                    id: headlineList; objectName: "headlineList"
                    visible: !page.readingArticle
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 0
                    model: headlines.items
                    ScrollBar.vertical: ScrollBar {}
                    delegate: FeedRow {
                        required property var entry
                        required property int index
                        objectName: "headlineRow" + index
                        headline: entry.title; source: entry.source || "News"
                        byline: (entry.section === "FINANCE" ? "Finance · " : entry.section === "BUSINESS" ? "Business · " : entry.section === "WORLD" ? "World · " : "") + (entry.when || "")
                        onClicked: { page.article = headlines.story(index); articleReader.ScrollBar.vertical.position = 0 }
                    }
                    Text { anchors.centerIn: parent; width: parent.width - 24; visible: headlineList.count === 0; text: headlines.busy ? "Loading headlines…" : "Headlines will appear here when connected."; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                }
                ScrollView {
                    id: articleReader
                    visible: page.readingArticle
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    contentWidth: availableWidth
                    Column {
                        width: articleReader.availableWidth; spacing: 18
                        Text { width: parent.width; text: page.article.title || ""; color: Theme.ink; font.pixelSize: 27; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: (page.article.source || "") + " · " + (page.article.when || ""); color: Theme.accent; font.pixelSize: 14; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                        Text { text: "FEED SUMMARY"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 12 }
                        Text { width: parent.width; text: page.article.summary || "This publisher supplied a headline only."; color: Theme.ink; font.pixelSize: 19; textFormat: Text.PlainText; wrapMode: Text.WordWrap; lineHeight: 1.2 }
                    }
                }
            }
        }
        Rectangle {
            objectName: "hackerNewsPanel"
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.preferredWidth: 1; Layout.preferredHeight: 1
            color: Theme.surface; radius: Theme.radius; border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    SmallButton { objectName: "hnBackButton"; visible: page.readingDiscussion; iconName: "back"; text: "Stories"; onClicked: hackerNews.closeStory() }
                    Text { text: page.readingDiscussion ? "Discussion" : "Hacker News"; color: Theme.ink; font.pixelSize: 26; Layout.fillWidth: true; elide: Text.ElideRight }
                    SmallButton { text: "Update"; enabled: page.readingDiscussion ? !hackerNews.commentsBusy : !hackerNews.busy; onClicked: page.readingDiscussion ? hackerNews.reloadComments() : hackerNews.refresh() }
                }
                Text { visible: !page.readingDiscussion; text: hackerNews.status; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { visible: !page.readingDiscussion && hackerNews.problem.length > 0; text: hackerNews.problem; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Divider {}
                ListView {
                    id: hnStories; objectName: "hnStories"
                    visible: !page.readingDiscussion
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: hackerNews.items
                    ScrollBar.vertical: ScrollBar {}
                    delegate: FeedRow {
                        required property var entry
                        required property int index
                        objectName: "hnStoryRow" + index
                        headline: entry.title; source: entry.domain || "Hacker News"
                        byline: entry.points + " points · " + entry.count + " comments · " + entry.by
                        onClicked: { hackerNews.openStory(index); comments.positionViewAtBeginning() }
                    }
                    Text { anchors.centerIn: parent; width: parent.width - 24; visible: hnStories.count === 0; text: hackerNews.busy ? "Loading Hacker News…" : "Stories will appear here when connected."; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                }
                ListView {
                    id: comments; objectName: "hnComments"
                    visible: page.readingDiscussion
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 16
                    model: hackerNews.comments
                    ScrollBar.vertical: ScrollBar {}
                    header: Column {
                        width: comments.width; spacing: 12; bottomPadding: 20
                        Text { width: parent.width; text: hackerNews.selected.title || ""; color: Theme.ink; font.pixelSize: 25; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: (hackerNews.selected.points || 0) + " points · " + (hackerNews.selected.count || 0) + " comments · " + (hackerNews.selected.by || ""); color: Theme.accent; font.pixelSize: 13; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                        Text { width: parent.width; visible: text.length > 0; text: hackerNews.selected.text || ""; color: Theme.ink; font.pixelSize: 18; textFormat: Text.PlainText; wrapMode: Text.WordWrap; lineHeight: 1.2 }
                    }
                    delegate: Item {
                        id: comment
                        required property var entry
                        objectName: "hnComment" + entry.id
                        width: comments.width; implicitHeight: commentBody.implicitHeight
                        Rectangle { x: Math.min(5,comment.entry.depth)*12; width: 2; height: parent.height; color: Theme.border }
                        Column {
                            id: commentBody
                            x: Math.min(5,comment.entry.depth)*12 + 12
                            width: parent.width - x; spacing: 9
                            Text { width: parent.width; text: (comment.entry.by || "") + (comment.entry.when ? " · " + comment.entry.when : ""); color: Theme.accent; font.pixelSize: 12; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                            Text { width: parent.width; text: comment.entry.text || ""; color: comment.entry.loading ? Theme.muted : Theme.ink; font.pixelSize: 18; textFormat: Text.PlainText; wrapMode: Text.WordWrap; lineHeight: 1.2 }
                            Flow {
                                width: parent.width; spacing: 8
                                SmallButton { objectName: "hnReplies" + comment.entry.id; visible: comment.entry.replyCount > 0; text: comment.entry.expanded ? "Hide replies" : "Show " + comment.entry.replyCount + " replies"; enabled: !hackerNews.commentsBusy; onClicked: hackerNews.toggleReplies(comment.entry.id) }
                                SmallButton { visible: comment.entry.expanded && comment.entry.moreReplies; text: "More replies"; enabled: !hackerNews.commentsBusy; onClicked: hackerNews.moreReplies(comment.entry.id) }
                            }
                        }
                    }
                    footer: Column {
                        width: comments.width; spacing: 14; topPadding: 16; bottomPadding: 16
                        Text { visible: hackerNews.commentsBusy; text: "Loading comments…"; color: Theme.muted; font.pixelSize: 16 }
                        Text { width: parent.width; visible: comments.count === 0 && !hackerNews.commentsBusy; text: "No comments yet."; color: Theme.muted; font.pixelSize: 18 }
                        Text { width: parent.width; visible: hackerNews.commentsProblem.length > 0; text: hackerNews.commentsProblem; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap }
                        SmallButton { visible: hackerNews.commentsProblem.length > 0; text: "Retry discussion"; enabled: !hackerNews.commentsBusy; onClicked: hackerNews.reloadComments() }
                        SmallButton { visible: hackerNews.moreComments; text: "More comments"; enabled: !hackerNews.commentsBusy; onClicked: hackerNews.loadMoreComments() }
                    }
                }
            }
        }
    }
}
