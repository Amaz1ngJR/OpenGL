#version 330 core
out vec4 FragColor;
uniform vec4 ourColor; // 传入三角形的颜色
void main() {
    FragColor = ourColor; // 使用 uniform 颜色
}