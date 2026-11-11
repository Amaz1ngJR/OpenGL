#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform vec3 color;
uniform sampler2D surfaceTexture;
uniform bool useTexture;
void main() {
    // 第一遍箱子采样贴图，第二遍放大的描边输出纯色。
    // 模板值写入与比较由 CPU 配置的 glStencilOp/glStencilFunc 决定，并非此颜色开关控制。
    FragColor = useTexture ? texture(surfaceTexture, texCoord) : vec4(color, 1.0);
}
