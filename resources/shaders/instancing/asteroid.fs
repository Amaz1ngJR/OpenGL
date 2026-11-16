#version 330 core
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D surfaceTexture;
// 每个天体一个色调：太阳偏暖、天王星海王星略带冷色，其他为纯白（不改动贴图原色）。
uniform vec3 surfaceTint = vec3(1.0);
// 自发光标记：太阳设为 1，跳过方向光衰减，避免贴图上出现明显的一半暗一半亮。
uniform int emissive = 0;
void main() {
    // 简单的方向光 + 环境项突出岩石表面，光照与实例化是两个独立概念。
    float diffuse = max(dot(normalize(worldNormal), normalize(vec3(-0.3, 0.8, 0.6))), 0.0);
    vec3 color = texture(surfaceTexture, texCoord).rgb;
    float light = (emissive == 1) ? 1.0 : (0.25 + 0.75 * diffuse);
    FragColor = vec4(color * surfaceTint * light, 1.0);
}
