#version 330 core
// 位置来自 VBO 的属性 0；本例 w=1，无额外坐标变换。
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}