#version 330 core
out vec4 FragColor;

uniform vec3 lightColor; // ← 添加 uniform

void main()
{
    FragColor = vec4(lightColor, 1.0f); // ← 使用传入的颜色
}