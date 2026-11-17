#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float energy;
    float speaking;
    float listening;
    float thinking;
    float aspect;
    vec4 accentColor;
    vec4 secondaryColor;
    vec4 highlightColor;
};
const float PI = 3.14159265359;
float line(float distance, float width) {
    // Filter each strand using its screen-space pixel footprint. Thin lines
    // otherwise disappear between pixels and shimmer as the field rotates.
    // Preserve their integrated light when widening a subpixel strand.
    float pixel = fwidth(distance);
    float filteredWidth = sqrt(width * width + 0.25 * pixel * pixel);
    float d = distance / filteredWidth;
    return (width / filteredWidth) / (1.0 + d * d);
}
void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * 2.0;
    p.x *= aspect;
    float e = clamp(energy, 0.0, 1.0);
    float t = time;
    float r = length(p);
    float angle = atan(p.y, p.x);
    float radius = 0.72 + e * (0.055 + 0.018 * sin(angle * 3.0 - t * 1.3));
    float normalR = r / radius;
    float sphere = 1.0 - smoothstep(0.97, 1.015, normalR);
    float z = sqrt(max(0.0, 1.0 - normalR * normalR));
    vec3 n = vec3(p / radius, z);
    float turn = t * (0.19 + speaking * 0.13 + thinking * 0.15);
    float ct = cos(turn), st = sin(turn);
    vec3 q = vec3(n.x * ct + n.z * st, n.y, -n.x * st + n.z * ct);
    float fold = sin(q.x * 4.4 + q.y * 2.8 + t * 0.35) * 0.95
        + sin(q.z * 5.0 - q.y * 3.2 - t * 0.28) * 0.65;
    float spiral = atan(q.y, q.x) * 3.0 + q.z * 10.0;
    float ribbons = sin(spiral + fold * (2.0 + e * 0.8) + t * 0.48);
    float strands = line(ribbons, 0.075 + e * 0.012);
    float haloStrands = line(ribbons, 0.24) * 0.16;
    float crossing = line(sin(q.z * 12.0 - q.x * 6.0 + fold * 2.1 - t * 0.35), 0.060) * 0.38;
    float depth = 0.32 + z * 0.68;
    float body = sphere * ((strands * 1.15 + haloStrands + crossing * 0.4) * depth + 0.022);
    float edge = line(r - radius, 0.011) * (0.40 + e * 0.35);
    float corona = exp(-abs(r - radius) * 18.0) * (0.05 + e * 0.11);
    float breath = 0.96 + 0.04 * sin(t * 1.1);
    float centre = exp(-r * r * 13.0) * (0.045 + e * 0.08);

    // Tilted magnetic orbit, with a pair of travelling lights.
    vec2 orbit = vec2(p.x, (p.y + p.x * 0.20) / 0.31);
    float orbitR = length(orbit);
    float orbitAngle = atan(orbit.y, orbit.x);
    float orbitGlow = line(orbitR - 0.82, 0.006) * (0.15 + 0.10 * sin(orbitAngle));
    float orbitBack = smoothstep(-0.15, 0.30, p.y + p.x * 0.2);
    orbitGlow *= mix(0.35, 1.0, orbitBack);
    float orbitPhase = t * (0.35 + thinking * 0.8);
    vec2 tracer = vec2(cos(orbitPhase), sin(orbitPhase) * 0.31) * 0.82;
    tracer.y -= tracer.x * 0.2;
    float spark = exp(-dot(p - tracer, p - tracer) * 4200.0);

    // Speaking opens the field outwards; listening gathers it towards the core.
    float direction = mix(1.0, -1.0, listening);
    float ripplePhase = fract(r * 1.6 - t * 0.26 * direction);
    float ripple = line(ripplePhase - 0.5, 0.009) * smoothstep(0.66, 0.82, r)
        * (1.0 - smoothstep(0.86, 1.03, r)) * (0.12 + e * 0.45) * max(speaking, listening * 0.45);
    float arcRadius = 0.90 + e * 0.035;
    float arcMask = smoothstep(0.25, 0.70, sin(angle * 3.0 + t * 0.16));
    float arcs = line(r - arcRadius, 0.0026) * arcMask * (0.12 + thinking * 0.15);
    float haze = exp(-r * r * 4.0) * 0.035;
    float brightness = (body + edge + corona + centre) * breath;
    vec3 core = mix(accentColor.rgb, highlightColor.rgb, clamp(strands * z * 0.48, 0.0, 0.60));
    vec3 rgb = core * brightness + secondaryColor.rgb * (orbitGlow + ripple + arcs + crossing * sphere * depth * 0.55)
        + highlightColor.rgb * spark * 0.9 + accentColor.rgb * haze;
    float alpha = clamp(brightness + crossing * sphere * depth * 0.55 + orbitGlow + ripple + arcs + spark + haze, 0.0, 1.0);
    // RGB is already emitted light; keep the Qt Quick output premultiplied.
    fragColor = vec4(min(rgb, vec3(alpha)), alpha) * qt_Opacity;
}
