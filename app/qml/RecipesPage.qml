import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property bool active: false
    property bool swipeToExit: false
    signal back()
    readonly property var output: home.recipes.current
    readonly property bool wide: width>height
    // Observe swipes without taking the grab from vertical scrolling or taps.
    Item {
        anchors.fill: parent; z: 5
        PointHandler {
            target: null
            enabled: page.active && page.swipeToExit && !settings.visible && !removeConfirm.visible
            property point origin: Qt.point(0,0)
            property point lastPosition: Qt.point(0,0)
            onPointChanged: if(active)lastPosition=point.position
            onActiveChanged: {
                if(active){origin=point.position;lastPosition=origin}
                else if(enabled){
                    const dx=lastPosition.x-origin.x,dy=lastPosition.y-origin.y
                    if(Math.abs(dx)>Math.min(160,page.width*0.18) && Math.abs(dx)>Math.abs(dy)*1.4)page.back()
                }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Text { visible: home.recipes.problem.length>0 || home.recipes.busy; text: home.recipes.problem || "Preparing…"; textFormat: Text.PlainText; color: home.recipes.problem.length ? Theme.danger : Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        GridLayout {
            id: panes; objectName: "recipePanes"
            columns: page.wide && (page.output.left || "").length>0 && (page.output.right || "").length>0 ? 2 : 1
            Layout.fillWidth: true; Layout.fillHeight: true; columnSpacing: Theme.gap; rowSpacing: 12
            RecipePane { objectName: "recipeIngredients"; visible: home.recipes.count>0 && (page.output.left || "").length>0; title: page.output.title || ""; markdown: page.output.left || ""; side: "left"; outputId: page.output.id || ""; bottomClearance: (page.output.right || "").length>0 ? 0 : toolbar.height+24; Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; Layout.preferredHeight: 1; Layout.minimumWidth: 0; Layout.minimumHeight: 0 }
            RecipePane { objectName: "recipeMethod"; visible: home.recipes.count>0 && (page.output.right || "").length>0; title: (page.output.left || "").length>0 ? "" : page.output.title || ""; markdown: page.output.right || ""; side: "right"; outputId: page.output.id || ""; bottomClearance: toolbar.height+24; Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; Layout.preferredHeight: 1; Layout.minimumWidth: 0; Layout.minimumHeight: 0 }
            Rectangle {
                visible: home.recipes.count===0; Layout.fillWidth: true; Layout.fillHeight: true; color: Theme.surface; radius: Theme.radius; border.color: Theme.border
                Text { anchors.centerIn: parent; width: parent.width-64; text: home.recipes.busy ? "Preparing your recipe…" : "Ask Sheila for a recipe\n\nIngredients and method will appear here.\nTap ingredient checkboxes as you cook."; color: Theme.muted; font.pixelSize: home.recipes.fontSize; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
            }
        }
    }
    component IslandButton: UiButton {
        implicitWidth: 54; implicitHeight: 54
        background: Rectangle { radius: height/2; color: parent.down || parent.activeFocus ? Theme.tint : "transparent" }
    }
    Rectangle {
        id: toolbar; objectName: "recipeToolbar"
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 16
        z: 10; width: toolbarRow.implicitWidth+16; height: toolbarRow.implicitHeight+16
        radius: height/2; color: Theme.raised; border.color: Theme.border
        MouseArea { anchors.fill: parent }
        Row {
            id: toolbarRow; anchors.centerIn: parent; spacing: 4
            IslandButton { objectName: "recipeOlder"; iconName: "back"; description: "Older output"; enabled: !home.recipes.busy && home.recipes.index+1<home.recipes.count; onClicked: home.recipes.navigate(1) }
            IslandButton { objectName: "recipeNewer"; iconName: "forward"; description: "Newer output"; enabled: !home.recipes.busy && home.recipes.index>0; onClicked: home.recipes.navigate(-1) }
            IslandButton { objectName: "recipeDelete"; iconName: "trash"; description: "Delete this output"; enabled: !home.recipes.busy && home.recipes.count>0; onClicked: removeConfirm.open() }
            IslandButton { objectName: "recipeSettingsButton"; iconName: "settings"; description: "Recipe settings"; onClicked: settings.open() }
        }
    }
    RecipeSettings { id: settings }
    Popup {
        id: removeConfirm; parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(480,parent.width-48); padding: 22; modal: true
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 18
            Text { text: "Delete this output?"; color: Theme.ink; font.pixelSize: 24 }
            RowLayout {
                UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: removeConfirm.close() }
                UiButton { objectName: "recipeConfirmDelete"; text: "Delete"; Layout.fillWidth: true; onClicked: { home.recipes.removeCurrent(); removeConfirm.close() } }
            }
        }
    }
}
