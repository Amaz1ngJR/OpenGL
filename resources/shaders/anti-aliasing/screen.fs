#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D screenTexture;
uniform bool fxaa;
uniform float zoom;
float luma(vec3 c) { return dot(c,vec3(.299,.587,.114)); }
vec3 sampleAt(vec2 uv) { return texture(screenTexture,uv).rgb; }
void main() {
    vec2 uv=(texCoord-.5)/zoom+.5;
    vec3 centre=sampleAt(uv);
    if(!fxaa) { FragColor=vec4(centre,1); return; }
    // FXAA directional edge filtering, using neighbours in source pixel units.
    vec2 pixel=1.0/vec2(textureSize(screenTexture,0));
    float nw=luma(sampleAt(uv+vec2(-1,-1)*pixel)),ne=luma(sampleAt(uv+vec2(1,-1)*pixel));
    float sw=luma(sampleAt(uv+vec2(-1,1)*pixel)),se=luma(sampleAt(uv+vec2(1,1)*pixel));
    float mid=luma(centre),lo=min(mid,min(min(nw,ne),min(sw,se))),hi=max(mid,max(max(nw,ne),max(sw,se)));
    if(hi-lo<max(.0312,hi*.125)) { FragColor=vec4(centre,1); return; }
    vec2 direction=vec2(-(nw+ne-sw-se),nw+sw-ne-se);
    float reduce=max((nw+ne+sw+se)*(.25*.125),1.0/128.0);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8),vec2(8))*pixel;
    vec3 a=.5*(sampleAt(uv+direction*(-1.0/6.0))+sampleAt(uv+direction*(1.0/6.0)));
    vec3 b=a*.5+.25*(sampleAt(uv-direction*.5)+sampleAt(uv+direction*.5));
    float lb=luma(b);
    FragColor=vec4(lb<lo || lb>hi ? a : b,1);
}
