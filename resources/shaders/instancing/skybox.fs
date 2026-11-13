#version 330 core
// 小行星带背景天空盒的片元着色器。
in vec3 sampleDirection;
out vec4 FragColor;

// samplerCube 与 sampler2D 的区别：采样时给的是三维方向向量，而不是二维 UV。
// 硬件会选出该方向命中立方体的哪一面，再在面内做双线性插值；
// 六个面在边角处能连续过渡，因此不会看到接缝（还需开启 GL_TEXTURE_CUBE_MAP_SEAMLESS）。
uniform samplerCube spaceMap;

void main() {
    // 背景不透明：alpha 固定为 1，避免万一开启混合时与场景内容混色。
    FragColor = vec4(texture(spaceMap, sampleDirection).rgb, 1.0);
}
