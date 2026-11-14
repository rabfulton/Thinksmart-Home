import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    property bool active: false
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    onActiveChanged: { if(!active && audioLab.running) audioLab.stop() }
    function number(value, suffix) { return typeof value === "number" && isFinite(value) ? value.toFixed(1) + suffix : "—" }
    function meter(value) { return typeof value === "number" && isFinite(value) ? Math.max(0,Math.min(1,(value+80)/65)) : 0 }
    component Meter: ColumnLayout {
        property string label
        property var reading
        Layout.fillWidth: true; spacing: 10
        Text { text: label; color: Theme.muted; font.pixelSize: 17 }
        Text { text: page.number(reading," dBFS"); color: Theme.ink; font.pixelSize: 28 }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 9; radius: 4.5; color: Theme.background
            Rectangle { width: parent.width * page.meter(reading); height: parent.height; radius: 4.5; color: Theme.accent }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 16
        RowLayout {
            spacing: 20
            UiButton { objectName: "assistantBackButton"; iconName: "back"; description: "Back"; onClicked: { audioLab.stop();page.back() } }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 5
                Text { text: "Assistant · Audio setup"; color: Theme.ink; font.pixelSize: 29 }
                Text { text: audioLab.status; color: audioLab.running ? Theme.accent : Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }
        Text { visible: audioLab.problem.length > 0; text: audioLab.problem; color: Theme.danger; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ScrollView {
            id: scroll
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: scroll.availableWidth; spacing: 18
                RowLayout {
                    spacing: 22
                    VoiceVisual {
                        Layout.preferredWidth: page.width > 900 ? 220 : 160; Layout.preferredHeight: 110
                        active: page.active && audioLab.running
                        mode: audioLab.running ? "listening" : "ready"
                        level: page.meter(audioLab.levels.cleanDbfs)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 10
                        Text { text: "Echo cancellation first"; color: Theme.ink; font.pixelSize: 25 }
                        Text { text: "The device plays a test voice while processing its microphones. Speak over it to check that your voice remains clear."; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Text { text: "Local processing · no cloud connection"; color: Theme.accent; font.pixelSize: 15 }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Text { text: "Test voice"; color: Theme.muted; font.pixelSize: 17 }
                    Slider {
                        objectName: "testVoiceSlider"
                        Layout.fillWidth: true; Layout.minimumHeight: 50
                        from: 5; to: 75; stepSize: 5; value: audioLab.testVoiceLevel * 100
                        enabled: !audioLab.running
                        onMoved: audioLab.testVoiceLevel = value / 100
                    }
                    Text { text: Math.round(audioLab.testVoiceLevel * 100) + "%"; color: Theme.ink; font.pixelSize: 18; Layout.preferredWidth: 52 }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 122; radius: 18; color: Theme.raised
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 20; spacing: 25
                        Meter { label: "Microphone"; reading: audioLab.levels.rawDbfs }
                        Meter { label: "Cleaned"; reading: audioLab.levels.cleanDbfs }
                        Meter { label: "Linear / wake path"; reading: audioLab.levels.linearDbfs }
                    }
                }
                RowLayout {
                    spacing: 12
                    UiButton { objectName: "audioLabStartButton"; text: "Start 20-second test"; primary: true; enabled: audioLab.available && !audioLab.running; onClicked: audioLab.start(saveClips.checked) }
                    UiButton { objectName: "audioLabStopButton"; text: "Stop"; enabled: audioLab.running; onClicked: audioLab.stop() }
                    Text { text: page.number(audioLab.levels.elapsed," s"); color: Theme.muted; font.pixelSize: 20 }
                }
                RowLayout {
                    spacing: 10
                    CheckBox { id: saveClips; enabled: !audioLab.running; Accessible.name: "Save microphone and processed clips locally" }
                    Text { text: "Save raw and processed clips locally for listening"; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
                Text {
                    text: "Clipping: " + page.number(audioLab.levels.clippedPercent,"%") + "    ·    Level difference: " + page.number(audioLab.levels.levelDifferenceDb," dB") + "    ·    CPU: " + page.number(audioLab.levels.cpuPercent,"% of one core")
                    color: Theme.faint; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Text { text: "Numeric results are saved automatically; voice clips are optional. A lower cleaned level alone does not prove your speech is preserved. Music pauses for the test. Leaving this page, screen sleep, or Stop closes the microphones."; color: Theme.faint; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { visible: audioLab.recordingPath.length > 0; text: "Saved on device: " + audioLab.recordingPath; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
