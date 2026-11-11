#include "SceneFactories.h"
#include "SceneSupport.h"

using namespace sceneSupport;
namespace {
// FBO 只组织附件，本身不存像素；颜色纹理存图像，renderbuffer 存深度/模板数据。
struct RenderTarget {
    GLuint fbo=0, color=0, depthStencil=0;
    int width=0, height=0;
    RenderTarget() {
        glGenFramebuffers(1,&fbo); glGenTextures(1,&color);
        glGenRenderbuffers(1,&depthStencil);
    }
    ~RenderTarget() {
        glDeleteFramebuffers(1,&fbo); glDeleteTextures(1,&color);
        glDeleteRenderbuffers(1,&depthStencil);
    }
    RenderTarget(const RenderTarget&)=delete;
    RenderTarget& operator=(const RenderTarget&)=delete;
    void resize(int w,int h) {
        if (width==w && height==h) return;
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glBindTexture(GL_TEXTURE_2D,color);
        // nullptr 只分配存储，不上传图片；窗口尺寸变化后两个附件都要重新分配。
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        // 深度/模板不需要在后处理中采样，用 renderbuffer 即可；颜色则必须用纹理。
        glBindRenderbuffer(GL_RENDERBUFFER,depthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depthStencil);
        // 检查附件是否完整可绘制，只有成功分配 ID 并不代表 FBO 配置有效。
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Framebuffer scene: incomplete off-screen framebuffer");
        width=w; height=h;
    }
};

// RAII 保存上下文状态，即使分配/完整性检查抛异常，也恢复原 FBO、视口和写掩码。
struct RenderState {
    GLint drawFbo,readFbo,viewport[4],renderbuffer,activeTexture,texture0,depthFunc;
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),
              cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST),
              scissor=glIsEnabled(GL_SCISSOR_TEST),depthMask;
    GLfloat clearColor[4]; GLdouble clearDepth;
    RenderState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);
        glGetIntegerv(GL_VIEWPORT,viewport); glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture); glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);
        glGetIntegerv(GL_DEPTH_FUNC,&depthFunc); glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColor); glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clearDepth);
    }
    ~RenderState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture0); glActiveTexture(activeTexture);
        glDepthFunc(depthFunc); glDepthMask(depthMask);
        glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]); glClearDepth(clearDepth);
        restore(GL_DEPTH_TEST,depth); restore(GL_BLEND,blend); restore(GL_CULL_FACE,cull);
        restore(GL_STENCIL_TEST,stencil); restore(GL_SCISSOR_TEST,scissor);
    }
    static void restore(GLenum cap,GLboolean enabled) { if (enabled) glEnable(cap); else glDisable(cap); }
};

class FramebufferScene final : public CameraScene {
public:
    FramebufferScene(const std::string& root, int effect)
        : sceneShader_(loadShader(root,"framebuffers","scene")),
          screenShader_(loadShader(root,"framebuffers","screen")),
          cube_(cubeVertices(false),{3,2}),
          floor_(std::vector<float>{5,-.5f,5,2,0, -5,-.5f,5,0,0, -5,-.5f,-5,0,2,
                                   5,-.5f,5,2,0, -5,-.5f,-5,0,2, 5,-.5f,-5,2,2},{3,2}),
          screen_(std::vector<float>{-1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1},{2,2},{0,1,2,0,2,3}),
          marble_(root+"/resources/textures/matrix.jpg"),metal_(root+"/resources/textures/wood.png") {
        effect_=effect;
        sceneShader_->use(); sceneShader_->setInt("surfaceTexture",0);
        screenShader_->use(); screenShader_->setInt("screenTexture",0);
    }
    void render(const SceneFrame& frame) override {
        RenderState previous;
        const int w=previous.viewport[2],h=previous.viewport[3];
        if (w<=0 || h<=0) return; // 最小化窗口没有可绘制尺寸。
        target_.resize(w,h);
        // 第一遍：把普通三维场景画到离屏颜色纹理，深度测试仍依赖它的深度附件。
        glBindFramebuffer(GL_FRAMEBUFFER,target_.fbo); glViewport(0,0,w,h);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glClearColor(.08f,.1f,.14f,1); glClearDepth(1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        sceneShader_->use(); sceneShader_->setMat4("view",camera_.GetViewMatrix());
        sceneShader_->setMat4("projection",projection(frame));
        marble_.bind(0);
        draw(cube_,{-1,0,-1}); draw(cube_,{2,0,0});
        metal_.bind(0); draw(floor_,{0,0,0});
        // 第二遍：切回调用者的绘制 FBO（主循环中为窗口），采样上一遍颜色纹理。
        // 不能同时向同一纹理附件写入并采样，否则产生未定义的反馈环路。
        // 后处理只绘制屏幕四边形，无需深度；ImGui 随后单独绘制，不受效果影响。
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous.drawFbo);
        glViewport(previous.viewport[0],previous.viewport[1],w,h);
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        screenShader_->use(); screenShader_->setInt("effect",effect_);
        screenShader_->setFloat("strength",strength_); screenShader_->setFloat("radius",radius_);
        screenShader_->setInt("splitView",splitView_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,target_.color);
        screen_.draw();
    }
    void drawControls() override {
        const char* effects[]={"Original", "Invert", "Grayscale", "Sharpen", "Gaussian blur", "Edge detection", "Sepia"};
        ImGui::Combo("Post-processing",&effect_,effects,IM_ARRAYSIZE(effects));
        ImGui::SliderFloat("Effect strength",&strength_,0.f,1.f,"%.2f");
        if (effect_>=3 && effect_<=5) ImGui::SliderFloat("Kernel radius (pixels)",&radius_,1.f,4.f,"%.1f");
        ImGui::Checkbox("Split view: original left / effect right",&splitView_);
        ImGui::Text("Off-screen texture: %d x %d",target_.width,target_.height);
        ImGui::TextWrapped("Pass 1: scene -> RGB texture + depth/stencil renderbuffer. Pass 2: texture -> full-screen quad. Kernel offsets use actual texture pixels and follow window resizing.");
        ImGui::TextWrapped("WASD / right drag / wheel control the scene camera. Post-processing affects the rendered image, while the UI stays clear.");
        if (ImGui::Button("Reset post-processing")) { effect_=0; strength_=radius_=1; splitView_=false; }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) camera_=Camera(glm::vec3(0,0,3));
    }
private:
    void draw(const Mesh& mesh,glm::vec3 position) {
        sceneShader_->setMat4("model",glm::translate(glm::mat4(1),position)); mesh.draw();
    }
    std::unique_ptr<Shader> sceneShader_,screenShader_;
    Mesh cube_,floor_,screen_;
    Texture marble_,metal_;
    RenderTarget target_;
    int effect_=0;
    float strength_=1,radius_=1;
    bool splitView_=false;
};
} // namespace
std::unique_ptr<Scene> createFramebufferScene(const std::string& root, int effect) {
    return std::make_unique<FramebufferScene>(root, effect);
}
