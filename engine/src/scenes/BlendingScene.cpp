#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
class BlendingScene final : public CameraScene {
public:
    explicit BlendingScene(const std::string& root)
        : shader_(loadShader(root, "blending", "scene")),
          cube_(cubeVertices(false), {3,2}),
          floor_(std::vector<float>{
              5,-.5f,5,2,0, -5,-.5f,5,0,0, -5,-.5f,-5,0,2,
              5,-.5f,5,2,0, -5,-.5f,-5,0,2, 5,-.5f,-5,2,2
          }, {3,2}),
          // Same upright quad as the tutorial. Texture loading already flips V.
          quad_(std::vector<float>{0,-.5f,0,0,0, 1,-.5f,0,1,0,
                                  1,.5f,0,1,1, 0,.5f,0,0,1}, {3,2}, {0,1,2,0,2,3}),
          marble_(root + "/resources/textures/marble.jpg"),
          metal_(root + "/resources/textures/metal.png"),
          grass_(root + "/resources/textures/grass.png", GL_CLAMP_TO_EDGE),
          window_(root + "/resources/textures/window.png", GL_CLAMP_TO_EDGE) {
        shader_->use(); shader_->setInt("surfaceTexture", 0);
    }
    void render(const SceneFrame& frame) override {
        State previous;
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        shader_->use();
        shader_->setMat4("view", camera_.GetViewMatrix());
        shader_->setMat4("projection", projection(frame));
        shader_->setFloat("alphaCutoff", 0.f);
        // 1. All opaque surfaces write depth before any translucent windows.
        marble_.bind(0);
        draw(cube_, {-1,0,-1}); draw(cube_, {2,0,0});
        metal_.bind(0); draw(floor_, {0,0,0});
        // Alpha cutouts belong to the opaque pass: discarded fragments write no depth.
        if (showGrass_) {
            shader_->setFloat("alphaCutoff", discardGrass_ ? cutoff_ : 0.f);
            grass_.bind(0);
            for (const auto& position : grassPositions_) draw(quad_, position);
        }
        shader_->setFloat("alphaCutoff", 0.f);
        if (showWindows_) {
            // 2. Re-sort every frame as the camera moves. Stable sort preserves all
            // windows at equal distances (a distance-keyed map would lose entries).
            auto order = windowOrder();
            // 3. Blend against opaque geometry and the previously drawn windows.
            if (blend_) glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(windowDepthWrite_ ? GL_TRUE : GL_FALSE);
            window_.bind(0);
            for (int index : order) draw(quad_, windowPositions_[index]);
        }
    }
    void drawControls() override {
        ImGui::TextWrapped("Opaque boxes + floor, then cutout grass, then sorted windows. WASD / right drag / wheel move the camera.");
        ImGui::Checkbox("Show grass", &showGrass_);
        ImGui::Checkbox("Discard transparent grass pixels", &discardGrass_);
        ImGui::SliderFloat("Grass alpha cutoff", &cutoff_, .01f, .99f, "%.2f");
        ImGui::Checkbox("Show windows", &showWindows_);
        ImGui::Checkbox("Enable window blending", &blend_);
        const char* orders[] = {"Far to near (correct)", "Near to far", "Original placement order"};
        ImGui::Combo("Window draw order", &sortMode_, orders, 3);
        ImGui::Checkbox("Write window depth", &windowDepthWrite_);
        ImGui::TextWrapped("RGB = source * source alpha + background * (1 - source alpha). Blending needs far-to-near order; depth testing stays enabled. Window depth writes are off by default.");
        ImGui::TextWrapped("Try near-to-far with depth writes ON: farther windows can disappear. With writes OFF they remain, but layer colors can still be incorrect. Grass uses discard, not blending.");
        if (ImGui::TreeNode("Current window order")) {
            for (int i : windowOrder())
                ImGui::Text("Window %d: distance %.2f", i+1, glm::length(windowPositions_[i] - camera_.Position));
            ImGui::TreePop();
        }
        if (ImGui::Button("Reset blending settings")) {
            showGrass_=showWindows_=discardGrass_=blend_=true;
            windowDepthWrite_=false; cutoff_=.1f; sortMode_=0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) camera_=Camera(glm::vec3(0,0,3));
    }
private:
    struct State {
        GLboolean depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND), cull=glIsEnabled(GL_CULL_FACE), mask;
        GLint depthFunc, srcRGB, dstRGB, srcAlpha, dstAlpha, equationRGB, equationAlpha;
        State() {
            glGetBooleanv(GL_DEPTH_WRITEMASK, &mask); glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
            glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB); glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
            glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
        }
        ~State() {
            glDepthMask(mask); glDepthFunc(depthFunc);
            glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);
            glBlendEquationSeparate(equationRGB,equationAlpha);
            if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }
    };
    void draw(const Mesh& mesh, glm::vec3 position) {
        shader_->setMat4("model", glm::translate(glm::mat4(1), position)); mesh.draw();
    }
    std::vector<int> windowOrder() const {
        std::vector<int> order{0,1,2,3,4};
        if (sortMode_ != 2) std::stable_sort(order.begin(),order.end(),[&](int a,int b) {
            // Use placement distances, matching LearnOpenGL's object sorting.
            float da=glm::length(windowPositions_[a]-camera_.Position);
            float db=glm::length(windowPositions_[b]-camera_.Position);
            return sortMode_ == 0 ? da > db : da < db;
        });
        return order;
    }
    std::unique_ptr<Shader> shader_;
    Mesh cube_, floor_, quad_;
    Texture marble_, metal_, grass_, window_;
    const std::vector<glm::vec3> windowPositions_{{-1.5f,0,-.48f},{1.5f,0,.51f},{0,0,.7f},{-.3f,0,-2.3f},{.5f,0,-.6f}};
    // Additional cutouts allow grass and tutorial windows to coexist without coplanar overlap.
    const std::vector<glm::vec3> grassPositions_{{-2,0,-.8f},{-1.2f,0,.8f},{.4f,0,-2.8f}};
    bool showGrass_=true, showWindows_=true, discardGrass_=true, blend_=true, windowDepthWrite_=false;
    float cutoff_=.1f;
    int sortMode_=0;
};

} // namespace

std::unique_ptr<Scene> createBlendingScene(const std::string& root) {
    return std::make_unique<BlendingScene>(root);
}
