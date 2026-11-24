#include "SceneFactories.h"
#include "SceneSupport.h"

using namespace sceneSupport;
namespace {
class AdvancedLightingScene final : public CameraScene {
public:
    AdvancedLightingScene(const std::string& root, int mode)
        : shader_(loadShader(root, "advanced-lighting")),
          floor_(std::vector<float>{
              10,-.5f,10, 0,1,0, 10,0, -10,-.5f,-10, 0,1,0, 0,10,
              -10,-.5f,10, 0,1,0, 0,0,
              10,-.5f,10, 0,1,0, 10,0, 10,-.5f,-10, 0,1,0, 10,10,
              -10,-.5f,-10, 0,1,0, 0,10
          }, {3,3,2}), wood_(root + "/resources/textures/wood.png"), mode_(mode) {
        resetCamera();
        shader_->use(); shader_->setInt("wood", 0);
    }
    void render(const SceneFrame& frame) override {
        State state;
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        shader_->use(); wood_.bind(0);
        shader_->setMat4("view", camera_.GetViewMatrix());
        shader_->setVec3("viewPos", camera_.Position);
        shader_->setVec3("lightPos", lightPos_);
        shader_->setInt("specularOnly", specularOnly_);
        shader_->setFloat("specularStrength", specularStrength_);
        if (mode_ == 2 && state.viewport[2] >= 2) {
            // 两侧完整重绘同一视角，而非把同一幅画面从中间切开。
            int left = state.viewport[2] / 2;
            glViewport(state.viewport[0], state.viewport[1], left, state.viewport[3]);
            draw(false, float(left) / state.viewport[3]);
            glViewport(state.viewport[0]+left, state.viewport[1], state.viewport[2]-left, state.viewport[3]);
            draw(true, float(state.viewport[2]-left) / state.viewport[3]);
        } else draw(mode_ == 1, frame.aspect);
    }
    void drawControls() override {
        const char* modes[] = {"Phong", "Blinn-Phong", "Side by side"};
        ImGui::Combo("Lighting comparison", &mode_, modes, 3);
        if (mode_ == 2) ImGui::TextUnformatted("Left: Phong   |   Right: Blinn-Phong (same camera)");
        ImGui::SliderFloat("Phong shininess", &phongExponent_, 1.f, 128.f, "%.1f");
        ImGui::SliderFloat("Blinn-Phong shininess", &blinnExponent_, 1.f, 512.f, "%.1f");
        if (ImGui::Button("Tutorial: 8 / 32")) { phongExponent_=8; blinnExponent_=32; }
        ImGui::SameLine();
        if (ImGui::Button("Broad highlight: 1 / 4")) { phongExponent_=1; blinnExponent_=4; }
        ImGui::Checkbox("Specular only", &specularOnly_);
        ImGui::SliderFloat("Specular strength", &specularStrength_, 0.f, 1.f);
        ImGui::SliderFloat3("Light position", &lightPos_.x, -3.f, 3.f);
        ImGui::TextWrapped("Phong: pow(max(dot(V, reflect(-L, N)), 0), exponent). Blinn-Phong: pow(max(dot(N, normalize(L+V)), 0), exponent).");
        ImGui::TextWrapped("L points to the light; V points to the camera; N is the surface normal. Larger exponents make highlights narrower. Blinn-Phong usually needs 2-4 times the Phong exponent for a similar width.");
        ImGui::TextWrapped("Try Broad highlight + Specular only: Phong clamps to zero when V and R differ by more than 90 degrees. WASD / right drag / wheel move the shared camera.");
        if (ImGui::Button("Reset lighting")) {
            mode_=2; phongExponent_=8; blinnExponent_=32;
            specularStrength_=.3f; specularOnly_=false; lightPos_=glm::vec3(0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) resetCamera();
    }
private:
    struct State {
        GLboolean depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND), cull=glIsEnabled(GL_CULL_FACE), mask;
        GLint func, viewport[4];
        State() { glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); glGetIntegerv(GL_DEPTH_FUNC,&func); glGetIntegerv(GL_VIEWPORT,viewport); }
        ~State() {
            glDepthMask(mask); glDepthFunc(func);
            glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
            if(depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if(blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if(cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }
    };
    void resetCamera() { camera_=Camera(glm::vec3(0,0,3),glm::vec3(0,1,0),-90,-10); }
    void draw(bool blinn, float aspect) {
        if (aspect <= 0) return;
        shader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),aspect,.1f,100.f));
        shader_->setInt("blinn",blinn);
        shader_->setFloat("shininess",blinn ? blinnExponent_ : phongExponent_);
        floor_.draw();
    }
    std::unique_ptr<Shader> shader_;
    Mesh floor_;
    Texture wood_;
    int mode_=2;
    float phongExponent_=8.f, blinnExponent_=32.f, specularStrength_=.3f;
    bool specularOnly_=false;
    glm::vec3 lightPos_{0};
};
}
std::unique_ptr<Scene> createAdvancedLightingScene(const std::string& root, int mode) {
    return std::make_unique<AdvancedLightingScene>(root,mode);
}
