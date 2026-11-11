#include "Scene.h"
#include "ModelScene.h"
#include "scenes/SceneFactories.h"
#include <stdexcept>

namespace {
// Registration order is shared by the UI and smoke test.
const std::vector<SceneInfo> sceneInfos = {
    {"No scene"}, {"Model viewer"}, {"Triangle"}, {"Uniform rectangle"}, {"Texture"},
    {"Transform+Face culling"}, {"Phong lighting + materials"}, {"Depth testing"}, {"Stencil testing"}, {"Blending"}, {"Framebuffers"}
};

}

const std::vector<SceneInfo>& availableScenes() { return sceneInfos; }
// 工厂按面板索引延迟创建单个场景；旧场景由 unique_ptr 自动析构释放资源。
std::unique_ptr<Scene> createScene(int index, const std::string& root) {
    // 索引 0 为清屏场景；其余选项为教程示例与本地模型预览。
    switch (index) {
        case 0: return createEmptyScene(root);
        case 1: return createModelScene(root);
        case 2: return createTriangleScene(root);
        case 3: return createUniformRectangleScene(root);
        case 4: return createTextureScene(root);
        case 5: return createTransformScene(root);
        case 6: return createLightingScene(root);
        case 7: return createDepthTestingScene(root);
        case 8: return createStencilTestingScene(root);
        case 9: return createBlendingScene(root);
        case 10: return createFramebufferScene(root);
        default: throw std::out_of_range("Invalid scene index");
    }
}
