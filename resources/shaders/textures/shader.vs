#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

uniform float uvScale;

void main()
{
	gl_Position = vec4(aPos, 1.0);
	ourColor = aColor;
	// 围绕中心缩放坐标，以便在 UI 中观察重复、钳制及多级渐远过滤。
	TexCoord = (aTexCoord - vec2(0.5)) * uvScale + vec2(0.5);
}