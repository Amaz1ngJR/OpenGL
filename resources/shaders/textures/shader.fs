#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

// 采样器值是 CPU 指定的纹理单元编号，环绕/过滤规则属于绑定的纹理对象。
uniform sampler2D texture0;
uniform sampler2D texture1;
// UI 控制单纹理/混合模式，以及是否叠加顶点颜色。
uniform bool combined;
uniform bool vertexTint;
uniform float mixFactor;

void main()
{
    vec4 face = texture(texture1, TexCoord);
    // mix(a,b,t)=a*(1-t)+b*t；mixFactor 表示第二张纹理 awesomeface 所占的比例。
    vec4 color = combined ? mix(texture(texture0, TexCoord), face, mixFactor) : face;
    FragColor = vertexTint ? color * vec4(ourColor, 1.0) : color;
}
