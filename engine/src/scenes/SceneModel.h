#pragma once
#include "Shader.h"
#include <memory>

// 模型查看器与多物体场景共用导入、材质和骨骼动画；不暴露 Assimp 类型。
class SceneModel {
public:
    virtual ~SceneModel() = default;
    virtual double duration(unsigned clip) const = 0;
    virtual void update(int clip, double seconds) = 0;
    // placement 在模型归一化并移到地面后应用；深度 pass 不需要材质。
    virtual void drawPlaced(Shader& shader, const glm::mat4& placement, bool materials) const = 0;
};

std::unique_ptr<SceneModel> loadSceneModel(const std::string& path, bool animated);
