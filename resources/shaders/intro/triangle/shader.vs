#version 330 core
// ===========================================================================
// shader.vs —— 顶点着色器 (Vertex Shader)
// ---------------------------------------------------------------------------
// 角色：对每一个输入的顶点运行一次，负责：
//   1) 决定该顶点在裁剪空间中的最终位置 gl_Position
//   2) 把顶点属性（这里是颜色）插值地传给片段着色器
// 输入来自 VBO，通过 glVertexAttribPointer 配置的 location 编号对应。
// ===========================================================================

// ---- 顶点属性输入 (in) ----
// location = 0 / 1 必须与 CPU 侧 glVertexAttribPointer 的第一个参数一致
layout (location = 0) in vec3 aPos;    // 顶点位置 (x, y, z)
layout (location = 1) in vec3 aColor;  // 顶点颜色 (r, g, b)

// ---- 传递给片段着色器的输出 (out) ----
// 光栅化阶段 GPU 会对三个顶点的 vertexColor 做重心插值，
// 因此三角形内部每个像素会得到一个平滑过渡的颜色
out vec3 vertexColor;

void main() {
    // gl_Position 是内置输出，类型为 vec4，表示该顶点在裁剪空间中的齐次坐标
    // 这里没有做任何变换，直接把 NDC 坐标扩展为齐次坐标 (w = 1.0)
    gl_Position = vec4(aPos, 1.0);

    // 把顶点颜色原样传给片段着色器
    vertexColor = aColor;
}
