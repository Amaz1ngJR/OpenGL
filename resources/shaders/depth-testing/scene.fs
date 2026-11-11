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
    } else {
        // 透视投影使 gl_FragCoord.z 非线性，靠近相机处分配更多精度。
        // 从窗口深度还原 NDC [-1,1]，再逆转透视映射得到线性观察轴深度（-viewZ），
        // 而非到相机的欧氏距离；displayMode=1 对应 UI 的线性深度模式。
        float zNdc = gl_FragCoord.z * 2.0 - 1.0;
        float distanceToCamera = (2.0 * nearPlane * farPlane) /
            (farPlane + nearPlane - zNdc * (farPlane - nearPlane));
        // 除以远平面仅用于灰度显示：近处黑，远处白；不改写 gl_FragDepth。
        FragColor = vec4(vec3(distanceToCamera / farPlane), 1.0);
    }
}
