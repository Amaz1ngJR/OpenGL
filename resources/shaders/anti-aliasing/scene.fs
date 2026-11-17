#version 330 core
in vec3 surfaceNormal;
out vec4 FragColor;
uniform vec3 surfaceColor;
void main() {
    float light=0.45+0.55*max(dot(normalize(surfaceNormal),normalize(vec3(1,2,3))),0.0);
    FragColor=vec4(surfaceColor*light,1.0);
}
