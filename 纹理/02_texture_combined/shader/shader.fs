#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

// texture sampler
uniform sampler2D texture0;
uniform sampler2D texture1;

void main()
{
	//使用mix函数，最后一个参数是第二个纹理占的比例
	FragColor = mix(texture(texture0, TexCoord), texture(texture1,TexCoord), 0.8);
}