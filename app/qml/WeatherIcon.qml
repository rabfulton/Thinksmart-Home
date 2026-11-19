import QtQuick

Item {
    id: icon
    property var weatherCode
    property var isDay: 1
    property color cloudColor: Theme.ink
    property color detailColor: Theme.accent
    property color backgroundColor: Theme.surface
    readonly property string kind: {
        const c = weatherCode
        if(typeof c !== "number" || !isFinite(c) || Math.floor(c) !== c)return "unknown"
        if(c === 0 || c === 1)return isDay === 0 ? "moon" : "sun"
        if(c === 2)return isDay === 0 ? "partly-night" : "partly"
        if(c === 3)return "cloud"
        if(c === 45 || c === 48)return "fog"
        if([51,53,55,56,57,61,63,65,66,67,80,81,82].indexOf(c) >= 0)return "rain"
        if([71,73,75,77,85,86].indexOf(c) >= 0)return "snow"
        if([95,96,99].indexOf(c) >= 0)return "thunder"
        return "unknown"
    }
    implicitWidth: 64; implicitHeight: 64
    onKindChanged: drawing.requestPaint()
    onCloudColorChanged: drawing.requestPaint()
    onDetailColorChanged: drawing.requestPaint()
    onBackgroundColorChanged: drawing.requestPaint()
    Canvas {
        id: drawing
        width: Math.max(1,parent.width*2); height: Math.max(1,parent.height*2)
        scale: 0.5; transformOrigin: Item.TopLeft
        smooth: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset();c.clearRect(0,0,canvasSize.width,canvasSize.height)
            const scale = Math.min(canvasSize.width,canvasSize.height)/64
            c.translate((canvasSize.width-64*scale)/2,(canvasSize.height-64*scale)/2);c.scale(scale,scale)
            c.lineWidth=2.4;c.lineCap="round";c.lineJoin="round"
            function line(x,y,x2,y2) { c.beginPath();c.moveTo(x,y);c.lineTo(x2,y2);c.stroke() }
            function sun(x,y,r) {
                c.strokeStyle=icon.detailColor;c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.stroke()
                for(let i=0;i<8;i++){const a=i*Math.PI/4;line(x+Math.cos(a)*(r+5),y+Math.sin(a)*(r+5),x+Math.cos(a)*(r+9),y+Math.sin(a)*(r+9))}
            }
            function moon(x,y) {
                c.strokeStyle=icon.detailColor;c.beginPath();c.moveTo(x+8,y-14);c.bezierCurveTo(x-16,y-17,x-18,y+15,x+5,y+16);c.bezierCurveTo(x+16,y+16,x+20,y+7,x+20,y+3);c.bezierCurveTo(x+5,y+8,x-2,y-4,x+8,y-14);c.stroke()
            }
            function cloud() {
                c.strokeStyle=icon.cloudColor;c.fillStyle=icon.backgroundColor;c.beginPath();c.moveTo(17,43);c.bezierCurveTo(1,43,6,24,20,28);c.bezierCurveTo(23,9,49,13,49,29);c.bezierCurveTo(64,29,64,44,50,44);c.lineTo(17,44);c.closePath();c.fill();c.stroke()
            }
            const k=icon.kind
            if(k === "sun")sun(32,31,12)
            else if(k === "moon")moon(29,28)
            else if(k === "unknown") { c.strokeStyle=icon.cloudColor;c.beginPath();c.arc(32,32,18,0,2*Math.PI);c.stroke();line(25,32,39,32) }
            else {
                if(k === "partly")sun(23,21,10)
                if(k === "partly-night")moon(21,20)
                cloud();c.strokeStyle=icon.detailColor
                if(k === "rain")for(let i=0;i<3;i++)line(22+i*12,51,18+i*12,59)
                if(k === "fog"){line(12,51,52,51);line(18,59,46,59)}
                if(k === "snow")for(let i=0;i<3;i++){const x=18+i*14;line(x,50,x,60);line(x-4,52,x+4,58);line(x-4,58,x+4,52)}
                if(k === "thunder"){c.beginPath();c.moveTo(34,47);c.lineTo(26,55);c.lineTo(34,55);c.lineTo(29,63);c.stroke()}
            }
        }
    }
}
