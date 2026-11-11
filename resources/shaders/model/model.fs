#version 330 core
in vec3 worldPos;
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;
uniform vec3 baseColor;
uniform vec3 viewPos;
uniform sampler2D diffuseMap;
uniform bool hasTexture;
uniform bool flipV;
void main() {
    // 模型 UV 和图片行序可能约定不同，开关只翻转采样坐标，不修改模型文件。
    vec2 uv = flipV ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;
    // 基础颜色乘漫反射贴图；缺失贴图时用白色作为乘法单位元。
    vec3 albedo = baseColor * (hasTexture ? texture(diffuseMap, uv).rgb : vec3(1.0));
    // 相机旁的白色预览光照让模型始终可见，不依赖外部场景的光源参数。
    vec3 normal = length(worldNormal) > 0.00001 ? normalize(worldNormal) : vec3(0.0, 1.0, 0.0);
    vec3 lightDir = normalize(viewPos + vec3(2.0, 3.0, 1.0) - worldPos);
    float diffuse = abs(dot(normal, lightDir)); // 双面预览，兼容单面薄片模型。
    FragColor = vec4(albedo * (0.25 + 0.75 * diffuse), 1.0);
}
