import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    objectName: "recipeSettings"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(620,parent.width-48); height: Math.min(content.implicitHeight+40,parent.height-48)
    modal: true; focus: true; padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        fontChoice.value=home.recipes.fontSize
        modelChoice.currentIndex=Math.max(0,home.recipes.models.findIndex(m=>m.id===home.recipes.model))
        purposeChoice.currentIndex=home.recipes.purpose==="text"?1:0
    }
    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
    component Choice: ComboBox {
        id: choice
        implicitHeight: 58; font.pixelSize: 22
        palette.button: Theme.raised; palette.buttonText: Theme.ink; palette.base: Theme.raised; palette.text: Theme.ink; palette.window: Theme.raised
        contentItem: Text { leftPadding: 14; text: choice.displayText; color: Theme.ink; font.pixelSize: 22; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    }
    ColumnLayout {
        id: content; width: popup.availableWidth; spacing: 16
        Text { text: "Recipe settings"; color: Theme.ink; font.pixelSize: 28 }
        Text { text: "Base text size"; color: Theme.muted; font.pixelSize: 20 }
        RowLayout {
            Layout.fillWidth: true
            Slider { id: fontChoice; objectName: "recipeFontSize"; from: 18; to: 36; stepSize: 1; Layout.fillWidth: true }
            Text { text: Math.round(fontChoice.value)+" px"; color: Theme.ink; font.pixelSize: 22; Layout.preferredWidth: 70 }
        }
        Text { text: "Text model"; color: Theme.muted; font.pixelSize: 20 }
        Choice {
            id: modelChoice; objectName: "recipeModel"; Layout.fillWidth: true
            model: home.recipes.models; textRole: "name"; valueRole: "id"
            delegate: ItemDelegate {
                required property int index; width: modelChoice.width; height: 58
                contentItem: Text { text: modelChoice.textAt(index); color: Theme.ink; font.pixelSize: 22; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.highlighted ? Theme.tint : Theme.raised }
            }
        }
        Text { text: "Use"; color: Theme.muted; font.pixelSize: 20 }
        Choice {
            id: purposeChoice; objectName: "recipePurpose"; Layout.fillWidth: true; model: ["Recipes","General text"]
            delegate: ItemDelegate {
                required property int index; width: purposeChoice.width; height: 58
                contentItem: Text { text: purposeChoice.textAt(index); color: Theme.ink; font.pixelSize: 22; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.highlighted ? Theme.tint : Theme.raised }
            }
        }
        Text { text: "Uses your saved OpenAI key only when asked to generate an output."; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: popup.close() }
            UiButton { objectName: "recipeSaveSettings"; text: "Save"; primary: true; Layout.fillWidth: true; enabled: !home.recipes.busy; onClicked: if(home.recipes.configure(Math.round(fontChoice.value),modelChoice.currentValue,purposeChoice.currentIndex===0?"recipe":"text"))popup.close() }
        }
    }
}
