import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var client
    readonly property bool readingArticle: false
    readonly property bool readingDiscussion: client.selected.id !== undefined
    component SmallButton: UiButton { implicitHeight: 44; font.pixelSize: 15 }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    Layout.fillWidth: true; Layout.fillHeight: true
    Layout.preferredWidth: 1; Layout.preferredHeight: 1
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            SmallButton { objectName: "hnBackButton"; visible: panel.readingDiscussion; iconName: "back"; text: "Stories"; onClicked: panel.client.closeStory() }
            Text { text: panel.readingDiscussion ? "Discussion" : "Hacker News"; color: Theme.ink; font.pixelSize: 26; Layout.fillWidth: true; elide: Text.ElideRight }
            SmallButton { text: "Update"; enabled: panel.readingDiscussion ? !panel.client.commentsBusy : !panel.client.busy; onClicked: panel.readingDiscussion ? panel.client.reloadComments() : panel.client.refresh() }
        }
        Text { visible: !panel.readingDiscussion; text: panel.client.status; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { visible: !panel.readingDiscussion && panel.client.problem.length > 0; text: panel.client.problem; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Divider {}
        ListView {
            id: hnStories; objectName: "hnStories"
            visible: !panel.readingDiscussion
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: panel.client.items
            ScrollBar.vertical: ScrollBar {}
            delegate: NewsFeedRow {
                required property var entry
                required property int index
                objectName: "hnStoryRow" + index
                headline: entry.title; source: entry.domain || "Hacker News"
                byline: entry.points + " points · " + entry.count + " comments · " + entry.by
                onClicked: { panel.client.openStory(index); comments.positionViewAtBeginning() }
            }
            Text { anchors.centerIn: parent; width: parent.width - 24; visible: hnStories.count === 0; text: panel.client.busy ? "Loading Hacker News…" : "Stories will appear here when connected."; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
        ListView {
            id: comments; objectName: "hnComments"
            visible: panel.readingDiscussion
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 16
            model: panel.client.comments
            ScrollBar.vertical: ScrollBar {}
            header: Column {
                width: comments.width; spacing: 12; bottomPadding: 20
                Text { width: parent.width; text: panel.client.selected.title || ""; color: Theme.ink; font.pixelSize: 25; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                Text { width: parent.width; text: (panel.client.selected.points || 0) + " points · " + (panel.client.selected.count || 0) + " comments · " + (panel.client.selected.by || ""); color: Theme.accent; font.pixelSize: 13; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                Text { width: parent.width; visible: text.length > 0; text: panel.client.selected.text || ""; color: Theme.ink; font.pixelSize: 18; textFormat: Text.PlainText; wrapMode: Text.WordWrap; lineHeight: 1.2 }
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
                        SmallButton { objectName: "hnReplies" + comment.entry.id; visible: comment.entry.replyCount > 0; text: comment.entry.expanded ? "Hide replies" : "Show " + comment.entry.replyCount + " replies"; enabled: !panel.client.commentsBusy; onClicked: panel.client.toggleReplies(comment.entry.id) }
                        SmallButton { visible: comment.entry.expanded && comment.entry.moreReplies; text: "More replies"; enabled: !panel.client.commentsBusy; onClicked: panel.client.moreReplies(comment.entry.id) }
                    }
                }
            }
            footer: Column {
                width: comments.width; spacing: 14; topPadding: 16; bottomPadding: 16
                Text { visible: panel.client.commentsBusy; text: "Loading comments…"; color: Theme.muted; font.pixelSize: 16 }
                Text { width: parent.width; visible: comments.count === 0 && !panel.client.commentsBusy; text: "No comments yet."; color: Theme.muted; font.pixelSize: 18 }
                Text { width: parent.width; visible: panel.client.commentsProblem.length > 0; text: panel.client.commentsProblem; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap }
                SmallButton { visible: panel.client.commentsProblem.length > 0; text: "Retry discussion"; enabled: !panel.client.commentsBusy; onClicked: panel.client.reloadComments() }
                SmallButton { visible: panel.client.moreComments; text: "More comments"; enabled: !panel.client.commentsBusy; onClicked: panel.client.loadMoreComments() }
            }
        }
    }
}
