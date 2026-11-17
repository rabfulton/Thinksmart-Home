import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ThinkSmart 1.0

Rectangle {
    id: playback
    anchors.fill: parent; z: 1000; color: "black"
    visible: home.video.visible
    property bool controlsVisible: true
    function timeLabel(seconds) { const n = Math.max(0, Math.floor(seconds)); return Math.floor(n / 3600) > 0 ? Math.floor(n / 3600) + ":" + String(Math.floor(n / 60) % 60).padStart(2,"0") + ":" + String(n % 60).padStart(2,"0") : Math.floor(n / 60) + ":" + String(n % 60).padStart(2,"0") }
    function reveal() { controlsVisible = true; hideControls.restart() }
    onVisibleChanged: if(visible) reveal()
    VideoSurface { anchors.fill: parent; player: home.video }
    MouseArea { anchors.fill: parent; onClicked: playback.controlsVisible ? playback.controlsVisible = false : playback.reveal() }
    Timer { id: hideControls; interval: 4000; onTriggered: if(home.state === "playing") playback.controlsVisible = false }
    Rectangle {
        visible: playback.controlsVisible || home.state !== "playing"
        anchors.fill: parent
        gradient: Gradient { GradientStop { position: 0; color: "#c0000000" } GradientStop { position: .35; color: "transparent" } GradientStop { position: .65; color: "transparent" } GradientStop { position: 1; color: "#e8000000" } }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 28
            RowLayout {
                spacing: 20
                UiButton { objectName: "videoCloseButton"; iconName: "back"; description: home.source === "iptv" ? "Stop TV and return to channels" : "Stop video and return to library"; onClicked: home.stop() }
                Text { text: home.title; color: "white"; font.pixelSize: 26; Layout.fillWidth: true; elide: Text.ElideRight }
                UiButton { text: "Audio"; enabled: home.video.audioTracks.length > 1; onClicked: { audioMenu.open(); playback.reveal() } }
                UiButton { text: "Subtitles"; enabled: home.video.subtitleTracks.length > 1; onClicked: { subtitleMenu.open(); playback.reveal() } }
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 24
                UiButton { visible: home.source !== "iptv"; text: "−10 s"; onClicked: { home.seek(home.position - 10); playback.reveal() } }
                UiButton { iconName: home.state === "playing" ? "pause" : "play"; primary: true; description: home.state === "playing" ? "Pause video" : "Resume video"; onClicked: { home.toggle(); playback.reveal() } }
                UiButton { visible: home.source !== "iptv"; text: "+10 s"; onClicked: { home.seek(home.position + 10); playback.reveal() } }
            }
            Item { Layout.fillHeight: true }
            Text { visible: home.state === "connecting" || home.state === "error"; text: home.message; color: "white"; font.pixelSize: 20; Layout.alignment: Qt.AlignHCenter }
            Text { visible: home.video.controlError.length > 0; text: home.video.controlError; color: Theme.danger; font.pixelSize: 18; Layout.alignment: Qt.AlignHCenter }
            RowLayout {
                Layout.fillWidth: true
                Text { text: home.source === "iptv" ? "LIVE" : playback.timeLabel(home.position); color: "white"; font.pixelSize: 18 }
                Slider { visible: home.source !== "iptv"; Layout.fillWidth: true; from: 0; to: Math.max(1,home.duration); value: home.position; enabled: home.duration > 0; onMoved: { home.seek(value); playback.reveal() } }
                Item { visible: home.source === "iptv"; Layout.fillWidth: true }
                Text { visible: home.source !== "iptv"; text: playback.timeLabel(home.duration); color: "white"; font.pixelSize: 18 }
                UiButton { iconName: "volume"; description: "Mute"; selected: home.muted; onClicked: { home.toggleMute(); playback.reveal() } }
                Slider { Layout.preferredWidth: 140; from: 0; to: 100; value: home.volume; onMoved: { home.setVolume(Math.round(value)); playback.reveal() } }
            }
        }
    }
    Popup {
        id: audioMenu; anchors.centerIn: parent; width: 360; modal: true; padding: 18
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Audio track"; color: Theme.ink; font.pixelSize: 24 }
            Repeater { model: home.video.audioTracks; UiButton { required property var modelData; text: modelData.label; selected: modelData.selected; Layout.fillWidth: true; onClicked: { home.video.selectTrack("audio",modelData.id); audioMenu.close() } } }
        }
    }
    Popup {
        id: subtitleMenu; anchors.centerIn: parent; width: 360; modal: true; padding: 18
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ScrollView {
            implicitHeight: Math.min(500, subtitleColumn.implicitHeight)
            ColumnLayout {
                id: subtitleColumn; width: subtitleMenu.availableWidth; spacing: 12
                Text { text: "Subtitles"; color: Theme.ink; font.pixelSize: 24 }
                Repeater { model: home.video.subtitleTracks; UiButton { required property var modelData; text: modelData.label; selected: modelData.selected; Layout.fillWidth: true; onClicked: { home.video.selectTrack("sub",modelData.id); subtitleMenu.close() } } }
            }
        }
    }

}
