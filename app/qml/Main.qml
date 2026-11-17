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
    readonly property var pageDefinitions: home.pageDefinitions
    function pageForModule(name) { for(let i=0;i<pageDefinitions.length;i++)if(pageDefinitions[i].module === name)return i;return -1 }
    function pageIndex(name) { return pageNames.indexOf(name) }
    function showDashboardPage(index) { if(index < 0)return; selectedApp = ""; dashboardPages.setCurrentIndex(index) }
    function weatherValue(n, unit) { return typeof n === "number" && isFinite(n) ? Math.round(n) + unit : "—" }
    readonly property color ink: Theme.ink
    readonly property color quiet: Theme.muted
    readonly property color gold: Theme.accent
    Connections { target: home; function onOpenAppRequested(app) { if(window.pageIndex(app) >= 0)window.showDashboardPage(window.pageIndex(app));else if(window.pageForModule(app) >= 0)window.showDashboardPage(window.pageForModule(app));else if(["chess","news","notes"].indexOf(app) < 0)window.selectedApp = app } }
    Binding { target: Theme; property: "paletteName"; value: home.themeName }
    Binding { target: Theme; property: "palette"; value: home.themePalette }
    Binding { target: Theme; property: "names"; value: home.personal.themeNames }
    Binding {
        target: home.systemStats; property: "active"
        value: home.modules.assistant && window.selectedApp === "" && window.pageDefinitions[dashboardPages.currentIndex] && (window.pageDefinitions[dashboardPages.currentIndex].widgets || []).indexOf("assistant") >= 0 && !dashboardPages.transitioning && !screenControl.sleeping
    }
    Binding { target: home.iptv; property: "active"; value: home.modules.iptv && !screenControl.sleeping && !home.video.visible && (window.selectedApp === "iptv" || (window.pageRunning && window.currentPage.module === "iptv")) }
    // Services have one owner even when several pages show the same module.
    readonly property var currentPage: pageDefinitions[dashboardPages.currentIndex] || ({})
    readonly property bool browsingTv: selectedApp === "iptv" || (selectedApp === "" && currentPage.module === "iptv")
    readonly property bool pageRunning: selectedApp === "" && !dashboardPages.transitioning && !screenControl.sleeping
    Binding { target: chessGame; property: "active"; value: home.modules.chess && window.pageRunning && window.currentPage.module === "chess" }
    Binding { target: headlines; property: "active"; value: home.modules.news && window.pageRunning && window.currentPage.module === "news" }
    Binding { target: hackerNews; property: "active"; value: home.modules.news && window.pageRunning && window.currentPage.module === "news" }


    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        visible: window.selectedApp !== "layout-settings"
        enabled: !screenControl.sleeping && !screenControl.powerPending
        Item {
            id: topBar
            visible: window.selectedApp !== "video" && !window.browsingTv
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
                UiButton { objectName: "powerButton"; iconName: "settings"; description: "Device settings"; onClicked: screenControl.showPowerMenu() }
                UiButton { objectName: "screenOffButton"; iconName: "power"; description: "Screen off"; onClicked: screenControl.sleep() }
            }
            Flickable {
                objectName: "dashboardPageSelector"
                anchors.centerIn: parent
                width: Math.min(pageTabs.implicitWidth, parent.width - 420); height: 54
                contentWidth: pageTabs.implicitWidth; clip: true; flickableDirection: Flickable.HorizontalFlick
                Row {
                    id: pageTabs; spacing: 6
                    Repeater {
                        model: window.pageDefinitions
                        UiButton {
                            id: pageTab
                            required property var modelData
                            required property int index
                            objectName: modelData.id === "news" ? "moreAppsPageButton" : modelData.id === "home" ? "homePageButton" : modelData.id === "chess" ? "chessPageButton" : modelData.id === "notes" ? "notesPageButton" : "pageButton_" + modelData.id
                            text: modelData.title; implicitHeight: 54; implicitWidth: Math.max(70, Math.min(180, implicitContentWidth + 24)); font.pixelSize: 17
                            contentItem: Text { text: pageTab.text; textFormat: Text.PlainText; elide: Text.ElideRight; font: pageTab.font; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            selected: window.selectedApp === "" && dashboardPages.currentIndex === index
                            onClicked: window.showDashboardPage(index)
                        }
                    }
                }
            }
        }
        Text { visible: home.configProblem.length > 0; text: home.configProblem; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: window.selectedApp === "iptv" ? 7 : window.selectedApp === "solar" ? 2 : (window.selectedApp === "music" ? 3 : (window.selectedApp === "video" ? 4 : (window.selectedApp === "weather" ? 5 : (window.selectedApp === "radio" ? 6 : (window.selectedApp.length ? 1 : 0)))))
            PathView {
                id: dashboardPages; objectName: "dashboardPages"
                clip: true; currentIndex: window.pageIndex("home"); model: window.pageNames.length; pathItemCount: count
                // Finger left: Home -> Chess -> Notes -> News -> Home.
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
                    property var pageData: window.pageDefinitions[index] || ({})
                    property bool pageActive: window.selectedApp === "" && dashboardPages.currentIndex === index && !screenControl.sleeping
                    sourceComponent: pageData.kind === "widgets" ? homeComponent : pageData.module === "chess" ? chessComponent : pageData.module === "news" ? newsComponent : pageData.module === "notes" ? notesComponent : pageData.module === "iptv" ? iptvComponent : null
                }
                Component { id: notesComponent; NotesPage { objectName: "notesPage" } }
                Component { id: iptvComponent; IptvPage { objectName: "iptvFullPage"; active: parent.pageActive && !dashboardPages.transitioning; keyboardHeight: virtualKeyboard.item ? virtualKeyboard.item.visibleHeight : 0; onBack: window.showDashboardPage(window.pageIndex("home")) } }
                Component {
                    id: chessComponent
                    ChessPage {
                        objectName: "chessPage"
                        active: parent.pageActive && !dashboardPages.transitioning
                    }
                }
                Component {
                    id: newsComponent
                    NewsPage {
                        objectName: "newsPage"
                        active: parent.pageActive && !dashboardPages.transitioning
                    }
                }
                Component {
                    id: homeComponent
                    WidgetDeck {
                        hostWindow: window
                        widgets: parent.pageData.widgets || []
                        pageActive: parent.pageActive
                        transitioning: dashboardPages.transitioning
                    }
                }
            }

            AssistantPage {
                objectName: "assistantPage"
                active: window.selectedApp === "assistant" && !screenControl.sleeping
                backgroundPlayback: home.source === "iptv" && home.video.visible
                onBack: window.selectedApp = ""
            }
            EnergyPage { onBack: window.selectedApp = "" }
            MusicPage { active: window.selectedApp === "music"; onBack: window.selectedApp = "" }
            VideoLibraryPage { library: videoLibrary; heading: "Video"; active: window.selectedApp === "video"; onBack: window.selectedApp = "" }
            WeatherPage { onBack: window.selectedApp = "" }
            RadioPage { objectName: "radioAppPage"; active: window.selectedApp === "radio" && !screenControl.sleeping; keyboardHeight: virtualKeyboard.item ? virtualKeyboard.item.visibleHeight : 0; onBack: window.selectedApp = "" }
            IptvPage { objectName: "iptvAppPage"; active: window.selectedApp === "iptv" && !screenControl.sleeping; keyboardHeight: virtualKeyboard.item ? virtualKeyboard.item.visibleHeight : 0; onBack: window.selectedApp = "" }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 96
            visible: !window.browsingTv && (home.modules.radio || home.modules.bluetooth || home.modules.navidrome || home.modules.jellyfin || home.modules.iptv)
            color: Theme.surface; border.color: Theme.border; radius: Theme.radius
            RowLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Rectangle {
                    width: 52; height: 52; radius: 14; color: Theme.tint
                    Text { visible: home.source === "radio" && home.radioId === "radio4"; anchors.centerIn: parent; text: "4"; color: Theme.accent; font.pixelSize: 36 }
                    UiIcon { visible: home.source !== "radio" || home.radioId !== "radio4"; anchors.centerIn: parent; name: home.source === "radio" ? "radio" : home.source === "bluetooth" ? "bluetooth" : (home.source === "iptv" ? "tv" : home.source === "jellyfin" ? "video" : "music"); tint: Theme.accent; width: 30; height: 30 }
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
    Popup {
        objectName: "alarmPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(window.width - 48, 560); padding: 28
        modal: true; closePolicy: Popup.NoAutoClose
        visible: home.alarmText.length > 0
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.accent }
        contentItem: ColumnLayout {
            spacing: 24
            UiIcon { name: "alarm"; tint: Theme.accent; Layout.preferredWidth: 54; Layout.preferredHeight: 54; Layout.alignment: Qt.AlignHCenter }
            Text { text: home.alarmText; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 28; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
            UiButton { objectName: "dismissAlarmButton"; text: "Dismiss"; primary: true; Layout.fillWidth: true; onClicked: home.dismissAlarm() }
        }
    }
    Loader {
        id: virtualKeyboard; active: home.virtualKeyboardEnabled
        Component.onCompleted: if(active)setSource("VirtualKeyboard.qml", {hostWindow: window})
    }
    LayoutSettingsPage {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 24
        height: Math.max(200, window.height - 48 - (virtualKeyboard.item ? virtualKeyboard.item.visibleHeight : 0))
        visible: window.selectedApp === "layout-settings"
        onBack: { Qt.inputMethod.hide(); window.selectedApp = "" }
    }
    PowerDialogs { hostWindow: window; keyboardHeight: virtualKeyboard.item ? virtualKeyboard.item.visibleHeight : 0 }
    Rectangle {
        anchors.fill: parent; z: 10000
        visible: screenControl.sleeping || screenControl.powerPending
        color: screenControl.sleeping ? "black" : Theme.background
        Text { anchors.centerIn: parent; text: screenControl.powerMessage; visible: screenControl.powerPending; color: Theme.ink; font.pixelSize: 32 }
    }
}
