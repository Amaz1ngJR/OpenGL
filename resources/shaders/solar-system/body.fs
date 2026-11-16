#version 330 core
// 太阳系天体的片元着色器。
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;

uniform sampler2D bodyTexture;
// 每个天体一个色调：太阳偏暖、天王星海王星略带冷色，其他为纯白（不改动贴图原色）。
uniform vec3 surfaceTint = vec3(1.0);
// 自发光标记：太阳设为 1，跳过方向光衰减。
// 太阳贴图本身就很亮，若照常参与漫反射会一半黑一半亮，看起来像被切成两半。
uniform int emissive = 0;

// 太阳光照模型。
//   sunDirection：从片元指向太阳的单位方向（世界空间，由 CPU 按天体位置算好）
//   ambient     ：环境光强度。0 = 背光面全黑，符合"真实太阳光"；>0 保留一点暗部细节。
// 用 uniform 而不是写死方向，是因为太阳固定在原点、行星绕它公转，
// 每颗行星、每一帧的受光方向都不一样。
uniform vec3 sunDirection = vec3(0.0, 0.0, 1.0);
uniform float ambient = 0.0;

void main() {
    // 点光源退化成的方向光：球面上法线朝太阳的一侧被照亮，背光面只有环境项。
    // 这里不做 1/r² 衰减——太阳系里各行星距太阳的远近差几十倍，
    // 真实衰减会让海王星几乎全黑，视觉上反而不像"太阳照着"。
    float diffuse = max(dot(normalize(worldNormal), normalize(sunDirection)), 0.0);
    vec3 color = texture(bodyTexture, texCoord).rgb;
    // 受光面从 ambient 过渡到 1.0；emissive 的天体（太阳）跳过衰减直接满亮。
    float light = (emissive == 1) ? 1.0 : (ambient + (1.0 - ambient) * diffuse);
    FragColor = vec4(color * surfaceTint * light, 1.0);
}
