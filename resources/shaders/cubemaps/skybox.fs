#version 330 core
in vec3 sampleDirection;
out vec4 FragColor;
// 按插值的三维方向采样背景，不需要普通二维纹理的 UV。
uniform samplerCube environmentMap;
void main() { FragColor=vec4(texture(environmentMap,sampleDirection).rgb,1.0); }
