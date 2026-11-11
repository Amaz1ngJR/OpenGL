#version 330 core
in vec2 texCoord;
out vec4 FragColor;
// 第一遍普通场景采样；输出写到哪个 FBO，由 CPU 的 glBindFramebuffer 决定。
uniform sampler2D surfaceTexture;
void main() { FragColor = texture(surfaceTexture, texCoord); }
