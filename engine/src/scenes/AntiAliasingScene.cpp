#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
// Smooth surfaces share positions with analytic normals; enough segments keep
// polygon faceting from dominating the pixel-coverage comparison.
std::vector<float> curvedVertices(bool torus) {
    constexpr int rows=40,columns=80;
    constexpr float pi=3.14159265359f;
    std::vector<float> vertices;
    auto emit=[&](int row,int column) {
        float u=2*pi*column/columns,v=pi*row/rows;
        glm::vec3 normal,position;
        if(torus) {
            v*=2;
            normal={std::cos(v)*std::cos(u),std::cos(v)*std::sin(u),std::sin(v)};
            position={(.65f+.19f*std::cos(v))*std::cos(u),(.65f+.19f*std::cos(v))*std::sin(u),.19f*std::sin(v)};
        } else {
            normal={std::sin(v)*std::cos(u),std::cos(v),std::sin(v)*std::sin(u)};
            position=normal;
        }
        vertices.insert(vertices.end(),{position.x,position.y,position.z,normal.x,normal.y,normal.z});
    };
    for(int r=0;r<rows;++r) for(int c=0;c<columns;++c) {
        if(torus || r>0) { emit(r,c); emit(r,c+1); emit(r+1,c); }
        if(torus || r<rows-1) { emit(r,c+1); emit(r+1,c+1); emit(r+1,c); }
    }
    return vertices;
}
std::vector<float> pyramidVertices() {
    const glm::vec3 points[]={{-.7f,-.65f,.7f},{.7f,-.65f,.7f},{.7f,-.65f,-.7f},{-.7f,-.65f,-.7f},{0,.9f,0}};
    const int triangles[][3]={{0,1,4},{1,2,4},{2,3,4},{3,0,4},{0,3,2},{0,2,1}};
    std::vector<float> vertices;
    for(const auto& triangle : triangles) {
        auto normal=glm::normalize(glm::cross(points[triangle[1]]-points[triangle[0]],points[triangle[2]]-points[triangle[0]]));
        for(int index : triangle) {
            auto p=points[index]; vertices.insert(vertices.end(),{p.x,p.y,p.z,normal.x,normal.y,normal.z});
        }
    }
    return vertices;
}

// Renderbuffer color/depth attachments must use identical sample counts.
// Resolve into a regular texture before display or FXAA; MSAA resolve cannot resize.
struct Target {
    GLuint fbo=0,color=0,depth=0,resolved=0,texture=0;
    int width=0,height=0,samples=-1,outWidth=0,outHeight=0;
    Target() {
        glGenFramebuffers(1,&fbo); glGenFramebuffers(1,&resolved);
        glGenRenderbuffers(1,&color); glGenRenderbuffers(1,&depth); glGenTextures(1,&texture);
    }
    ~Target() {
        glDeleteFramebuffers(1,&fbo); glDeleteFramebuffers(1,&resolved);
        glDeleteRenderbuffers(1,&color); glDeleteRenderbuffers(1,&depth); glDeleteTextures(1,&texture);
    }
    Target(const Target&)=delete; Target& operator=(const Target&)=delete;
    void allocate(int w,int h,int n,int ow,int oh) {
        if(w==width && h==height && n==samples && ow==outWidth && oh==outHeight) return;
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glBindRenderbuffer(GL_RENDERBUFFER,color);
        if(n>1) glRenderbufferStorageMultisample(GL_RENDERBUFFER,n,GL_RGBA8,w,h);
        else glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
        glBindRenderbuffer(GL_RENDERBUFFER,depth);
        if(n>1) glRenderbufferStorageMultisample(GL_RENDERBUFFER,n,GL_DEPTH24_STENCIL8,w,h);
        else glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
        check();
        glBindFramebuffer(GL_FRAMEBUFFER,resolved); glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,ow,oh,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(); width=w; height=h; samples=n; outWidth=ow; outHeight=oh;
    }
    static void check() {
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Anti-aliasing framebuffer incomplete");
    }
    void resolve() const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resolved);
        glBlitFramebuffer(0,0,width,height,0,0,outWidth,outHeight,GL_COLOR_BUFFER_BIT,
                          samples>1 ? GL_NEAREST : GL_LINEAR);
    }
};
struct RenderState {
    GLint drawFbo,readFbo,viewport[4],renderbuffer,activeTexture,texture0,depthFunc;
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),
              cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST),
              scissor=glIsEnabled(GL_SCISSOR_TEST),multisample=glIsEnabled(GL_MULTISAMPLE),depthMask;
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
        restore(GL_MULTISAMPLE,multisample); restore(GL_STENCIL_TEST,stencil); restore(GL_SCISSOR_TEST,scissor);
    }
    static void restore(GLenum cap,GLboolean enabled) { if (enabled) glEnable(cap); else glDisable(cap); }
};

class AntiAliasingScene final : public CameraScene {
public:
    AntiAliasingScene(const std::string& root,int mode)
        : object_(loadShader(root,"anti-aliasing","scene")),screenShader_(loadShader(root,"anti-aliasing","screen")),
          cube_(cubeVertices(true),{3,3}),
          sphere_(curvedVertices(false),{3,3}),torus_(curvedVertices(true),{3,3}),
          pyramid_(pyramidVertices(),{3,3}),
          screen_(std::vector<float>{-1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1},{2,2},{0,1,2,0,2,3}),mode_(mode) {
        glGetIntegerv(GL_MAX_SAMPLES,&maxSamples_);
        requestedSamples_=std::min(4,maxSamples_);
        GLint textureLimit=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&textureLimit);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&maxSize_); maxSize_=std::min(maxSize_,textureLimit);
        screenShader_->use(); screenShader_->setInt("screenTexture",0);
        camera_=Camera(glm::vec3(0,0,6.5f));
    }
    void render(const SceneFrame& frame) override {
        RenderState previous;
        int w=previous.viewport[2],h=previous.viewport[3]; if(w<=0 || h<=0) return;
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
        glEnable(GL_MULTISAMPLE); glDepthFunc(GL_LESS); glClearDepth(1);
        int samples=mode_==1 ? std::min(requestedSamples_,maxSamples_) : 1;
        // SSAA renders at twice the width/height (four times the pixel workload).
        int rw=mode_==3 ? std::min(w*2,maxSize_) : w;
        int rh=mode_==3 ? std::min(h*2,maxSize_) : h;
        processed_.allocate(rw,rh,std::max(samples,1),w,h);
        renderGeometry(processed_,frame); processed_.resolve();
        if(split_) {
            reference_.allocate(w,h,1,w,h); renderGeometry(reference_,frame); reference_.resolve();
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous.drawFbo);
        glViewport(previous.viewport[0],previous.viewport[1],w,h);
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        screenShader_->use(); screenShader_->setInt("fxaa",mode_==2); screenShader_->setFloat("zoom",zoom_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,processed_.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,mode_==2 ? GL_LINEAR : GL_NEAREST);
        screen_.draw();
        if(split_) {
            // Scissor keeps identical framing on both sides; no squeezed viewports.
            glEnable(GL_SCISSOR_TEST); GLint old[4]; glGetIntegerv(GL_SCISSOR_BOX,old);
            glScissor(previous.viewport[0],previous.viewport[1],w/2,h);
            screenShader_->setInt("fxaa",false); glBindTexture(GL_TEXTURE_2D,reference_.texture); screen_.draw();
            glScissor(old[0],old[1],old[2],old[3]); glDisable(GL_SCISSOR_TEST);
        }
    }
    void drawControls() override {
        const char* modes[]={"None", "MSAA", "FXAA", "SSAA (2x width and height)"};
        ImGui::Combo("Anti-aliasing algorithm",&mode_,modes,4);
        if(mode_==1) {
            if(ImGui::BeginCombo("MSAA samples",std::to_string(requestedSamples_).c_str())) {
                for(int n : {2,4,8}) if(n<=maxSamples_) {
                    if(ImGui::Selectable(std::to_string(n).c_str(),requestedSamples_==n)) requestedSamples_=n;
                }
                ImGui::EndCombo();
            }
            ImGui::Text("Device maximum: %d; current: %d",maxSamples_,processed_.samples);
        }
        ImGui::Checkbox("Split view: none left / selected right",&split_);
        ImGui::SliderFloat("Pixel magnification",&zoom_,1.f,8.f,"%.1fx");
        const char* geometry[]={"All geometry", "Cube", "Sphere", "Torus", "Pyramid", "Thin diagonal bars"};
        ImGui::Combo("Focus geometry",&geometry_,geometry,6);
        ImGui::Text("Render size: %d x %d",processed_.width,processed_.height);
        ImGui::TextWrapped("MSAA smooths geometry coverage; FXAA filters high-contrast image edges and can blur detail; SSAA renders more pixels then downsamples, with higher rendering cost.");
        ImGui::TextWrapped("Cube and pyramid show sloping edges; sphere and torus show curved silhouettes and holes; thin bars show subpixel coverage. Select one geometry to centre it. Magnification crops the centre and enlarges pixels. WASD / right drag / wheel control the camera.");
        if(ImGui::Button("Reset anti-aliasing")) { mode_=1; requestedSamples_=std::min(4,maxSamples_); zoom_=1; split_=true; geometry_=0; }
        ImGui::SameLine(); if(ImGui::Button("Reset camera")) camera_=Camera(glm::vec3(0,0,6.5f));
    }
private:
    void renderGeometry(const Target& target,const SceneFrame& frame) {
        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo); glViewport(0,0,target.width,target.height);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
        glClearColor(.035f,.045f,.065f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        object_->use(); object_->setMat4("view",camera_.GetViewMatrix()); object_->setMat4("projection",projection(frame));
        auto draw=[&](const Mesh& mesh,glm::vec3 position,glm::vec3 size,glm::vec3 color,float angle,glm::vec3 axis) {
            auto model=glm::translate(glm::mat4(1),position);
            model=glm::rotate(model,glm::radians(angle),axis);
            object_->setMat4("model",glm::scale(model,size));
            object_->setVec3("surfaceColor",color); mesh.draw();
        };
        const bool all=geometry_==0;
        // Both comparison passes use exactly the same deterministic transforms.
        if(all || geometry_==1) draw(cube_,all ? glm::vec3(-1.7f,.8f,0) : glm::vec3(0),glm::vec3(1.05f),{.3f,.85f,.55f},23,{1,.6f,.3f});
        if(all || geometry_==2) draw(sphere_,all ? glm::vec3(0,.85f,0) : glm::vec3(0),glm::vec3(.65f),{.25f,.65f,1},0,{0,1,0});
        if(all || geometry_==3) draw(torus_,all ? glm::vec3(1.7f,.85f,0) : glm::vec3(0),glm::vec3(.85f),{1,.6f,.2f},28,{1,.4f,0});
        if(all || geometry_==4) draw(pyramid_,all ? glm::vec3(-1.5f,-.95f,0) : glm::vec3(0),glm::vec3(.85f),{.85f,.35f,.8f},20,{0,1,0});
        if(all || geometry_==5) {
            for(int i=0;i<6;++i) {
                glm::vec3 position=glm::vec3((i-2.5f)*.34f,0,0)+(all ? glm::vec3(.9f,-.95f,0) : glm::vec3(0));
                draw(cube_,position,{.018f+i*.012f,1.25f,.045f},{.95f,.95f,.8f},-28,{0,0,1});
            }
        }
    }
    std::unique_ptr<Shader> object_,screenShader_;
    Mesh cube_,sphere_,torus_,pyramid_,screen_;
    Target processed_,reference_;
    int mode_=1,requestedSamples_=4,maxSamples_=1,maxSize_=0;
    int geometry_=0;
    float zoom_=1;
    bool split_=true;
};
} // namespace
std::unique_ptr<Scene> createAntiAliasingScene(const std::string& root,int mode) {
    return std::make_unique<AntiAliasingScene>(root,mode);
}
