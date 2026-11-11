#version 330 core
// location 0/1 对应 CPU 网格的 position/uv；UV 经光栅化插值后交给片元阶段。
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
out vec2 texCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoord = uv;
    // 列向量右侧矩阵先作用：局部 -> 世界 -> 观察 -> 裁剪空间。
    gl_Position = projection * view * model * vec4(position, 1.0);
}
