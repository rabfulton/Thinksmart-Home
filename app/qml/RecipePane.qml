import QtQuick
import QtQuick.Controls

Rectangle {
    id: pane
    required property string markdown
    required property string side
    required property string outputId
    property string title: ""
    property real bottomClearance: 0
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function updateDocument() {
        if(!editor || !editor.textDocument)return
        home.recipes.render(editor.textDocument,markdown,home.recipes.fontSize,Theme.accent,Theme.secondary)
    }
    onMarkdownChanged: updateDocument()
    onOutputIdChanged: { flick.contentY=0; updateDocument() }
    Component.onCompleted: updateDocument()
    Connections { target: home.recipes; function onSettingsChanged() { Qt.callLater(pane.updateDocument) } }
    Connections {
        target: Theme
        function onAccentChanged() { Qt.callLater(pane.updateDocument) }
        function onSecondaryChanged() { Qt.callLater(pane.updateDocument) }
    }
    Flickable {
        id: flick; objectName: "recipeScroll_"+pane.side
        anchors.fill: parent; anchors.margins: 18; clip: true
        contentWidth: width; contentHeight: editor.y+editor.height
        boundsBehavior: Flickable.StopAtBounds; flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Text {
            id: titleText; objectName: "recipeTitle_"+pane.side
            x: 8; y: 2; width: flick.width-16; visible: pane.title.length>0
            text: pane.title; textFormat: Text.PlainText
            color: Theme.accent; font.pixelSize: Math.round(home.recipes.fontSize*1.55); font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        TextArea {
            id: editor; objectName: "recipeText_"+pane.side
            y: titleText.visible ? titleText.y+titleText.implicitHeight+20 : 0
            width: flick.width; height: Math.max(0,flick.height-y,contentHeight+topPadding+bottomPadding)
            readOnly: true; activeFocusOnPress: false; selectByMouse: false; selectByKeyboard: false
            wrapMode: TextEdit.Wrap; textFormat: TextEdit.MarkdownText
            font.pixelSize: home.recipes.fontSize; color: Theme.ink; selectedTextColor: Theme.accentInk; selectionColor: Theme.accent
            leftPadding: 8; rightPadding: 8; topPadding: 2; bottomPadding: 16+pane.bottomClearance
            background: null
            MouseArea {
                objectName: "recipeTasks_"+pane.side
                x: 0; y: 0; width: Math.min(parent.width,home.recipes.fontSize*3); height: parent.height
                preventStealing: false
                onClicked: mouse => home.recipes.toggleTask(editor.textDocument,editor.positionAt(mouse.x,mouse.y),pane.side,pane.outputId)
            }
        }
    }
}
