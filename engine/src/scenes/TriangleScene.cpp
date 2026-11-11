#include "SceneFactories.h"
#include "SceneSupport.h"
#include <cmath>

using namespace sceneSupport;
namespace {
// 最小渲染流程：准备顶点和布局 -> 使用 GLSL 程序 -> 提交一个三角形。
// 坐标直接写入裁剪空间（w=1），不需要模型、观察、投影矩阵。
class TriangleScene final : public Scene {
public:
    explicit TriangleScene(const std::string& root)
        : shader_(loadShader(root, "intro/triangle")),
          // 每顶点六个 float：xyz 位置、rgb 颜色，分别送入 location 0 和 1。
          mesh_(std::vector<float>{.5f,-.5f,0, 1,0,0, -.5f,-.5f,0, 0,1,0, 0,.5f,0, 0,0,1}, {3,3}) {}
    void render(const SceneFrame&) override {
        // 2D 示例不做深度遮挡；内部颜色由光栅化插值，不是在 CPU 逐像素生成。
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
