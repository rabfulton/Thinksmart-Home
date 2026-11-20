import QtQuick
import QtQuick.Layouts

GridLayout {
    id: deck
    required property var hostWindow
    required property var widgets
    property bool pageActive: false
    property bool transitioning: false
    columns: hostWindow.wide ? 3 : 2
    rowSpacing: Theme.gap; columnSpacing: Theme.gap
    readonly property var available: widgets.filter(id => id === "energy" ? home.modules.vrm : id === "music" ? home.modules.navidrome || home.modules.bluetooth : id === "video" ? home.modules.jellyfin : home.modules[id])
    component HomeTile: WidgetTile {}
    component SmallLabel: Text { color: Theme.muted; font.pixelSize: 17; elide: Text.ElideRight }
    component BigValue: Text { color: Theme.ink; font.pixelSize: 48; font.weight: Font.Medium }
    Repeater {
        model: deck.available
        delegate: Loader {
            required property string modelData
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; Layout.preferredHeight: 1; Layout.minimumHeight: 180
            sourceComponent: modelData === "assistant" ? assistantWidget : modelData === "weather" ? weatherWidget : modelData === "energy" ? energyWidget : modelData === "radio" ? radioWidget : modelData === "music" ? musicWidget : modelData === "video" ? videoWidget : modelData === "iptv" ? iptvWidget : modelData === "markets" ? marketsWidget : modelData === "audiobooks" ? audiobooksWidget : modelData === "tvarchive" ? tvArchiveWidget : modelData === "recipes" ? recipesWidget : null
        }
    }
    // Retain six grid slots so a single launcher remains a card, rather than
    // stretching to fill the whole page.
    Repeater {
        model: deck.available.length ? Math.max(0, 6 - deck.available.length) : 0
        Item { Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; Layout.preferredHeight: 1; Layout.minimumHeight: 180 }
    }
    Text { visible: deck.available.length === 0; text: "Add widgets in Settings → Pages & modules"; color: Theme.muted; font.pixelSize: 22; Layout.fillWidth: true; Layout.fillHeight: true; Layout.columnSpan: deck.columns; Layout.rowSpan: deck.hostWindow.wide ? 2 : 3; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap }
    Component { id: recipesWidget; RecipesWidget { objectName: "recipesTile"; onClicked: deck.hostWindow.selectedApp="recipes" } }
    Component { id: tvArchiveWidget; TvArchiveWidget { objectName: "tvArchiveTile"; running: deck.pageActive && !deck.transitioning && !screenControl.sleeping; onClicked: deck.hostWindow.selectedApp="tvarchive" } }
    Component { id: audiobooksWidget; AudiobooksWidget { objectName: "audiobooksTile"; running: deck.pageActive && !deck.transitioning && !screenControl.sleeping; onClicked: deck.hostWindow.selectedApp="audiobooks" } }
    Component { id: marketsWidget; MarketsWidget { objectName: "marketsTile"; running: deck.pageActive && !deck.transitioning && !screenControl.sleeping; onClicked: deck.hostWindow.selectedApp="markets" } }
    Component {
        id: iptvWidget
        HomeTile {
            objectName: "iptvTile"; heading: "TV"; indexLabel: "LIVE TV & SAVED CHANNELS"
            onClicked: deck.hostWindow.selectedApp = "iptv"
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 82
                    UiIcon { anchors.verticalCenter: parent.verticalCenter; name: "tv"; tint: Theme.accent; width: 100; height: 100 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.topMargin: 24; spacing: 12
                    Text { text: home.iptv.favourites.length ? home.iptv.favourites.length + " saved channels" : "Live channels"; color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; elide: Text.ElideRight }
                    SmallLabel { text: "News · Business · Documentaries"; Layout.fillWidth: true }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
    Component {
        id: assistantWidget
        HomeTile {
            objectName: "assistantTile"; visible: home.modules.assistant; heading: "Sheila"; iconName: "voice"; indexLabel: assistant.enabled ? "YOUR EVERYDAY COMPANION" : "ASSISTANT OFF"
            onClicked: deck.hostWindow.selectedApp = "assistant"
            VoiceVisual {
                objectName: "dashboardAssistantVisual"
                anchors.right: parent.right; anchors.rightMargin: -8
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - systemIndicators.width - 4)
                height: Math.min(parent.height * 1.25, width * 1.05)
                mode: "ready"; idleMotion: true
                active: home.modules.assistant && assistant.enabled && deck.pageActive && !screenControl.sleeping
                animate: !deck.transitioning
            }
            SystemIndicators {
                id: systemIndicators; objectName: "systemIndicators"
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                width: 84; stats: home.systemStats
            }
        }
    }
    Component {
        id: weatherWidget
        HomeTile {
            objectName: "weatherTile"; visible: home.modules.weather; heading: weather.location; iconName: "weather"
            badge: weather.stale ? "Stale" : ""; badgeColor: Theme.accent; indexLabel: weather.status.toUpperCase()
            onClicked: deck.hostWindow.selectedApp = "weather"
            ColumnLayout {
                anchors.fill: parent; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    BigValue { objectName: "weatherTemperature"; text: deck.hostWindow.weatherValue(weather.current.temperature_2m,"°") }
                    WeatherIcon { objectName: "weatherCurrentIcon"; weatherCode: weather.current.weather_code; isDay: weather.current.is_day; Layout.preferredWidth: 62; Layout.preferredHeight: 62 }
                    Item { Layout.fillWidth: true; Layout.minimumWidth: 4 }
                    ColumnLayout {
                        objectName: "weatherSummary"; Layout.preferredWidth: 144; Layout.maximumWidth: 160; spacing: 3
                        Text { text: weather.current.condition || "Forecast unavailable"; color: Theme.ink; font.pixelSize: 17; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: "Sunrise  " + (weather.daily.length ? weather.daily[0].sunriseLabel : "—"); color: Theme.muted; font.pixelSize: 15 }
                        Text { text: "Sunset   " + (weather.daily.length ? weather.daily[0].sunsetLabel : "—"); color: Theme.muted; font.pixelSize: 15 }
                    }
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    objectName: "weatherHourStrip"; Layout.fillWidth: true; spacing: 12
                    Repeater {
                        model: weather.hourly.slice(0,5)
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumWidth: 0; spacing: 3
                            Text { text: modelData.label; color: Theme.muted; font.pixelSize: 16; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                            WeatherIcon { weatherCode: modelData.weather_code; isDay: modelData.is_day; Layout.preferredWidth: 34; Layout.preferredHeight: 34; Layout.alignment: Qt.AlignHCenter }
                            Text { text: deck.hostWindow.weatherValue(modelData.temperature_2m,"°"); color: Theme.ink; font.pixelSize: 23; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                        }
                    }
                }
            }
        }
    }
    Component {
        id: energyWidget
        HomeTile {
            objectName: "energyTile"; visible: home.modules.vrm; heading: "Energy"; iconName: "solar"
            badge: energy.stale ? "Stale" : (energy.hasData ? "VRM" : "")
            badgeColor: energy.stale ? Theme.accent : Theme.faint
            indexLabel: energy.hasData ? energy.metrics.soc.age.toUpperCase() : "SOLAR & BATTERY"
            onClicked: deck.hostWindow.selectedApp = "solar"
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
    }
    Component {
        id: radioWidget
        HomeTile {
            objectName: "radioTile"; visible: home.modules.radio; heading: "Radio"; iconName: "radio"; indexLabel: "NEWS, SPEECH & LIVE STREAMS"
            onClicked: deck.hostWindow.selectedApp = "radio"
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
    }
    Component {
        id: musicWidget
        HomeTile {
            objectName: "musicTile"; visible: home.modules.navidrome || home.modules.bluetooth; heading: "Music"; iconName: "music"; indexLabel: "YOUR COLLECTION"
            onClicked: deck.hostWindow.selectedApp = "music"
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
    }
    Component {
        id: videoWidget
        HomeTile {
            objectName: "videoTile"; visible: home.modules.jellyfin; heading: "Video"; iconName: "video"; indexLabel: "FILMS & SERIES"
            onClicked: deck.hostWindow.selectedApp = "video"
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
