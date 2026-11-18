import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    property real keyboardHeight: 0
    readonly property var markets: home.markets
    readonly property var quote: markets.selected
    readonly property bool wide: width > 900
    signal back()
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    function price(v) { return typeof v === "number" ? v.toLocaleString(Qt.locale("en_GB"), "f", 2) : "—" }
    function percent(q) { return typeof q.percent === "number" ? (q.percent >= 0 ? "+" : "") + q.percent.toFixed(2) + "%" : "—" }
    function endpointLabel(first) {
        if(!markets.points.length)return ""
        const p=markets.points[first?0:markets.points.length-1]
        return new Date(p.time*1000).toLocaleString(Qt.locale("en_GB"),markets.range==="1D"?"HH:mm":"d MMM")
    }
    function extreme(high) { const values=markets.points.map(p=>p.value);return values.length?price(high?Math.max.apply(null,values):Math.min.apply(null,values)):"" }
    function finishInput(item) { item.focus=false;Qt.inputMethod.hide() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            UiButton { objectName: "marketsBack"; iconName: "back"; onClicked: page.back() }
            Text { text: "Markets"; color: Theme.ink; font.pixelSize: 25 }
            Item { Layout.fillWidth: true }
            UiButton { objectName: "marketsMode"; text: markets.mode === "list" ? "Focus widget" : "List widget"; implicitHeight: 54; onClicked: markets.setMode(markets.mode === "list" ? "focus" : "list") }
            UiButton { objectName: "marketsEdit"; iconName: "settings"; onClicked: { symbolsEdit.text=markets.symbols.join(", ");watchlistDialog.open() } }
            UiButton { objectName: "marketsRefresh"; iconName: "refresh"; enabled: !markets.busy; onClicked: markets.refresh() }
        }
        Text { visible: markets.problem.length>0; text: markets.problem; color: Theme.danger; font.pixelSize: 15; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        GridLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; columns: page.wide ? 2 : 1; columnSpacing: 20; rowSpacing: 14
            ListView {
                id: quoteList; objectName: "marketsQuotes"
                Layout.fillWidth: true; Layout.preferredWidth: page.wide ? 310 : 1; Layout.maximumWidth: page.wide ? 350 : Infinity
                Layout.fillHeight: page.wide; Layout.preferredHeight: page.wide ? 400 : Math.min(300,contentHeight)
                clip: true; spacing: 6; model: markets.quotes
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: quoteList.width; height: 67; radius: 12
                    color: modelData.symbol===markets.focus ? Theme.tint : Theme.raised
                    border.color: modelData.symbol===markets.focus ? Theme.accent : "transparent"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 3
                            Text { text: modelData.label; color: Theme.ink; font.pixelSize: 19 }
                            Text { text: modelData.marketStatus + (modelData.cached ? " · cached" : modelData.stale && modelData.available ? " · old quote" : ""); color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                        ColumnLayout {
                            spacing: 3
                            Text { text: page.price(modelData.price); color: Theme.ink; font.pixelSize: 19; Layout.alignment: Qt.AlignRight }
                            Text { text: page.percent(modelData) + " " + (modelData.changePeriod || "day"); color: modelData.percent<0?Theme.danger:Theme.secondary; font.pixelSize: 13; Layout.alignment: Qt.AlignRight }
                        }
                    }
                    TapHandler { onTapped: markets.select(modelData.symbol) }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.label
                    objectName: "marketQuote"+index
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 3
                        Text { text: quote.name || markets.focus; color: Theme.muted; font.pixelSize: 18; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: page.price(quote.price) + " " + (quote.currency || ""); color: Theme.ink; font.pixelSize: 36; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: page.percent(quote) + " · " + (quote.changePeriod || "day") + " · " + (quote.marketStatus || "Unknown"); color: quote.percent<0?Theme.danger:Theme.secondary; font.pixelSize: 17 }
                    }
                    UiButton { objectName: "marketsAddAlert"; text: "Alert"; implicitHeight: 54; onClicked: { alertPrice.text=typeof quote.price==="number" ? quote.price.toFixed(2):"";alertDialog.open() } }
                }
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
                    MarketChart { objectName: "marketsChart"; anchors.fill: parent; points: markets.points; active: page.active; grid: true; lineColor: quote.percent<0?Theme.danger:Theme.secondary }
                    Text { anchors.top: parent.top; anchors.right: parent.right; text: page.extreme(true); color: Theme.faint; font.pixelSize: 12 }
                    Text { anchors.bottom: parent.bottom; anchors.right: parent.right; text: page.extreme(false); color: Theme.faint; font.pixelSize: 12 }
                    Text { visible: markets.points.length<2; anchors.centerIn: parent; text: markets.busy?"Loading chart…":"Chart unavailable"; color: Theme.faint; font.pixelSize: 17 }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: page.endpointLabel(true); color: Theme.faint; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                    Text { text: page.endpointLabel(false); color: Theme.faint; font.pixelSize: 12 }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: ["1D","1W","1M","1Y"]
                        UiButton { required property string modelData; objectName: "marketsRange"+modelData; text: modelData; selected: markets.range===modelData; Layout.fillWidth: true; implicitHeight: 48; onClicked: markets.setRange(modelData) }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Quote: " + (quote.quoteLabel || "unavailable") + (quote.regularSessionQuote ? " · regular session" : ""); color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    UiButton { objectName: "marketsAlerts"; text: "Alerts ("+markets.alerts.length+")"; implicitHeight: 44; onClicked: alertsDialog.open() }
                }
            }
        }
        Text { text: "Yahoo Finance · free feed · may be delayed · updates ~1 min while visible"; color: Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    component MarketDialog: Dialog {
        parent: Overlay.overlay; x: (parent.width-width)/2; width: Math.min(650,parent.width-48)
        y: Math.max(12,(parent.height-page.keyboardHeight-height)/2)
        modal: true; padding: 20
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        onClosed: Qt.inputMethod.hide()
    }
    MarketDialog {
        id: watchlistDialog; objectName: "marketsWatchlistDialog"
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Watchlist"; color: Theme.ink; font.pixelSize: 25 }
            Text { text: "1–12 symbols, in display order. Use commas or new lines.\nExamples: ^GSPC, DXY, NVDA, BTC, ETH, MSFT, VOD.L"; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextArea {
                id: symbolsEdit; objectName: "marketsSymbols"; Layout.fillWidth: true; implicitHeight: 100
                color: Theme.ink; font.pixelSize: 21; wrapMode: TextEdit.Wrap; selectByMouse: true
                Keys.onReturnPressed: event => { page.finishInput(symbolsEdit);event.accepted=true }
                Keys.onEnterPressed: event => { page.finishInput(symbolsEdit);event.accepted=true }
                inputMethodHints: Qt.ImhNoPredictiveText|Qt.ImhNoAutoUppercase
                background: Rectangle { color: Theme.raised; radius: 10; border.color: symbolsEdit.activeFocus?Theme.accent:Theme.border }
            }
            Text { visible: markets.problem.length>0; text: markets.problem; color: Theme.danger; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: watchlistDialog.close() }
                UiButton { objectName: "marketsSaveSymbols"; text: "Save"; primary: true; Layout.fillWidth: true; onClicked: if(markets.setSymbols(symbolsEdit.text)){page.finishInput(symbolsEdit);watchlistDialog.close()} }
            }
        }
    }
    MarketDialog {
        id: alertDialog; objectName: "marketsAlertDialog"
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Price alert · " + (quote.label || markets.focus); color: Theme.ink; font.pixelSize: 24 }
            RowLayout {
                Layout.fillWidth: true
                ComboBox { id: direction; objectName: "marketsAlertDirection"; model: ["Crosses above","Crosses below"]; Layout.fillWidth: true; implicitHeight: 58 }
                TextField { id: alertPrice; objectName: "marketsAlertPrice"; Layout.fillWidth: true; implicitHeight: 58; color: Theme.ink; font.pixelSize: 22; inputMethodHints: Qt.ImhFormattedNumbersOnly; onAccepted: page.finishInput(alertPrice); background: Rectangle { color: Theme.raised; radius: 10; border.color: Theme.border } }
            }
            CheckBox { id: chime; text: "Short chime"; contentItem: Text { text: chime.text; font.pixelSize: 18; color: Theme.ink; leftPadding: chime.indicator.width+8; verticalAlignment: Text.AlignVCenter } }
            Text { text: "One-shot crossing alert. Monitoring continues with the screen off. Quotes older than 3 minutes pause alerts; cached prices never trigger them. The first fresh quote sets the baseline."; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Text { visible: markets.problem.length>0; text: markets.problem; color: Theme.danger; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                UiButton { text: "Cancel"; Layout.fillWidth: true; onClicked: alertDialog.close() }
                UiButton { objectName: "marketsSaveAlert"; text: "Save alert"; primary: true; Layout.fillWidth: true; onClicked: if(markets.addAlert(markets.focus,direction.currentIndex===0?"above":"below",Number(alertPrice.text),chime.checked)){page.finishInput(alertPrice);alertDialog.close()} }
            }
        }
    }
    MarketDialog {
        id: alertsDialog; objectName: "marketsAlertsDialog"
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Price alerts"; color: Theme.ink; font.pixelSize: 25 }
            Text { visible: markets.alerts.length===0; text: "Choose a symbol, then tap Alert."; color: Theme.muted; font.pixelSize: 18 }
            ListView {
                id: alertList; Layout.fillWidth: true; Layout.preferredHeight: Math.min(420,contentHeight); clip: true; spacing: 8; model: markets.alerts
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    width: alertList.width; height: 72
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Text { text: modelData.symbol+" "+modelData.direction+" "+page.price(modelData.price); color: Theme.ink; font.pixelSize: 17; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: modelData.armed?"Armed · waiting for a fresh crossing":"Triggered"; color: Theme.faint; font.pixelSize: 13 }
                    }
                    UiButton { visible: !modelData.armed; text: "Rearm"; implicitHeight: 50; onClicked: markets.rearmAlert(modelData.id) }
                    UiButton { objectName: "marketsRemoveAlert"+index; iconName: "close"; implicitHeight: 50; onClicked: markets.removeAlert(modelData.id) }
                }
            }
            UiButton { text: "Done"; Layout.fillWidth: true; onClicked: alertsDialog.close() }
        }
    }
}
