#version 330 core
// 太阳系天体的顶点着色器。
//
// 与 instancing/asteroid.vs 的区别：这里只做普通的单次绘制，没有实例矩阵，
// 也不需要在"实例化 / 非实例化"之间切换。每颗行星由 CPU 单独提交一次 model 矩阵，
// 九颗天体共用一个球体网格，因此顶点属性只需要 location 0/1/2。
layout(location = 0) in vec3 position;   // 单位球上的位置
layout(location = 1) in vec3 normal;     // 单位球上位置即法线（球心在原点）
layout(location = 2) in vec2 uv;         // 等距柱状 UV：u 绕赤道、v 从北极到南极

uniform mat4 model;        // 缩放 -> 自转 -> 平移到轨道位置，全部烘进这一个矩阵
uniform mat4 view;
uniform mat4 projection;

out vec3 worldNormal;
out vec2 texCoord;

void main() {
    // 世界空间法线要用 model 的逆转置变换，否则非等比缩放后法线会偏离表面。
    // 本场景里 model 只有等比缩放，逆转置与 model 本身等价，
    // 但保持通用写法，将来换成椭球或带非等比缩放的天体也不会出错。
    worldNormal = mat3(transpose(inverse(model))) * normal;
    texCoord = uv;
    gl_Position = projection * view * model * vec4(position, 1.0);
}
