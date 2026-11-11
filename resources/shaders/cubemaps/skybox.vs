#version 330 core
// 天空盒立方体的位置也作为采样方向；CPU 传来的 view 已去掉相机平移。
layout(location=0) in vec3 position;
out vec3 sampleDirection;
uniform mat4 view;
uniform mat4 projection;
void main() {
    sampleDirection=position;
    vec4 clip=projection*view*vec4(position,1.0);
    // 用 w 替换 z，透视除法后 z/w=1，落在最远深度；CPU 配合 LEQUAL 并关闭深度写入。
    gl_Position=clip.xyww;
}
