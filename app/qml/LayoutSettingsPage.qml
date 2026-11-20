import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    objectName: "layoutSettingsPage"
    readonly property var editor: home.layoutEditor
    property string route: "main"
    onRouteChanged: { Qt.inputMethod.hide();Qt.callLater(function(){if(scroll.contentItem)scroll.contentItem.contentY=0}) }
    property var application: ({})
    property bool movingApplication: false
    property string displayMode: "widget"
    property var removals: []
    property int selected: -1
    property bool editingNews: false
    readonly property var current: selected >= 0 ? editor.pages[selected] || ({}) : ({})
    signal back()
    function widgetTitle(id) { for(const w of editor.widgets)if(w.id===id)return w.title;return id }
    function startApplication(app, moving) {
        application=app; movingApplication=moving
        displayMode=app.widget ? "widget" : "page"
        newPageName.text=moving && app.pageTitle ? app.pageTitle : app.title
        route=app.widget && app.page ? "display" : "location"
    }
    function finishApplication(mode, location, title) {
        Qt.inputMethod.hide()
        if(editor.placeApplication(application.id,mode,location,title,movingApplication))route="main"
    }
    function goBack() {
        Qt.inputMethod.hide()
        if(route==="new-page")route="location"
        else if(route==="location")route=application.widget && application.page ? "display" : movingApplication ? "main" : "add"
        else if(route==="display")route=movingApplication ? "main" : "add"
        else if(route==="page")route="pages"
        else if(route!=="main")route="main"
        else { editor.begin();page.back() }
    }
    function toggleRemoval(id) { const next=removals.slice();const i=next.indexOf(id);if(i<0)next.push(id);else next.splice(i,1);removals=next }
    component Label: Text { color: Theme.ink; font.pixelSize: 21; textFormat: Text.PlainText; wrapMode: Text.WordWrap }
    component NameField: TextField {
        implicitHeight: 60; maximumLength: 32; font.pixelSize: 22
        color: Theme.ink; placeholderTextColor: Theme.muted
        selectionColor: Theme.accent; selectedTextColor: Theme.accentInk
        background: Rectangle { color: Theme.raised; radius: 12; border.color: parent.activeFocus?Theme.accent:Theme.border }
    }
    component PickButton: Button {
        id: pick
        property string heading: ""
        property string detail: ""
        implicitHeight: Math.max(78, pickContent.implicitHeight+28)
        Accessible.name: heading
        contentItem: Column {
            id: pickContent; spacing: 5
            Text { width: parent.width; text: pick.heading; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 23; wrapMode: Text.WordWrap }
            Text { width: parent.width; visible: text.length>0; text: pick.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap }
        }
        padding: 14
        background: Rectangle { color: pick.down?Theme.tint:Theme.raised; radius: 14; border.color: pick.activeFocus?Theme.accent:Theme.border }
    }
    NewsSettingsEditor { anchors.fill: parent; visible: page.editingNews; onBack: page.editingNews=false }
    RecipeSettings { id: recipeSettings }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 14; visible: !page.editingNews
        RowLayout {
            Layout.fillWidth: true
            UiButton { objectName: "layoutBackButton"; iconName: "back"; onClicked: page.goBack() }
            Label {
                Layout.fillWidth: true; font.pixelSize: 28
                text: page.route==="main"?"Pages & modules":page.route==="add"?"Choose an application":page.route==="display"?"How should it appear?":page.route==="remove"?"Remove applications":page.route==="pages"?"Pages":page.route==="page"?"Edit page":"Choose a location"
            }
            UiButton { objectName: "layoutPagesTab"; visible: page.route==="main"; text: "Pages"; onClicked: page.route="pages" }
        }
        RowLayout {
            visible: page.route==="main"; Layout.fillWidth: true; spacing: 12
            UiButton { objectName: "layoutAddApplication"; text: "Add application"; Layout.fillWidth: true; onClicked: page.route="add" }
            UiButton { objectName: "layoutRemoveApplication"; text: "Remove application"; Layout.fillWidth: true; enabled: page.editor.activeApplications.length>0; onClicked: {page.removals=[];page.route="remove"} }
        }
        Label { visible: page.editor.problem.length>0; text: page.editor.problem; color: Theme.danger; font.pixelSize: 17; Layout.fillWidth: true }
        ScrollView {
            id: scroll; Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: scroll.availableWidth; spacing: 12
                Label { visible: page.route==="main"; text: "Active applications"; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.route==="main" ? page.editor.activeApplications : []
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; implicitHeight: appRow.implicitHeight+24; color: Theme.surface; radius: 14; border.color: Theme.border
                        RowLayout {
                            id: appRow; anchors.fill: parent; anchors.margins: 12; spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                Label { text: modelData.title; Layout.fillWidth: true }
                                Label { text: modelData.placement; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true }
                            }
                            UiButton { objectName: "moduleEdit_"+modelData.id; visible: modelData.id==="news"||modelData.id==="recipes"; text: "Edit"; enabled: !!home.modules[modelData.id]; onClicked: modelData.id==="news" ? page.editingNews=true : recipeSettings.open() }
                            UiButton { objectName: "applicationPlacement_"+modelData.id; text: "Placement"; onClicked: page.startApplication(modelData,true) }
                        }
                    }
                }
                Label { visible: page.route==="main" && !page.editor.activeApplications.length; text: "Add an application to get started."; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.route==="add" ? page.editor.availableApplications : []
                    PickButton { required property var modelData; objectName: "addApplication_"+modelData.id; Layout.fillWidth: true; heading: modelData.title; detail: modelData.description; onClicked: page.startApplication(modelData,false) }
                }
                Label { visible: page.route==="add" && !page.editor.availableApplications.length; text: "All applications are already active. Use Placement to move one, or Remove application to disable it."; color: Theme.muted; Layout.fillWidth: true }
                Label { visible: ["display","location","new-page"].indexOf(page.route)>=0; text: page.application.title || ""; color: Theme.accent; Layout.fillWidth: true }
                PickButton { objectName: "applicationWidgetDisplay"; visible: page.route==="display"; Layout.fillWidth: true; heading: "Widget"; detail: "A small card that opens the application."; onClicked: {page.displayMode="widget";page.route="location"} }
                PickButton { objectName: "applicationPageDisplay"; enabled: page.editor.pages.length<24 || page.editor.canCreatePage(page.application.id||""); visible: page.route==="display"; Layout.fillWidth: true; heading: "Full page"; detail: "Give this application its own page."; onClicked: {page.displayMode="page";page.route="location"} }
                Label { visible: page.route==="location" && page.displayMode==="page"; text: "A full-page application gets a new dedicated page."; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.route==="location" && page.displayMode==="widget" ? page.editor.locations(page.application.id) : []
                    PickButton { required property var modelData; objectName: "applicationLocation_"+modelData.id; Layout.fillWidth: true; heading: modelData.title; detail: modelData.description; onClicked: page.finishApplication("widget",modelData.id,"") }
                }
                PickButton { objectName: "applicationNewPage"; enabled: page.editor.pages.length<24 || page.editor.canCreatePage(page.application.id||""); visible: page.route==="location" && page.displayMode==="widget"; Layout.fillWidth: true; heading: "New page"; detail: "Create a widget page with room for more applications."; onClicked: {newPageName.text=page.application.title;page.route="new-page"} }
                ColumnLayout {
                    visible: page.route==="new-page" || (page.route==="location" && page.displayMode==="page"); Layout.fillWidth: true; spacing: 14
                    Label { text: "Page name" }
                    NameField { id: newPageName; objectName: "applicationNewPageName"; Layout.fillWidth: true; selectByMouse: true; onAccepted: {focus=false;Qt.inputMethod.hide()} }
                    UiButton { objectName: "applicationFinish"; text: page.movingApplication?"Move application":"Add application"; primary: true; Layout.fillWidth: true; enabled: newPageName.text.trim().length>0; onClicked: page.finishApplication(page.displayMode,"",newPageName.text) }
                }
                PickButton { objectName: "applicationOnDemand"; visible: page.route==="display" || page.route==="location"; Layout.fillWidth: true; heading: "Open on request"; detail: "Keep it available to Sheila without a permanent page or card."; onClicked: page.finishApplication("ondemand","","") }
                Label { visible: page.route==="remove"; text: "Select applications to disable and remove from pages. Saved data and credentials are kept."; color: Theme.muted; Layout.fillWidth: true }
                Repeater {
                    model: page.route==="remove" ? page.editor.activeApplications : []
                    CheckDelegate {
                        id: removeChoice
                        required property var modelData
                        objectName: "removeApplication_"+modelData.id
                        Layout.fillWidth: true; implicitHeight: 68; leftPadding: 14; rightPadding: 58
                        text: modelData.title; font.pixelSize: 23
                        checked: page.removals.indexOf(modelData.id)>=0; onClicked: page.toggleRemoval(modelData.id)
                        contentItem: Text { text: removeChoice.text; textFormat: Text.PlainText; font: removeChoice.font; color: Theme.ink; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                        indicator: Rectangle {
                            width: 28; height: 28; x: removeChoice.width-width-14; y: (removeChoice.height-height)/2; radius: 5
                            color: removeChoice.checked?Theme.accent:Theme.raised; border.color: removeChoice.checked?Theme.accent:Theme.muted
                            Text { anchors.centerIn: parent; visible: removeChoice.checked; text: "✓"; color: Theme.accentInk; font.pixelSize: 22 }
                        }
                        background: Rectangle { radius: 12; color: removeChoice.checked?Theme.tint:Theme.surface; border.color: removeChoice.checked?Theme.accent:Theme.border }
                    }
                }
                Repeater {
                    model: page.route==="pages" ? page.editor.pages : []
                    Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true; implicitHeight: 92; color: Theme.raised; radius: 14
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 10
                            ColumnLayout { Layout.fillWidth: true; Label {text:modelData.title;Layout.fillWidth:true} Label {text:modelData.kind==="widgets"?modelData.widgets.length+" / 6 widgets":"Full page";color:Theme.muted;font.pixelSize:16} }
                            UiButton { text: "↑"; implicitWidth: 48; enabled: index>0; onClicked: page.editor.movePage(index,-1) }
                            UiButton { text: "↓"; implicitWidth: 48; enabled: index<page.editor.pages.length-1; onClicked: page.editor.movePage(index,1) }
                            UiButton { objectName: "layoutEditPage"+index; text: "Edit"; onClicked: {page.selected=index;pageName.text=modelData.title;page.route="page"} }
                        }
                    }
                }
                ColumnLayout {
                    visible: page.route==="page"; Layout.fillWidth: true; spacing: 14
                    Label { text: "Page name" }
                    NameField { id: pageName; objectName: "layoutPageName"; Layout.fillWidth: true; onAccepted: {focus=false;Qt.inputMethod.hide()} }
                    UiButton { text: "Save name"; onClicked: {Qt.inputMethod.hide();page.editor.renamePage(page.selected,pageName.text)} }
                    Repeater {
                        model: page.route==="page" && page.current.kind==="widgets" ? page.current.widgets : []
                        RowLayout {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            Label { text: page.widgetTitle(modelData); Layout.fillWidth: true }
                            UiButton { text: "↑"; implicitWidth: 48; enabled: index>0; onClicked: page.editor.moveWidget(page.selected,index,-1) }
                            UiButton { text: "↓"; implicitWidth: 48; enabled: index<page.current.widgets.length-1; onClicked: page.editor.moveWidget(page.selected,index,1) }
                            UiButton { objectName: "layoutRemoveWidget"+index; text: "Remove card"; onClicked: page.editor.removeWidget(page.selected,index) }
                        }
                    }
                    Label { text: "Use application Placement to add or move cards. Removing a card leaves its application available on request."; color: Theme.muted; font.pixelSize: 17; Layout.fillWidth: true }
                    UiButton { objectName: "layoutRemovePage"; text: "Remove page"; enabled: page.current.id!=="home"; onClicked: {page.editor.removePage(page.selected);page.route="pages";page.selected=-1} }
                }
            }
        }
        Label { visible: ["main","pages","page"].indexOf(page.route)>=0; text: "Apply restarts the dashboard and stops playback or conversation. SSH remains available."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            UiButton { objectName: "layoutCancel"; text: "Cancel changes"; Layout.fillWidth: true; onClicked: {page.editor.begin();page.route="main";page.back()} }
            UiButton { objectName: "applicationRemoveSelected"; visible: page.route==="remove"; text: "Remove selected"; Layout.fillWidth: true; enabled: page.removals.length>0; onClicked: {if(page.editor.removeApplications(page.removals))page.route="main"} }
            UiButton { objectName: "layoutApply"; visible: ["main","pages","page"].indexOf(page.route)>=0; text: "Apply & restart"; primary: true; Layout.fillWidth: true; enabled: page.editor.dirty; onClicked: {Qt.inputMethod.hide();page.editor.apply()} }
        }
    }
}
