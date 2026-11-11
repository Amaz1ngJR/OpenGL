#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
// 受光物体和可见灯泡是两次绘制：前者计算材质响应，后者直接显示光源颜色。
class LightingScene final : public CameraScene {
public:
    explicit LightingScene(const std::string& root)
        : shader_(loadShader(root, "lighting/phong-materials", "object")),
          lightShader_(loadShader(root, "lighting/light-source", "light_cube")),
          mesh_(cubeVertices(true), {3,3}) {}
    void render(const SceneFrame& frame) override {
        glEnable(GL_DEPTH_TEST);
        // 光源使用独立动画时间；暂停时仍记录帧时间，恢复后不会补跑暂停期间的动画。
        if (previousFrameTime_ < 0.0f) lightTime_ = frame.time;
        else if (!lightPaused_) lightTime_ += frame.time - previousFrameTime_;
        previousFrameTime_ = frame.time;
        const float angle = lightTime_ * .8f; // 光源每秒旋转 0.8 弧度。
        const glm::vec3 lightPos(std::sin(angle) * lightRadius_, lightHeight_, std::cos(angle) * lightRadius_);
        // 手动颜色只在暂停期间覆盖显示，不修改动画时间，恢复后继续原来的颜色轨迹。
        const glm::vec3 lightColor = lightPaused_ ? pausedLightColor_ : animatedLightColor();
        const auto proj = projection(frame), view = camera_.GetViewMatrix();
        shader_->use();
        shader_->setMat4("projection", proj);
        shader_->setMat4("view", view);
        shader_->setMat4("model", glm::mat4(1));
        shader_->setVec3("viewPos", camera_.Position);
        // 环境/漫反射/镜面分量使用同一光色但不同强度；物体最终颜色还会乘材质系数。
        // shader 中用的是世界坐标，lightPos、viewPos 和顶点位置必须在同一空间。
        shader_->setVec3("light.position", lightPos);
        shader_->setVec3("light.ambient", lightColor * .1f);
        shader_->setVec3("light.diffuse", lightColor * .5f);
        shader_->setVec3("light.specular", lightColor);
        shader_->setVec3("material.ambient", materialAmbient_);
        shader_->setVec3("material.diffuse", materialDiffuse_);
        shader_->setVec3("material.specular", materialSpecular_);
        // shininess 控制高光集中程度：指数越大高光越窄，不等同于增加灯光亮度。
        shader_->setFloat("material.shininess", materialShininess_);
        // 先绘制受光物体，再切换灯泡 shader 用同一网格绘制缩小后的光源。
        mesh_.draw();
        lightShader_->use();
        lightShader_->setMat4("projection", proj);
        lightShader_->setMat4("view", view);
        lightShader_->setVec3("lightColor", lightColor);
        auto model = glm::translate(glm::mat4(1), lightPos);
        lightShader_->setMat4("model", glm::scale(model, glm::vec3(.2f)));
        mesh_.draw();
    }
    void drawControls() override {
        // 刚进入暂停时继承当前动画颜色，避免勾选瞬间发生跳色。
        if (ImGui::Checkbox("Pause light animation", &lightPaused_) && lightPaused_)
            pausedLightColor_ = animatedLightColor();
        if (lightPaused_)
            ImGui::ColorEdit3("Paused light color", &pausedLightColor_.x);
        // 参数直接参与光源位置计算；即便动画暂停，改变轨道也会立即更新灯泡和照明位置。
        ImGui::SliderFloat("Light radius", &lightRadius_, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Light height", &lightHeight_, -5.0f, 5.0f, "%.2f");
        ImGui::ColorEdit3("Material ambient", &materialAmbient_.x);
        ImGui::ColorEdit3("Material diffuse", &materialDiffuse_.x);
        ImGui::ColorEdit3("Material specular", &materialSpecular_.x);
        ImGui::SliderFloat("Material shininess", &materialShininess_, 1.0f, 256.0f, "%.1f");
    }
private:
    glm::vec3 animatedLightColor() const {
        // 正弦波先映射到合法 RGB [0,1]，避免负光强抵消物体颜色。
        return glm::vec3(.5f) + .5f * glm::vec3(
            std::sin(lightTime_ * 2.f), std::sin(lightTime_ * .7f), std::sin(lightTime_ * 1.3f));
    }
    bool lightPaused_ = false;
    float lightTime_ = 0.0f;
    float previousFrameTime_ = -1.0f;
    float lightRadius_ = 2.0f;
    float lightHeight_ = 1.0f;
    glm::vec3 pausedLightColor_{1.0f};
    glm::vec3 materialAmbient_{1.0f, 0.5f, 0.31f};
    glm::vec3 materialDiffuse_{1.0f, 0.5f, 0.31f};
    glm::vec3 materialSpecular_{0.5f, 0.5f, 0.5f};
    float materialShininess_ = 32.0f;
    std::unique_ptr<Shader> shader_, lightShader_;
    Mesh mesh_;
};

} // namespace

std::unique_ptr<Scene> createLightingScene(const std::string& root) {
    return std::make_unique<LightingScene>(root);
}
