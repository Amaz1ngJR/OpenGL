#version 330 core
out vec4 FragColor;

struct Material {
    vec3 ambient;//环境光照
    vec3 diffuse;//漫反射光照
    vec3 specular;//镜面光照
    float shininess;//反光度
}; 

struct Light {
    vec3 position;//光源位置

    vec3 ambient;//环境光照
    vec3 diffuse;//漫反射光照
    vec3 specular;//镜面光照
};

in vec3 FragPos;  
in vec3 Normal;  
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    // 环境项：用常量近似间接光，不依赖表面朝向；材质逐通道调制光源颜色。
    vec3 ambient = light.ambient * material.ambient;
  	
    // 漫反射：插值后的法线重新归一化，N·L 表示受光角度，负值截断为 0。
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    // 镜面项：lightDir 指向光源，所以 reflect 使用 -lightDir 作为入射方向。
    // 观察方向越接近反射方向高光越强；shininess 越大，高光越集中。
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);  
        
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
} 