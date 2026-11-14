import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: page
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function value(n, unit, decimals) { return typeof n === "number" && isFinite(n) ? n.toFixed(decimals || 0) + (unit || "") : "—" }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 26; spacing: 18
        RowLayout {
            spacing: 20
            UiButton { iconName: "back"; description: "Back"; onClicked: page.back() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: "Weather · " + weather.location; color: Theme.ink; font.pixelSize: 29 }
                Text { text: weather.status; color: weather.stale ? Theme.accent : Theme.muted; font.pixelSize: 15 }
            }
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
                    Layout.fillWidth: true; implicitHeight: 150; radius: 18; color: Theme.raised
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 22; spacing: 22
                        UiIcon { name: "weather"; width: 44; height: 44; tint: Theme.accent }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: page.value(weather.current.temperature_2m,"°"); color: Theme.ink; font.pixelSize: 48 }
                            Text { text: weather.current.condition || "Waiting for forecast"; color: Theme.ink; font.pixelSize: 21 }
                            Text { text: "Feels like " + page.value(weather.current.apparent_temperature,"°C"); color: Theme.muted; font.pixelSize: 16 }
                        }
                        ColumnLayout {
                            spacing: 10
                            Text { text: "Wind  " + page.value(weather.current.wind_speed_10m," mph"); color: Theme.ink; font.pixelSize: 19 }
                            Text { text: "Gusts  " + page.value(weather.current.wind_gusts_10m," mph"); color: Theme.muted; font.pixelSize: 17 }
                            Text { text: "Humidity  " + page.value(weather.current.relative_humidity_2m,"%"); color: Theme.muted; font.pixelSize: 17 }
                        }
                    }
                }
                Text { text: "The next 12 hours"; color: Theme.ink; font.pixelSize: 23 }
                ListView {
                    Layout.fillWidth: true; Layout.preferredHeight: 145
                    orientation: ListView.Horizontal; spacing: 10; clip: true; model: weather.hourly
                    ScrollBar.horizontal: ScrollBar { }
                    delegate: Rectangle {
                        required property var modelData
                        width: 112; height: 132; radius: 14; color: Theme.raised
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 7
                            Text { text: modelData.label; color: Theme.muted; font.pixelSize: 16 }
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
                        Layout.fillWidth: true; implicitHeight: page.width > 900 ? 74 : 100; radius: 14; color: Theme.raised
                        GridLayout {
                            anchors.fill: parent; anchors.margins: 14; columns: page.width > 900 ? 4 : 2; columnSpacing: 18
                            Text { text: modelData.label; color: Theme.ink; font.pixelSize: 20; Layout.preferredWidth: 150 }
                            Text { text: modelData.condition; color: Theme.muted; font.pixelSize: 18; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: page.value(modelData.temperature_2m_max,"°") + " / " + page.value(modelData.temperature_2m_min,"°"); color: Theme.ink; font.pixelSize: 22; Layout.preferredWidth: 115 }
                            Text { text: page.value(modelData.precipitation_probability_max,"% rain") + " · " + page.value(modelData.precipitation_sum," mm",1); color: Theme.accent; font.pixelSize: 16 }
                        }
                    }
                }
                Text {
                    visible: weather.daily.length > 0
                    text: weather.daily.length ? "Sunrise " + weather.daily[0].sunriseLabel + "  ·  Sunset " + weather.daily[0].sunsetLabel : ""
                    color: Theme.muted; font.pixelSize: 17
                }
                Text { text: "Forecast data: Open-Meteo.com · CC BY 4.0\n" + weather.location + " · °C · mph · rain in mm"; color: Theme.faint; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
