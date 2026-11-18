import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    readonly property var settings: home.newsSettings
    property var draft: ["WORLD_FINANCE","HN"]
    signal back()
    function begin() { draft=settings.columns.slice() }
    function choose(column,source) { const next=draft.slice();next[column]=source;draft=next }
    function resizeColumns(count) {
        let next=draft.slice(0,count)
        if(count===2&&next.length===1)next.push(next[0]==="HN"?"WORLD_FINANCE":"HN")
        draft=next
    }
    function option(id) { return settings.sources.find(s=>s.id===id) || ({}) }
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            UiButton { objectName: "newsSettingsBack"; iconName: "back"; onClicked: page.back() }
            Text { text: "News settings"; color: Theme.ink; font.pixelSize: 28 }
            Item { Layout.fillWidth: true }
            UiButton { objectName: "newsSettingsDefaults"; text: "Defaults"; onClicked: page.draft=["WORLD_FINANCE","HN"] }
        }
        ScrollView {
            id: newsScroll
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: newsScroll.availableWidth; spacing: 18
                Text { text: "Choose one or two sources. Two columns sit side by side in landscape and stack in portrait. Feeds update daily; Update still refreshes them manually."; color: Theme.muted; font.pixelSize: 19; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Columns"; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true }
                    UiButton { objectName: "newsOneColumn"; text: "1"; selected: page.draft.length===1; onClicked: page.resizeColumns(1) }
                    UiButton { objectName: "newsTwoColumns"; text: "2"; selected: page.draft.length===2; onClicked: page.resizeColumns(2) }
                }
                Repeater {
                    model: page.draft.length
                    ColumnLayout {
                        id: sourceColumn
                        required property int index
                        Layout.fillWidth: true; spacing: 8
                        Text { text: "Column " + (index+1); color: Theme.ink; font.pixelSize: 21 }
                        ComboBox {
                            id: choice
                            objectName: "newsColumnSource"+index
                            model: page.settings.sources; textRole: "label"; valueRole: "id"
                            Layout.fillWidth: true; implicitHeight: 64; font.pixelSize: 21
                            currentIndex: { for(let i=0;i<model.length;i++)if(model[i].id===page.draft[index])return i;return -1 }
                            onActivated: sourceIndex => page.choose(sourceColumn.index,model[sourceIndex].id)
                            palette.text: Theme.ink; palette.buttonText: Theme.ink; palette.base: Theme.surface; palette.window: Theme.surface; palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
                            contentItem: Text { text: choice.displayText; textFormat: Text.PlainText; font: choice.font; color: Theme.ink; leftPadding: 16; rightPadding: 40; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            background: Rectangle { color: Theme.raised; radius: 12; border.color: choice.activeFocus?Theme.accent:Theme.border }
                            popup.height: Math.min(420,Math.max(160,page.height-180))
                        }
                        Text { text: page.option(page.draft[index]).description || ""; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
                Text { text: "Articles show the publisher’s feed summary. Hacker News opens native comment threads. Available summaries vary by publisher."; color: Theme.faint; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { visible: page.settings.problem.length>0; text: page.settings.problem; color: Theme.danger; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            UiButton { objectName: "newsSettingsCancel"; text: "Cancel"; Layout.fillWidth: true; onClicked: page.back() }
            UiButton { objectName: "newsSettingsSave"; text: "Save news settings"; primary: true; Layout.fillWidth: true; onClicked: if(page.settings.saveColumns(page.draft))page.back() }
        }
    }
}
