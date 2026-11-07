#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D surfaceTexture;
uniform int displayMode;
uniform bool solidColor;
uniform vec3 color;
uniform float nearPlane;
uniform float farPlane;

void main() {
    if (displayMode == 0) {
        FragColor = solidColor ? vec4(color, 1.0) : texture(surfaceTexture, texCoord);
    } else if (displayMode == 1) {
        // 透视投影后的窗口深度在 [0,1] 内非线性分布，远处数值通常非常接近 1。
        FragColor = vec4(vec3(gl_FragCoord.z), 1.0);
    } else {
        // 从窗口深度还原 NDC [-1,1]，再逆转透视深度映射得到观察空间距离。
        float zNdc = gl_FragCoord.z * 2.0 - 1.0;
        float distanceToCamera = (2.0 * nearPlane * farPlane) /
            (farPlane + nearPlane - zNdc * (farPlane - nearPlane));
        // 除以远平面仅用于灰度显示：近处黑，远处白；不改写 gl_FragDepth。
        FragColor = vec4(vec3(distanceToCamera / farPlane), 1.0);
    }
}
