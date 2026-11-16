#include "SceneFactories.h"
#include "SceneSupport.h"
#include "OrbitalPhase.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

using namespace sceneSupport;
namespace {
// 背景天空盒：直接加载六张独立的立方体贴图面。
// 文件按 GL 约定顺序排列：+X、-X、+Y、-Y、+Z、-Z。
//
// 注意 corona 素材的命名约定与 GL 相反：素材的 ft 朝 -Z、bk 朝 +Z。
// 所以这里 +Z 取 bk、-Z 取 ft，不能按字面顺序摆。
//
// up/dn 两面已经离线处理过：素材里这两面相对 GL cubemap 约定转了 90°，
// 原始图片直接上传会让顶部出现十字错缝。现存的 corona_up.png / corona_dn.png
// 已分别是原图逆时针、顺时针转 90° 后的结果（12 条棱的平均色差从 13.0/255 降到 1.4/255），
// 因此运行时按原样上传即可，不要再做任何旋转或镜像。
class SpaceCubemap {
public:
    explicit SpaceCubemap(const std::string& root) {
        GLint binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
        // 与 GL 的面顺序对齐：+X, -X, +Y, -Y, +Z, -Z
        // 注意 ft/bk 是反的：素材的 bk 朝 +Z、ft 朝 -Z。
        static const char* const files[6] = {
            "corona_rt.png", "corona_lf.png", "corona_up.png",
            "corona_dn.png", "corona_bk.png", "corona_ft.png",
        };
        // 天空盒图片遵循自身的朝向约定，不能沿用二维纹理的垂直翻转，
        // 否则接缝和上下方向会错位。该开关是 stbi_image 的全局状态，用完必须恢复。
        stbi_set_flip_vertically_on_load(false);
        try {
            int faceSize = 0;
            for (int i = 0; i < 6; ++i) {
                const std::string path = root + "/resources/textures/skybox_corona/" + files[i];
                int w = 0, h = 0, channels = 0;
                unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                if (!data) throw std::runtime_error("Cannot load skybox face: " + path);
                // 六个面必须同为正方形且尺寸一致，否则立方体贴图不完整。
                if (w != h || (i && w != faceSize)) {
                    stbi_image_free(data);
                    throw std::runtime_error("Skybox faces must be equal-sized squares: " + path);
                }
                faceSize = w;
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, w, h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            }
            // 星空是高频细节：缩小过滤用三线性 mipmap，避免远处闪烁噪点。
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // 三个轴都钳制到边缘，防止跨面边缘采样时出现重复图像。
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        } catch (...) {
            // 任意一面失败都要清理纹理、恢复全局解码开关和 GL 绑定，再继续抛出。
            stbi_set_flip_vertically_on_load(true);
            glBindTexture(GL_TEXTURE_CUBE_MAP, binding);
            glDeleteTextures(1, &id_);
            throw;
        }
        stbi_set_flip_vertically_on_load(true);
        glBindTexture(GL_TEXTURE_CUBE_MAP, binding);
    }
    ~SpaceCubemap() { glDeleteTextures(1, &id_); }
    SpaceCubemap(const SpaceCubemap&) = delete;
    SpaceCubemap& operator=(const SpaceCubemap&) = delete;
public:
    void bind(unsigned int unit) const {
        // 纹理单元是采样器的槽位；shader.setInt 里传的也是这个槽位编号。
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
    }
private:
    GLuint id_ = 0;
};

// 本场景的构造和绘制都会改变绑定；保存后恢复，避免影响场景切换和 ImGui。
struct State {
    GLint vao, buffer, program, active, texture, function;
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND),
              cull=glIsEnabled(GL_CULL_FACE), mask;
    State() {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao); glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_DEPTH_FUNC,&function); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
    }
    ~State() {
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer); glUseProgram(program);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(active);
        glDepthFunc(function); glDepthMask(mask);
        restore(GL_DEPTH_TEST,depth); restore(GL_BLEND,blend); restore(GL_CULL_FACE,cull);
    }
    static void restore(GLenum cap,GLboolean enabled) { if (enabled) glEnable(cap); else glDisable(cap); }
};

// 一颗行星的静态定义：贴图、半径、轨道半径、公转与自转速度。
// 半径和轨道半径都是场景单位，不是真实公里数——见下方 kPlanets 的注释。
struct PlanetDef {
    const char* texture;
    const char* name;      // 面板与镜头按钮上显示的名字
    float radius;          // 视觉半径（场景单位，非真实比例）
    float orbitRadius;     // 视觉轨道半径（场景单位，近似等差排布）
    float orbitSpeed;      // 公转角速度倍率（地球约 1 圈/100 秒时取 1.0）
    float spinSpeed;       // 自转角速度倍率
    float phaseDeg;        // 初始公转相位，避免所有行星排成一条线
    glm::vec3 tint;        // 贴图与顶点色相乘的色调（太阳用来做暖色偏移）
    bool emissive;         // 太阳自发光，不参与"背光面变暗"
};

// 半径做了非线性压缩：真实太阳半径是木星的 10 倍、地球的 109 倍，
// 海王星轨道又是水星的 77 倍远。按真实比例画出来，除太阳外全都只剩一个像素点。
// 这里保持轨道的相对顺序与疏密（内四颗紧凑、外四颗拉开），半径压缩到同一量级内可读。
constexpr float kEarthPeriod = 100.f;  // 地球公转一圈的参考秒数，只用于换算速度倍率

// 尺度说明：本场景不再追求真实比例，改用一套手工调好的视觉尺度。
//
// 为什么放弃真实比例：真实太阳远大于所有行星、日地距离是地球半径的
// 23481 倍。站在能看全海王星轨道的位置，内行星必然只有零点几个像素；
// 把太阳缩小到不遮挡则内行星更小。两个要求无法同时满足，所以直接按"看得清"排版。
//
// 设计原则：
//   · 半径只做适度区分——太阳明显最大，气态巨行星次之，内行星最小，
//     水星保留地球的 0.70 倍大小，方便总览与近距离观察。
//   · 轨道按完整径向包络排布，包含月球轨道、土星环和最大大气厚度，
//     保留"内密外疏"的观感，同时保证相邻天体不会挤在一起。
//   真实数据（km、AU）保留在 kPlanets 的注释里供对照，不参与计算。

const PlanetDef kPlanets[] = {
    // 半径与轨道都是"看起来合适"的数值，不是真实比例。
    // 真实数据对照：半径 km / 轨道 AU 写在注释里，仅作参考。
    // 贴图                     名称        半径  轨道   公转  自转  相位   色调                     自发光
    // 太阳只放到 8.0 而不是和其它天体一样翻倍：半径 12 会盖住水星轨道，
    // 让水星贴着太阳表面掠过。8.0 仍小于水星轨道（19.5）的一半，留出可见空隙。
    // 地球轨道两侧专门为月球预留空间；各轨道包络之间至少间隔 2 单位。
    {"2k_sun.jpg",         "Sun",       8.00f,  0.0f, 0.00f, 3.0f,   0.f, {1.00f,0.94f,0.80f}, true},  // 696340km
    {"2k_mercury.jpg",     "Mercury",   2.10f, 19.5f, 4.15f, 1.0f,  35.f, {1.00f,1.00f,1.00f}, false}, // 2440km  / 0.387AU
    {"2k_venus_surface.jpg","Venus",    2.80f, 29.0f, 1.62f, 0.6f, 120.f, {1.00f,0.97f,0.90f}, false}, // 6052km  / 0.723AU
    {"2k_earth_daymap.jpg","Earth",     3.00f, 46.0f, 1.00f, 1.0f, 210.f, {1.00f,1.00f,1.00f}, false}, // 6371km  / 1.000AU
    {"2k_mars.jpg",        "Mars",      2.40f, 64.0f, 0.53f, 1.0f, 300.f, {1.00f,1.00f,1.00f}, false}, // 3390km  / 1.524AU
    {"2k_jupiter.jpg",     "Jupiter",   7.20f, 87.0f, 0.30f, 2.4f,  60.f, {1.00f,1.00f,1.00f}, false}, // 69911km / 5.203AU
    {"2k_saturn.jpg",      "Saturn",    6.20f,119.0f, 0.20f, 2.2f, 155.f, {1.00f,0.97f,0.90f}, false}, // 58232km / 9.537AU
    {"2k_uranus.jpg",      "Uranus",    4.20f,151.0f, 0.14f, 1.4f, 250.f, {0.92f,1.00f,1.00f}, false}, // 25362km / 19.19AU
    {"2k_neptune.jpg",     "Neptune",   4.00f,177.0f, 0.11f, 1.4f, 335.f, {0.94f,0.96f,1.00f}, false}, // 24622km / 30.07AU
};
constexpr int kPlanetCount = static_cast<int>(sizeof(kPlanets)/sizeof(kPlanets[0]));
constexpr int kSunIndex = 0;
constexpr int kSaturnIndex = 6;
constexpr int kEarthIndex = 3;

// 土星环：按土星视觉半径（6.20）的倍数给，内缘 1.35 倍、外缘 2.35 倍。
// 真实主环是 1.28~2.41 倍土星半径，视觉上取整成这组数更整齐。
constexpr float kSaturnRingInner = 8.36f;   // 6.20 * 1.35
constexpr float kSaturnRingOuter = 14.56f;  // 6.20 * 2.35

// 月球：视觉半径 1.15，约为地球（3.00）的 2/5，保证定位后看得清；
// 轨道半径 8.80，既避开最大地球大气，也与相邻行星轨道保持间隔。
constexpr float kMoonRadius = 1.15f;
constexpr float kMoonOrbitRadius = 8.80f;

// 大气层/云层外壳的默认厚度（相对本体半径）。太薄看不到，
// 1.03 时地球在屏幕上只有 28 像素，露出的环不到 1 像素，等于没有；
// 1.12 能露出约 3 像素的一圈，既有存在感又不至于把本体包成一团。
// 面板可以实时调整，这里只是初值。
constexpr float kAtmosphereScale = 1.12f;
constexpr float kMaxAtmosphereScale = 1.6f;
// 月球公转周期 27.32 天，地球公转 365.25 天，所以月球绕地球约是地球绕太阳的 13.4 倍快。
constexpr float kMoonOrbitSpeed = 365.25f / 27.32f;
constexpr float kMoonPhaseDeg = 40.f;

// 最外层轨道 177，总览相机退到轨道外；镜头飞到天体外表面时最小距离也只有
// 几个单位，所以近平面要小、远平面留足余量。ratio 2e6 对 24 位深度缓冲仍够用。
constexpr float kNearPlane = 0.01f;
constexpr float kFarPlane = 2000.f;

// 顶点格式：位置、法线、UV。与 solar-system/body.vs 中的 layout(location) 一一对应。
struct Vertex { glm::vec3 position, normal; glm::vec2 uv; };

// 大气层/云层外壳的绘制参数。
// 云层使用贴图；薄雾用无贴图的边缘散射近似。水星和月球不添加可见大气。
struct AtmosphereDef {
    int bodyIndex;            // kPlanets 中的下标
    const char* texture;      // 云层贴图；nullptr 表示无贴图的薄雾光晕
    glm::vec3 color;          // 叠加色调：金星淡黄、地球淡蓝
    float opacity;            // 整体不透明度
    float luminanceAlpha;     // 贴图亮度转透明度的强度；0 表示不跟贴图走
    float rimPower;           // 菲涅尔指数，越大边缘光晕越窄
    float spinSpeed;          // 云层自转相对本体的倍率
};

// 大气参数全部可在面板上调整，这里只是初值。
// 数组本身不存可变参数，避免"表里的常量"和"面板上的值"两份状态打架；
// 面板直接改 Scene::atmospheres_ 里的副本。
const AtmosphereDef kAtmospheres[] = {
    // 地球云层：贴图中海洋/陆地接近全黑，用亮度当透明度正好让云浮在表面之上。
    {3, "2k_earth_clouds.jpg",   {1.00f,1.00f,1.00f}, 0.85f, 2.2f, 1.2f, 1.15f},
    // 金星大气：整张图都是亮的硫酸云，用较低亮度和偏黄的颜色叠出一层雾。
    {2, "2k_venus_atmosphere.jpg",{1.00f,0.92f,0.70f}, 0.55f, 0.55f, 0.8f, 0.55f},
    {3, nullptr, {0.28f,0.55f,1.00f}, 0.30f, 0.f, 2.5f, 1.f}, // Earth haze
    {4, nullptr, {0.90f,0.48f,0.25f}, 0.14f, 0.f, 3.0f, 1.f}, // Mars dust haze
    {5, nullptr, {0.90f,0.73f,0.52f}, 0.20f, 0.f, 2.8f, 1.f},
    {6, nullptr, {0.95f,0.81f,0.55f}, 0.20f, 0.f, 2.8f, 1.f},
    {7, nullptr, {0.35f,0.90f,0.95f}, 0.28f, 0.f, 2.5f, 1.f},
    {8, nullptr, {0.25f,0.43f,1.00f}, 0.30f, 0.f, 2.5f, 1.f},
};
constexpr int kAtmosphereCount = static_cast<int>(sizeof(kAtmospheres)/sizeof(kAtmospheres[0]));

// Conservative radial envelopes guarantee separation for every orbital phase,
// including the full lunar orbit, Saturn's tilted ring, and maximum UI shell size.
float orbitalEnvelope(int index) {
    float extent=kPlanets[index].radius;
    for(const auto& atmosphere : kAtmospheres)
        if(atmosphere.bodyIndex==index) extent=std::max(extent,kPlanets[index].radius*kMaxAtmosphereScale);
    if(index==kEarthIndex) extent=std::max(extent,kMoonOrbitRadius+kMoonRadius);
    if(index==kSaturnIndex) extent=std::max(extent,kSaturnRingOuter);
    return extent;
}
void validateOrbitalLayout() {
    for(int i=1;i<kPlanetCount;++i) {
        float clearance=kPlanets[i].orbitRadius-kPlanets[i-1].orbitRadius
                        -orbitalEnvelope(i)-orbitalEnvelope(i-1);
        if(clearance<2.f) throw std::runtime_error("Solar system orbital envelopes overlap");
    }
    if(kMoonOrbitRadius-kMoonRadius-kPlanets[kEarthIndex].radius*kMaxAtmosphereScale<2.f)
        throw std::runtime_error("Moon intersects Earth's atmosphere");
}

// 面板可调的运行时大气配置。kAtmospheres 只固定"哪颗行星、用哪张贴图"，
// 其余参数都从这里读，改动立刻生效，不会和常量表互相覆盖。
struct AtmosphereConfig {
    bool enabled = true;
    glm::vec3 color{1.f};
    float opacity = 0.85f;
    float luminanceAlpha = 1.0f;
    float rimPower = 1.2f;
    float scale = 1.12f;
    float spinSpeed = 1.0f;
};
// 行星贴图封装：本场景自用，不依赖 SceneSupport 里带交互面板的 Texture。
//
// 为什么不复用 Texture：那套 Texture 带 wrap/filter/border 的 ImGui 控件，
// 是给 Texture 教学场景准备的；太阳系只需要"加载好并按固定参数采样"。
// 自带一份封装后，采样策略写死在这里，不会因为别处调整面板而影响行星外观。
class PlanetTexture {
public:
    // 行星贴图是等距柱状投影：u=0 与 u=1 是同一条经线。
    //   S 方向必须 GL_REPEAT，接缝两侧才能连上；
    //   T 方向必须 GL_CLAMP_TO_EDGE，否则南极那一行会被卷回北极，
    //   极点附近出现一圈重复图像。
    // 缩小过滤用三线性 mipmap：行星在画面里可能只有几个像素，
    // 没有 mipmap 会闪烁成噪点。
    explicit PlanetTexture(const std::string& filename) {
        int width = 0, height = 0, channels = 0;
        // 图片以左上角为起点，而 OpenGL 的纹理坐标以左下角为起点，需要翻转行。
        // 统一解码成 RGBA，避免 RGB 行宽导致的默认 4 字节对齐问题。
        stbi_set_flip_vertically_on_load(true);
        unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("Cannot load planet texture: " + filename);
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        // GL_RGBA8 是 GPU 内部存储格式；GL_RGBA/GL_UNSIGNED_BYTE 描述 CPU 侧像素格式。
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        // 生成逐级缩小到 1x1 的 mipmap 链，供缩小过滤选择；只生成一次，不是每帧。
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    ~PlanetTexture() { glDeleteTextures(1, &id_); }
    PlanetTexture(const PlanetTexture&) = delete;
    PlanetTexture& operator=(const PlanetTexture&) = delete;
    void bind(unsigned int unit) const {
        // 纹理单元是采样器的槽位，不是纹理 ID；shader 里 setInt 传的也是这个编号。
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id_);
    }
private:
    GLuint id_ = 0;
};

// 程序生成的标准 UV 球体，供九颗天体共用。
//
// 为什么不读 planet.obj：那个模型的 UV 不是等距柱状布局——逐顶点比对可见
// u 与三维经度不是单调对应，接缝两侧的 u 被分到 0.009 与 0.991 却对应同一经度，
// 中间那一段贴图被完全跳过，行星表面会出现明显的错位接缝。
//
// UV 约定：u = 经度/360（0..1 绕赤道一圈，接缝处 u=0 与 u=1 是同一条经线），
//          v = 0.5 - 纬度/180（v=0 是北极，v=1 是南极，与常见行星贴图一致）。
// 接缝处的顶点必须拆成两份（u=0 和 u=1），否则三角形会横跨整张贴图。
//
// 经度起点取 +X 方向（θ=0 对应 x=+1、z=0），u=0.5 才是 +Z。
// 之所以不用 +Z 当起点：u=0 与 u=1 落在同一条经线上，若让 u=0 指向 +Z，
// 接缝就会正好穿过默认机位正对的位置，接缝瑕疵最容易被看见。
class UVSphere {
public:
    UVSphere(int rings,int segments) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(static_cast<size_t>(rings+1)*(segments+1));
        for (int r=0;r<=rings;++r) {
            const float v=static_cast<float>(r)/rings;
            const float phi=v*glm::pi<float>();          // 0=北极, pi=南极
            const float y=std::cos(phi);
            const float ringRadius=std::sin(phi);
            for (int s=0;s<=segments;++s) {
                // 每一圈都生成 segments+1 个顶点，最后一个与第一个位置相同但 u=1，
                // 这样接缝两侧各自采样贴图的两端，纹理才连续。
                const float u=static_cast<float>(s)/segments;
                const float theta=u*2.f*glm::pi<float>();
                Vertex vertex;
                vertex.position=glm::vec3(std::cos(theta)*ringRadius,y,-std::sin(theta)*ringRadius);
                vertex.normal=vertex.position;            // 单位球上位置就是法线
                vertex.uv=glm::vec2(u,v);
                vertices.push_back(vertex);
            }
        }
        const unsigned int stride=static_cast<unsigned int>(segments+1);
        for (int r=0;r<rings;++r) {
            for (int s=0;s<segments;++s) {
                const unsigned int a=static_cast<unsigned int>(r)*stride+static_cast<unsigned int>(s);
                const unsigned int b=a+stride;
                // 逆时针绕序，法线朝外。
                indices.insert(indices.end(),{a,b,a+1, a+1,b,b+1});
            }
        }
        count_=static_cast<GLsizei>(indices.size());
        glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glGenBuffers(1,&ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);
        const size_t offsets[]={offsetof(Vertex,position),offsetof(Vertex,normal),offsetof(Vertex,uv)};
        for (unsigned int i=0;i<3;++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i,i==2 ? 2 : 3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsets[i]));
        }
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }
    ~UVSphere() {
        glDeleteBuffers(1,&ebo_); glDeleteBuffers(1,&vbo_); glDeleteVertexArrays(1,&vao_);
    }
    UVSphere(const UVSphere&)=delete;
    UVSphere& operator=(const UVSphere&)=delete;
    void draw() const {
        glBindVertexArray(vao_); glDrawElements(GL_TRIANGLES,count_,GL_UNSIGNED_INT,nullptr);
    }
private:
    GLuint vao_=0,vbo_=0,ebo_=0;
    GLsizei count_=0;
};

// 土星环：一个环形平面（没有厚度），只有内外两圈各 64 段。
// 每段画两个三角形，一共 128 个三角形；相比给环做实体几何，这样既省又够看。
// 顶点属性：(x,z) 平面坐标 + 该顶点的半径，半径在顶点着色器里映射成环带的 u 坐标。
class RingMesh {
public:
    // inner/outer 直接是场景单位，与土星视觉半径配套。
    explicit RingMesh(float inner,float outer,int segments=64)
        : inner_(inner),outer_(outer) {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        for (int i=0;i<=segments;++i) {
            const float angle=6.28318530718f*i/segments;
            const float s=std::sin(angle),c=std::cos(angle);
            // 每圈两个顶点：先内缘后外缘，索引按 i*2 排列。
            // 顶点位置直接使用场景单位，环不再依赖外层的等比缩放。
            vertices.insert(vertices.end(),{s*inner_,c*inner_,inner_});
            vertices.insert(vertices.end(),{s*outer_,c*outer_,outer_});
        }
        for (int i=0;i<segments;++i) {
            const unsigned int base=static_cast<unsigned int>(i)*2;
            // 逆时针绕序，从 +Y 方向看是正面；双面可见所以不依赖面剔除。
            indices.insert(indices.end(),{base,base+1,base+3, base,base+3,base+2});
        }
        indexCount_=static_cast<GLsizei>(indices.size());
        glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glGenBuffers(1,&ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);
        // 每顶点 3 个 float：x、z、radius。步长按 3 个 float 算。
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
        // 半径属性绑定到 location 2，而不是 1。
        // 原因是环形网格和行星球体共用一套属性编号约定：球体把 location 0/1/2 用作
        // position/normal/uv。如果环也占 location 1 且给 1 个分量，与球体的 vec3 布局
        // 冲突，切换绘制时驱动会按错误的大小读取，直接崩溃。
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,3*sizeof(float),
                              reinterpret_cast<void*>(2*sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }
    ~RingMesh() {
        glDeleteBuffers(1,&ebo_); glDeleteBuffers(1,&vbo_); glDeleteVertexArrays(1,&vao_);
    }
    RingMesh(const RingMesh&)=delete;
    RingMesh& operator=(const RingMesh&)=delete;
    void draw() const {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES,indexCount_,GL_UNSIGNED_INT,nullptr);
    }
    float inner() const { return inner_; }
    float outer() const { return outer_; }
private:
    GLuint vao_=0,vbo_=0,ebo_=0;
    GLsizei indexCount_=0;
    float inner_,outer_;
};

// 太阳系：一颗太阳 + 八颗行星 + 月球，全部共用同一份程序生成的单位球。
// 使用强调可读性、并经过轨道包络校验的视觉尺度；
// 面板上的天体按钮可以把相机直接送到任意一颗星球旁边。
class SolarSystemScene final : public CameraScene {
public:
    explicit SolarSystemScene(const std::string& root)
        : shader_(loadShader(root,"solar-system","body")),
          skyShader_(loadShader(root,"solar-system","skybox")),
          ringShader_(loadShader(root,"solar-system","ring")),
          atmosphereShader_(loadShader(root,"solar-system","atmosphere")),
          sky_(std::make_unique<Mesh>(skyboxVertices(),std::initializer_list<int>{3})),
          space_(root),
          // 所有行星共用同一份程序生成的单位球：48 圈 x 96 段，接缝与两极 UV 都标准。
          sphere_(48,96),
          ring_(kSaturnRingInner,kSaturnRingOuter),
          moonTexture_(std::make_unique<PlanetTexture>(root+"/resources/models/planet/2k_moon.jpg")) {
        validateOrbitalLayout();
        // 按 kPlanets 的顺序加载九张贴图。纹理对象不可拷贝，用 unique_ptr 容器持有。
        // 采样参数（横 REPEAT、纵 CLAMP、三线性 mipmap）由 PlanetTexture 自己固定，
        // 不受 Texture 教学场景的面板设置影响。
        for (int i=0;i<kPlanetCount;++i) {
            const std::string path=root+"/resources/models/planet/"+kPlanets[i].texture;
            planetTextures_.emplace_back(std::make_unique<PlanetTexture>(path));
        }
        // 大气层/云层贴图按 kAtmospheres 的顺序加载，与行星贴图分开保存。
        for (int i=0;i<kAtmosphereCount;++i) {
            if(kAtmospheres[i].texture)
                atmosphereTextures_.emplace_back(std::make_unique<PlanetTexture>(root+"/resources/models/planet/"+kAtmospheres[i].texture));
            else atmosphereTextures_.emplace_back(nullptr);
            // 运行时配置用常量表里的初值填好，之后由面板修改。
            AtmosphereConfig cfg;
            cfg.color=kAtmospheres[i].color;
            cfg.opacity=kAtmospheres[i].opacity;
            cfg.luminanceAlpha=kAtmospheres[i].luminanceAlpha;
            cfg.rimPower=kAtmospheres[i].rimPower;
            cfg.spinSpeed=kAtmospheres[i].spinSpeed;
            atmospheres_.push_back(cfg);
        }
        resetCamera();
        shader_->use(); shader_->setInt("bodyTexture",0); shader_->setInt("emissive",0);
        // 光照参数的初值；每帧 drawPlanet 会按天体位置重新设置 sunDirection。
        shader_->setVec3("sunDirection",glm::vec3(0,0,1));
        shader_->setFloat("ambient",ambient_);
        // 采样器 uniform 保存的是纹理单元编号（0），不是纹理 ID；每帧只需重新绑定纹理。
        skyShader_->use(); skyShader_->setInt("spaceMap",0);
        atmosphereShader_->use(); atmosphereShader_->setInt("atmosphereTexture",0);
    }
    void input(const SceneInput& input,float dt) override {
        SceneInput motion=input;
        motion.scroll=0;
        CameraScene::input(motion,dt);
        // The overview uses a wider field of view than the other tutorial scenes.
        camera_.Zoom=std::clamp(camera_.Zoom-input.scroll,1.f,75.f);
    }
    void render(const SceneFrame& frame) override {
        State previous;
        if (animate_) {
            const double rate=(360.0/kEarthPeriod)*(speed_/5.0);
            for(int i=0;i<kPlanetCount;++i)
                orbitPhases_[i].advance(frame.deltaTime,rate*kPlanets[i].orbitSpeed);
            moonPhase_.advance(frame.deltaTime,rate*kMoonOrbitSpeed);
        }
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        // 立方体贴图相邻面之间做无缝过滤，否则天空盒边缘可能出现细线。
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        // 先画天空盒、再画不透明物体，可以避免大量被遮挡的片元仍然执行着色。
        // 天空盒写入的深度是 1.0（顶点着色器把 z 设为 w），物体只要比远平面更近就会覆盖它。
        // 天空盒可以关掉：关掉后背景就是清屏色，便于观察天体轮廓和光照。
        if (skybox_) drawSky(frame);
        shader_->use(); shader_->setMat4("view",camera_.GetViewMatrix());
        shader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),frame.aspect,kNearPlane,kFarPlane));
        // 行星都用同一个球体网格：orbit uniform 保持单位矩阵，几何全部写进 model。
        shader_->setMat4("orbit",glm::mat4(1)); shader_->setInt("instanced",false);
        // 九颗天体逐个绘制：它们共用同一个球体网格与着色器，只是贴图、缩放和位置不同。
        for (int i=0;i<kPlanetCount;++i) drawPlanet(i,frame.deltaTime);
        // 月球紧跟在地球之后绘制：它绕地球转，位置依赖地球当帧的位置。
        drawMoon();
        // 大气层/云层必须在不透明天体之后绘制：它们是半透明的，
        // 先画会被深度测试挡掉一部分，先于本体画又会把本体盖住。
        drawAtmospheres(frame);
        // 土星环要压在行星之后画：它带透明，必须在不透明物体全部写完之后再混合，
        // 否则环后面那半个土星会被深度测试提前挡掉，出现"环缺一段"的破图。
        drawSaturnRing(frame);
    }
    void drawControls() override {
        ImGui::Checkbox("Show skybox",&skybox_);
        ImGui::Checkbox("Animate orbits",&animate_);
        ImGui::SliderFloat("Animation speed (5 = normal)",&speed_,0.f,20.f,"%.1f");
        ImGui::Checkbox("Spin planets",&spinEnabled_);
        ImGui::SliderFloat("Ring opacity",&ringOpacity_,0.f,1.f,"%.2f");

        // ===== 太阳光照 =====
        // 每颗行星的受光方向由它相对太阳的位置实时算出，太阳固定在原点。
        ImGui::SeparatorText("Sun lighting");
        ImGui::SliderFloat("Ambient light",&ambient_,0.f,0.5f,"%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("0 = the side facing away from the Sun is completely black\n"
                              "higher values lift the dark side so surface detail stays readable");
        if (ImGui::Button("Real sunlight (ambient 0)")) ambient_=0.f;
        ImGui::SameLine();
        if (ImGui::Button("Soft fill (0.25)")) ambient_=0.25f;

        // ===== 大气层 =====
        // 每个配置了大气层的天体一个折叠页，参数与常量表一一对应。
        ImGui::SeparatorText("Atmosphere");
        for (int i=0;i<kAtmosphereCount;++i) {
            AtmosphereConfig& cfg=atmospheres_[i];
            const char* label=kPlanets[kAtmospheres[i].bodyIndex].name;
            ImGui::PushID(i);
            if (!ImGui::TreeNode(kAtmospheres[i].bodyIndex==kEarthIndex && !kAtmospheres[i].texture ? "Earth haze" : label)) { ImGui::PopID(); continue; }
            ImGui::Checkbox("Enabled",&cfg.enabled);
            ImGui::SliderFloat("Opacity",&cfg.opacity,0.f,1.f,"%.2f");
            ImGui::SliderFloat("Thickness",&cfg.scale,1.f,kMaxAtmosphereScale,"%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shell radius relative to the planet.\n"
                                  "1.00 hides the atmosphere entirely, 1.12 is a visible halo.");
            ImGui::SliderFloat("Rim power",&cfg.rimPower,0.3f,4.f,"%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Lower = the glow spreads further toward the centre.\n"
                                  "Higher = only a thin bright edge.");
            if(kAtmospheres[i].texture) ImGui::SliderFloat("Texture alpha",&cfg.luminanceAlpha,0.f,3.f,"%.2f");
            if (kAtmospheres[i].texture && ImGui::IsItemHovered())
                ImGui::SetTooltip("How strongly the texture brightness becomes opacity.\n"
                                  "0 = uniform shell, higher = only the bright parts show.");
            if(kAtmospheres[i].texture) ImGui::SliderFloat("Cloud spin",&cfg.spinSpeed,0.f,3.f,"%.2f");
            ImGui::ColorEdit3("Tint",&cfg.color.x,ImGuiColorEditFlags_NoInputs);
            if (ImGui::Button("Reset")) {
                const AtmosphereDef& def=kAtmospheres[i];
                cfg=AtmosphereConfig{};
                cfg.color=def.color; cfg.opacity=def.opacity;
                cfg.luminanceAlpha=def.luminanceAlpha; cfg.rimPower=def.rimPower;
                cfg.spinSpeed=def.spinSpeed;
            }
            ImGui::TreePop();
            ImGui::PopID();
        }

        ImGui::TextWrapped("All ten bodies share one procedurally generated unit sphere; each is drawn with its own scale, so planet size never needs a second model.");
        ImGui::TextWrapped("Sizes and orbits are hand-tuned for readability, not to scale: the Sun is the largest body, the gas giants come next, and the inner planets are the smallest - but every one of them is still clearly visible. Orbital envelopes include the Moon, rings and maximum atmosphere thickness, with at least 2 units clearance.");
        ImGui::TextWrapped("Each body is lit by the Sun only: the shader takes the direction from the body to the Sun, so the lit hemisphere always faces the Sun and changes as the planet orbits. Ambient 0 makes the far side fully black.");
        ImGui::TextWrapped("The Moon orbits Earth and is tidally locked, so it always keeps the same face toward Earth; its spin therefore equals its orbital angle.");
        ImGui::TextWrapped("The sphere's UVs are laid out as a proper equirectangular map, so planet textures wrap without a seam and the poles do not repeat.");
        ImGui::TextWrapped("The starfield is a skybox: a cube whose vertex positions double as cubemap sampling directions, drawn with depth z=w so it always sits at the far plane.");
        ImGui::TextWrapped("WASD / right drag / wheel control the camera. Orbit animation is off by default.");
        if (ImGui::Button("Reset solar system")) {
            orbitPhases_={}; moonPhase_={}; animate_=false; speed_=5; spinEnabled_=true; ringOpacity_=.85f;
            // 默认回到"真实太阳光"：背光面全黑。
            ambient_=0.f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) resetCamera();
    }
private:
    // 某颗行星的公转角（弧度）。速度倍率以地球为 1 基准：地球 speed=1 时
    // 绕太阳一圈大约 kEarthPeriod 秒，其他行星按 kPlanets 里的倍率缩放。
    float planetOrbitAngle(int index) const {
        return orbitPhases_[index].radians(kPlanets[index].phaseDeg);
    }
    // 某颗行星在世界空间中的位置：绕着 Y 轴、半径 orbitRadius 的圆周运动。
    glm::vec3 planetPosition(int index) const {
        const float a=planetOrbitAngle(index);
        const float r=kPlanets[index].orbitRadius;
        return glm::vec3(std::sin(a)*r,0.f,std::cos(a)*r);
    }
    // 月球相对地球的位置：在地球轨道平面内绕地球转一圈。
    // 月球以 kMoonOrbitSpeed 倍速独立累计相位，与行星相位各自连续循环。
    glm::vec3 moonOffset() const {
        const float a=moonPhase_.radians(kMoonPhaseDeg);
        return glm::vec3(std::sin(a)*kMoonOrbitRadius,0.f,std::cos(a)*kMoonOrbitRadius);
    }
    void drawPlanet(int index,float deltaTime) {
        const PlanetDef& def=kPlanets[index];
        const glm::vec3 worldPos=planetPosition(index);
        // 太阳固定在原点，所以"指向太阳"的方向就是 -世界位置。
        // 太阳自己不需要受光（emissive），传什么都无所谓，统一传一个合法方向。
        setSunLight(worldPos);
        // 自转单独累计：公转靠角度推导，自转要一直转下去，
        // 用 deltaTime 累积才不会因为暂停/恢复而跳变。
        if (spinEnabled_) spinDeg_[index]=std::fmod(spinDeg_[index]+deltaTime*speed_*def.spinSpeed*12.f,360.f);
        // GLM 列主序右乘先作用，所以这里等价于：先按半径缩放 -> 绕 Y 轴自转 -> 平移到轨道位置。
        auto model=glm::translate(glm::mat4(1),worldPos);
        model=glm::rotate(model,glm::radians(spinDeg_[index]),glm::vec3(0,1,0));
        model=glm::scale(model,glm::vec3(def.radius));
        shader_->setMat4("model",model);
        shader_->setVec3("surfaceTint",def.tint);
        planetTextures_[index]->bind(0);
        // 太阳自发光：贴图本身就很亮，再乘方向光会一半黑一半亮，看起来像被切成两半。
        // 用"自发光标记"让着色器跳过受光衰减，其余行星仍走正常的漫反射。
        shader_->setInt("emissive",def.emissive ? 1 : 0);
        sphere_.draw();
    }
    // 月球：位置 = 地球当前位置 + 月球相对地球的偏移。
    // 月球被潮汐锁定，永远同一面朝向地球，所以自转角直接等于它绕地球的公转角，
    // 恰好抵掉公转带来的朝向变化——不需要额外的自转累积变量。
    void drawMoon() {
        const float orbitAngle=moonPhase_.radians(kMoonPhaseDeg);
        const glm::vec3 worldPos=planetPosition(kEarthIndex)+moonOffset();
        // 月球同样只被太阳照亮；地球挡住的日食效果不在本场景范围内。
        setSunLight(worldPos);
        auto model=glm::translate(glm::mat4(1),worldPos);
        // 面向地球的朝向：把球体绕 Y 轴转到与公转角一致。
        model=glm::rotate(model,orbitAngle,glm::vec3(0,1,0));
        model=glm::scale(model,glm::vec3(kMoonRadius));
        shader_->setMat4("model",model);
        shader_->setVec3("surfaceTint",glm::vec3(1.f));
        moonTexture_->bind(0);
        // 月球照常受光，不发光。
        shader_->setInt("emissive",0);
        sphere_.draw();
    }
    // 绘制所有配置了大气层/云层的天体。
    // 每个外壳都是在行星本体外面套一层略大的球，开启混合但关闭深度写入：
    //   · 关闭写入：外壳不会挡住后面较远的天体；
    //   · 开启混合：边缘的光晕与云层自然叠在本体上。
    void drawAtmospheres(const SceneFrame& frame) {
        if (kAtmosphereCount==0) return;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        atmosphereShader_->use();
        atmosphereShader_->setMat4("view",camera_.GetViewMatrix());
        atmosphereShader_->setMat4("projection",
            glm::perspective(glm::radians(camera_.Zoom),frame.aspect,kNearPlane,kFarPlane));
        atmosphereShader_->setVec3("cameraPosition",camera_.Position);
        std::vector<int> order;
        for(int i=0;i<kAtmosphereCount;++i) order.push_back(i);
        std::stable_sort(order.begin(),order.end(),[&](int a,int b) {
            float da=glm::length(camera_.Position-planetPosition(kAtmospheres[a].bodyIndex));
            float db=glm::length(camera_.Position-planetPosition(kAtmospheres[b].bodyIndex));
            return da==db ? atmospheres_[a].scale>atmospheres_[b].scale : da>db;
        });
        for (int i : order) {
            // 用运行时副本而不是常量表：面板上的调整要立刻生效。
            const AtmosphereConfig& cfg=atmospheres_[i];
            if (!cfg.enabled) continue;
            const AtmosphereDef& def=kAtmospheres[i];
            const PlanetDef& body=kPlanets[def.bodyIndex];
            const glm::vec3 worldPos=planetPosition(def.bodyIndex);
            // 与本体一致的自转角，再乘云层自身的速度倍率，让云相对地面缓慢移动。
            const float spin=spinDeg_[def.bodyIndex]*cfg.spinSpeed;
            auto model=glm::translate(glm::mat4(1),worldPos);
            model=glm::rotate(model,glm::radians(spin),glm::vec3(0,1,0));
            // 外壳比本体略大，这样本体边缘会露出一圈大气光晕。
            model=glm::scale(model,glm::vec3(body.radius*cfg.scale));
            atmosphereShader_->setMat4("model",model);
            // 光照方向和本体一致：太阳在原点。
            atmosphereShader_->setVec3("sunDirection",glm::normalize(-worldPos));
            atmosphereShader_->setVec3("atmosphereColor",cfg.color);
            atmosphereShader_->setFloat("opacity",cfg.opacity);
            atmosphereShader_->setFloat("luminanceAlpha",cfg.luminanceAlpha);
            atmosphereShader_->setFloat("rimPower",cfg.rimPower);
            atmosphereShader_->setInt("texturedCloud",atmosphereTextures_[i] ? 1 : 0);
            if(atmosphereTextures_[i]) atmosphereTextures_[i]->bind(0);
            else planetTextures_[def.bodyIndex]->bind(0); // valid sampler binding; haze does not sample it
            sphere_.draw();
        }
        // 恢复不透明绘制状态，后面的土星环与下一帧都要用。
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        shader_->use();
    }
    void drawSaturnRing(const SceneFrame& frame) {
        const glm::vec3 center=planetPosition(kSaturnIndex);
        // 环平面与土星赤道一致：绕 Z 轴倾斜一点，看起来才是侧视而不是正面圆盘。
        // 环的顶点已经按真实半径生成，这里只做平移与倾斜，不再缩放。
        auto model=glm::translate(glm::mat4(1),center);
        model=glm::rotate(model,glm::radians(26.7f),glm::vec3(0,0,1));
        // 环带是透明的，必须：开混合、按 alpha 混合、且不写深度（否则会挡住后面的天体）。
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        ringShader_->use();
        ringShader_->setMat4("model",model);
        ringShader_->setMat4("view",camera_.GetViewMatrix());
        ringShader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),frame.aspect,kNearPlane,kFarPlane));
        ringShader_->setFloat("innerRadius",ring_.inner());
        ringShader_->setFloat("outerRadius",ring_.outer());
        ringShader_->setFloat("opacity",ringOpacity_);
        ringShader_->setVec3("baseColor",glm::vec3(.86f,.80f,.68f));
        ring_.draw();
        // 恢复成不透明绘制状态。
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        shader_->use();
    }
    // 立方体顶点以原点为中心、边长为 2。顶点位置被天空盒顶点着色器当作采样方向使用。
    static std::vector<float> skyboxVertices() {
        return {
            -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1, // 背面
            -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1, // 正面
            -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1, // 左面
             1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1, // 右面
            -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1, // 底面
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1, // 顶面
        };
    }
    void drawSky(const SceneFrame& frame) {
        const auto view=camera_.GetViewMatrix();
        // glDepthMask(GL_FALSE) 后所有物体都不再写深度，天空盒画完必须立刻恢复，
        // 否则后面的行星之间会互相穿透、遮挡关系全错。
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        skyShader_->use();
        // 只保留观察矩阵的旋转部分（mat3 去掉平移列），天空盒就永远以相机为中心，看起来无限远。
        skyShader_->setMat4("view",glm::mat4(glm::mat3(view)));
        // 远平面与物体共用：天空盒深度恒为 1，只要远平面比最远行星更远，球壳就不会被裁掉。
        skyShader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),frame.aspect,kNearPlane,kFarPlane));
        space_.bind(0);
        sky_->draw();
        // 恢复成常规的不透明绘制状态，同时把深度测试恢复为 LESS，供后面的物体使用。
        glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    }
    // 天体半径使用场景单位。
    // 按天体当前世界位置设置太阳光照参数。
    // 太阳在原点，因此"指向太阳"的方向 = -position 归一化。
    // 环境光由面板控制：设为 0 就是"照不到的地方全黑"。
    void setSunLight(const glm::vec3& worldPos) {
        const glm::vec3 toSun=glm::normalize(-worldPos);
        shader_->setVec3("sunDirection",toSun);
        shader_->setFloat("ambient",ambient_);
    }
    void resetCamera() {
        // 全局视角：退到最外层轨道外侧俯视整个太阳系。
        const float far=kPlanets[kPlanetCount-1].orbitRadius;
        camera_=Camera(glm::vec3(0,far*.95f,far*1.6f),glm::vec3(0,1,0),-90.f,-31.f);
        camera_.Zoom=65.f;
        camera_.MovementSpeed=std::max(0.5f,far*0.35f);
    }
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> skyShader_;
    std::unique_ptr<Shader> ringShader_;
    std::unique_ptr<Shader> atmosphereShader_;
    std::unique_ptr<Mesh> sky_;
    SpaceCubemap space_;
    UVSphere sphere_;
    // 九张行星贴图按 kPlanets 顺序排列；纹理对象不可拷贝，用 unique_ptr 容器持有。
    std::vector<std::unique_ptr<PlanetTexture>> planetTextures_;
    // 大气层/云层贴图，顺序与 kAtmospheres 一致。
    std::vector<std::unique_ptr<PlanetTexture>> atmosphereTextures_;
    // 面板可调的大气参数，与 atmosphereTextures_ 一一对应。
    std::vector<AtmosphereConfig> atmospheres_;
    RingMesh ring_;
    std::unique_ptr<PlanetTexture> moonTexture_;
    bool animate_=false,spinEnabled_=true;
    std::array<solarSystem::OrbitalPhase,kPlanetCount> orbitPhases_{};
    solarSystem::OrbitalPhase moonPhase_;
    float speed_=5;
    float ringOpacity_=.85f;
    // 环境光强度：0 表示背向太阳的一面完全不亮，模拟真实太阳光照。
    float ambient_=0.f;
    // 天空盒开关：关掉后只画天体，背景是清屏色。
    bool skybox_=true;
    float spinDeg_[kPlanetCount]={0};
};
} // namespace

std::unique_ptr<Scene> createSolarSystemScene(const std::string& root) {
    State previous;
    return std::make_unique<SolarSystemScene>(root);
}
