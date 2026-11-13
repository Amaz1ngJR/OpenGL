#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
// mat4 占用 3、4、5、6 四个属性位置；CPU 为每列配置 divisor=1。
layout(location = 3) in mat4 instanceModel;
uniform mat4 model;
uniform mat4 orbit;
uniform mat4 view;
uniform mat4 projection;
uniform bool instanced;
out vec3 worldNormal;
out vec2 texCoord;
void main() {
    // 普通绘制逐次更新 model；实例化绘制由 GPU 自动取当前实例的矩阵。
    mat4 world = orbit * (instanced ? instanceModel : model);
    worldNormal = mat3(transpose(inverse(world))) * normal;
    texCoord = uv;
    gl_Position = projection * view * world * vec4(position, 1.0);
}
