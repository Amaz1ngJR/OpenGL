#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
// 同一个四边形演示单纹理、双纹理混合及采样规则，UI 改动在同一帧上传 uniform。
class TextureScene final : public Scene {
public:
    explicit TextureScene(const std::string& root)
        : shader_(loadShader(root, "textures")),
          mesh_(std::vector<float>{
              .5f,.5f,0, 1,0,0, 1,1, .5f,-.5f,0, 0,1,0, 1,0,
              -.5f,-.5f,0, 0,0,1, 0,0, -.5f,.5f,0, 1,1,0, 0,1
          }, {3,3,2}, {0,1,3,1,2,3}),
          container_(root + "/resources/textures/container.jpg"),
          face_(root + "/resources/textures/awesomeface.png", GL_MIRRORED_REPEAT) {
        shader_->use();
        // sampler2D 的值是纹理单元编号，不是 container_/face_ 的 OpenGL 对象 ID。
        shader_->setInt("texture0", 0);
        shader_->setInt("texture1", 1);
    }
    void render(const SceneFrame&) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        shader_->setInt("combined", combined_);
        shader_->setInt("vertexTint", vertexTint_);
        shader_->setFloat("mixFactor", mixFactor_);
        shader_->setFloat("uvScale", uvScale_);
        // 绑定必须发生在 draw 之前；构造函数中的绑定可能已被其他纹理或 ImGui 改变。
        container_.bind(0);
        face_.bind(1);
        mesh_.draw();
    }
    void drawControls() override {
        ImGui::Separator();
        ImGui::TextUnformatted("Texture settings");
        ImGui::Checkbox("Combine textures", &combined_);
        if (combined_) ImGui::SliderFloat("Texture mix", &mixFactor_, 0.0f, 1.0f, "%.2f");
        else ImGui::TextUnformatted("Single texture: awesomeface");
        ImGui::Checkbox("Vertex color tint", &vertexTint_);
        // 以纹理中心缩放 UV，超出 [0,1] 时才能清晰观察各种环绕方式。
        ImGui::SliderFloat("UV scale", &uvScale_, .1f, 20.0f, "%.2f");
        ImGui::TextWrapped("Use UV scale > 1 to compare wrap modes. Min filters affect shrinking; mag filters affect enlargement.");
        if (combined_) container_.drawControls("Container (texture0)");
        face_.drawControls("Awesomeface (texture1)");
    }
private:
    bool combined_ = true;
    bool vertexTint_ = false;
    float mixFactor_ = 0.8f;
    float uvScale_ = 3.0f;
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
    Texture container_, face_;
};

} // namespace

std::unique_ptr<Scene> createTextureScene(const std::string& root) {
    return std::make_unique<TextureScene>(root);
}
