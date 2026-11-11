#version 330 core
in vec3 worldPosition;
in vec3 worldNormal;
out vec4 FragColor;
uniform samplerCube environmentMap;
uniform vec3 cameraPosition;
uniform int surfaceMode;
uniform float materialIor;
void main() {
    // 入射方向从相机指向表面；位置、法线和相机必须处于同一个世界坐标系。
    vec3 incident=normalize(worldPosition-cameraPosition);
    vec3 normal=normalize(worldNormal);
    float eta=1.0/materialIor;
    // 空气进入材质时 eta=空气折射率/材质折射率；从内部看则反转法线并交换比值。
    if(dot(incident,normal)>0.0) { normal=-normal; eta=materialIor; }
    vec3 direction=reflect(incident,normal);
    if(surfaceMode==1) {
        vec3 transmitted=refract(incident,normal,eta);
        // 全反射时 refract 返回零向量，此时保留上面算出的反射方向。
        if(dot(transmitted,transmitted)>0.000001) direction=transmitted;
    }
    // samplerCube 按三维方向选取六面之一；这是单表面近似，没有厚度或另一物体的倒影。
    FragColor=vec4(texture(environmentMap,direction).rgb,1.0);
}
