import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: preview
    property string appKey: "assistant"
    property bool active: true
    property string voiceMode: "speaking"
    readonly property var apps: ({
        assistant: { title: "Assistant", icon: "voice", subtitle: "A visual presence for voice and music.", items: ["Ready", "Listening", "Thinking", "Speaking", "Music"] },
        weather: { title: "Weather", icon: "weather", subtitle: "The day ahead, at a glance.", items: ["Hourly forecast", "The next few days", "Conditions & alerts"] },
        solar: { title: "Energy", icon: "solar", subtitle: "Your solar generation and battery, together.", items: ["Power flow", "Battery history", "Daily generation"] },
        radio: { title: "Radio", icon: "radio", subtitle: "News, conversation and live broadcasts.", items: ["BBC Radio 4", "World Service", "Parliament live"] },
        music: { title: "Music", icon: "music", subtitle: "A home for your collection.", items: ["Albums", "Artists", "Playlists & favourites"] },
        video: { title: "Video", icon: "video", subtitle: "Something for the bigger moment.", items: ["Continue watching", "Films", "Series"] }
    })
    readonly property var app: apps[appKey] || apps.assistant
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    onAppKeyChanged: voiceMode = "speaking"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 16
        RowLayout {
            spacing: 20
            UiButton { objectName: "previewBackButton"; iconName: "back"; description: "Back to dashboard"; onClicked: preview.back() }
            ColumnLayout {
                spacing: 5
                Text { text: preview.app.title; color: Theme.ink; font.pixelSize: 29; font.weight: Font.Medium }
                Text { text: "APP PREVIEW"; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 12; font.letterSpacing: 2 }
            }
            Item { Layout.fillWidth: true }
            UiIcon { name: preview.app.icon; tint: Theme.accent; width: 30; height: 30 }
        }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width, 680)
                spacing: 18
                VoiceVisual {
                    visible: preview.appKey === "assistant"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(380, parent.width)
                    Layout.preferredHeight: Math.min(260, preview.height * 0.32)
                    mode: preview.voiceMode
                    active: preview.active && preview.appKey === "assistant"
                }
                Text {
                    visible: preview.appKey === "assistant"
                    text: preview.voiceMode.charAt(0).toUpperCase() + preview.voiceMode.slice(1) + " · preview"
                    color: Theme.accent; font.pixelSize: 24; Layout.alignment: Qt.AlignHCenter
                }
                RowLayout {
                    visible: preview.appKey === "assistant"
                    Layout.alignment: Qt.AlignHCenter; spacing: 8
                    Repeater {
                        model: ["ready", "listening", "thinking", "speaking", "music"]
                        UiButton {
                            required property string modelData
                            text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                            selected: preview.voiceMode === modelData
                            implicitHeight: 60; font.pixelSize: 17
                            onClicked: preview.voiceMode = modelData
                        }
                    }
                }
                Text { text: preview.app.subtitle; color: Theme.ink; font.pixelSize: 23; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                Text {
                    text: preview.appKey === "assistant" ? "Synthetic motion preview. No audio is recorded or played." : "A placeholder for the full app. Sample sections below."
                    color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true
                }
                UiButton { visible: preview.appKey === "radio"; text: "Listen to BBC Radio 4"; primary: true; Layout.alignment: Qt.AlignHCenter; onClicked: { home.useRadio(); home.play() } }
                ColumnLayout {
                    visible: preview.appKey !== "assistant"
                    Layout.fillWidth: true; spacing: 10
                    Repeater {
                        model: preview.app.items
                        Rectangle {
                            required property string modelData
                            Layout.fillWidth: true; implicitHeight: 70
                            color: Theme.raised; radius: 14
                            Text { anchors.left: parent.left; anchors.leftMargin: 22; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.ink; font.pixelSize: 21 }
                            Text { anchors.right: parent.right; anchors.rightMargin: 22; anchors.verticalCenter: parent.verticalCenter; text: "Preview"; color: Theme.faint; font.pixelSize: 15 }
                        }
                    }
                }
            }
        }
    }
}
