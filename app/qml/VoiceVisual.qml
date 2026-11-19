import QtQuick

// A small procedural GPU field. The Canvas fallback keeps software rendering
// usable; no bitmap assets or extra audio capture are required.
Item {
    id: visual
    property string mode: "ready"
    property bool active: true
    property real level: -1
    property real phase: 2.1
    property real shaderPhase: 2.1
    readonly property bool cachedIdle: mode === "ready" && idleMotion
    property bool animate: true
    property bool idleMotion: false
    readonly property bool gpu: GraphicsInfo.api === GraphicsInfo.OpenGL || GraphicsInfo.api === GraphicsInfo.Vulkan
    readonly property bool moving: active && visible && animate && (mode !== "ready" || idleMotion)
    // Cache the GPU field between updates. Parent page transforms can then move
    // a texture without rerunning the fragment shader while animation is paused.
    layer.enabled: gpu
    layer.smooth: true
    // Idle motion transforms a cached texture. Only conversations redraw the
    // procedural field; a slow rotation does not need a new shader evaluation.
    rotation: cachedIdle ? ((phase - 2.1) * 6) % 360 : 0
    readonly property real targetLevel: !active ? 0 : level >= 0 ? Math.max(0, Math.min(1, level)) : mode === "speaking" ? 0.45 + 0.22 * Math.sin(phase * 6) : 0.12
    property real envelope: 0
    readonly property color phosphor: Theme.accent
    implicitWidth: 320; implicitHeight: 320
    onTargetLevelChanged: if(cachedIdle || !moving) envelope = targetLevel
    onMovingChanged: if(cachedIdle || !moving) envelope = targetLevel
    Timer {
        interval: visual.cachedIdle ? 100 : 33; repeat: true; running: visual.moving
        onTriggered: {
            visual.phase += interval / 1000 * (visual.mode === "ready" ? 0.3 : 1)
            if(!visual.cachedIdle) {
                visual.shaderPhase += interval / 1000
                visual.envelope += (visual.targetLevel - visual.envelope) * (visual.targetLevel > visual.envelope ? 0.55 : 0.16)
                if(fallback.visible)fallback.requestPaint()
            }
        }
    }
    onEnvelopeChanged: if(fallback.visible)fallback.requestPaint()
    onModeChanged: { if(cachedIdle)envelope=targetLevel; if(fallback.visible)fallback.requestPaint() }
    onPhosphorChanged: if(fallback.visible)fallback.requestPaint()
    Loader {
        id: field
        anchors.fill: parent; active: visual.gpu
        sourceComponent: ShaderEffect {
            objectName: "assistantShader"
            readonly property real time: visual.shaderPhase
            readonly property real energy: visual.envelope
            property real speaking: visual.mode === "speaking" || visual.mode === "music" ? 1 : 0
            property real listening: visual.mode === "listening" ? 1 : 0
            property real thinking: visual.mode === "thinking" ? 1 : 0
            readonly property real aspect: width / Math.max(1, height)
            readonly property color accentColor: Theme.accent
            readonly property color secondaryColor: Theme.secondary
            readonly property color highlightColor: Theme.ink
            Behavior on speaking { NumberAnimation { duration: 220 } }
            Behavior on listening { NumberAnimation { duration: 220 } }
            Behavior on thinking { NumberAnimation { duration: 220 } }
            fragmentShader: "qrc:/shaders/voice-core.frag.qsb"
        }
    }
    Canvas {
        id: fallback
        anchors.fill: parent
        visible: !visual.gpu || (field.item && field.item.status === ShaderEffect.Error)
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: requestPaint()
        onPaint: {
            const c = getContext("2d"); c.reset()
            const cx = width / 2, cy = height / 2, size = height / 2
            const e = visual.envelope, r = size * (0.72 + e * 0.055), t = visual.shaderPhase
            function ink(col, a) { return Qt.rgba(col.r, col.g, col.b, a) }
            const glow = c.createRadialGradient(cx,cy,r*0.1,cx,cy,r*1.55)
            glow.addColorStop(0,ink(Theme.accent,0.08+e*0.08));glow.addColorStop(0.65,ink(Theme.accent,0.05));glow.addColorStop(1,ink(Theme.accent,0))
            c.fillStyle=glow;c.fillRect(0,0,width,height)
            c.save();c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.clip()
            for(let j=-9;j<=9;++j){
                c.beginPath()
                for(let k=0;k<=64;++k){
                    const x=-r+k*r/32, z=Math.sqrt(Math.max(0,1-(x/r)*(x/r)))
                    const y=j*r/11+Math.sin(x/r*3+t*0.32+j*0.18)*r*0.12*z+Math.sin(x/r*5-t*0.2)*e*r*0.045
                    if(k===0)c.moveTo(cx+x,cy+y);else c.lineTo(cx+x,cy+y)
                }
                c.strokeStyle=ink(j%3===0?Theme.ink:Theme.accent,0.62+e*0.22);c.lineWidth=j%3===0?1.5:1.1;c.stroke()
            }
            c.restore();c.strokeStyle=ink(Theme.accent,0.65);c.lineWidth=1.2;c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.stroke()
            c.strokeStyle=ink(Theme.secondary,0.5);c.beginPath()
            for(let k=0;k<=100;k++){
                const a=k*Math.PI/50,x=Math.cos(a)*size*0.82,y=Math.sin(a)*size*0.82*0.31-x*0.2
                if(k===0)c.moveTo(cx+x,cy+y);else c.lineTo(cx+x,cy+y)
            }
            c.stroke()
            if(visual.mode!=="ready"){
                const a=t*(visual.mode==="thinking"?1.15:0.35),x=Math.cos(a)*size*0.82,y=Math.sin(a)*size*0.82*0.31-x*0.2
                c.fillStyle=Theme.ink;c.beginPath();c.arc(cx+x,cy+y,2.5+e*2,0,Math.PI*2);c.fill()
            }
        }
    }
}
