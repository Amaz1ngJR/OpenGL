#version 330 core
out vec4 FragColor;
uniform vec4 ourColor; // CPU 每帧更新矩形颜色；一次 draw 中所有片元共用，不按顶点插值。
void main() {
    FragColor = ourColor; // 使用 uniform 颜色
}