import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 800; height: 1280
    visibility: Window.FullScreen
    title: "ThinkSmart Home"
    color: Theme.background
    font.family: Theme.font
    readonly property bool wide: width > height
    property string selectedApp: ""
    readonly property var pageNames: home.dashboardPages
    function pageIndex(name) { return pageNames.indexOf(name) }
    function showDashboardPage(index) { if(index < 0)return; selectedApp = ""; dashboardPages.setCurrentIndex(index) }
    function weatherValue(n, unit) { return typeof n === "number" && isFinite(n) ? Math.round(n) + unit : "—" }
    readonly property color ink: Theme.ink
    readonly property color quiet: Theme.muted
    readonly property color gold: Theme.accent
    Connections { target: home; function onOpenAppRequested(app) { if(window.pageIndex(app) >= 0)window.showDashboardPage(window.pageIndex(app));else window.selectedApp = app } }
    Binding { target: Theme; property: "paletteName"; value: home.themeName }
    Binding {
        target: home.systemStats; property: "active"
        value: home.modules.assistant && window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("home") && !dashboardPages.transitioning && !screenControl.sleeping
    }

    component HomeTile: WidgetTile {
        property int columnUnits: 1
        property int rowUnits: 1
        Layout.columnSpan: Math.min(columnUnits, window.wide ? 3 : 2)
        Layout.rowSpan: rowUnits
        Layout.fillWidth: true; Layout.fillHeight: true
        Layout.preferredWidth: 1; Layout.preferredHeight: 1
        Layout.minimumHeight: 180
    }
    component SmallLabel: Text { color: Theme.muted; font.pixelSize: 17; elide: Text.ElideRight }
    component BigValue: Text { color: Theme.ink; font.pixelSize: 48; font.weight: Font.Medium }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        enabled: !screenControl.sleeping && !screenControl.powerPending
        Item {
            id: topBar
            visible: window.selectedApp !== "video"
            Layout.fillWidth: true; Layout.preferredHeight: 70
            RowLayout {
                anchors.fill: parent; spacing: 12
                ColumnLayout {
                    spacing: 3
                    Text { text: home.clock; color: Theme.ink; font.pixelSize: 36; font.weight: Font.Medium }
                    Text { text: home.date; color: Theme.muted; font.pixelSize: 15 }
                }
                Item { Layout.fillWidth: true }
                UiButton {
                    objectName: "themeButton"
                    iconName: "palette"
                    description: "Change colour theme: " + Theme.title
                    onClicked: home.setTheme(Theme.names[(Theme.names.indexOf(home.themeName) + 1) % Theme.names.length])
                }
                UiButton { objectName: "screenOffButton"; iconName: "moon"; description: "Screen off"; onClicked: screenControl.sleep() }
                UiButton { objectName: "powerButton"; iconName: "power"; description: "Power and display settings"; onClicked: screenControl.showPowerMenu() }
            }
            Row {
                objectName: "dashboardPageSelector"
                anchors.centerIn: parent
                spacing: 8
                UiButton {
                    objectName: "homePageButton"; text: "Home"
                    implicitHeight: 54; implicitWidth: 84; font.pixelSize: 17
                    selected: window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("home")
                    onClicked: window.showDashboardPage(window.pageIndex("home"))
                }
                UiButton {
                    objectName: "moreAppsPageButton"; text: "News"; visible: home.modules.news
                    implicitHeight: 54; implicitWidth: 84; font.pixelSize: 17
                    selected: window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("news")
                    onClicked: window.showDashboardPage(window.pageIndex("news"))
                }
                UiButton {
                    objectName: "chessPageButton"; text: "Chess"; visible: home.modules.chess
                    implicitHeight: 54; implicitWidth: 84; font.pixelSize: 17
                    selected: window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("chess")
                    onClicked: window.showDashboardPage(window.pageIndex("chess"))
                }
            }
        }
        Text { visible: home.configProblem.length > 0; text: home.configProblem; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: window.selectedApp === "solar" ? 2 : (window.selectedApp === "music" ? 3 : (window.selectedApp === "video" ? 4 : (window.selectedApp === "weather" ? 5 : (window.selectedApp === "radio" ? 6 : (window.selectedApp.length ? 1 : 0)))))
            PathView {
                id: dashboardPages; objectName: "dashboardPages"
                clip: true; currentIndex: window.pageIndex("home"); model: window.pageNames.length; pathItemCount: count
                // Finger left: News -> Home -> Chess -> News; right reverses it.
                preferredHighlightBegin: 0.5; preferredHighlightEnd: 0.5
                highlightRangeMode: PathView.StrictlyEnforceRange
                highlightMoveDuration: 220; interactive: false
                readonly property bool transitioning: swipe.active || pageSnap.running || moving || Math.abs(offset - Math.round(offset)) > 0.001
                function settleAt(target) {
                    pageSnap.stop(); pageSnap.from = offset; pageSnap.to = target; pageSnap.start()
                }
                function setCurrentIndex(index) {
                    const target = (count - index) % count
                    let distance = target - offset
                    distance -= Math.round(distance / count) * count
                    settleAt(offset + distance)
                }
                NumberAnimation {
                    id: pageSnap; target: dashboardPages; property: "offset"
                    duration: 220; easing.type: Easing.OutCubic
                }
                // Qt 6.8 PathView only filters mouse grabs from child controls.
                // An axis-locked handler also supports touchscreen buttons and
                // leaves vertical feed scrolling to the news lists.
                DragHandler {
                    id: swipe; target: null; yAxis.enabled: false; enabled: dashboardPages.count > 1
                    property real origin: 0
                    property real draggedOffset: 0
                    onActiveChanged: {
                        if(active) { pageSnap.stop(); origin = dashboardPages.offset; draggedOffset = origin }
                        else {
                            const distance = (draggedOffset - origin) * dashboardPages.width
                            const commit = Math.abs(distance) > dashboardPages.width * 0.18 || (Math.abs(distance) > 32 && Math.abs(centroid.velocity.x) > 600)
                            const step = commit ? (distance > 0 ? 1 : -1) : 0
                            pageSnap.stop(); pageSnap.from = draggedOffset; pageSnap.to = Math.round(origin) + step; pageSnap.start()
                        }
                    }
                    onActiveTranslationChanged: if(active) {
                        draggedOffset = origin + Math.max(-1, Math.min(1, activeTranslation.x / dashboardPages.width))
                        dashboardPages.offset = draggedOffset
                    }
                }
                path: Path {
                    startX: (1-dashboardPages.count)*dashboardPages.width/2; startY: dashboardPages.height / 2
                    PathLine { x: (1+dashboardPages.count)*dashboardPages.width/2; y: dashboardPages.height / 2 }
                }
                delegate: Loader {
                    required property int index
                    width: dashboardPages.width; height: dashboardPages.height
                    sourceComponent: window.pageNames[index] === "chess" ? chessComponent : window.pageNames[index] === "news" ? newsComponent : homeComponent
                }
                Component {
                    id: chessComponent
                    ChessPage {
                        objectName: "chessPage"
                        active: window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("chess") && !dashboardPages.transitioning && !screenControl.sleeping
                    }
                }
                Component {
                    id: newsComponent
                    NewsPage {
                        objectName: "newsPage"
                        active: window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("news") && !dashboardPages.transitioning && !screenControl.sleeping
                    }
                }
                Component {
                    id: homeComponent
                    GridLayout {
                        id: dashboard
                        columns: window.wide ? 3 : 2
                        rowSpacing: Theme.gap; columnSpacing: Theme.gap
                        HomeTile {
                            objectName: "assistantTile"; visible: home.modules.assistant; heading: "Sheila"; iconName: "voice"; indexLabel: assistant.enabled ? "YOUR EVERYDAY COMPANION" : "ASSISTANT OFF"
                            onClicked: window.selectedApp = "assistant"
                            VoiceVisual {
                                objectName: "dashboardAssistantVisual"
                                anchors.right: parent.right; anchors.rightMargin: -8
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(0, parent.width - systemIndicators.width - 4)
                                height: Math.min(parent.height * 1.25, width * 1.05)
                                mode: "ready"; idleMotion: true
                                active: home.modules.assistant && assistant.enabled && window.selectedApp === "" && dashboardPages.currentIndex === window.pageIndex("home") && !screenControl.sleeping
                                animate: !dashboardPages.transitioning
                            }
                            SystemIndicators {
                                id: systemIndicators; objectName: "systemIndicators"
                                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                                width: 84; stats: home.systemStats
                            }
                        }
                        HomeTile {
                            objectName: "weatherTile"; visible: home.modules.weather; heading: weather.location; iconName: "weather";
                            badge: weather.stale ? "Stale" : ""; badgeColor: Theme.accent; indexLabel: weather.status.toUpperCase()
                            onClicked: window.selectedApp = "weather"
                            ColumnLayout {
                                anchors.fill: parent; spacing: 8
                                RowLayout {
                                    BigValue { text: window.weatherValue(weather.current.temperature_2m,"°") }
                                    Item { Layout.fillWidth: true }
                                    UiIcon { name: "weather"; Layout.preferredWidth: 72; Layout.preferredHeight: 72; tint: Theme.accent }
                                }
                                SmallLabel { text: weather.current.condition || "Forecast unavailable"; Layout.fillWidth: true }
                                Item { Layout.fillHeight: true }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 8
                                    Repeater {
                                        model: weather.hourly.slice(0,6)
                                        ColumnLayout {
                                            required property var modelData
                                            Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumWidth: 0; spacing: 5
                                            Text { text: modelData.label; color: Theme.faint; font.pixelSize: 13; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                                            Text { text: window.weatherValue(modelData.temperature_2m,"°"); color: Theme.ink; font.pixelSize: 21; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                                        }
                                    }
                                }
                            }
                        }
                        HomeTile {
                            objectName: "energyTile"; visible: home.modules.vrm; heading: "Energy"; iconName: "solar"
                            badge: energy.stale ? "Stale" : (energy.hasData ? "VRM" : "")
                            badgeColor: energy.stale ? Theme.accent : Theme.faint
                            indexLabel: energy.hasData ? energy.metrics.soc.age.toUpperCase() : "SOLAR & BATTERY"
                            onClicked: window.selectedApp = "solar"
                            ColumnLayout {
                                anchors.fill: parent; spacing: 10
                                RowLayout {
                                    BigValue { text: energy.metrics.soc.text }
                                    Item { Layout.fillWidth: true }
                                    SmallLabel {
                                        text: energy.metrics.batteryPower.available ? (energy.stale ? "Last: " : "") + (energy.metrics.batteryPower.value > 5 ? "charging" : (energy.metrics.batteryPower.value < -5 ? "discharging" : "idle")) : ""
                                        color: Theme.accent; font.pixelSize: 15
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: 7; radius: 3.5; color: Theme.raised
                                    Rectangle { width: parent.width * (energy.metrics.soc.available ? Math.max(0,Math.min(100,energy.metrics.soc.value))/100 : 0); height: parent.height; radius: 3.5; color: Theme.accent }
                                }
                                Item { Layout.fillHeight: true }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 24
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 5
                                        SmallLabel { text: "Solar"; font.pixelSize: 14 }
                                        Text { text: energy.metrics.solar.text; color: Theme.ink; font.pixelSize: 23 }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 5
                                        SmallLabel { text: "DC load"; font.pixelSize: 14 }
                                        Text { text: energy.metrics.dcLoad.text; color: Theme.ink; font.pixelSize: 23 }
                                    }
                                }
                            }
                        }
                        HomeTile {
                            objectName: "radioTile"; visible: home.modules.radio; heading: "Radio"; iconName: "radio"; indexLabel: "NEWS, SPEECH & LIVE STREAMS"
                            onClicked: window.selectedApp = "radio"
                            ColumnLayout {
                                anchors.fill: parent; spacing: 0
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: 82
                                    UiIcon { anchors.verticalCenter: parent.verticalCenter; name: "radio"; tint: Theme.accent; width: 100; height: 100 }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.topMargin: 24; spacing: 12
                                    Text { text: home.source === "radio" ? home.title : "BBC Radio 4 & more"; color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; elide: Text.ElideRight }
                                    SmallLabel { text: "Parliament · Folk · Jazz · Rock"; Layout.fillWidth: true }
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }
                        HomeTile {
                            objectName: "musicTile"; visible: home.modules.navidrome || home.modules.bluetooth; heading: "Music"; iconName: "music"; indexLabel: "YOUR COLLECTION"
                            onClicked: window.selectedApp = "music"
                            ColumnLayout {
                                anchors.fill: parent; spacing: 0
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: 82
                                    UiIcon { anchors.verticalCenter: parent.verticalCenter; name: "music"; tint: Theme.accent; width: 100; height: 100 }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.topMargin: 24; spacing: 12
                                    Text { text: "Library & phone audio"; color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; elide: Text.ElideRight }
                                    SmallLabel { text: "Your collection or Bluetooth"; Layout.fillWidth: true }
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }
                        HomeTile {
                            objectName: "videoTile"; visible: home.modules.jellyfin; heading: "Video"; iconName: "video"; indexLabel: "FILMS & SERIES"
                            onClicked: window.selectedApp = "video"
                            ColumnLayout {
                                anchors.fill: parent; spacing: 0
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: 82
                                    UiIcon { anchors.verticalCenter: parent.verticalCenter; name: "video"; tint: Theme.accent; width: 100; height: 100 }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.topMargin: 24; spacing: 12
                                    Text { text: "Settle into a story"; color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; elide: Text.ElideRight }
                                    SmallLabel { text: "Browse your film library"; Layout.fillWidth: true }
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                }
            }
            AssistantPage {
                objectName: "assistantPage"
                active: window.selectedApp === "assistant" && !screenControl.sleeping
                onBack: window.selectedApp = ""
            }
            EnergyPage { onBack: window.selectedApp = "" }
            MusicPage { active: window.selectedApp === "music"; onBack: window.selectedApp = "" }
            VideoLibraryPage { library: videoLibrary; heading: "Video"; active: window.selectedApp === "video"; onBack: window.selectedApp = "" }
            WeatherPage { onBack: window.selectedApp = "" }
            RadioPage { onBack: window.selectedApp = "" }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 96
            visible: home.modules.radio || home.modules.bluetooth || home.modules.navidrome || home.modules.jellyfin
            color: Theme.surface; border.color: Theme.border; radius: Theme.radius
            RowLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Rectangle {
                    width: 52; height: 52; radius: 14; color: Theme.tint
                    Text { visible: home.source === "radio" && home.radioId === "radio4"; anchors.centerIn: parent; text: "4"; color: Theme.accent; font.pixelSize: 36 }
                    UiIcon { visible: home.source !== "radio" || home.radioId !== "radio4"; anchors.centerIn: parent; name: home.source === "radio" ? "radio" : home.source === "bluetooth" ? "bluetooth" : (home.source === "jellyfin" ? "video" : "music"); tint: Theme.accent; width: 30; height: 30 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 100; spacing: 5
                    Text { text: home.title; color: Theme.ink; font.pixelSize: 21; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text {
                        text: home.state === "playing" ? (home.subtitle.length ? home.subtitle : "Playing") : (home.state === "paused" ? "Paused" : (home.state === "connecting" ? "Connecting…" : home.message))
                        color: home.state === "error" ? Theme.danger : Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight
                    }
                }
                UiButton { visible: home.source === "bluetooth" || home.source === "navidrome"; iconName: "previous"; description: "Previous track"; onClicked: home.previous() }
                UiButton { objectName: "radioPlayButton"; iconName: home.state === "playing" ? "pause" : "play"; primary: true; description: home.state === "playing" ? "Pause" : "Play"; onClicked: home.toggle() }
                UiButton { visible: home.source === "bluetooth" || home.source === "navidrome"; iconName: "next"; description: "Next track"; onClicked: home.next() }
                UiButton { iconName: "stop"; description: "Stop playback"; onClicked: home.stop() }
                Rectangle { width: 1; height: 36; color: Theme.border }
                UiButton { iconName: "volume"; selected: home.muted; description: home.muted ? "Unmute" : "Mute"; onClicked: home.toggleMute() }
                Slider {
                    id: volumeSlider
                    Layout.preferredWidth: window.wide ? 180 : 110; Layout.minimumHeight: 64
                    from: 0; to: 100; stepSize: 1; value: home.volume
                    onMoved: home.setVolume(Math.round(value))
                    background: Rectangle {
                        x: volumeSlider.leftPadding; y: volumeSlider.topPadding + volumeSlider.availableHeight/2 - 3
                        width: volumeSlider.availableWidth; height: 6; radius: 3; color: Theme.raised
                        Rectangle { width: volumeSlider.visualPosition * parent.width; height: parent.height; radius: 3; color: Theme.accent }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth-width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight/2 - height/2
                        width: 30; height: 30; radius: 15; color: Theme.accent
                    }
                }
                Text { visible: window.wide; text: home.muted ? "Muted" : home.volume + "%"; color: Theme.muted; font.pixelSize: 17; Layout.preferredWidth: 56 }
            }
        }
        Text { visible: screenControl.error.length > 0; text: screenControl.error; color: Theme.danger; font.pixelSize: 15; Layout.fillWidth: true }
    }
    VideoPlayerOverlay {}
    PowerDialogs { hostWindow: window }
    Rectangle {
        anchors.fill: parent; z: 10000
        visible: screenControl.sleeping || screenControl.powerPending
        color: screenControl.sleeping ? "black" : Theme.background
        Text { anchors.centerIn: parent; text: screenControl.powerMessage; visible: screenControl.powerPending; color: Theme.ink; font.pixelSize: 32 }
    }
}
