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
uniform bool showTextures;
uniform bool flatShading;
uniform bool wirePass;
uniform vec3 surfaceColor;
uniform vec3 wireColor;
void main() {
    if(wirePass) { FragColor=vec4(wireColor,1.0); return; }
    // 模型 UV 和图片行序可能约定不同，开关只翻转采样坐标，不修改模型文件。
    vec2 uv = flipV ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;
    // 基础颜色乘漫反射贴图；缺失贴图时用白色作为乘法单位元。
    vec3 albedo = showTextures ? baseColor * (hasTexture ? texture(diffuseMap, uv).rgb : vec3(1.0)) : surfaceColor;
    // 相机旁的白色预览光照让模型始终可见，不依赖外部场景的光源参数。
    vec3 normal = length(worldNormal) > 0.00001 ? normalize(worldNormal) : vec3(0.0, 1.0, 0.0);
    if(flatShading) {
        // 导数长度随像素覆盖的世界尺寸变化，不能用固定面积阈值判断法线。
        vec3 tangentX=dFdx(worldPos), tangentY=dFdy(worldPos);
        if(dot(tangentX,tangentX)>0.0 && dot(tangentY,tangentY)>0.0) {
            vec3 faceNormal=cross(normalize(tangentX),normalize(tangentY));
            if(dot(faceNormal,faceNormal)>1e-12) normal=normalize(faceNormal);
        }
    }
    vec3 lightDir = normalize(viewPos + vec3(2.0, 3.0, 1.0) - worldPos);
    float diffuse = abs(dot(normal, lightDir)); // 双面预览，兼容单面薄片模型。
    // 使用当前着色模式的法线计算白色 Phong 高光，突出面片朝向及曲面起伏。
    // 双面预览时将法线朝向观察者，避免背面的高光方向错误。
    vec3 viewDir = normalize(viewPos - worldPos);
    vec3 facingNormal = dot(normal, viewDir) < 0.0 ? -normal : normal;
    float specular = 0.0;
    if (dot(facingNormal, lightDir) > 0.0) {
        vec3 reflectedLight = reflect(-lightDir, facingNormal);
        specular = pow(max(dot(viewDir, reflectedLight), 0.0), 32.0);
    }
    FragColor = vec4(albedo * (0.25 + 0.75 * diffuse) + vec3(0.25 * specular), 1.0);
}
