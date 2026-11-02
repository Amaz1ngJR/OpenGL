#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D texture0;
uniform sampler2D texture1;
// UI 控制单纹理/混合模式，以及是否叠加顶点颜色。
uniform bool combined;
uniform bool vertexTint;
uniform float mixFactor;

void main()
{
    vec4 face = texture(texture1, TexCoord);
    // mixFactor 表示第二张纹理 awesomeface 所占的比例。
    vec4 color = combined ? mix(texture(texture0, TexCoord), face, mixFactor) : face;
    FragColor = vertexTint ? color * vec4(ourColor, 1.0) : color;
}
