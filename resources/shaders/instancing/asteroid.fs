#version 330 core
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D surfaceTexture;
void main() {
    // 简单的方向光 + 环境项突出岩石表面，光照与实例化是两个独立概念。
    float diffuse = max(dot(normalize(worldNormal), normalize(vec3(-0.3, 0.8, 0.6))), 0.0);
    vec3 color = texture(surfaceTexture, texCoord).rgb;
    FragColor = vec4(color * (0.25 + 0.75 * diffuse), 1.0);
}
