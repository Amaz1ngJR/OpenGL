#include "Scene.h"
#include "ModelScene.h"
#include "scenes/SceneFactories.h"
#include <stdexcept>

namespace {
// 列表索引同时供主界面、工厂和冒烟测试使用；调整顺序时必须同步相关索引。
const std::vector<SceneInfo> sceneInfos = {
    {"No scene", SceneCategory::Basics},
    {"Model viewer", SceneCategory::Models},
    {"Triangle", SceneCategory::Basics},
    {"Uniform rectangle", SceneCategory::Basics},
    {"Texture", SceneCategory::Textures},
    {"Transform+Face culling", SceneCategory::Basics},
    {"Phong lighting + materials", SceneCategory::Lighting},
    {"Depth testing", SceneCategory::Rendering},
    {"Stencil testing", SceneCategory::Rendering},
    {"Blending", SceneCategory::Rendering},
    {"Framebuffers", SceneCategory::Rendering},
    {"Cubemaps", SceneCategory::Textures},
    {"Asteroid belt", SceneCategory::Models},
    {"Solar system", SceneCategory::Models},
    {"Anti Aliasing", SceneCategory::Rendering},
    {"Advanced lighting: Phong / Blinn-Phong", SceneCategory::Lighting},
    {"Gamma correction", SceneCategory::Lighting}
};
const std::vector<SceneCategoryInfo> sceneCategories = {
    {SceneCategory::Basics, "Basics & geometry"},
    {SceneCategory::Textures, "Textures & environment maps"},
    {SceneCategory::Lighting, "Lighting & color"},
    {SceneCategory::Rendering, "Rendering techniques"},
    {SceneCategory::Models, "Models & instancing"}
};

}

const std::vector<SceneInfo>& availableScenes() { return sceneInfos; }
const std::vector<SceneCategoryInfo>& availableSceneCategories() { return sceneCategories; }
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
        case 11: return createCubemapScene(root);
        case 12: return createAsteroidBeltScene(root);
        case 13: return createSolarSystemScene(root);
        case 14: return createAntiAliasingScene(root);
        case 15: return createAdvancedLightingScene(root);
        case 16: return createGammaCorrectionScene(root);
        default: throw std::out_of_range("Invalid scene index");
    }
}
