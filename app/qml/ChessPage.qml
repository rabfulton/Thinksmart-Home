import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    property bool active: false
    readonly property bool wide: width > height
    readonly property real boardSize: Math.floor(Math.min(width - (wide ? 300 : 40), height - (wide ? 40 : chessGame.error.length ? 410 : 330)) / 8) * 8
    readonly property var cells: chessGame.squares
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    Binding { target: chessGame; property: "active"; value: page.active }
    function pieceName(code) { if(!code)return "empty"; const names={K:"king",Q:"queen",R:"rook",B:"bishop",N:"knight",P:"pawn"};return (code[0]==="w"?"White ":"Black ")+names[code[1]] }
    Connections {
        target: chessGame
        function onChanged() { if(chessGame.promotionPending)promotion.open();else promotion.close() }
    }
    GridLayout {
        anchors.centerIn: parent
        columns: page.wide ? 2 : 1
        columnSpacing: 28; rowSpacing: 20
        Rectangle {
            Layout.preferredWidth: page.boardSize + 4; Layout.preferredHeight: page.boardSize + 4
            color: Theme.border
            Grid {
                anchors.centerIn: parent; columns: 8
                Repeater {
                    model: 64
                    Rectangle {
                        id: cell
                        required property int index
                        readonly property int boardIndex: chessGame.playerWhite ? index : 63 - index
                        readonly property var entry: page.cells[boardIndex]
                        readonly property bool light: (Math.floor(index / 8) + index % 8) % 2 === 0
                        objectName: "chessSquare_" + entry.square
                        width: page.boardSize / 8; height: width
                        color: light ? Theme.chessLight : Theme.chessDark
                        Accessible.role: Accessible.Button
                        Accessible.name: entry.square + ": " + page.pieceName(entry.piece)
                        Rectangle { anchors.fill: parent; visible: cell.entry.last; color: Theme.accent; opacity: 0.28 }
                        Rectangle { anchors.fill: parent; visible: cell.entry.check; color: Theme.danger; opacity: 0.65 }
                        Image {
                            anchors.fill: parent; anchors.margins: parent.width * .07
                            source: cell.entry.piece ? "qrc:/assets/chess/" + cell.entry.piece + ".svg" : ""
                            sourceSize: Qt.size(Math.ceil(cell.width * 2),Math.ceil(cell.width * 2))
                            fillMode: Image.PreserveAspectFit
                        }
                        Rectangle {
                            visible: cell.entry.legal
                            anchors.centerIn: parent
                            width: parent.width * (cell.entry.piece ? .84 : .18); height: width; radius: width / 2
                            color: cell.entry.piece ? "transparent" : Theme.background
                            border.color: Theme.background; border.width: cell.entry.piece ? 2 : 0; opacity: 0.35
                        }
                        Rectangle { anchors.fill: parent; visible: cell.entry.selected; color: "transparent"; border.color: Theme.ink; border.width: 3 }
                        Text { visible: cell.index % 8 === 0; x: 3; y: 2; text: cell.entry.square[1]; color: cell.light ? Theme.chessDark : Theme.chessLight; font.pixelSize: Math.max(10,parent.width*.15); font.weight: Font.Medium }
                        Text { visible: cell.index >= 56; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 3; text: cell.entry.square[0]; color: cell.light ? Theme.chessDark : Theme.chessLight; font.pixelSize: Math.max(10,parent.width*.15); font.weight: Font.Medium }
                        MouseArea { anchors.fill: parent; onClicked: chessGame.selectSquare(cell.boardIndex) }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.preferredWidth: page.wide ? 230 : page.boardSize
            Layout.fillWidth: true; spacing: 14
            Text { text: chessGame.status; color: Theme.ink; font.pixelSize: 26; Layout.fillWidth: true; wrapMode: Text.WordWrap; horizontalAlignment: page.wide ? Text.AlignLeft : Text.AlignHCenter }
            Text { text: "You play " + (chessGame.playerWhite ? "White" : "Black") + "\n" + (chessGame.playerWhite ? "Black" : "White") + " · Arasan"; color: Theme.muted; font.pixelSize: 16; lineHeight: 1.25; Layout.fillWidth: true; horizontalAlignment: page.wide ? Text.AlignLeft : Text.AlignHCenter }
            RowLayout {
                spacing: 6
                Layout.alignment: page.wide ? Qt.AlignLeft : Qt.AlignHCenter
                UiButton { objectName: "chessRatingDownButton"; text: "−"; description: "Lower engine rating"; implicitWidth: 48; implicitHeight: 48; enabled: chessGame.rating > 1000; onClicked: chessGame.rating = Math.max(1000, Math.ceil(chessGame.rating / 100) * 100 - 100) }
                Text { text: chessGame.rating; color: Theme.ink; font.pixelSize: 19; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignHCenter }
                UiButton { objectName: "chessRatingUpButton"; text: "+"; description: "Raise engine rating"; implicitWidth: 48; implicitHeight: 48; enabled: chessGame.rating < 2400; onClicked: chessGame.rating = Math.min(2400, Math.floor(chessGame.rating / 100) * 100 + 100) }
            }
            Text { text: chessGame.lastMove; color: Theme.accent; font.pixelSize: 15; Layout.minimumHeight: 20; Layout.fillWidth: true; horizontalAlignment: page.wide ? Text.AlignLeft : Text.AlignHCenter }
            Text { text: chessGame.error; visible: text.length > 0; color: Theme.danger; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            GridLayout {
                columns: page.wide ? 1 : 3; columnSpacing: 12; rowSpacing: 12
                uniformCellWidths: true
                Layout.alignment: page.wide ? Qt.AlignLeft : Qt.AlignHCenter
                UiButton { objectName: "chessNewGameButton"; text: "New game"; Layout.fillWidth: true; onClicked: chessGame.moveCount > 0 ? newGameConfirm.open() : chessGame.newGame() }
                UiButton { objectName: "chessUndoButton"; text: "Undo"; Layout.fillWidth: true; enabled: chessGame.canUndo; onClicked: chessGame.undo() }
                UiButton { objectName: "chessSwitchSidesButton"; text: "Switch sides"; Layout.fillWidth: true; onClicked: chessGame.switchSides() }
            }
            UiButton { visible: chessGame.error.length > 0; text: "Retry"; enabled: !chessGame.busy; onClicked: chessGame.retry() }
        }
    }
    Popup {
        id: promotion; anchors.centerIn: parent; modal: true; padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: if(chessGame.promotionPending)chessGame.cancelPromotion()
        background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.radius }
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: "Promote pawn"; color: Theme.ink; font.pixelSize: 24 }
            Row {
                spacing: 10
                Repeater {
                    model: ["q","r","b","n"]
                    Button {
                        required property string modelData
                        objectName: "chessPromote_" + modelData
                        implicitWidth: 76; implicitHeight: 76
                        Accessible.name: "Promote to " + page.pieceName((chessGame.playerWhite ? "w" : "b") + modelData.toUpperCase())
                        background: Rectangle { radius: 12; color: Theme.chessDark; border.color: Theme.border }
                        contentItem: Image { source: "qrc:/assets/chess/" + (chessGame.playerWhite ? "w" : "b") + modelData.toUpperCase() + ".svg"; sourceSize.width: 144; sourceSize.height: 144; fillMode: Image.PreserveAspectFit }
                        onClicked: chessGame.promote(modelData)
                    }
                }
            }
        }
    }
    Popup {
        id: newGameConfirm; anchors.centerIn: parent; modal: true; padding: 24
        background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.radius }
        contentItem: ColumnLayout {
            spacing: 22
            Text { text: "Start a new game?"; color: Theme.ink; font.pixelSize: 25 }
            RowLayout {
                spacing: 12
                UiButton { text: "Cancel"; onClicked: newGameConfirm.close() }
                UiButton { objectName: "chessConfirmNewGameButton"; text: "New game"; primary: true; onClicked: { chessGame.newGame(); newGameConfirm.close() } }
            }
        }
    }
}
