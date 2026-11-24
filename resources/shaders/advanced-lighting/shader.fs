#version 330 core
in vec3 worldPos;
in vec3 worldNormal;
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D wood;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool blinn;
uniform bool specularOnly;
uniform float shininess;
uniform float specularStrength;
void main() {
    vec3 N = normalize(worldNormal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(viewPos - worldPos);
    float spec = 0.0;
    if (dot(N, L) > 0.0) {
        if (blinn) {
            // H 是光线方向与视线方向的半程向量。
            vec3 sum = L + V;
            if (dot(sum, sum) > 1e-10)
                spec = pow(max(dot(N, normalize(sum)), 0.0), shininess);
        } else {
            vec3 R = reflect(-L, N);
            spec = pow(max(dot(V, R), 0.0), shininess);
        }
    }
    vec3 color = specularOnly ? vec3(0.0) :
        texture(wood, texCoord).rgb * (0.05 + max(dot(N, L), 0.0));
    FragColor = vec4(color + vec3(specularStrength * spec), 1.0);
}
