#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
class TransformScene final : public CameraScene {
public:
    explicit TransformScene(const std::string& root)
        : shader_(loadShader(root, "intro/transformations")),
          mesh_(cubeVertices(false), {3,2}),
          container_(root + "/resources/textures/container.jpg"),
          face_(root + "/resources/textures/awesomeface.png") {
        shader_->use();
        shader_->setInt("texture0", 0);
        shader_->setInt("texture1", 1);
    }
    void render(const SceneFrame& frame) override {
        glEnable(GL_DEPTH_TEST);
        // 面剔除是 OpenGL 上下文状态，不属于 VAO。保存并在绘制结束后恢复，
        // 避免 GL_FRONT_AND_BACK 把其他场景（或后续绘制）也一并剔除。
        const GLboolean previousCullEnabled = glIsEnabled(GL_CULL_FACE);
        GLint previousCullFace = GL_BACK, previousFrontFace = GL_CCW;
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);
        glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
        const GLenum modes[] = {GL_BACK, GL_FRONT, GL_FRONT_AND_BACK};
        // 立方体各面的顶点从外部看按逆时针排列；正反面由绕序决定，而不是顶点法线。
        glFrontFace(GL_CCW);
        glCullFace(modes[cullMode_]);
        if (cullEnabled_) glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);
        shader_->use();
        shader_->setMat4("projection", projection(frame));
        shader_->setMat4("view", camera_.GetViewMatrix());
        container_.bind(0);
        face_.bind(1);
        const glm::vec3 positions[] = {
            {0,0,0}, {2,5,-15}, {-1.5f,-2.2f,-2.5f}, {-3.8f,-2,-12.3f},
            {2.4f,-.4f,-3.5f}, {-1.7f,3,-7.5f}, {1.3f,-2,-2.5f},
            {1.5f,2,-2.5f}, {1.5f,.2f,-1.5f}, {-1.3f,1,-1.5f}
        };
        // GLM 在矩阵右侧追加变换，下面得到 T*R：作用于顶点时先绕自身原点旋转，
        // 再平移到世界位置。写代码的顺序与顶点实际经历的变换顺序相反。
        for (int i = 0; i < 10; ++i) {
            auto model = glm::translate(glm::mat4(1), positions[i]);
            model = glm::rotate(model, frame.time * glm::radians(i == 0 ? 50.f : 20.f * i), glm::vec3(.5f,1,0));
            shader_->setMat4("model", model);
            mesh_.draw();
        }
        glCullFace(static_cast<GLenum>(previousCullFace));
        glFrontFace(static_cast<GLenum>(previousFrontFace));
        if (previousCullEnabled) glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);
    }
    void drawControls() override {
        ImGui::Checkbox("Enable face culling", &cullEnabled_);
        if (cullEnabled_) {
            // GL_BACK：只剔除背向面（glCullFace 的默认选项）。
            // GL_FRONT：只剔除正向面，可观察闭合模型的内部表面。
            // GL_FRONT_AND_BACK：剔除所有正反向三角形，立方体将完全不可见；UI 不受影响。
            const char* modes[] = {"GL_BACK", "GL_FRONT", "GL_FRONT_AND_BACK"};
            ImGui::Combo("Cull face", &cullMode_, modes, IM_ARRAYSIZE(modes));
            if (cullMode_ == 2) ImGui::TextWrapped("All triangle faces are culled; cubes are intentionally invisible.");
        }
    }
private:
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
    Texture container_, face_;
    bool cullEnabled_ = false;
    int cullMode_ = 0; // 默认剔除背面；未勾选启用时不进行剔除。
};

} // namespace

std::unique_ptr<Scene> createTransformScene(const std::string& root) {
    return std::make_unique<TransformScene>(root);
}
