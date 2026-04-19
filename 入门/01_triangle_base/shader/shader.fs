#version 330 core
out vec4 FragColor;
in vec3 vertexColor; // 传入三角形的颜色
void main() {
    FragColor = vec4(vertexColor, 1.0); // 使用顶点颜色
}