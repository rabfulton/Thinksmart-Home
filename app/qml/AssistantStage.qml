import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: stage
    property bool active: false
    readonly property bool wide: width > 850
    readonly property bool speaking: assistant.active && assistant.mode === "speaking"
    readonly property real audioLevel: speaking ? assistant.outputLevel : assistant.inputLevel
    readonly property string stateLabel: !assistant.enabled ? "Offline" : !assistant.active ? (assistant.wakeListening ? "Say Sheila" : assistant.wakePreparing ? "Warming up" : home.state === "playing" || home.state === "paused" || home.state === "connecting" ? "Wake paused for media" : "Ready when you are") : assistant.mode === "speaking" ? "Speaking" : assistant.mode === "thinking" ? "Thinking" : "Listening"
    spacing: 16
    GridLayout {
        columns: stage.wide ? 2 : 1; columnSpacing: 18; rowSpacing: 16
        Layout.fillWidth: true; Layout.fillHeight: true
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: stage.wide
            Layout.preferredWidth: stage.wide ? 430 : 1
            Layout.preferredHeight: stage.wide ? 1 : 320
            Layout.minimumHeight: 180
            radius: 20; color: Theme.background; border.color: Theme.border; clip: true
            // Deliberately restrained markings leave room for the moving field.
            RowLayout {
                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                anchors.margins: 18
                Rectangle { width: 6; height: 6; radius: 3; color: assistant.active || assistant.wakeListening ? Theme.accent : Theme.faint }
                Text { text: stage.stateLabel.toUpperCase(); color: Theme.muted; font.family: Theme.mono; font.pixelSize: 12; font.letterSpacing: 1.2 }
                Item { Layout.fillWidth: true }
                Text { text: "◌"; color: Theme.accent; font.pixelSize: 19 }
            }
            VoiceVisual {
                id: core; objectName: "assistantCore"
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height + 30); height: Math.min(parent.height + 30, parent.width)
                active: stage.active && assistant.enabled; idleMotion: true
                mode: assistant.active ? assistant.mode : "ready"
                level: assistant.active ? stage.audioLevel : 0.12
            }
            RowLayout {
                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                anchors.margins: 18; spacing: 12
                Text { text: stage.speaking ? "VOICE" : assistant.active ? "YOU" : "SHEILA"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 11; font.letterSpacing: 2 }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 3; height: 19
                    Repeater {
                        model: 16
                        Rectangle {
                            required property int index
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3; radius: 1.5
                            height: 3 + (assistant.active ? Math.max(0,core.envelope-(index/20)) * 17 : 0)
                            color: index / 16 < core.envelope && assistant.active ? Theme.accent : Theme.border
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: stage.wide ? 570 : 1
            Layout.minimumHeight: 120; color: "transparent"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: stage.wide ? 10 : 0; spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: assistant.transcript.length ? "CONVERSATION" : "A LITTLE COMPANY"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 12; font.letterSpacing: 2 }
                    Item { Layout.fillWidth: true }
                    Text { text: assistant.voice.charAt(0).toUpperCase()+assistant.voice.slice(1); color: Theme.muted; font.pixelSize: 14 }
                }
                ColumnLayout {
                    visible: assistant.transcript.length === 0; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 14
                    Item { Layout.fillHeight: stage.wide; Layout.preferredHeight: stage.wide ? 1 : 16 }
                    Text { text: assistant.active ? (assistant.mode === "thinking" ? "A moment of thought." : "I’m listening.") : "What’s on\nyour mind?"; color: Theme.ink; font.pixelSize: stage.wide ? 37 : 34; font.weight: Font.Medium; lineHeight: 1.1; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Text { text: "The forecast for " + weather.location + ". A favourite station.\nA question you’ve been meaning to ask."; color: Theme.muted; font.pixelSize: 18; lineHeight: 1.25; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Item { Layout.fillHeight: true }
                    Text { text: !assistant.enabled ? "Enable Sheila in Settings to begin." : assistant.active ? "Speak naturally. You can interrupt a reply." : assistant.wakeListening ? "Say “Sheila”, or start a conversation below." : "Start a conversation below."; color: Theme.accent; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
                ListView {
                    id: captions; objectName: "assistantCaptions"
                    visible: assistant.transcript.length > 0
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: assistant.transcript; spacing: 22
                    property bool followTail: true
                    onMovementStarted: followTail = false
                    onMovementEnded: followTail = atYEnd
                    onCountChanged: { if(count === 0) followTail = true }
                    onContentHeightChanged: if(followTail) Qt.callLater(function() { captions.positionViewAtEnd() })
                    ScrollIndicator.vertical: ScrollIndicator { }
                    delegate: Column {
                        required property var modelData
                        width: captions.width; spacing: 9
                        Text { text: modelData.role === "user" ? "YOU" : "SHEILA"; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 11; font.letterSpacing: 2 }
                        Text { width: parent.width; text: modelData.text; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: modelData.role === "user" ? Theme.muted : Theme.ink; font.pixelSize: stage.wide ? 27 : 25; lineHeight: 1.2 }
                    }
                }
            }
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    RowLayout {
        Layout.fillWidth: true; spacing: 12
        UiButton { objectName: "assistantStartButton"; iconName: "voice"; text: "Start conversation"; primary: true; enabled: assistant.enabled && assistant.configured && assistant.promptsReady && !assistant.active; onClicked: assistant.start() }
        UiButton { objectName: "assistantStopButton"; iconName: "stop"; text: "End"; enabled: assistant.active; onClicked: assistant.stop() }
        UiButton { text: "Interrupt"; visible: assistant.live || assistant.vad !== "manual"; enabled: assistant.active; onClicked: assistant.interrupt() }
        Button {
            objectName: "assistantTalkButton"; text: "Hold Talk"; visible: !assistant.live && assistant.vad === "manual"; enabled: assistant.active
            implicitWidth: 160; implicitHeight: 64; font.pixelSize: 19
            onPressed: assistant.beginTurn(); onReleased: assistant.endTurn(); onCanceled: assistant.endTurn()
        }
        Item { Layout.fillWidth: true }
        Text { visible: stage.wide; text: assistant.active ? "You can speak over a reply" : "YOUR EVERYDAY COMPANION"; color: Theme.faint; font.pixelSize: 12; font.letterSpacing: assistant.active ? 0 : 1.2 }
    }
}
