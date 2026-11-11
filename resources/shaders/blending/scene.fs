#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D surfaceTexture;
uniform float alphaCutoff;
void main() {
    vec4 color = texture(surfaceTexture, texCoord);
    // 草叶用阈值丢弃透明部分：discard 的片元不写颜色和深度，不等同于输出 alpha=0。
    if (color.a < alphaCutoff) discard;
    // 半透明窗户保留 alpha，由 CPU 开启混合并从远到近绘制；shader 本身不执行混合公式。
    FragColor = color;
}
