#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
out vec3 worldPos;
out vec3 worldNormal;
out vec2 texCoord;
uniform mat4 view;
uniform mat4 projection;
void main() {
    worldPos = position;
    worldNormal = normal;
    texCoord = uv;
    gl_Position = projection * view * vec4(position, 1.0);
}
