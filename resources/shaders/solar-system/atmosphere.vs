#version 330 core
// 大气层 / 云层外壳的顶点着色器。
//
// 几何是一层比行星本体略大的球。除了位置和法线，还需要知道片元相对观察者的
// 朝向，用来在边缘处做菲涅尔式的增亮（真实大气在轮廓边缘最厚、最亮）。
layout(location = 0) in vec3 position;   // 单位球位置
layout(location = 1) in vec3 normal;     // 单位球上位置即法线
layout(location = 2) in vec2 uv;         // 等距柱状 UV

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
// 相机世界位置，片元着色器用它算视线方向（菲涅尔项需要）。
uniform vec3 cameraPosition;

out vec3 worldNormal;
out vec3 worldPosition;
out vec3 viewDirection;
out vec2 texCoord;

void main() {
    vec4 world = model * vec4(position, 1.0);
    worldPosition = world.xyz;
    // 视线方向从顶点指向相机；插值到片元后仍然近似正确（球面很大，误差可忽略）。
    viewDirection = cameraPosition - world.xyz;
    // 等比缩放时法线用 model 的 3x3 部分即可；这里保持与 body.vs 一致的通用写法。
    worldNormal = mat3(transpose(inverse(model))) * normal;
    texCoord = uv;
    gl_Position = projection * view * world;
}
