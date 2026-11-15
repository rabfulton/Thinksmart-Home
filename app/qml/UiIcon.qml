import QtQuick

Canvas {
    id: icon
    property string name: "arrow"
    property color tint: Theme.ink
    implicitWidth: 24; implicitHeight: 24
    onNameChanged: requestPaint()
    onTintChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset(); c.scale(width / 24, height / 24)
        c.strokeStyle = tint; c.fillStyle = tint; c.lineWidth = 1.7
        c.lineCap = "round"; c.lineJoin = "round"
        function line(x,y,a,b) { c.moveTo(x,y); c.lineTo(a,b) }
        c.beginPath()
        switch(name) {
        case "wifi": c.arc(12,20,17,-2.15,-0.99); c.moveTo(5,10); c.arc(12,20,12,-2.15,-0.99); c.moveTo(8,15); c.arc(12,20,7,-2.15,-0.99); c.moveTo(13,20); c.arc(12,20,1,0,Math.PI*2); break
        case "trash": line(4,6,20,6); line(9,3,15,3); line(6,6,7,21); line(7,21,17,21); line(17,21,18,6); line(10,10,10,17); line(14,10,14,17); break
        case "notes": c.rect(5,3,14,18); line(8,8,16,8); line(8,12,16,12); line(8,16,13,16); break
        case "alarm": c.arc(12,13,8,0,Math.PI*2); line(12,8,12,13); line(12,13,16,15); line(3,5,6,2); line(18,2,21,5); break
        case "bluetooth": line(12,2,12,22); line(12,2,19,8); line(19,8,5,18); line(5,6,19,16); line(19,16,12,22); break
        case "next": line(17,5,17,19); c.moveTo(5,5); c.lineTo(14,12); c.lineTo(5,19); c.closePath(); break
        case "previous": line(7,5,7,19); c.moveTo(19,5); c.lineTo(10,12); c.lineTo(19,19); c.closePath(); break
        case "moon": c.arc(12,12,8,0.35,5.1); c.quadraticCurveTo(7,9,12,4); break
        case "power": c.arc(12,13,8,-0.8,3.95); line(12,2,12,11); break
        case "settings":
            for (let i = 0; i < 32; i++) {
                const angle = -Math.PI / 2 + i * Math.PI / 16
                const radius = i % 4 < 2 ? 9.5 : 7.5
                const x = 12 + Math.cos(angle) * radius, y = 12 + Math.sin(angle) * radius
                if (i === 0) c.moveTo(x,y); else c.lineTo(x,y)
            }
            c.closePath(); c.moveTo(15,12); c.arc(12,12,3,0,Math.PI*2)
            break
        case "radio":
            c.lineWidth = 0.5
            c.moveTo(4,8); c.lineTo(20,8); c.quadraticCurveTo(22,8,22,10)
            c.lineTo(22,19); c.quadraticCurveTo(22,21,20,21)
            c.lineTo(4,21); c.quadraticCurveTo(2,21,2,19)
            c.lineTo(2,10); c.quadraticCurveTo(2,8,4,8)
            line(5,8,17,3)
            c.moveTo(11,15); c.arc(8,15,3,0,Math.PI*2)
            line(14,12,19,12); line(16,11,16,13); line(14,17,19,17)
            break
        case "music":
            c.lineWidth = 0.5
            line(8,18,8,6); line(8,6,20,3); line(8,9,20,6); line(20,3,20,15)
            c.moveTo(8,18); c.arc(5,18,3,0,Math.PI*2)
            c.moveTo(20,15); c.arc(17,15,3,0,Math.PI*2)
            break
        case "weather": c.moveTo(5,18); c.bezierCurveTo(-1,16,3,9,7,11); c.bezierCurveTo(7,3,18,3,18,11); c.bezierCurveTo(24,11,24,18,18,18); line(18,18,5,18); break
        case "solar": c.moveTo(16,7); c.arc(12,7,4,0,Math.PI*2); line(12,0,12,1); line(4,3,5,4); line(19,3,20,2); c.rect(3,14,18,8); line(9,14,9,22); line(15,14,15,22); line(3,18,21,18); break
        case "video":
            c.lineWidth = 0.5
            c.rect(2,3,20,5)
            line(6,3,3,8); line(11,3,8,8); line(16,3,13,8); line(21,3,18,8)
            c.moveTo(2,8); c.lineTo(2,19); c.quadraticCurveTo(2,21,4,21)
            c.lineTo(20,21); c.quadraticCurveTo(22,21,22,19); c.lineTo(22,8)
            line(5,13,19,13); line(5,17,11,17); line(15,17,19,17)
            break
        case "voice": line(3,10,3,14); line(7,6,7,18); line(12,3,12,21); line(17,6,17,18); line(21,10,21,14); break
        case "play": c.moveTo(7,4); c.lineTo(20,12); c.lineTo(7,20); c.closePath(); break
        case "pause": line(8,5,8,19); line(16,5,16,19); c.lineWidth = 3; break
        case "stop": c.rect(6,6,12,12); break
        case "volume": c.moveTo(3,9); c.lineTo(7,9); c.lineTo(12,4); c.lineTo(12,20); c.lineTo(7,15); c.lineTo(3,15); c.closePath(); c.moveTo(16,7); c.quadraticCurveTo(22,12,16,17); break
        case "back": line(16,5,9,12); line(9,12,16,19); break
        case "palette": c.moveTo(20,12); c.arc(12,12,9,0,Math.PI*2); c.moveTo(10,7); c.arc(9,7,1,0,Math.PI*2); c.moveTo(17,10); c.arc(16,10,1,0,Math.PI*2); c.moveTo(9,16); c.arc(8,16,1,0,Math.PI*2); break
        default: line(5,12,19,12); line(13,6,19,12); line(19,12,13,18)
        }
        c.stroke()
    }
}
