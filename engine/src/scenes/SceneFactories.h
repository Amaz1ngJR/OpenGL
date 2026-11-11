#pragma once
#include "Scene.h"

// Each factory is implemented next to its scene, keeping concrete types private.
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
