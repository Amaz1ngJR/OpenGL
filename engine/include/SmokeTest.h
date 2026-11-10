#pragma once
#include <string>

struct GLFWwindow;

// 调用前须已初始化当前 OpenGL 上下文和 ImGui 后端；失败时抛出异常给主入口。
// modelPath 为空时仅验证未加载文件的模型场景，否则也检查指定模型的渲染结果。
void smokeTest(GLFWwindow* window, const std::string& root, const std::string& modelPath = "");
