#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

// 采样器对应 CPU 绑定的纹理单元；UV 由顶点阶段透视正确插值而来。
uniform sampler2D texture0;
uniform sampler2D texture1;

void main()
{
    // 两张图片按 20% / 80% 加权；面剔除由 CPU 状态控制，与此混合无关。
	FragColor = mix(texture(texture0, TexCoord), texture(texture1, TexCoord), 0.8);
}