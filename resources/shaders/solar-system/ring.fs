#version 330 core
// 土星环的片元着色器。
// 环没有实体厚度，靠"半径方向的亮度/透明度变化"表现它由很多细环组成：
// 这里用几层正弦叠加生成条带，再乘上一个整体透明度。
in vec2 ringCoord;
out vec4 FragColor;

uniform float opacity;     // 整体不透明度，面板可调
uniform vec3 baseColor;    // 环的基色（土星的环偏米黄）

void main() {
    float t = ringCoord.x;

    // 几段不同频率的条带相乘，得到不规则但连续的疏密变化。
    // 频率取非整数倍，避免出现明显重复的周期性花纹。
    float bands = 0.65
                + 0.20 * sin(t * 41.0)
                + 0.12 * sin(t * 97.0 + 1.7)
                + 0.08 * sin(t * 211.0 + 0.4);
    bands = clamp(bands, 0.0, 1.0);

    // 内缘和外缘渐隐：真实环带不是一刀切的边界，边缘要淡出。
    float edgeFade = smoothstep(0.0, 0.08, t) * smoothstep(1.0, 0.88, t);

    // 卡西尼缝：在半径约 0.62 处开一条明显的暗缝。
    float gap = 1.0 - 0.85 * exp(-pow((t - 0.62) / 0.022, 2.0));

    float alpha = bands * edgeFade * gap * opacity;
    // 条带越亮的地方越不透明，颜色也稍微提亮，模拟密集冰粒对阳光的散射。
    vec3 color = baseColor * (0.75 + 0.35 * bands);
    FragColor = vec4(color, alpha);
}
