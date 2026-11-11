#version 330 core
// 对应 ModelVertex 的 position/normal/uv；CPU 用 offsetof 描述实际内存布局。
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 worldPos;
out vec3 worldNormal;
out vec2 texCoord;
void main() {
    vec4 world = model * vec4(position, 1.0);
    worldPos = world.xyz;
    // 非等比变换时，法线必须使用逆转置矩阵，而非直接乘模型矩阵。
    worldNormal = mat3(transpose(inverse(model))) * normal;
    texCoord = uv;
    // 世界坐标用于光照，裁剪坐标用于显示，两者用途不同，不可混用。
    gl_Position = projection * view * world;
}
