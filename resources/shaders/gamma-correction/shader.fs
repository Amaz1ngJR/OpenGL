#version 330 core
in vec3 worldPos;
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D wood;
uniform vec3 viewPos;
uniform bool encodeOutput;
uniform bool inverseSquare;
uniform float gamma;
uniform float lightStrength;
void main() {
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(viewPos - worldPos);
    vec3 lighting = vec3(0.0);
    // 与教程一致：四盏白光从左到右逐渐增强，照亮同一张木地板。
    for (int i = 0; i < 4; ++i) {
        vec3 delta = vec3(-3.0 + 2.0 * float(i), 0.0, 0.0) - worldPos;
        float distance = max(length(delta), 0.001);
        vec3 L = delta / distance;
        float diffuse = max(dot(N, L), 0.0);
        vec3 sum = L + V;
        float specular = 0.0;
        if (diffuse > 0.0 && dot(sum, sum) > 1e-10)
            specular = pow(max(dot(N, normalize(sum)), 0.0), 64.0);
        float attenuation = inverseSquare ? 1.0 / (distance * distance) : 1.0 / distance;
        lighting += vec3((diffuse + specular) * attenuation * float(i + 1) * 0.25 * lightStrength);
    }
    // sRGB 纹理采样已在线性空间。所有光源先累加，最后只编码一次。
    vec3 color = texture(wood, texCoord).rgb * lighting;
    if (encodeOutput) color = pow(max(color, vec3(0.0)), vec3(1.0 / gamma));
    FragColor = vec4(color, 1.0);
}
