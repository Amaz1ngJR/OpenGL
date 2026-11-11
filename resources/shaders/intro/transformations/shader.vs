#version 330 core
// CPU 每个顶点是位置 vec3 + UV vec2，属性编号必须匹配。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 列向量从右到左：局部 -> 世界 -> 观察 -> 裁剪；GPU 再除以 w 得到 NDC。
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	TexCoord = vec2(aTexCoord.x, aTexCoord.y);
}