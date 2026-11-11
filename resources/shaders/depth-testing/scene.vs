#version 330 core
// 属性布局须与 CPU 的 position/uv 一致；UV 会插值到片元阶段。
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
out vec2 texCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoord = uv;
    // 先模型、再观察、最后透视投影；此处输出不是窗口深度，而是齐次裁剪坐标。
    gl_Position = projection * view * model * vec4(position, 1.0);
}
