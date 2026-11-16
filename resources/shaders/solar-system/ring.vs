#version 330 core
// 土星环的顶点着色器。
// 环面是一块只有内外两圈顶点的平面圆环，纹理坐标不是从模型读的，
// 而是由顶点自带的半径直接算出：内外半径之间归一化后就是环带的 u 坐标。
layout(location = 0) in vec2 position;   // 平面坐标 (x, z)，圆心是 (0,0)
// 半径属性放在 location 2：球体网格用 0/1/2 表示 position/normal/uv，
// 环与球体共用同一套属性编号约定，不要占用 1（否则与球体 vec3 布局冲突）。
layout(location = 2) in float radius;

out vec2 ringCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float innerRadius;
uniform float outerRadius;

void main() {
    // 把半径映射到 0..1，内缘是 0、外缘是 1；片元着色器用它查环带的颜色和透明度。
    ringCoord = vec2((radius - innerRadius) / (outerRadius - innerRadius), 0.0);
    gl_Position = projection * view * model * vec4(position.x, 0.0, position.y, 1.0);
}
