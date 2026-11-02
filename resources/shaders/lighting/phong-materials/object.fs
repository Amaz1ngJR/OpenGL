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
    // ambient
    vec3 ambient = light.ambient * material.ambient;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);  
        
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
} 