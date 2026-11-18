import QtQuick
import QtQuick.Layouts

WidgetTile {
    id: card
    property bool running: false
    readonly property var markets: home.markets
    heading: "Markets"
    badge: markets.mode === "focus" ? markets.selected.label || "" : "USD"
    indexLabel: "FREE FEED · MAY BE DELAYED"
    function price(v) { return typeof v === "number" ? v.toLocaleString(Qt.locale("en_GB"), "f", 2) : "—" }
    function change(q) { return typeof q.percent === "number" ? (q.percent >= 0 ? "+" : "") + q.percent.toFixed(2) + "%" : "—" }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        visible: card.markets.mode === "list"
        Repeater {
            model: card.markets.quotes.slice(0,5)
            RowLayout {
                required property var modelData
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
                Text { text: modelData.label; color: Theme.muted; font.pixelSize: 16; Layout.preferredWidth: 83; elide: Text.ElideRight }
                Text { text: card.price(modelData.price); color: Theme.ink; font.pixelSize: 19; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                Text { text: card.change(modelData); color: modelData.percent < 0 ? Theme.danger : Theme.secondary; font.pixelSize: 14; Layout.preferredWidth: 73; horizontalAlignment: Text.AlignRight }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 4
        visible: card.markets.mode === "focus"
        Text { text: card.price(card.markets.selected.price); color: Theme.ink; font.pixelSize: 36; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: card.change(card.markets.selected) + " · " + (card.markets.selected.changePeriod || "day") + " · " + (card.markets.selected.marketStatus || "Unknown"); color: Theme.muted; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
        MarketChart { Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 30; points: card.markets.points; active: card.running && visible; lineColor: card.markets.selected.percent < 0 ? Theme.danger : Theme.secondary }
        Text { text: card.markets.range + " · " + (card.markets.selected.quoteLabel || "Waiting for quotes"); color: Theme.faint; font.pixelSize: 12 }
    }
}
