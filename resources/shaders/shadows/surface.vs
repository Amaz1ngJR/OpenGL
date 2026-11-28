#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 texCoord;

void main() {
    vec4 world = model * vec4(position, 1.0);
    worldPosition = world.xyz;
    worldNormal = transpose(inverse(mat3(model))) * normal;
    texCoord = uv;
    gl_Position = projection * view * world;
}
