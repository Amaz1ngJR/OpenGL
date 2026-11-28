#version 330 core
in vec3 worldPosition;
uniform bool radial;
uniform vec3 pointPosition;
uniform float pointFar;

void main() {
    // Cubemap 的六个面统一保存归一化径向距离，避免相邻面的深度定义不同。
    float depth = radial ? length(worldPosition - pointPosition) / pointFar : gl_FragCoord.z;
    // fwidth measures the depth change across a shadow texel on this triangle,
    // including radial depth. Cap the offset to preserve contact at silhouettes.
    gl_FragDepth = min(1.0, depth + min(fwidth(depth), radial ? 0.001 : 0.0004));
}
