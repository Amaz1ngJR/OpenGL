#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // ambient 环境光照 ：用光的颜色乘以一个很小的常量环境因子，再乘以物体的颜色
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 漫反射光照：用光的方向向量和物体的法向量计算出一个 diffuse 系数，然后乘以光的颜色和物体的颜色，得到漫反射光照
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);//两个向量之间的角度大于90度，点乘的结果就会变成负数，所以需要取最大值
    vec3 diffuse = diff * lightColor;
    
    // specular 镜面光照：用光的方向向量和物体的法向量计算出一个 specular 系数，然后乘以光的颜色和物体的颜色，得到镜面光照
    float specularStrength = 0.5;//镜面强度
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
} 