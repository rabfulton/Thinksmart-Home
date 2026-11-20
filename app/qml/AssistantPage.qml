import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    property bool active: false
    property bool backgroundPlayback: false
    property string tab: "conversation"
    readonly property var voices: assistant.voices
    readonly property var vadModes: ["semantic", "server", "manual"]
    function personalityIndex() {
        const profiles = assistant.personalities
        for(let i = 0; i < profiles.length; ++i) if(profiles[i].id === assistant.personality) return i
        return -1
    }
    function modelIndex() {
        const models = assistant.models
        for(let i = 0; i < models.length; ++i) if(models[i].id === assistant.model) return i
        return -1
    }
    function backendIndex() {
        const models = assistant.backendModels
        for(let i = 0; i < models.length; ++i) if(models[i].id === assistant.backendModel) return i
        return -1
    }
    component Choice: ComboBox {
        id: choice
        font.family: Theme.font; font.pixelSize: 24
        leftPadding: 16; rightPadding: 44
        palette.button: Theme.raised; palette.buttonText: Theme.ink
        palette.base: Theme.raised; palette.text: Theme.ink; palette.window: Theme.raised
        palette.highlight: Theme.tint; palette.highlightedText: Theme.accent
        contentItem: Text { text: choice.displayText; textFormat: Text.PlainText; font: choice.font; color: Theme.ink; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        indicator: UiIcon { name: "back"; rotation: -90; tint: Theme.ink; width: 20; height: 20; x: choice.width-width-16; y: (choice.height-height)/2 }
        background: Rectangle { radius: 12; color: Theme.raised; border.color: choice.activeFocus||choice.popup.visible?Theme.accent:Theme.border }
        delegate: ItemDelegate {
            id: option
            required property int index
            objectName: choice.objectName+"Option"+index
            width: choices.width; implicitHeight: 58; leftPadding: 14; rightPadding: 14; font: choice.font
            highlighted: choice.highlightedIndex===index
            contentItem: Text { text: choice.textAt(option.index); textFormat: Text.PlainText; font: option.font; color: option.highlighted?Theme.accent:Theme.ink; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { color: option.highlighted?Theme.tint:Theme.raised; radius: 10 }
        }
        popup: Popup {
            y: choice.height+6; width: choice.width; padding: 6
            implicitHeight: Math.min(360,contentItem.implicitHeight+topPadding+bottomPadding)
            contentItem: ListView {
                id: choices; clip: true; implicitHeight: contentHeight
                model: choice.popup.visible?choice.delegateModel:null
                currentIndex: choice.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle { color: Theme.raised; radius: 14; border.color: Theme.border }
        }
    }
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    onActiveChanged: {
        if(!active) {
            audioLab.stop()
            // Let navigation bindings settle before deciding whether the new
            // view supports the ongoing conversation (Recipes and IPTV do).
            Qt.callLater(function() { if(!page.active && !page.backgroundPlayback)assistant.stop("page_navigation") })
        }
    }
    onTabChanged: assistant.stopVoicePreview()
    Connections { target: home; function onAudioSetupRequested() { page.tab = "audio" } }
    AudioTestPage {
        anchors.fill: parent; visible: page.tab === "audio"; active: page.active && visible
        onBack: page.tab = "conversation"
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 18; visible: page.tab !== "audio"
        RowLayout {
            spacing: 20
            UiButton { objectName: "voiceBackButton"; iconName: "back"; description: "Back"; onClicked: { if(page.tab === "settings")page.tab = "conversation";else page.back() } }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 5
                Text { text: page.tab === "settings" ? "Assistant settings" : "Sheila"; color: Theme.ink; font.pixelSize: 29 }
                Text { text: assistant.status; color: assistant.active || assistant.wakeListening ? Theme.accent : Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            Switch {
                objectName: "assistantEnableSwitch"; visible: page.tab === "conversation"; implicitWidth: 80; implicitHeight: 54
                checked: assistant.enabled; Accessible.name: "Enable assistant"; onToggled: assistant.enabled = checked
            }
            UiButton { objectName: "assistantSettingsButton"; text: page.tab === "settings" ? "Conversation" : "Settings"; onClicked: page.tab = page.tab === "settings" ? "conversation" : "settings" }
            UiButton { objectName: "assistantAudioTestButton"; visible: page.tab === "settings"; text: "Audio test"; onClicked: { assistant.stop(); page.tab = "audio" } }
        }
        Text { visible: assistant.problem.length > 0; text: assistant.problem; color: Theme.danger; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Text { visible: assistant.promptProblem.length > 0; text: assistant.promptProblem; color: Theme.danger; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ScrollView {
            id: settingsScroll; visible: page.tab === "settings"
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: settingsScroll.availableWidth; spacing: 18
                RowLayout {
                    Layout.fillWidth: true; spacing: 20
                    Text { text: "Personality"; color: Theme.ink; font.pixelSize: 21; Layout.preferredWidth: 150 }
                    Choice {
                        objectName: "assistantPersonalityChoice"; model: assistant.personalities
                        textRole: "name"; valueRole: "id"; Layout.fillWidth: true; implicitHeight: 54
                        enabled: !assistant.active; currentIndex: page.personalityIndex()
                        onActivated: function(index) { assistant.personality = assistant.personalities[index].id }
                        Accessible.name: "Assistant personality"
                    }
                    UiButton { objectName: "assistantPromptsReloadButton"; text: "Reload"; enabled: !assistant.active; onClicked: assistant.reloadConfiguration() }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 20
                    Text { text: "Model"; color: Theme.ink; font.pixelSize: 21; Layout.preferredWidth: 150 }
                    Choice {
                        objectName: "assistantModelChoice"; model: assistant.models
                        textRole: "name"; valueRole: "id"; Layout.fillWidth: true; implicitHeight: 54
                        enabled: !assistant.active; currentIndex: page.modelIndex()
                        onActivated: function(index) { assistant.model = assistant.models[index].id }
                        Accessible.name: "Assistant model"
                    }
                }
                RowLayout {
                    visible: assistant.live; Layout.fillWidth: true; spacing: 20
                    Text { text: "Backend"; color: Theme.ink; font.pixelSize: 21; Layout.preferredWidth: 150 }
                    Choice {
                        objectName: "assistantBackendChoice"; model: assistant.backendModels
                        textRole: "name"; valueRole: "id"; Layout.fillWidth: true; implicitHeight: 54
                        enabled: !assistant.active; currentIndex: page.backendIndex()
                        onActivated: function(index) { assistant.backendModel = assistant.backendModels[index].id }
                        Accessible.name: "Live reasoning backend"
                    }
                }
                Text { visible: assistant.live; text: "OpenAI manages reasoning and tool selection. This device executes local tools. Low reasoning effort keeps short requests responsive."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                RowLayout {
                    Layout.fillWidth: true; spacing: 20
                    Text { text: "Voice"; color: Theme.ink; font.pixelSize: 21; Layout.preferredWidth: 150 }
                    Choice {
                        objectName: "assistantVoiceChoice"; model: page.voices; Layout.fillWidth: true; implicitHeight: 54
                        enabled: !assistant.active; currentIndex: page.voices.indexOf(assistant.voice)
                        onActivated: function(index) { assistant.voice = page.voices[index]; assistant.previewVoice() }
                        Accessible.name: "Assistant voice"
                    }
                    UiButton {
                        objectName: "assistantVoicePreviewButton"; implicitHeight: 54
                        iconName: assistant.previewPlaying ? "stop" : "volume"
                        description: assistant.previewPlaying ? "Stop voice preview" : "Preview voice"
                        enabled: !assistant.active && (assistant.previewAvailable || assistant.previewPlaying)
                        onClicked: { if(assistant.previewPlaying) assistant.stopVoicePreview(); else assistant.previewVoice() }
                    }
                }
                Text {
                    visible: assistant.previewProblem.length > 0 || !assistant.previewAvailable
                    text: assistant.previewProblem.length > 0 ? assistant.previewProblem : "No saved preview for this voice."
                    color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Text { text: "Each model remembers its voice. Select a voice to hear a saved preview."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                RowLayout {
                    Layout.fillWidth: true; spacing: 20
                    visible: !assistant.live
                    Text { text: "Turn detection"; color: Theme.ink; font.pixelSize: 21; Layout.preferredWidth: 150 }
                    Choice {
                        objectName: "assistantVadChoice"; model: ["Natural pauses", "Silence detection", "Hold to talk"]
                        Layout.fillWidth: true; implicitHeight: 54; enabled: !assistant.active
                        currentIndex: page.vadModes.indexOf(assistant.vad)
                        onActivated: function(index) { assistant.vad = page.vadModes[index] }
                        Accessible.name: "Turn detection"
                    }
                }
                Text { visible: assistant.live; text: "Live handles natural pauses and spoken interruptions while listening and speaking together."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { text: "Model, personality, voice and turn detection apply to the next conversation."; color: Theme.faint; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                Text { text: "OpenAI · " + assistant.model; color: Theme.accent; font.pixelSize: 19 }
                Text { text: assistant.configured ? "API key configured privately on this device" : "API key needed"; color: Theme.muted; font.pixelSize: 17 }
                UiButton { text: "Reload configuration"; enabled: !assistant.active; onClicked: assistant.reloadConfiguration() }
                Text { text: "Sheila is detected locally during media playback and screen sleep. Turning Assistant off closes the listener and any conversation. Only active conversations send audio to OpenAI. Conversation audio and captions are not saved. Responses use a generated AI voice."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { text: "Edit prompts over SSH: " + assistant.promptDirectory; color: Theme.accent; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                Text { text: "Shared system instructions, household notes and the selected personality are read at each Start and sent to OpenAI. Add people and preferences to household.md. Reload refreshes the personality list."; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { text: "Wake word · Sheila"; color: Theme.ink; font.pixelSize: 21 }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Activation sound"; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true }
                    Switch {
                        objectName: "wakeSoundSwitch"; implicitHeight: 54; implicitWidth: 80
                        checked: assistant.wakeSoundEnabled; onToggled: assistant.wakeSoundEnabled = checked
                        Accessible.name: "Wake-word activation sound"
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 16
                    Text { text: "Wake sensitivity"; color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true }
                    Text { text: Math.round(wakeThresholdSlider.value) + " / 100"; color: Theme.accent; font.pixelSize: 21 }
                    UiButton { objectName: "wakeThresholdReset"; text: "Default"; enabled: !assistant.active && assistant.wakeThresholdOverridden; onClicked: assistant.resetWakeThreshold() }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    UiButton { objectName: "wakeThresholdLower"; text: "−"; implicitWidth: 54; enabled: !assistant.active && assistant.wakeSensitivity > 0; onClicked: assistant.wakeSensitivity = Math.max(0, assistant.wakeSensitivity - 5) }
                    Slider {
                        id: wakeThresholdSlider; objectName: "wakeThresholdSlider"; Layout.fillWidth: true; implicitHeight: 54
                        from: 0; to: 100; stepSize: 5; value: assistant.wakeSensitivity; enabled: !assistant.active
                        palette.highlight: Theme.accent; palette.button: Theme.ink; palette.mid: Theme.border
                        Accessible.name: "Wake sensitivity"
                        onMoved: if(!pressed) assistant.wakeSensitivity = value
                        onPressedChanged: if(!pressed) assistant.wakeSensitivity = value
                    }
                    UiButton { objectName: "wakeThresholdHigher"; text: "+"; implicitWidth: 54; enabled: !assistant.active && assistant.wakeSensitivity < 100; onClicked: assistant.wakeSensitivity = Math.min(100, assistant.wakeSensitivity + 5) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Less sensitive"; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; Layout.preferredWidth: 1 }
                    Text { text: "Default"; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; Layout.preferredWidth: 1; horizontalAlignment: Text.AlignHCenter }
                    Text { text: "More sensitive"; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; Layout.preferredWidth: 1; horizontalAlignment: Text.AlignRight }
                }
                Text {
                    text: "Lower sensitivity to reduce TV or video activations. Increase it if Sheila misses you. Changes apply when you release the slider."
                    color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Text { text: assistant.wakeStatus; color: assistant.wakeListening ? Theme.accent : Theme.muted; font.pixelSize: 17; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { visible: assistant.wakeProblem.length > 0; text: assistant.wakeProblem; color: Theme.danger; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        AssistantStage {
            visible: page.tab === "conversation"; active: page.active && visible
            Layout.fillWidth: true; Layout.fillHeight: true
        }
    }
}
