#version 330 core
// location 与 CPU 的 position/normal 属性布局一致，法线不包含平移分量。
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
out vec3 worldPosition;
out vec3 worldNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    vec4 world=model*vec4(position,1.0);
    worldPosition=world.xyz;
    // 逆转置保证非等比缩放后法线仍垂直于表面；插值后的法线在片元阶段重新归一化。
    worldNormal=mat3(transpose(inverse(model)))*normal;
    // 局部 -> 世界 -> 观察 -> 裁剪；GPU 随后做透视除法得到 NDC。
    gl_Position=projection*view*world;
}
