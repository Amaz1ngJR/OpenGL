#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform vec3 color;
uniform sampler2D surfaceTexture;
uniform bool useTexture;
void main() {
    // 箱子本体采样贴图，描边与模板掩码预览仍输出纯色，不改变模板测试规则。
    FragColor = useTexture ? texture(surfaceTexture, texCoord) : vec4(color, 1.0);
}
