#pragma once
#include "Scene.h"

// 模型预览复用唯一窗口与主循环；可选 UTF-8 路径用于启动时直接打开模型。
std::unique_ptr<Scene> createModelScene(const std::string& root, const std::string& modelPath = "");
