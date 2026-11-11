#include "SceneFactories.h"
#include "SceneSupport.h"
#include <cmath>

using namespace sceneSupport;
namespace {
// 立方体贴图是一个纹理对象的六个面；采样时传入三维方向，而不是二维 UV。
class Cubemap {
public:
    explicit Cubemap(const std::string& root) {
        GLint binding=0; glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&binding);
        glGenTextures(1,&id_); glBindTexture(GL_TEXTURE_CUBE_MAP,id_);
        // 六面图片遵循天空盒约定，不能沿用二维纹理的垂直翻转，否则接缝和方向会错位。
        stbi_set_flip_vertically_on_load(false);
        try {
            // GL 的面枚举连续排列：+X、-X、+Y、-Y、+Z、-Z，文件顺序必须对应。
            const char* faces[]={"right.jpg","left.jpg","top.jpg","bottom.jpg","front.jpg","back.jpg"};
            int faceSize=0;
            for (int i=0;i<6;++i) {
                const std::string path=root+"/resources/textures/skybox/"+faces[i];
                int w=0,h=0,channels=0;
                unsigned char* data=stbi_load(path.c_str(),&w,&h,&channels,STBI_rgb_alpha);
                if (!data) throw std::runtime_error("Cannot load cubemap face: "+path);
                // 六面必须为同尺寸正方形，否则纹理不完整，无法正常采样。
                if (w!=h || (i && w!=faceSize)) {
                    stbi_image_free(data);
                    throw std::runtime_error("Cubemap faces must be equal-sized squares: "+path);
                }
                faceSize=w;
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
                stbi_image_free(data);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            // 三个坐标轴都钳制到边缘，防止跨面边缘采样时出现重复图像。
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        } catch (...) {
            // 加载任意一面失败时清理纹理，并恢复共享的解码开关和 GL 绑定。
            stbi_set_flip_vertically_on_load(true);
            glBindTexture(GL_TEXTURE_CUBE_MAP,binding); glDeleteTextures(1,&id_); throw;
        }
        stbi_set_flip_vertically_on_load(true);
        glBindTexture(GL_TEXTURE_CUBE_MAP,binding);
    }
    ~Cubemap() { glDeleteTextures(1,&id_); }
    Cubemap(const Cubemap&)=delete;
    Cubemap& operator=(const Cubemap&)=delete;
    void bind() const { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP,id_); }
private:
    GLuint id_=0;
};

// 单位球上位置向量就是朝外单位法线；环境贴图由反射/折射方向采样，无需展开二维 UV。
std::vector<float> sphereVertices() {
    constexpr int rings=32,segments=64;
    const float pi=3.14159265359f;
    std::vector<float> vertices;
    auto point=[&](int r,int s) {
        float latitude=pi*r/rings,longitude=2*pi*s/segments;
        return glm::vec3(std::sin(latitude)*std::cos(longitude),std::cos(latitude),std::sin(latitude)*std::sin(longitude));
    };
    // 每个顶点存放 position + normal，对应 Mesh 的 {3,3} 属性布局。
    auto emit=[&](glm::vec3 p) { vertices.insert(vertices.end(),{p.x,p.y,p.z,p.x,p.y,p.z}); };
    for (int r=0;r<rings;++r) for (int s=0;s<segments;++s) {
        auto a=point(r,s),b=point(r+1,s),c=point(r+1,s+1),d=point(r,s+1);
        if (r>0) { emit(a); emit(d); emit(b); }
        if (r<rings-1) { emit(d); emit(c); emit(b); }
    }
    return vertices;
}
// GL 状态属于上下文，不属于某个场景；RAII 在 render 退出时恢复被此场景修改的状态。
struct State {
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),
              seamless=glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS),mask;
    GLint function,active,binding;
    State() {
        glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); glGetIntegerv(GL_DEPTH_FUNC,&function);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active); glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&binding);
    }
    ~State() {
        glDepthFunc(function); glDepthMask(mask);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP,binding); glActiveTexture(active);
        restore(GL_DEPTH_TEST,depth); restore(GL_BLEND,blend); restore(GL_CULL_FACE,cull);
        restore(GL_TEXTURE_CUBE_MAP_SEAMLESS,seamless);
    }
    static void restore(GLenum cap,GLboolean enabled) { if(enabled) glEnable(cap); else glDisable(cap); }
};
class CubemapScene final : public CameraScene {
public:
    CubemapScene(const std::string& root,int cubeMode,int sphereMode)
        : objectShader_(loadShader(root,"cubemaps","object")),skyShader_(loadShader(root,"cubemaps","skybox")),
          cube_(cubeVertices(true),{3,3}),sphere_(sphereVertices(),{3,3}),environment_(root),
          cubeMode_(cubeMode),sphereMode_(sphereMode) {
        camera_=Camera(glm::vec3(0,0,5));
        objectShader_->use(); objectShader_->setInt("environmentMap",0);
        skyShader_->use(); skyShader_->setInt("environmentMap",0);
    }
    void render(const SceneFrame& frame) override {
        State previous;
        // 先画不透明物体并正常写深度；seamless 允许立方体贴图在相邻面之间过滤。
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        environment_.bind();
        const auto view=camera_.GetViewMatrix(),proj=projection(frame);
        objectShader_->use(); objectShader_->setMat4("view",view); objectShader_->setMat4("projection",proj);
        objectShader_->setVec3("cameraPosition",camera_.Position);
        draw(cube_,{-1.1f,0,0},1.3f,cubeMode_,cubeIor_);
        draw(sphere_,{1.1f,0,0},.8f,sphereMode_,sphereIor_);
        // 最后画天空盒：去掉 view 的平移，只保留旋转，模拟无限远背景。
        // 顶点 shader 输出 z=w，透视除法后深度为 1；LEQUAL 使它通过清屏深度 1，
        // 又被前面较近的物体挡住。关闭深度写入，避免背景污染后续深度内容。
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        skyShader_->use(); skyShader_->setMat4("view",glm::mat4(glm::mat3(view)));
        skyShader_->setMat4("projection",proj); cube_.draw();
    }
    void drawControls() override {
        const char* modes[]={"Reflection","Refraction"};
        ImGui::Combo("Cube surface",&cubeMode_,modes,2);
        if(cubeMode_==1) ImGui::SliderFloat("Cube refractive index",&cubeIor_,1.f,2.5f,"%.3f");
        ImGui::Combo("Sphere surface",&sphereMode_,modes,2);
        if(sphereMode_==1) ImGui::SliderFloat("Sphere refractive index",&sphereIor_,1.f,2.5f,"%.3f");
        ImGui::TextWrapped("Air = 1.000, water ~ 1.333, glass ~ 1.520, diamond ~ 2.420. Refraction uses air / material as the index ratio.");
        ImGui::TextWrapped("Both objects sample the same skybox. The sky rotates with your view but stays fixed when you move. WASD / right drag / wheel control the camera.");
        ImGui::TextWrapped("This is single-surface environment refraction: no glass thickness, second-surface ray tracing, or reflections of the other object.");
        if(ImGui::Button("Reset cubemap settings")) { cubeMode_=0; sphereMode_=1; cubeIor_=sphereIor_=1.52f; }
        ImGui::SameLine();
        if(ImGui::Button("Reset camera")) camera_=Camera(glm::vec3(0,0,5));
    }
private:
    void draw(const Mesh& mesh,glm::vec3 position,float scale,int mode,float ior) {
        // GLM 列向量从右向左作用：先缩放局部网格，再平移到世界位置。
        objectShader_->setMat4("model",glm::scale(glm::translate(glm::mat4(1),position),glm::vec3(scale)));
        objectShader_->setInt("surfaceMode",mode); objectShader_->setFloat("materialIor",ior); mesh.draw();
    }
    std::unique_ptr<Shader> objectShader_,skyShader_;
    Mesh cube_,sphere_;
    Cubemap environment_;
    int cubeMode_=0,sphereMode_=1;
    float cubeIor_=1.52f,sphereIor_=1.52f;
};
} // namespace
std::unique_ptr<Scene> createCubemapScene(const std::string& root,int cubeMode,int sphereMode) {
    return std::make_unique<CubemapScene>(root,cubeMode,sphereMode);
}
