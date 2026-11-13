#version 330 core
// 小行星带背景天空盒的顶点着色器。
//
// 输入只有一个立方体顶点位置，但它同时承担两个作用：
//   1) 作为立方体的几何位置参与变换，用来铺满屏幕；
//   2) 作为三维采样方向传给片元着色器，去查询 cubemap 的对应纹素。
// 因此这里不需要 UV 属性，也不需要法线。
layout(location = 0) in vec3 position;

out vec3 sampleDirection;

// CPU 传入的 view 已经去掉了平移分量（只保留旋转），所以相机移动时天空盒不会靠近或远离。
// 这正是"天空在无限远处"的近似：只剩方向有意义，距离没有意义。
uniform mat4 view;
uniform mat4 projection;

void main() {
    // 立方体以原点为中心，位置向量本身就是指向该面的方向，可直接用于采样。
    sampleDirection = position;

    vec4 clip = projection * view * vec4(position, 1.0);
    // xyww 把裁剪空间的 z 直接设成 w。透视除法（除以 w）之后 z/w 恒等于 1，
    // 也就是落在 NDC 的最远处（深度 1.0）。这样无论立方体多大，都不会被视锥远平面裁掉。
    // 配合 CPU 端的 glDepthFunc(GL_LEQUAL) 与 glDepthMask(GL_FALSE)：
    //   LEQUAL 让深度 1.0 通过清屏值 1.0 的测试，从而能画出来；
    //   关闭深度写入，保证天空盒不会污染其他物体的深度比较。
    gl_Position = clip.xyww;
}
