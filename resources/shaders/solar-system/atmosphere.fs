#version 330 core
// 大气层 / 云层外壳的片元着色器。
//
// 这层壳比行星本体略大，包在外面，所以只在"行星本体之外、壳之内"的那圈区域
// 看得见——正好是大气的视觉表现。片元着色器要做三件事：
//   1) 用和本体一致的光照方向决定这一面是白天还是黑夜；
//   2) 用菲涅尔项让边缘更亮、更正对镜头的中心更透明，形成大气边缘的光晕；
//   3) 云层贴图用亮度当作透明度，黑色（海洋/陆地）完全透出底下的行星。
in vec3 worldNormal;
in vec3 worldPosition;
in vec3 viewDirection;
in vec2 texCoord;
out vec4 FragColor;

uniform sampler2D atmosphereTexture;
uniform bool texturedCloud;
// 从片元指向太阳的单位方向，与 body.fs 共用同一套约定。
uniform vec3 sunDirection;
uniform vec3 atmosphereColor;   // 金星的淡黄、地球的淡蓝
// 整体不透明度。云层用较高值，稀薄大气用较低值。
uniform float opacity;
// 贴图亮度转透明度的强度：云层贴图黑=透明，所以 1.0；
// 金星大气贴图本身就是完整的亮色云图，用较低值让它半透明。
uniform float luminanceAlpha;
// 菲涅尔强度：边缘增亮多少。
uniform float rimPower;

void main() {
    // 视线方向由顶点着色器算好并插值过来，指向相机。
    vec3 viewDir = normalize(viewDirection);
    vec3 n = normalize(worldNormal);

    // 菲涅尔：视线与法线越垂直（边缘），值越接近 1。
    float fresnel = pow(1.0 - max(dot(n, viewDir), 0.0), rimPower);

    // 与本体一致的光照：朝太阳的一面被照亮，背面只有一点点环境光，
    // 避免夜面的大气完全消失。
    float diffuse = max(dot(n, normalize(sunDirection)), 0.0);
    float light = 0.06 + 0.94 * diffuse;

    // 只采样一次：贴图亮度既作为颜色，也作为透明度来源。
    // 云层贴图中黑色代表海洋/陆地，红色通道接近 0，正好自然变成全透明。
    vec3 tex = texturedCloud ? texture(atmosphereTexture, texCoord).rgb : vec3(1.0);
    // Untextured haze is a rim effect, not an opaque replacement surface.
    if(!texturedCloud) {
        if(dot(n,viewDir)<=0.0) discard;
        float alpha=opacity*fresnel;
        FragColor=vec4(atmosphereColor*light*(0.75+fresnel),alpha);
        return;
    }
    // luminanceAlpha = 0 时表示"贴图整体是一层均匀大气"，透明度不跟贴图走。
    float texAlpha = (luminanceAlpha > 0.0) ? clamp(tex.r * luminanceAlpha, 0.0, 1.0) : 1.0;

    // 透明度：边缘更实、中心更淡，形成大气层的体积感。
    // 基底取 0.55 而不是 0.25——太低的话可见半球的大部分区域都近乎全透明，
    // 只有最边上一圈发亮，观感上等于"没有大气"。
    // 注意这里不再乘 light：光照只影响颜色，若同时压低透明度，
    // 背向太阳的那半边大气会直接消失，看起来像被切掉一块。
    float alpha = opacity * texAlpha * mix(0.55, 1.0, fresnel);

    // 颜色随光照变化，边缘额外增亮，模拟大气在轮廓处最厚的散射效果。
    vec3 color = atmosphereColor * tex * light * (0.75 + 1.0 * fresnel);
    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
