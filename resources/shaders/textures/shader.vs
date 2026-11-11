#version 330 core
// 属性编号对应 CPU 的位置、顶点色、UV；out 在光栅化时插值给片元 shader。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

uniform float uvScale;

void main()
{
    // 无 MVP 的二维例子，w=1 时裁剪坐标与透视除法后的 NDC 数值一致。
	gl_Position = vec4(aPos, 1.0);
	ourColor = aColor;
	// 围绕中心缩放坐标，以便在 UI 中观察重复、钳制及多级渐远过滤。
	TexCoord = (aTexCoord - vec2(0.5)) * uvScale + vec2(0.5);
}