#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
// 仅保留主循环的清屏和 ImGui：空场景不上传网格、不提交 draw call。
class EmptyScene final : public Scene {
public:
    void render(const SceneFrame&) override { glDisable(GL_DEPTH_TEST); }
};

} // namespace

std::unique_ptr<Scene> createEmptyScene(const std::string&) {
    return std::make_unique<EmptyScene>();
}
