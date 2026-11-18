import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    objectName: "layoutSettingsPage"
    readonly property var editor: home.layoutEditor
    property string section: "pages"
    property bool editingNews: false
    function editNews() { newsEditor.begin();editingNews=true }
    property int selected: -1
    property var current: selected >= 0 && selected < editor.pages.length ? editor.pages[selected] : ({})
    signal back()
    function widgetTitle(id) { const found = editor.widgets.find(w => w.id === id); return found ? found.title : id }
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    enabled: !screenControl.sleeping && !screenControl.powerPending
    onSelectedChanged: Qt.callLater(function() { scroll.contentItem.contentY = 0 })
    onSectionChanged: Qt.callLater(function() { scroll.contentItem.contentY = 0 })
    onVisibleChanged: if(!visible) { Qt.inputMethod.hide(); selected = -1; editingNews=false }
    component Label: Text { color: Theme.ink; font.pixelSize: 20; wrapMode: Text.WordWrap }
    component Choice: ComboBox {
        implicitHeight: 54; font.pixelSize: 20; textRole: "title"; valueRole: "id"
        palette.button: Theme.raised; palette.buttonText: Theme.ink; palette.base: Theme.raised; palette.text: Theme.ink; palette.window: Theme.raised
        palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
    }
    NewsSettingsEditor { id: newsEditor; anchors.fill: parent; visible: page.editingNews; onBack: page.editingNews=false }
    ColumnLayout {
        visible: !page.editingNews
        anchors.fill: parent; anchors.margins: 24; spacing: 14
        RowLayout {
            UiButton { objectName: "layoutBackButton"; iconName: "back"; onClicked: { if(page.selected >= 0) { Qt.inputMethod.hide(); page.selected = -1 } else { page.editor.begin(); page.back() } } }
            Label { text: page.selected >= 0 ? "Edit page" : "Pages & modules"; font.pixelSize: 28; Layout.fillWidth: true }
            UiButton { visible: page.selected >= 0; text: "Page list"; onClicked: { Qt.inputMethod.hide(); page.selected = -1 } }
        }
        RowLayout {
            visible: page.selected < 0; spacing: 12
            UiButton { objectName: "layoutPagesTab"; text: "Pages"; selected: page.section === "pages"; onClicked: page.section = "pages" }
            UiButton { objectName: "layoutModulesTab"; text: "Modules"; selected: page.section === "modules"; onClicked: page.section = "modules" }
        }
        Text { visible: page.editor.problem.length > 0; text: page.editor.problem; color: Theme.danger; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            visible: page.selected < 0 && page.section === "pages"; Layout.fillWidth: true; spacing: 12
            Choice { id: newType; objectName: "layoutNewType"; Layout.fillWidth: true; model: [{id:"",title:"Widget page"}].concat(page.editor.fullPages) }
            UiButton { objectName: "layoutAddPage"; text: "Add page"; onClicked: page.editor.addPage(newType.currentValue) }
        }
        ScrollView {
            id: scroll; Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: scroll.availableWidth; spacing: 12
                Label { visible: page.selected < 0 && page.section === "pages"; text: "Widget pages hold up to six cards. Chess, Notes and News use a full page."; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.selected < 0 && page.section === "pages" ? page.editor.pages : []
                    Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true; implicitHeight: 94; radius: 14; color: Theme.raised
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.title; Layout.fillWidth: true; textFormat: Text.PlainText }
                                Label { text: modelData.kind === "widgets" ? (modelData.widgets.length + " / 6 widgets") : (page.editor.modules[modelData.module] ? "Full page" : "Module disabled"); color: Theme.muted; font.pixelSize: 16 }
                            }
                            UiButton { text: "↑"; implicitWidth: 48; enabled: index > 0; onClicked: page.editor.movePage(index,-1) }
                            UiButton { text: "↓"; implicitWidth: 48; enabled: index < page.editor.pages.length-1; onClicked: page.editor.movePage(index,1) }
                            UiButton { objectName: "layoutEditPage" + index; text: "Edit"; onClicked: page.selected = index }
                        }
                    }
                }
                Label { visible: page.selected < 0 && page.section === "modules"; text: "Removing a widget leaves its module available to Sheila. Disable a module here to stop its integration and remove its AI tools. Existing data and credentials are kept."; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.selected < 0 && page.section === "modules" ? page.editor.catalogue : []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: modelData.title }
                            Label { text: modelData.description; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true }
                        }
                        UiButton { objectName: "moduleEdit_"+modelData.id; visible: modelData.id==="news"; text: "Edit"; onClicked: page.editNews() }
                        Switch { objectName: "moduleSwitch_" + modelData.id; checked: !!page.editor.modules[modelData.id]; implicitHeight: 54; implicitWidth: 80; onToggled: page.editor.setModule(modelData.id,checked) }
                    }
                }
                ColumnLayout {
                    visible: page.selected >= 0; Layout.fillWidth: true; spacing: 14
                    Label { text: "Page name" }
                    TextField {
                        id: pageName; objectName: "layoutPageName"; Layout.fillWidth: true; implicitHeight: 58; font.pixelSize: 22; maximumLength: 32
                        text: page.current.title || ""; color: Theme.ink; inputMethodHints: Qt.ImhNoPredictiveText
                        background: Rectangle { color: Theme.raised; radius: 12; border.color: pageName.activeFocus ? Theme.accent : Theme.border }
                        onEditingFinished: if(page.selected >= 0)page.editor.renamePage(page.selected,text)
                        onAccepted: Qt.inputMethod.hide()
                    }
                    UiButton { text: "Save name"; onClicked: { Qt.inputMethod.hide(); page.editor.renamePage(page.selected,pageName.text) } }
                    Label { visible: page.current.kind === "module"; text: "Full-page view · " + (page.current.module || ""); color: Theme.muted }
                    Repeater {
                        model: page.current.kind === "widgets" ? page.current.widgets : []
                        RowLayout {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true; spacing: 10
                            Label { text: page.widgetTitle(modelData); Layout.fillWidth: true }
                            UiButton { text: "↑"; implicitWidth: 48; enabled: index > 0; onClicked: page.editor.moveWidget(page.selected,index,-1) }
                            UiButton { text: "↓"; implicitWidth: 48; enabled: index < page.current.widgets.length-1; onClicked: page.editor.moveWidget(page.selected,index,1) }
                            UiButton { objectName: "layoutRemoveWidget" + index; text: "Remove"; onClicked: page.editor.removeWidget(page.selected,index) }
                        }
                    }
                    RowLayout {
                        visible: page.current.kind === "widgets"; Layout.fillWidth: true; spacing: 12
                        Choice { id: widgetChoice; objectName: "layoutWidgetChoice"; Layout.fillWidth: true; model: page.editor.widgets.filter(w => !(page.current.widgets || []).includes(w.id)) }
                        UiButton { objectName: "layoutAddWidget"; text: "Add widget"; enabled: (page.current.widgets || []).length < 6 && widgetChoice.count > 0; onClicked: page.editor.addWidget(page.selected,widgetChoice.currentValue) }
                    }
                    Label { visible: page.current.kind === "widgets"; text: "Widgets from disabled modules stay saved and reappear when enabled."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true }
                    UiButton { objectName: "layoutNewsSources"; visible: page.current.module==="news"; text: "Edit news sources"; onClicked: page.editNews() }
                    UiButton { objectName: "layoutRemovePage"; text: "Remove page"; enabled: page.current.id !== "home"; onClicked: { page.editor.removePage(page.selected); page.selected = -1 } }
                }
            }
        }
        Label { text: "Apply restarts the dashboard and stops current playback or conversation. SSH stays available."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            UiButton { objectName: "layoutCancel"; text: "Cancel"; Layout.fillWidth: true; onClicked: { page.editor.begin(); page.back() } }
            UiButton { objectName: "layoutApply"; text: "Apply & restart"; primary: true; Layout.fillWidth: true; enabled: page.editor.dirty; onClicked: { Qt.inputMethod.hide(); page.editor.apply() } }
        }
    }
}
