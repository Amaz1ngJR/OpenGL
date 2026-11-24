#include "SceneFactories.h"
#include "SceneSupport.h"

using namespace sceneSupport;
namespace {
class GammaCorrectionScene final : public CameraScene {
public:
    GammaCorrectionScene(const std::string& root, int mode)
        : shader_(loadShader(root,"gamma-correction")),
          floor_(std::vector<float>{
              10,-.5f,10, 0,1,0, 10,0, -10,-.5f,-10, 0,1,0, 0,10,
              -10,-.5f,10, 0,1,0, 0,0,
              10,-.5f,10, 0,1,0, 10,0, 10,-.5f,-10, 0,1,0, 10,10,
              -10,-.5f,-10, 0,1,0, 0,10
          }, {3,3,2}),
          rawWood_(root+"/resources/textures/wood.png"),
          srgbWood_(root+"/resources/textures/wood.png",GL_REPEAT,true), mode_(mode) {
        resetCamera(); shader_->use(); shader_->setInt("wood",0);
    }
    void render(const SceneFrame& frame) override {
        State state;
        if(state.viewport[2]<=0 || state.viewport[3]<=0) return;
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        // 最终着色器手动编码，禁止帧缓冲再次转换；离开场景恢复原状态。
        glDisable(GL_FRAMEBUFFER_SRGB);
        shader_->use();
        shader_->setMat4("view",camera_.GetViewMatrix());
        shader_->setVec3("viewPos",camera_.Position);
        shader_->setFloat("gamma",gamma_);
        shader_->setFloat("lightStrength",lightStrength_);
        if(mode_==2 && state.viewport[2]>=2) {
            int left=state.viewport[2]/2;
            glViewport(state.viewport[0],state.viewport[1],left,state.viewport[3]);
            draw(0,float(left)/state.viewport[3]);
            glViewport(state.viewport[0]+left,state.viewport[1],state.viewport[2]-left,state.viewport[3]);
            draw(1,float(state.viewport[2]-left)/state.viewport[3]);
        } else draw(mode_,frame.aspect);
    }
    void drawControls() override {
        const char* modes[]={"Uncorrected", "Gamma corrected", "Side by side", "Output only (missing sRGB decode)"};
        ImGui::Combo("Gamma comparison",&mode_,modes,4);
        if(mode_==2) ImGui::TextUnformatted("Left: uncorrected | Right: corrected (same camera)");
        ImGui::Checkbox("Decode sRGB wood texture",&decodeTexture_);
        ImGui::Checkbox("Encode final output",&encodeOutput_);
        ImGui::SliderFloat("Output gamma",&gamma_,1.f,3.f,"%.2f");
        const char* attenuation[]={"Tutorial: 1/r off, 1/r^2 on", "1 / distance", "1 / distance squared"};
        ImGui::Combo("Light attenuation",&attenuation_,attenuation,3);
        ImGui::SliderFloat("Light strength",&lightStrength_,.1f,2.f);
        ImGui::TextUnformatted("Lights, left to right: 0.25 / 0.50 / 0.75 / 1.00");
        ImGui::TextWrapped("Corrected pipeline: sRGB color texture -> linear lighting -> pow(color, 1/gamma) once at the end. Gamma 2.2 approximates the sRGB output curve; texture decoding uses the hardware sRGB curve.");
        ImGui::TextWrapped("Decode/encode switches affect the corrected view. Output only deliberately skips texture decoding, so colors become too bright. Uncorrected skips both steps. Alpha is not gamma corrected.");
        ImGui::TextWrapped("Fix attenuation to 1/r^2 to isolate color-space changes; Tutorial also switches attenuation with output encoding. Larger light strength can clip highlights in this LDR scene. WASD / right drag / wheel move the camera.");
        if(ImGui::Button("Reset gamma settings")) {
            mode_=2; decodeTexture_=encodeOutput_=true; attenuation_=0; gamma_=2.2f; lightStrength_=1.f;
        }
        ImGui::SameLine();
        if(ImGui::Button("Reset camera")) resetCamera();
    }
private:
    struct State {
        GLboolean depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND), cull=glIsEnabled(GL_CULL_FACE), srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB), mask;
        GLint func,viewport[4];
        State() { glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); glGetIntegerv(GL_DEPTH_FUNC,&func); glGetIntegerv(GL_VIEWPORT,viewport); }
        ~State() {
            glDepthMask(mask); glDepthFunc(func);
            glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
            if(depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if(blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if(cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if(srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        }
    };
    void resetCamera() { camera_=Camera(glm::vec3(0,0,3),glm::vec3(0,1,0),-90,-12); }
    void draw(int mode,float aspect) {
        if(aspect<=0) return;
        const bool encode=mode!=0 && encodeOutput_;
        const bool decode=mode!=0 && mode!=3 && decodeTexture_;
        (decode ? srgbWood_ : rawWood_).bind(0);
        shader_->setInt("encodeOutput",encode);
        shader_->setInt("inverseSquare",attenuation_==2 || (attenuation_==0 && encode));
        shader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),aspect,.1f,100.f));
        floor_.draw();
    }
    std::unique_ptr<Shader> shader_;
    Mesh floor_;
    Texture rawWood_,srgbWood_;
    int mode_=2,attenuation_=0;
    bool decodeTexture_=true,encodeOutput_=true;
    float gamma_=2.2f,lightStrength_=1.f;
};
}
std::unique_ptr<Scene> createGammaCorrectionScene(const std::string& root,int mode) {
    return std::make_unique<GammaCorrectionScene>(root,mode);
}
