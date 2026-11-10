#include "SceneFactories.h"
#include "SceneSupport.h"
#include <cmath>

using namespace sceneSupport;
namespace {
class TriangleScene final : public Scene {
public:
    explicit TriangleScene(const std::string& root)
        : shader_(loadShader(root, "intro/triangle")),
          mesh_(std::vector<float>{.5f,-.5f,0, 1,0,0, -.5f,-.5f,0, 0,1,0, 0,.5f,0, 0,0,1}, {3,3}) {}
    void render(const SceneFrame&) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        mesh_.draw();
    }
private:
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
};
} // namespace

std::unique_ptr<Scene> createTriangleScene(const std::string& root) {
    return std::make_unique<TriangleScene>(root);
}
