#version 330 core
// 第二遍绘制屏幕四边形：位置已经覆盖 NDC [-1,1]，不需要相机或 MVP 矩阵。
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
out vec2 texCoord;
void main() { texCoord=uv; gl_Position=vec4(position,0.0,1.0); }
