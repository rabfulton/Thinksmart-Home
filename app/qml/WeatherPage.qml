import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    required property var hostWindow
    property real keyboardHeight: 0
    signal back()
    onVisibleChanged: if(!visible)locationDialog.close()
    WeatherLocationDialog { id: locationDialog; hostWindow: page.hostWindow; keyboardHeight: page.keyboardHeight }
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function value(n, unit, decimals) { return typeof n === "number" && isFinite(n) ? n.toFixed(decimals || 0) + (unit || "") : "—" }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 18
        RowLayout {
            spacing: 20
            UiButton { iconName: "back"; description: "Back"; onClicked: page.back() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: weather.location; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 29; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: weather.status; color: weather.stale ? Theme.accent : Theme.muted; font.pixelSize: 15 }
            }
            UiButton { objectName: "weatherLocationButton"; text: "Location"; onClicked: locationDialog.open() }
            UiButton { text: "Refresh"; enabled: !weather.busy; onClicked: weather.refresh() }
        }
        Text { visible: weather.problem.length > 0; text: weather.problem; color: Theme.accent; font.pixelSize: 16; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ScrollView {
            id: scroll
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: scroll.availableWidth; spacing: 20
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 176; radius: 18; color: Theme.raised
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 22; spacing: 22
                        WeatherIcon { objectName: "weatherExpandedIcon"; weatherCode: weather.current.weather_code; isDay: weather.current.is_day; Layout.preferredWidth: 118; Layout.preferredHeight: 118; backgroundColor: Theme.raised }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: page.value(weather.current.temperature_2m,"°"); color: Theme.ink; font.pixelSize: 48 }
                            Text { text: weather.current.condition || "Waiting for forecast"; color: Theme.ink; font.pixelSize: 21 }
                            Text { text: "Feels like " + page.value(weather.current.apparent_temperature,"°C"); color: Theme.muted; font.pixelSize: 16 }
                        }
                        ColumnLayout {
                            spacing: 6
                            Text { text: "Wind  " + page.value(weather.current.wind_speed_10m," mph"); color: Theme.ink; font.pixelSize: 19 }
                            Text { text: "Gusts  " + page.value(weather.current.wind_gusts_10m," mph"); color: Theme.muted; font.pixelSize: 17 }
                            Text { text: "Humidity  " + page.value(weather.current.relative_humidity_2m,"%"); color: Theme.muted; font.pixelSize: 17 }
                            Text { objectName: "weatherSunrise"; text: "Sunrise  " + (weather.daily.length ? weather.daily[0].sunriseLabel : "—"); color: Theme.ink; font.pixelSize: 17 }
                            Text { objectName: "weatherSunset"; text: "Sunset  " + (weather.daily.length ? weather.daily[0].sunsetLabel : "—"); color: Theme.ink; font.pixelSize: 17 }
                        }
                    }
                }
                Text { text: "The next 12 hours"; color: Theme.ink; font.pixelSize: 23 }
                ListView {
                    Layout.fillWidth: true; Layout.preferredHeight: 200
                    orientation: ListView.Horizontal; spacing: 10; clip: true; model: weather.hourly
                    ScrollBar.horizontal: ScrollBar { }
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: 112; height: 188; radius: 14; color: Theme.raised
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 7
                            Text { text: modelData.label; color: Theme.muted; font.pixelSize: 16 }
                            WeatherIcon { objectName: "weatherHourIcon"+index; weatherCode: modelData.weather_code; isDay: modelData.is_day; Layout.preferredWidth: 42; Layout.preferredHeight: 42; backgroundColor: Theme.raised }
                            Text { text: page.value(modelData.temperature_2m,"°"); color: Theme.ink; font.pixelSize: 29 }
                            Text { text: page.value(modelData.precipitation_probability,"% rain"); color: Theme.accent; font.pixelSize: 14 }
                            Text { text: page.value(modelData.wind_speed_10m," mph"); color: Theme.faint; font.pixelSize: 13 }
                        }
                    }
                }
                Text { text: "The next few days"; color: Theme.ink; font.pixelSize: 23 }
                Repeater {
                    model: weather.daily
                    Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true; implicitHeight: page.width > 900 ? 80 : 122; radius: 14; color: Theme.raised
                        GridLayout {
                            anchors.fill: parent; anchors.margins: 14; columns: page.width > 900 ? 4 : 2; columnSpacing: 18
                            RowLayout {
                                Layout.preferredWidth: 200; spacing: 10
                                WeatherIcon { objectName: "weatherDayIcon"+index; weatherCode: modelData.weather_code; Layout.preferredWidth: 42; Layout.preferredHeight: 42; backgroundColor: Theme.raised }
                                Text { text: modelData.label; color: Theme.ink; font.pixelSize: 19 }
                            }
                            Text { text: modelData.condition; color: Theme.muted; font.pixelSize: 18; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: page.value(modelData.temperature_2m_max,"°") + " / " + page.value(modelData.temperature_2m_min,"°"); color: Theme.ink; font.pixelSize: 22; Layout.preferredWidth: 115 }
                            Text { text: page.value(modelData.precipitation_probability_max,"% rain") + " · " + page.value(modelData.precipitation_sum," mm",1); color: Theme.accent; font.pixelSize: 16 }
                        }
                    }
                }
                Text { text: "Forecast data: Open-Meteo.com · CC BY 4.0\n" + weather.location + " · °C · mph · rain in mm"; color: Theme.faint; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
