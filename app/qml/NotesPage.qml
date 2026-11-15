import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property int section: 0
    property string problem: ""
    readonly property var items: section === 0 ? home.personal.notes : section === 1 ? home.personal.routines : home.personal.schedules
    function act(action, data) {
        const result = home.personalAction(action, data)
        problem = result.ok ? "" : result.error
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Repeater {
                model: ["Notes", "Routines", "Schedules"]
                UiButton {
                    required property string modelData
                    required property int index
                    objectName: "personalSection" + index
                    text: modelData; selected: page.section === index
                    onClicked: { page.section = index; page.problem = "" }
                }
            }
            Item { Layout.fillWidth: true }
            UiButton {
                objectName: "dictateNoteButton"
                text: assistant.noteDictation ? "Stop" : "Dictate"
                iconName: assistant.noteDictation ? "stop" : "voice"
                primary: !assistant.noteDictation
                visible: page.section === 0 && home.modules.assistant
                enabled: assistant.enabled && assistant.configured && (!assistant.active || assistant.noteDictation)
                onClicked: assistant.noteDictation ? assistant.stop() : assistant.startNote()
            }
        }
        Text {
            visible: page.problem.length > 0 || home.personal.problem.length > 0
            text: page.problem || home.personal.problem; textFormat: Text.PlainText
            color: Theme.danger; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        Rectangle {
            visible: assistant.noteDictation
            Layout.fillWidth: true; implicitHeight: dictationContent.implicitHeight + 32
            radius: Theme.radius; color: Theme.tint; border.color: Theme.accent
            ColumnLayout {
                id: dictationContent; anchors.fill: parent; anchors.margins: 16; spacing: 6
                Text { text: assistant.mode === "thinking" ? "Preparing your note…" : "Speak your note"; color: Theme.accent; font.pixelSize: 22 }
                Text { text: assistant.status; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        ListView {
            id: list; objectName: "personalList"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            spacing: 12; model: page.items; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: card
                required property var modelData
                required property int index
                width: list.width; height: content.implicitHeight + 36
                radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                RowLayout {
                    id: content; anchors.fill: parent; anchors.margins: 18; spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text {
                            visible: (card.modelData.title || "").length > 0
                            text: card.modelData.title || ""; textFormat: Text.PlainText
                            color: Theme.ink; font.pixelSize: 23; font.weight: Font.Medium
                            wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        Text {
                            visible: page.section === 0
                            text: card.modelData.text || ""; textFormat: Text.PlainText
                            color: Theme.ink; font.pixelSize: 22; lineHeight: 1.2
                            wrapMode: Text.Wrap; Layout.fillWidth: true
                        }
                        Text {
                            text: page.section === 0 ? (card.modelData.date || "") : page.section === 1 ? (card.modelData.steps || []).length + " actions" : (card.modelData.when || "")
                            textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 16
                            wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        Text {
                            visible: page.section === 2 && (card.modelData.status || "pending") !== "pending"
                            text: (card.modelData.status || "") + (card.modelData.lastResult ? " · " + card.modelData.lastResult : "")
                            textFormat: Text.PlainText; color: card.modelData.status === "failed" ? Theme.danger : Theme.faint
                            font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                    UiButton {
                        objectName: "runRoutine" + card.index
                        visible: page.section === 1; iconName: "play"; description: "Run " + (card.modelData.title || "routine")
                        onClicked: page.act("routine.run", { id: card.modelData.id })
                    }
                    UiButton {
                        objectName: "toggleSchedule" + card.index
                        visible: page.section === 2; text: card.modelData.enabled ? "On" : "Off"
                        selected: card.modelData.enabled === true; implicitWidth: 76
                        description: "Enable or disable " + (card.modelData.title || "schedule")
                        onClicked: page.act("schedule.enable", { id: card.modelData.id, enabled: !card.modelData.enabled })
                    }
                    UiButton {
                        objectName: "deletePersonal" + card.index
                        Layout.alignment: Qt.AlignTop
                        iconName: "trash"; description: "Delete " + (card.modelData.title || "item")
                        onClicked: page.act(page.section === 0 ? "notes.delete" : page.section === 1 ? "routine.delete" : "schedule.delete", { id: card.modelData.id })
                    }
                }
            }
            Column {
                visible: list.count === 0; anchors.centerIn: parent; width: Math.min(parent.width - 64, 620); spacing: 16
                UiIcon { name: page.section === 0 ? "notes" : page.section === 1 ? "play" : "alarm"; width: 58; height: 58; tint: Theme.accent; anchors.horizontalCenter: parent.horizontalCenter }
                Text {
                    width: parent.width; horizontalAlignment: Text.AlignHCenter
                    text: page.section === 0 ? "A place to leave a thought" : page.section === 1 ? "Your everyday routines" : "Something to look forward to"
                    color: Theme.ink; font.pixelSize: 28; wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width; horizontalAlignment: Text.AlignHCenter
                    text: page.section === 0 ? "Ask Sheila to save a note, or tap Dictate.\nYour notes stay here until you remove them." : page.section === 1 ? "Try asking Sheila: “Save an evening routine\nthat plays jazz at 30% volume.”" : "Try asking Sheila: “Play Radio 4 at nine\non weekdays” or “Set a timer for 20 minutes.”"
                    color: Theme.muted; font.pixelSize: 19; wrapMode: Text.WordWrap; lineHeight: 1.4
                }
            }
        }
        Text {
            visible: page.section === 0 && !assistant.enabled
            text: "Enable the assistant in Settings to dictate. Saved notes remain available."
            color: Theme.faint; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
    }
}
