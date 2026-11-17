#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
out vec3 surfaceNormal;
uniform mat4 model, view, projection;
void main() {
    surfaceNormal=mat3(transpose(inverse(model)))*normal;
    gl_Position=projection*view*model*vec4(position,1.0);
}
