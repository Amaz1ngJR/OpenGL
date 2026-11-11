#version 330 core
out vec4 FragColor;

uniform vec3 lightColor; // 与受光物体使用同一份光源颜色，直接显示而不再计算受光。

void main()
{
    FragColor = vec4(lightColor, 1.0f); // 可见灯泡只是光源标记；它本身不会自动照亮其他物体。
}