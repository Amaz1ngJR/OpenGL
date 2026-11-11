#version 330 core
// 灯泡与物体可复用同一网格；这里只读取位置，不需要法线或纹理。
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // model 负责把灯泡缩小并移动到光源位置，view/projection 与物体保持一致。
	gl_Position = projection * view * model * vec4(aPos, 1.0);
}