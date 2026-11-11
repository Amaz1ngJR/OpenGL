#include "SceneFactories.h"
#include "SceneSupport.h"
#include <cmath>

using namespace sceneSupport;
namespace {
// uniform 是整次绘制共享的参数；与逐顶点 attribute 不同，它不会随顶点索引变化。
class UniformRectangleScene final : public Scene {
public:
    explicit UniformRectangleScene(const std::string& root)
        : shader_(loadShader(root, "intro/uniform")),
          // 四个位置通过六个 EBO 索引复用，形成两个三角形（OpenGL Core 没有四边形图元）。
          mesh_(std::vector<float>{.5f,.5f,0, .5f,-.5f,0, -.5f,-.5f,0, -.5f,.5f,0}, {3}, {0,1,3,1,2,3}) {}
    void render(const SceneFrame& frame) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        // sin 的 [-1,1] 映射为合法颜色 [0,1]；先 use()，再更新当前程序的 uniform。
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
