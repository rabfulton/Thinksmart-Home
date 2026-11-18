import QtQuick

Canvas {
    id: chart
    property var points: []
    property color lineColor: Theme.accent
    property bool grid: false
    property bool active: true
    onPointsChanged: if(active)requestPaint()
    onLineColorChanged: if(active)requestPaint()
    onActiveChanged: if(active)requestPaint()
    onWidthChanged: if(active)requestPaint()
    onHeightChanged: if(active)requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if(points.length < 2)return
        let low = Infinity, high = -Infinity
        for(const p of points) { low = Math.min(low, p.value); high = Math.max(high, p.value) }
        const span = Math.max(high-low, Math.abs(high)*0.0001, 0.01)
        const inset = grid ? 16 : 3
        const y = v => height-inset-(v-low)/span*(height-2*inset)
        const x = i => inset+i/(points.length-1)*(width-2*inset)
        if(grid) {
            ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
            for(let i=0;i<4;i++){const gy=inset+i*(height-2*inset)/3;ctx.beginPath();ctx.moveTo(inset,gy);ctx.lineTo(width-inset,gy);ctx.stroke()}
        }
        ctx.beginPath();ctx.moveTo(x(0),height)
        for(let i=0;i<points.length;i++)ctx.lineTo(x(i),y(points[i].value))
        ctx.lineTo(x(points.length-1),height);ctx.closePath()
        const fill=ctx.createLinearGradient(0,0,0,height)
        fill.addColorStop(0,Qt.rgba(lineColor.r,lineColor.g,lineColor.b,0.20));fill.addColorStop(1,Qt.rgba(lineColor.r,lineColor.g,lineColor.b,0))
        ctx.fillStyle=fill;ctx.fill()
        ctx.beginPath()
        for(let i=0;i<points.length;i++){if(i===0)ctx.moveTo(x(i),y(points[i].value));else ctx.lineTo(x(i),y(points[i].value))}
        ctx.strokeStyle=lineColor;ctx.lineWidth=grid?2.2:1.6;ctx.lineJoin="round";ctx.stroke()
    }
}
