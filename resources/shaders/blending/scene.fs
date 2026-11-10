#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D surfaceTexture;
uniform float alphaCutoff;
void main() {
    vec4 color = texture(surfaceTexture, texCoord);
    // Grass is binary coverage: discarded pixels modify neither color nor depth.
    if (color.a < alphaCutoff) discard;
    FragColor = color;
}
