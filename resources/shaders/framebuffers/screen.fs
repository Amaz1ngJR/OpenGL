#version 330 core
in vec2 texCoord;
out vec4 FragColor;
// 输入是第一遍渲染的颜色附件；此时 CPU 已切回默认帧缓冲，避免边写边读同一附件。
uniform sampler2D screenTexture;
uniform int effect;
uniform float strength;
uniform float radius;
uniform bool splitView;
void main() {
    vec3 original=texture(screenTexture,texCoord).rgb;
    vec3 result=original;
    if (effect==1) result=vec3(1.0)-original;
    else if (effect==2) result=vec3(dot(original,vec3(0.2126,0.7152,0.0722)));
    else if (effect>=3 && effect<=5) {
        // textureSize 返回实际附件尺寸；其倒数是一个纹素的 UV 大小，适配高分屏和窗口缩放。
        vec2 stepUV=radius/vec2(textureSize(screenTexture,0));
        float sharpen[9]=float[](-1,-1,-1,-1,9,-1,-1,-1,-1);
        float blur[9]=float[](1,2,1,2,4,2,1,2,1);
        float edge[9]=float[](1,1,1,1,-8,1,1,1,1);
        result=vec3(0.0);
        // 3x3 卷积：邻域颜色乘对应权重后求和。模糊核总和 16，归一化后不改变恒定亮度。
        for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x) {
            int i=(y+1)*3+x+1;
            float weight=effect==3 ? sharpen[i] : effect==4 ? blur[i]/16.0 : edge[i];
            result+=texture(screenTexture,texCoord+vec2(x,y)*stepUV).rgb*weight;
        }
        if (effect==5) result=abs(result);
    } else if (effect==6) {
        result=vec3(dot(original,vec3(.393,.769,.189)),
                    dot(original,vec3(.349,.686,.168)),
                    dot(original,vec3(.272,.534,.131)));
    }
    // strength 在原图与效果间插值；分屏只改变显示，不重新渲染三维场景。
    result=mix(original,clamp(result,0.0,1.0),strength);
    if (splitView && texCoord.x<0.5) result=original;
    FragColor=vec4(result,1.0);
}
