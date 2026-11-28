#version 330 core
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D albedo;
uniform bool useWallMaterial;
uniform sampler2D wallNormal, wallRoughness, wallMetallic, wallAO;
uniform sampler2DArrayShadow planarDepth;
uniform samplerCube pointDepth;
uniform vec3 baseColor;
uniform vec3 viewPos;
uniform bool useShadows;

uniform vec3 toSun;
uniform vec3 sunColor;
uniform mat4 sunMatrix;
uniform vec3 pointPosition;
uniform vec3 pointColor;
uniform float pointFar;
const int areaSamples = 4;
uniform vec3 emitters[areaSamples];
uniform mat4 areaMatrices[areaSamples];
uniform vec3 areaColor;
uniform vec3 spotPosition, spotDirection, spotColor;
uniform float spotInnerCos, spotOuterCos;
uniform mat4 spotMatrix;
uniform int spotLayer;

float planarVisibility(mat4 lightMatrix, int layer, vec3 receiver, vec3 N, vec3 toLight) {
    if (!useShadows) return 1.0;
    vec4 clip = lightMatrix * vec4(receiver, 1.0);
    if (clip.w <= 0.0) return 1.0;
    vec3 uvz = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(uvz, vec3(0.0))) || any(greaterThan(uvz, vec3(1.0)))) return 1.0;
    float bias = max(0.00008, 0.00035 * (1.0 - max(dot(N, toLight), 0.0)));
    vec2 texel = 1.0 / vec2(textureSize(planarDepth, 0).xy);
    float taps = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = uvz.xy + vec2(x,y) * texel;
            taps += texture(planarDepth, vec4(uv, float(layer), uvz.z - bias));
        }
    return taps / 9.0;
}

float pointVisibility(vec3 receiver) {
    if (!useShadows) return 1.0;
    vec3 fromLight = receiver - pointPosition;
    float distanceToLight = length(fromLight);
    if (distanceToLight >= pointFar || distanceToLight < 0.001) return 1.0;
    float depth = (distanceToLight - 0.012) / pointFar;
    float radius = distanceToLight * 0.005;
    float taps = step(depth, texture(pointDepth, fromLight).r);
    for (int axis = 0; axis < 3; ++axis) {
        vec3 offset = vec3(0.0);
        offset[axis] = radius;
        taps += step(depth, texture(pointDepth, fromLight + offset).r);
        taps += step(depth, texture(pointDepth, fromLight - offset).r);
    }
    return taps / 7.0;
}

vec3 specularColor = vec3(0.08);
float shininess = 32.0;
vec3 shade(vec3 color, vec3 L, vec3 N, vec3 V, vec3 material) {
    float diffuse = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float specular = diffuse > 0.0 ? pow(max(dot(N,H), 0.0), shininess) : 0.0;
    return color * (material * diffuse + specularColor * specular);
}

void main() {
    vec3 material = pow(texture(albedo, texCoord).rgb, vec3(2.2)) * baseColor;
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(viewPos - worldPosition);
    // 接收点沿法线偏移少许，减少低分辨率/斜入射造成的自身阴影噪声。
    vec3 receiver = worldPosition + N * 0.015;
    vec3 lit = material * 0.20;
    if (useWallMaterial) {
        // 墙面 UV 的切线朝 -Z，副切线朝 +Y；阴影偏移仍用几何法线。
        vec3 detail = texture(wallNormal, texCoord).xyz * 2.0 - 1.0;
        N = normalize(vec3(detail.z, detail.y, -detail.x));
        float roughness = texture(wallRoughness, texCoord).r;
        float metallic = texture(wallMetallic, texCoord).r;
        shininess = mix(128.0, 4.0, roughness);
        specularColor = mix(vec3(0.04), material, metallic);
        material *= 1.0 - metallic;
        lit *= texture(wallAO, texCoord).r;
    }
    if (any(greaterThan(sunColor, vec3(0.0))) && dot(N, toSun) > 0.0)
        lit += planarVisibility(sunMatrix, 0, receiver, N, toSun) * shade(sunColor, toSun, N, V, material);

    if (any(greaterThan(pointColor, vec3(0.0)))) {
        vec3 delta = pointPosition - worldPosition;
        float r2 = dot(delta, delta);
        if (dot(N, delta) > 0.0)
            lit += pointVisibility(receiver) * shade(pointColor / (1.0 + r2), normalize(delta), N, V, material);
    }

    if (any(greaterThan(areaColor, vec3(0.0)))) {
        for (int i = 0; i < areaSamples; ++i) {
            vec3 delta = emitters[i] - worldPosition;
            if (delta.y > 0.0 && dot(N, delta) > 0.0) {
                vec3 L = normalize(delta);
                // 水平矩形朝下发光：以出射角余弦加权，再合成各采样点的可见度。
                vec3 energy = areaColor * L.y / (float(areaSamples) * (1.0 + dot(delta,delta)));
                lit += planarVisibility(areaMatrices[i], i + 1, receiver, N, L) * shade(energy, L, N, V, material);
            }
        }
    }
    if (any(greaterThan(spotColor, vec3(0.0)))) {
        vec3 delta = spotPosition - worldPosition;
        if (dot(N, delta) > 0.0) {
            vec3 L = normalize(delta);
            float cone = smoothstep(spotOuterCos, spotInnerCos, dot(spotDirection, -L));
            if (cone > 0.0) {
                vec3 energy = spotColor * cone / (1.0 + dot(delta, delta));
                lit += planarVisibility(spotMatrix, spotLayer, receiver, N, L) * shade(energy, L, N, V, material);
            }
        }
    }
    fragColor = vec4(pow(lit, vec3(1.0 / 2.2)), 1.0);
}
