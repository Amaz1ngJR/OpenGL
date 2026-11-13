#pragma once
#include "Scene.h"

// 工厂实现在各自的场景文件中；主入口只依赖 Scene 接口，不接触具体场景类。
// 返回 unique_ptr 让切换场景自动析构旧对象；必须在窗口/GL 上下文销毁前释放。
std::unique_ptr<Scene> createEmptyScene(const std::string& root);
std::unique_ptr<Scene> createTriangleScene(const std::string& root);
std::unique_ptr<Scene> createUniformRectangleScene(const std::string& root);
std::unique_ptr<Scene> createTextureScene(const std::string& root);
std::unique_ptr<Scene> createTransformScene(const std::string& root);
std::unique_ptr<Scene> createDepthTestingScene(const std::string& root);
std::unique_ptr<Scene> createStencilTestingScene(const std::string& root);
std::unique_ptr<Scene> createBlendingScene(const std::string& root);
std::unique_ptr<Scene> createLightingScene(const std::string& root);
std::unique_ptr<Scene> createFramebufferScene(const std::string& root, int effect = 0);
std::unique_ptr<Scene> createCubemapScene(const std::string& root, int cubeMode = 0, int sphereMode = 1);
std::unique_ptr<Scene> createAsteroidBeltScene(const std::string& root, int amount = 2000, bool instanced = true);
