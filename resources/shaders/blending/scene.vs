#version 330 core
// position/uv 对应 CPU 属性位置 0/1；输出的 UV 默认进行透视正确插值。
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
out vec2 texCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoord = uv;
    // 模型、观察、投影矩阵依次把局部顶点送到裁剪空间，之后由 GPU 做透视除法。
    gl_Position = projection * view * model * vec4(position, 1.0);
}
