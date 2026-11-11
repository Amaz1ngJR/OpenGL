#version 330 core
// CPU 网格按位置、法线顺序上传，属性编号必须与 glVertexAttribPointer 对应。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 光照在世界空间计算，位置和法线都先变换到世界空间。
    FragPos = vec3(model * vec4(aPos, 1.0));
    // 非等比缩放时不能直接乘 model；逆转置保持法线与表面垂直。
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    // 世界 -> 观察 -> 裁剪空间，随后 GPU 裁剪、透视除法、光栅化。
    gl_Position = projection * view * vec4(FragPos, 1.0);
}