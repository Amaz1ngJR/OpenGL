#version 330 core
// 属性位置对应 CPU 的 position/uv，描边时 CPU 只改变 model 来放大几何体。
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
out vec2 texCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoord = uv;
    // MVP 按右到左作用，得到齐次裁剪坐标；透视除法由 GPU 自动进行。
    gl_Position = projection * view * model * vec4(position, 1.0);
}
