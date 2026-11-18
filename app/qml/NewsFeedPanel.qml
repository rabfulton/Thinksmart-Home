import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var client
    property string keyPrefix: ""
    property var article: ({})
    readonly property bool readingArticle: article.id !== undefined
    component SmallButton: UiButton { implicitHeight: 44; font.pixelSize: 15 }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    Layout.fillWidth: true; Layout.fillHeight: true
    Layout.preferredWidth: 1; Layout.preferredHeight: 1
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 10
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            SmallButton { objectName: panel.keyPrefix + "headlinesBackButton"; visible: panel.readingArticle; iconName: "back"; text: "News"; onClicked: panel.article = ({}) }
            Text { visible: !panel.readingArticle; text: panel.client.title; color: Theme.ink; Layout.fillWidth: true; elide: Text.ElideRight; font.pixelSize: 26 }
            Item { Layout.fillWidth: true }
            SmallButton { visible: !panel.readingArticle; text: "Update"; enabled: !panel.client.busy; onClicked: panel.client.refresh() }
        }
        Text { visible: !panel.readingArticle; text: panel.client.status + " · " + panel.client.provider; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { visible: !panel.readingArticle && panel.client.problem.length > 0; text: panel.client.problem; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Divider {}
        ListView {
            id: headlineList; objectName: panel.keyPrefix + "headlineList"
            visible: !panel.readingArticle
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 0
            model: panel.client.items
            ScrollBar.vertical: ScrollBar {}
            delegate: NewsFeedRow {
                required property var entry
                required property int index
                objectName: panel.keyPrefix + "headlineRow" + index
                headline: entry.title; source: entry.source || "News"
                byline: (entry.section === "FINANCE" ? "Finance · " : entry.section === "BUSINESS" ? "Business · " : entry.section === "WORLD" ? "World · " : "") + (entry.when || "")
                onClicked: { panel.article = panel.client.story(index); articleReader.ScrollBar.vertical.position = 0 }
            }
            Text { anchors.centerIn: parent; width: parent.width - 24; visible: headlineList.count === 0; text: panel.client.busy ? "Loading panel.client…" : "Headlines will appear here when connected."; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
        ScrollView {
            id: articleReader
            visible: panel.readingArticle
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            Column {
                width: articleReader.availableWidth; spacing: 18
                Text { width: parent.width; text: panel.article.title || ""; color: Theme.ink; font.pixelSize: 27; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                Text { width: parent.width; text: (panel.article.source || "") + " · " + (panel.article.when || ""); color: Theme.accent; font.pixelSize: 14; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
                Text { text: "FEED SUMMARY"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 12 }
                Text { width: parent.width; text: panel.article.summary || "This publisher supplied a headline only."; color: Theme.ink; font.pixelSize: 19; textFormat: Text.PlainText; wrapMode: Text.WordWrap; lineHeight: 1.2 }
            }
        }
    }
}
