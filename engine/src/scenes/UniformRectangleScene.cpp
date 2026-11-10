#include "SceneFactories.h"
#include "SceneSupport.h"
#include <cmath>

using namespace sceneSupport;
namespace {
class UniformRectangleScene final : public Scene {
public:
    explicit UniformRectangleScene(const std::string& root)
        : shader_(loadShader(root, "intro/uniform")),
          mesh_(std::vector<float>{.5f,.5f,0, .5f,-.5f,0, -.5f,-.5f,0, -.5f,.5f,0}, {3}, {0,1,3,1,2,3}) {}
    void render(const SceneFrame& frame) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        const float green = std::sin(frame.time) * .5f + .5f;
        glUniform4f(glGetUniformLocation(shader_->ID, "ourColor"), 0, green, 0, 1);
        mesh_.draw();
    }
private:
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
};
} // namespace

std::unique_ptr<Scene> createUniformRectangleScene(const std::string& root) {
    return std::make_unique<UniformRectangleScene>(root);
}
